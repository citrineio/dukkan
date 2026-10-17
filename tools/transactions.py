#!/usr/bin/env python3
"""Dukkan's user-service transaction queue. Python standard library only."""
import argparse
import asyncio
import fcntl
import json
import os
from pathlib import Path
import re
import signal
import socket
import sqlite3
import struct
import sys
import time
import uuid

TERMINAL = {'succeeded', 'failed', 'cancelled', 'interrupted'}
ID = re.compile(r'[a-f0-9]{32}')
ROOT = Path(__file__).resolve().parent


def private_dir(path):
    path.mkdir(mode=0o700, parents=True, exist_ok=True)
    info = path.lstat()
    if path.is_symlink() or not path.is_dir() or info.st_uid != os.getuid() or info.st_mode & 0o077:
        raise RuntimeError('Unsafe private directory: ' + str(path))
    return path


def runtime():
    return Path(os.environ.get('XDG_RUNTIME_DIR', f'/run/user/{os.getuid()}'))


def socket_path():
    return runtime() / 'dukkan-transactions/control.sock'


def database_path():
    return Path(os.environ.get('XDG_STATE_HOME', str(Path.home() / '.local/state'))) / 'dukkan/transactions.sqlite3'


def catalog(app):
    module = os.environ.get('ZAMAN_CATALOG_LIBEXEC', '/usr/local/libexec/zaman')
    if not os.path.isabs(module):
        raise ValueError('ZAMAN_CATALOG_LIBEXEC must be absolute')
    if module not in sys.path:
        sys.path.insert(0, module)
    from zaman_catalog import require_enabled
    return require_enabled(app)


class Queue:
    def __init__(self, path):
        private_dir(path.parent)
        if path.is_symlink():
            raise RuntimeError('Unsafe transaction database link')
        self.db = sqlite3.connect(path)
        self.db.execute('PRAGMA journal_mode=WAL')
        self.db.execute('PRAGMA synchronous=FULL')
        version = self.db.execute('PRAGMA user_version').fetchone()[0]
        if version not in (0, 1):
            raise RuntimeError('Unsupported transaction database; keep it for recovery')
        self.db.executescript('''CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT NOT NULL);
            CREATE TABLE IF NOT EXISTS jobs (id TEXT PRIMARY KEY, request TEXT UNIQUE NOT NULL,
                created REAL NOT NULL, data TEXT NOT NULL);
            CREATE TABLE IF NOT EXISTS requests (request TEXT PRIMARY KEY, job TEXT NOT NULL);
            INSERT OR IGNORE INTO requests SELECT request, id FROM jobs;
            PRAGMA user_version=1;''')
        with self.db:
            self.db.execute('INSERT OR IGNORE INTO meta VALUES (?, ?)', ('identity', uuid.uuid4().hex))
            self.db.execute('INSERT OR IGNORE INTO meta VALUES (?, ?)', ('event', '0'))
        self.identity = self.db.execute("SELECT value FROM meta WHERE key='identity'").fetchone()[0]

    def all(self):
        return [json.loads(row[0]) for row in self.db.execute('SELECT data FROM jobs ORDER BY created, rowid')]

    def get(self, job_id):
        row = self.db.execute('SELECT data FROM jobs WHERE id=?', (job_id,)).fetchone()
        if not row:
            raise ValueError('Unknown transaction')
        return json.loads(row[0])

    def put(self, job):
        job['updated'] = time.time()
        with self.db:
            self.db.execute('INSERT INTO jobs VALUES (?, ?, ?, ?) ON CONFLICT(id) DO UPDATE SET data=excluded.data',
                (job['id'], job['request'], job['created'], json.dumps(job, ensure_ascii=False)))
            self.db.execute('INSERT OR IGNORE INTO requests VALUES (?, ?)', (job['request'], job['id']))

    def submit(self, app, operation, request, retry_of=''):
        if not isinstance(request, str) or not ID.fullmatch(request):
            raise ValueError('request must be a UUID hex string')
        if operation not in ('install', 'uninstall'):
            raise ValueError('Unknown transaction operation')
        row = self.db.execute('SELECT jobs.data FROM requests JOIN jobs ON requests.job=jobs.id WHERE requests.request=?', (request,)).fetchone()
        if row:
            job = json.loads(row[0])
            if (job['app'], job['operation']) != (app, operation):
                raise ValueError('Request ID already belongs to another operation')
            return job
        game = catalog(app)
        pending = [j for j in self.all() if j['phase'] not in TERMINAL]
        for job in pending:
            if job['app'] == app:
                if job['operation'] != operation:
                    raise ValueError('This game already has a different queued operation')
                with self.db:
                    self.db.execute('INSERT INTO requests VALUES (?, ?)', (request, job['id']))
                return job
        if len(pending) >= 32:
            raise ValueError('Queue is full; finish or cancel an existing job')
        job = dict(id=uuid.uuid4().hex, request=request, app=app, name=game['name'], operation=operation,
            phase='queued', percent=None, stage='Queued', result='', error='', created=time.time(),
            cancelled=False, event=0, delivered=False, libraryPending=False, libraryError='', retryOf=retry_of)
        self.put(job)
        return job

    def finish(self, job, phase, result, error=''):
        # Terminal result and outbox sequence are committed together.
        if job['phase'] in TERMINAL:
            return
        with self.db:
            sequence = int(self.db.execute("SELECT value FROM meta WHERE key='event'").fetchone()[0]) + 1
            job.update(phase=phase, result=result, error=error[:600], percent=None,
                stage=result, event=sequence, delivered=False, libraryPending=True, updated=time.time())
            self.db.execute("UPDATE meta SET value=? WHERE key='event'", (str(sequence),))
            self.db.execute('UPDATE jobs SET data=? WHERE id=?', (json.dumps(job, ensure_ascii=False), job['id']))


