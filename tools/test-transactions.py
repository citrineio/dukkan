#!/usr/bin/env python3
"""Offline queue/recovery tests. No real Flatpak or sessiond mutation."""
import asyncio
import importlib.util
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import AsyncMock, patch

spec = importlib.util.spec_from_file_location('transactions', Path(__file__).with_name('transactions.py'))
t = importlib.util.module_from_spec(spec)
spec.loader.exec_module(t)


class QueueTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.path = Path(self.tmp.name) / 'state/queue.sqlite3'
        self.queue = t.Queue(self.path)
        self.catalog = patch.object(t, 'catalog', side_effect=lambda app: {'name': app})
        self.catalog.start()

    def tearDown(self):
        self.catalog.stop()
        self.queue.db.close()
        self.tmp.cleanup()

    def submit(self, app='org.example.Game', operation='install', request=None):
        return self.queue.submit(app, operation, request or t.uuid.uuid4().hex)

    def test_reopen_keeps_identity_and_queue(self):
        job = self.submit()
        other = t.Queue(self.path)
        self.assertEqual(other.identity, self.queue.identity)
        self.assertEqual(other.get(job['id']), job)
        other.db.close()

    def test_duplicate_request_returns_same_transaction(self):
        request = t.uuid.uuid4().hex
        first = self.submit(request=request)
        self.assertEqual(self.submit(request=request)['id'], first['id'])
        self.assertEqual(self.submit()['id'], first['id'])
        self.assertEqual(len(self.queue.all()), 1)

    def test_coalesced_request_alias_remains_idempotent_after_completion(self):
        first = self.submit(request='a' * 32)
        duplicate = self.submit(request='b' * 32)
        self.assertEqual(first['id'], duplicate['id'])
        self.queue.finish(first, 'succeeded', 'Installed')
        self.assertEqual(self.submit(request='b' * 32)['id'], first['id'])
        self.assertEqual(len(self.queue.all()), 1)

    def test_conflicting_duplicate_is_rejected(self):
        job = self.submit()
        with self.assertRaises(ValueError):
            self.submit('org.example.Other', request=job['request'])
        with self.assertRaises(ValueError):
            self.submit(operation='uninstall')

    def test_queue_capacity_is_bounded(self):
        for index in range(32):
            self.submit('org.example.Game' + str(index))
        with self.assertRaises(ValueError):
            self.submit('org.example.Overflow')

    def test_catalog_validated_at_submission(self):
        with patch.object(t, 'catalog', side_effect=ValueError('disabled')):
            with self.assertRaises(ValueError):
                self.submit()
        self.assertEqual(self.queue.all(), [])

    def test_bad_operation_and_request_rejected(self):
        for operation, request in [('update', 'a' * 32), ('install', '../../command')]:
            with self.assertRaises(ValueError):
                self.queue.submit('org.example.Game', operation, request)

    def test_outbox_and_result_are_one_commit(self):
        job = self.submit()
        self.queue.finish(job, 'succeeded', 'Installed')
        self.assertEqual(self.queue.get(job['id'])['event'], 1)
        self.assertFalse(self.queue.get(job['id'])['delivered'])
        self.queue.finish(job, 'failed', 'Duplicate callback')
        self.assertEqual(self.queue.get(job['id'])['phase'], 'succeeded')
        second = self.submit('org.example.Other')
        self.queue.finish(second, 'cancelled', 'Cancelled')
        self.assertEqual(second['event'], 2)

    def test_queued_cancel_and_retry_have_distinct_stable_ids(self):
        service = t.Service(self.queue)
        job = self.submit()
        service.dispatch({'action': 'cancel', 'id': job['id']})
        self.assertEqual(self.queue.get(job['id'])['phase'], 'cancelled')
        result = service.dispatch({'action': 'retry', 'id': job['id'], 'request': t.uuid.uuid4().hex})
        self.assertNotEqual(result['job']['id'], job['id'])
        self.assertEqual(result['job']['retryOf'], job['id'])

    def test_navigation_resolves_known_job(self):
        service = t.Service(self.queue)
        job = self.submit()
        with self.assertRaises(ValueError):
            service.dispatch({'action': 'navigate', 'id': '../../shell'})
        result = service.dispatch({'action': 'navigate', 'id': job['id']})
        self.assertEqual(result['navigation']['app'], job['app'])

    def test_recovery_only_for_previously_started_work(self):
        queued = self.submit()
        running = self.submit('org.example.Running')
        running['phase'] = 'running'
        self.queue.put(running)
        service = t.Service(self.queue)
        self.assertEqual(service.recover, {running['id']})
        self.assertNotIn(queued['id'], service.recover)

    def test_recovery_reconciles_install_and_remove(self):
        async def check(operation, present, phase):
            job = self.submit('org.example.' + operation + str(present), operation)
            job['phase'] = 'running'
            self.queue.put(job)
            fd = os.open(os.devnull, os.O_RDONLY)
            with patch.object(t, 'game_lock', return_value=fd), patch.object(t, 'installed', AsyncMock(return_value=present)):
                await t.Service(self.queue).reconcile(job)
            self.assertEqual(self.queue.get(job['id'])['phase'], phase)
        async def run():
            await check('install', True, 'succeeded')
            await check('install', False, 'interrupted')
            await check('uninstall', False, 'succeeded')
            await check('uninstall', True, 'interrupted')
        asyncio.run(run())

    def test_recovery_waits_for_lock_without_claiming_success(self):
        job = self.submit()
        job['phase'] = 'running'
        self.queue.put(job)
        with patch.object(t, 'game_lock', side_effect=BlockingIOError):
            with self.assertRaises(BlockingIOError):
                asyncio.run(t.Service(self.queue).reconcile(job))
        self.assertEqual(self.queue.get(job['id'])['event'], 0)

    def test_real_lock_excludes_second_owner(self):
        with patch.dict(os.environ, {'XDG_RUNTIME_DIR': self.tmp.name}):
            fd = t.game_lock()
            try:
                with self.assertRaises(BlockingIOError):
                    t.game_lock()
            finally:
                os.close(fd)
            os.close(t.game_lock())

    def test_worker_uses_verified_result_after_network_failure_or_cancel(self):
        async def check(app, result, present, phase, cancelled=False):
            job = self.submit(app)
            service = t.Service(self.queue)
            reader = asyncio.StreamReader()
            if result is not None:
                reader.feed_data((json.dumps({'type': 'result', **result}) + '\n').encode())
            reader.feed_eof()
            class Worker:
                stdout = reader
                returncode = None
                def terminate(self):
                    self.returncode = -15
                def kill(self):
                    self.returncode = -9
                async def wait(self):
                    self.returncode = 1
                    return 1
            async def spawn(*args, **kwargs):
                if cancelled:
                    current = self.queue.get(job['id'])
                    current['cancelled'] = True
                    self.queue.put(current)
                return Worker()
            with patch.object(t.asyncio, 'create_subprocess_exec', spawn), \
                patch.object(t, 'installed', AsyncMock(return_value=present)), \
                patch.object(t, 'game_lock', side_effect=lambda: os.open(os.devnull, os.O_RDONLY)):
                await service.execute(job)
            self.assertEqual(self.queue.get(job['id'])['phase'], phase)
        async def run():
            await check('org.example.Network', {'success': False, 'message': 'Network lost'}, False, 'failed')
            await check('org.example.Cancel', {'success': False}, False, 'cancelled', True)
            await check('org.example.LateCancel', {'success': False}, True, 'succeeded', True)
            await check('org.example.Crash', None, False, 'interrupted')
            await check('org.example.CrashAfterCommit', None, True, 'succeeded')
            await check('org.example.GameLock', {'blocked': True}, False, 'waiting')
        asyncio.run(run())

    def test_catalog_revocation_before_execution(self):
        job = self.submit()
        with patch.object(t, 'catalog', side_effect=ValueError('disabled')):
            asyncio.run(t.Service(self.queue).execute(job))
        self.assertEqual(self.queue.get(job['id'])['phase'], 'failed')