async def command(*argv, timeout=12):
    proc = await asyncio.create_subprocess_exec(*argv, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
    try:
        out, err = await asyncio.wait_for(proc.communicate(), timeout)
    except BaseException:
        if proc.returncode is None:
            proc.kill()
        await proc.wait()
        raise
    if proc.returncode:
        raise RuntimeError(err.decode(errors='replace').strip()[:400] or f'{argv[0]} exited {proc.returncode}')
    return out.decode(errors='replace')


async def installed(app):
    rows = (await command('/usr/bin/flatpak', 'list', '--user', '--app', '--columns=ref:f')).splitlines()
    ref = f'{app}/aarch64/stable'
    return ref in rows or 'app/' + ref in rows


def game_lock():
    root = private_dir(runtime() / 'zaman-installed-game')
    fd = os.open(root / 'transaction.lock', os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    try:
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BaseException:
        os.close(fd)
        raise
    return fd


class Service:
    def __init__(self, queue):
        self.queue, self.worker, self.active = queue, None, ''
        self.recover = {j['id'] for j in queue.all() if j['phase'] not in TERMINAL | {'queued', 'waiting'}}
        self.navigation = None
        self.revision = 0
        self.stopping = False

    def snapshot(self):
        jobs = self.queue.all()
        pending = [j for j in jobs if j['phase'] not in TERMINAL]
        recent = [j for j in jobs if j['phase'] in TERMINAL][-64:]
        if self.navigation and not any(j['id'] == self.navigation['id'] for j in pending + recent):
            recent.append(self.queue.get(self.navigation['id']))
        return dict(type='transactions', version=1, revision=self.revision, jobs=pending + recent,
            active=self.active, queued=len(pending), navigation=self.navigation)

    def dispatch(self, msg):
        action = msg.get('action')
        job = None
        if action == 'submit':
            job = self.queue.submit(msg.get('app'), msg.get('operation'), msg.get('request'))
        elif action in ('get', 'cancel', 'retry', 'navigate'):
            job = self.queue.get(msg.get('id'))
            if action == 'cancel' and job['phase'] not in TERMINAL:
                if job['id'] == self.active:
                    job.update(cancelled=True, phase='cancelling', stage='Requesting cancellation…')
                    self.queue.put(job)
                    if self.worker and self.worker.returncode is None:
                        self.worker.terminate()
                elif job['id'] in self.recover:
                    job['cancelled'] = True
                    self.queue.put(job)  # Reconcile uncertain work before a terminal result.
                else:
                    self.queue.finish(job, 'cancelled', 'Cancelled before starting')
            elif action == 'retry':
                if job['phase'] not in ('failed', 'cancelled', 'interrupted'):
                    raise ValueError('Only a stopped transaction can be retried')
                job = self.queue.submit(job['app'], job['operation'], msg.get('request'), job['id'])
            elif action == 'navigate':
                catalog(job['app'])
                self.navigation = dict(id=job['id'], app=job['app'], nonce=uuid.uuid4().hex)
        elif action != 'snapshot':
            raise ValueError('Unknown transaction action')
        self.revision += 1
        result = self.snapshot()
        if job:
            result['job'] = job
        return result

    async def reconcile(self, job):
        fd = game_lock()  # Never inspect an uncertain commit while another worker/game holds it.
        try:
            found = await installed(job['app'])
        finally:
            os.close(fd)
        desired = found == (job['operation'] == 'install')
        self.queue.finish(job, 'succeeded' if desired else 'cancelled' if job['cancelled'] else 'interrupted',
            'Recovered: installation verified' if desired and found else
            'Recovered: removal verified' if desired else 'Transaction cancelled' if job['cancelled'] else 'Interrupted; review and retry',
            '' if desired else 'The requested Flatpak postcondition is absent. No automatic retry was made.')

    async def execute(self, job):
        try:
            catalog(job['app'])  # Validate again when dequeued; never trust an old UI snapshot.
        except (ValueError, OSError) as error:
            self.queue.finish(job, 'failed', 'Catalog validation failed', str(error))
            return
        self.active = job['id']
        job.update(phase='running', stage='Preparing transaction…', percent=None)
        self.queue.put(job)  # Durable before creating any modifying process.
        result = None
        try:
            self.worker = await asyncio.create_subprocess_exec('/usr/bin/python3', str(ROOT / 'flatpak-transaction.py'),
                job['operation'], job['app'], stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.DEVNULL, limit=16384)
            if self.queue.get(job['id'])['cancelled']:
                self.worker.terminate()
            while line := await self.worker.stdout.readline():
                event = json.loads(line)
                job = self.queue.get(job['id'])
                if event.get('type') == 'progress':
                    job.update(stage=str(event.get('stage', ''))[:200], percent=event.get('percent'))
                    self.queue.put(job)
                elif event.get('type') == 'result':
                    result = event
            await self.worker.wait()
            job = self.queue.get(job['id'])
            if result and result.get('blocked') and not job['cancelled']:
                job.update(phase='waiting', stage='Waiting for games or another transaction to close', percent=None)
                self.queue.put(job)
                return
            # A crashed relay cannot be trusted. Reconcile actual state after it exits.
            fd = game_lock()
            try:
                found = await installed(job['app'])
            finally:
                os.close(fd)
            desired = found == (job['operation'] == 'install')
            if desired:
                self.queue.finish(job, 'succeeded', 'Installation complete' if found else 'Game removed')
            elif job['cancelled']:
                self.queue.finish(job, 'cancelled', 'Transaction cancelled')
            else:
                self.queue.finish(job, 'failed' if result else 'interrupted', 'Transaction failed',
                    result.get('message', 'Worker failed') if result else 'Worker exited without a verified terminal result')
        except asyncio.CancelledError:
            raise  # Leave running state for reconciliation; service shutdown is not user cancellation.
        except Exception as error:
            self.recover.add(job['id'])
            job = self.queue.get(job['id'])
            job.update(stage='Recovery required: ' + str(error)[:160])
            self.queue.put(job)
        finally:
            if self.worker and self.worker.returncode is None:
                self.worker.terminate()
                try:
                    await asyncio.wait_for(self.worker.wait(), 10)
                except asyncio.TimeoutError:
                    self.worker.kill()
                    await self.worker.wait()
            self.worker, self.active = None, ''

    async def pump(self):
        last_error = ''
        while not self.stopping:
            try:
                for job_id in list(self.recover):
                    await self.reconcile(self.queue.get(job_id))
                    self.recover.discard(job_id)
                pending = [j for j in self.queue.all() if j['phase'] not in TERMINAL]
                if pending and not self.recover:
                    await self.execute(pending[0])
                last_error = ''
            except Exception as error:
                if str(error) != last_error:
                    print('Queue waiting/recovery: ' + str(error), file=sys.stderr, flush=True)
                    last_error = str(error)
            await asyncio.sleep(1)

    async def deliver(self):
        last_error = ''
        while not self.stopping:
            try:
                jobs = self.queue.all()
                # Library publication is retried independently of the verified transaction outcome.
                dirty = next((j for j in jobs if j['phase'] in TERMINAL and j['libraryPending']), None)
                if dirty:
                    try:
                        fd = game_lock()
                        try:
                            await command(os.environ.get('ZAMANCTL', '/usr/local/bin/zamanctl'), 'library', 'sync', timeout=30)
                        finally:
                            os.close(fd)
                        dirty.update(libraryPending=False, libraryError='')
                    except Exception as error:
                        dirty['libraryError'] = str(error)[:300]
                    self.queue.put(dirty)
                pending = sorted((j for j in jobs if j['event'] and not j['delivered']), key=lambda j: j['event'])
                if pending:
                    job = self.queue.get(pending[0]['id'])
                    key = f'transaction:{self.queue.identity}:{job["event"]}'
                    await self.notify(job, key)
                    job = self.queue.get(job['id'])
                    job['delivered'] = True
                    self.queue.put(job)
                last_error = ''
            except Exception as error:
                if str(error) != last_error:
                    print('Delivery pending: ' + str(error), file=sys.stderr, flush=True)
                    last_error = str(error)
            await asyncio.sleep(3)

    async def notify(self, job, key):
        path = runtime() / 'zaman-notifications/control.sock'
        reader, writer = await asyncio.wait_for(asyncio.open_unix_connection(str(path), limit=262144), 2)
        try:
            msg = dict(action='event', source='Dukkan', key=key, title=job['result'],
                body=job['name'] + (': ' + job['error'] if job['error'] else '') + (' · Library refresh pending' if job['libraryPending'] else ''), category='install',
                state='installed' if job['phase'] == 'succeeded' and job['operation'] == 'install' else
                    'removed' if job['phase'] == 'succeeded' else 'cancelled' if job['phase'] == 'cancelled' else 'error',
                target='dukkan-transaction:' + job['id'])
            writer.write((json.dumps(msg) + '\n').encode())
            await asyncio.wait_for(writer.drain(), 2)
            reply = json.loads(await asyncio.wait_for(reader.readline(), 3))
            if reply.get('acceptedEvent') != key:
                raise RuntimeError(reply.get('error', 'Notification server lacks durable receipt support'))
        finally:
            writer.close()
            await writer.wait_closed()

    async def peer(self, reader, writer):
        try:
            credentials = writer.get_extra_info('socket').getsockopt(socket.SOL_SOCKET, socket.SO_PEERCRED, 12)
            if struct.unpack('3i', credentials)[1] != os.getuid():
                return
            msg = json.loads(await asyncio.wait_for(reader.readline(), 3))
            if not isinstance(msg, dict):
                raise ValueError('Expected a JSON object')
            result = self.dispatch(msg)
        except Exception as error:
            result = dict(type='error', error=str(error)[:500])
        try:
            writer.write((json.dumps(result, ensure_ascii=False) + '\n').encode())
            await asyncio.wait_for(writer.drain(), 3)
        finally:
            writer.close()
            await writer.wait_closed()


async def serve():
    os.umask(0o077)
    root = private_dir(socket_path().parent)
    singleton = os.open(root / 'service.lock', os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    fcntl.flock(singleton, fcntl.LOCK_EX | fcntl.LOCK_NB)
    queue = Queue(database_path())
    service = Service(queue)
    socket_path().unlink(missing_ok=True)
    server = await asyncio.start_unix_server(service.peer, str(socket_path()), limit=8192)
    os.chmod(socket_path(), 0o600)
    stop = asyncio.Event()
    loop = asyncio.get_running_loop()
    for signum in (signal.SIGTERM, signal.SIGINT):
        loop.add_signal_handler(signum, stop.set)
    tasks = [asyncio.create_task(service.pump()), asyncio.create_task(service.deliver())]
    print('Dukkan transaction service ready', flush=True)
    try:
        async with server:
            await stop.wait()
    finally:
        service.stopping = True
        for task in tasks:
            task.cancel()
        await asyncio.gather(*tasks, return_exceptions=True)
        socket_path().unlink(missing_ok=True)
        queue.db.close()
        os.close(singleton)


def request(message):
    with socket.socket(socket.AF_UNIX) as sock:
        sock.settimeout(4)
        sock.connect(str(socket_path()))
        sock.sendall((json.dumps(message) + '\n').encode())
        data = bytearray()
        while b'\n' not in data:
            block = sock.recv(65536)
            if not block or len(data) > 2 * 1024 * 1024:
                raise RuntimeError('Incomplete or oversized service reply')
            data.extend(block)
        reply = json.loads(data.split(b'\n', 1)[0])
        if reply.get('type') == 'error':
            raise RuntimeError(reply['error'])
        return reply


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('serve', 'snapshot', 'get', 'cancel', 'retry', 'navigate', 'submit'))
    parser.add_argument('id', nargs='?')
    parser.add_argument('--operation', choices=('install', 'uninstall'))
    parser.add_argument('--request', default=None)
    args = parser.parse_args()
    try:
        if args.action == 'serve':
            asyncio.run(serve())
        else:
            msg = dict(action=args.action, id=args.id)
            if args.action in ('submit', 'retry'):
                msg.update(app=args.id, operation=args.operation, request=args.request or uuid.uuid4().hex)
            print(json.dumps(request(msg), ensure_ascii=False))
        return 0
    except Exception as error:
        print(json.dumps(dict(type='error', error=str(error))), file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