class SocketTests(unittest.IsolatedAsyncioTestCase):
    async def test_client_disconnect_does_not_stop_accepted_job_and_queue_is_serial(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(t, 'catalog', return_value={'name': 'Game'}):
            queue = t.Queue(Path(directory) / 'state/queue.sqlite3')
            service = t.Service(queue)
            first_started, release_first, second_started = asyncio.Event(), asyncio.Event(), asyncio.Event()
            starts = []
            async def execute(job):
                starts.append(job['id'])
                job['phase'] = 'running'
                queue.put(job)
                if len(starts) == 1:
                    first_started.set()
                    await release_first.wait()
                else:
                    second_started.set()
                queue.finish(job, 'succeeded', 'Installed')
            service.execute = execute
            try:
                server = await asyncio.start_unix_server(service.peer, str(Path(directory) / 'control.sock'), limit=8192)
            except PermissionError as error:
                queue.db.close()
                self.skipTest('Unix sockets unavailable in this authoring environment: ' + str(error))
            pump = asyncio.create_task(service.pump())
            async def request(msg):
                reader, writer = await asyncio.open_unix_connection(str(Path(directory) / 'control.sock'))
                writer.write((json.dumps(msg) + '\n').encode())
                await writer.drain()
                data = json.loads(await asyncio.wait_for(reader.readline(), 3))
                writer.close()
                await writer.wait_closed()
                return data
            try:
                first = await request(dict(action='submit', app='org.example.First', operation='install', request='a' * 32))
                await asyncio.wait_for(first_started.wait(), 3)
                second = await request(dict(action='submit', app='org.example.Second', operation='install', request='b' * 32))
                # Every request uses a fresh client connection; the first UI has gone away.
                snapshot = await request(dict(action='snapshot'))
                self.assertEqual(snapshot['jobs'][0]['id'], first['job']['id'])
                self.assertEqual(snapshot['jobs'][0]['phase'], 'running')
                self.assertEqual(snapshot['jobs'][1]['phase'], 'queued')
                self.assertEqual(len(starts), 1)
                release_first.set()
                await asyncio.wait_for(second_started.wait(), 3)
                self.assertEqual(starts, [first['job']['id'], second['job']['id']])
            finally:
                pump.cancel()
                await asyncio.gather(pump, return_exceptions=True)
                server.close()
                await server.wait_closed()
                queue.db.close()

    async def test_notification_ack_is_required_even_when_socket_delivery_succeeds(self):
        with tempfile.TemporaryDirectory() as directory, patch.dict(os.environ, {'XDG_RUNTIME_DIR': directory}), \
             patch.object(t, 'catalog', return_value={'name': 'Game'}):
            queue = t.Queue(Path(directory) / 'state/queue.sqlite3')
            job = queue.submit('org.example.Game', 'install', 'a' * 32)
            queue.finish(job, 'succeeded', 'Installed')
            root = Path(directory) / 'zaman-notifications'
            root.mkdir()
            replies = [{'type':'notifications'}, {'acceptedEvent':'transaction:' + queue.identity + ':1'}]
            async def peer(reader, writer):
                await reader.readline()
                writer.write((json.dumps(replies.pop(0)) + '\n').encode())
                await writer.drain()
                writer.close()
                await writer.wait_closed()
            try:
                server = await asyncio.start_unix_server(peer, str(root / 'control.sock'))
            except PermissionError as error:
                queue.db.close()
                self.skipTest('Unix sockets unavailable in this authoring environment: ' + str(error))
            try:
                service = t.Service(queue)
                key = 'transaction:' + queue.identity + ':1'
                with self.assertRaises(RuntimeError):
                    await service.notify(job, key)
                self.assertFalse(queue.get(job['id'])['delivered'])
                await service.notify(job, key)
            finally:
                server.close()
                await server.wait_closed()
                queue.db.close()


if __name__ == '__main__':
    unittest.main()
