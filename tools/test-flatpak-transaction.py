import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

path = Path(__file__).with_name('flatpak-transaction.py')
spec = importlib.util.spec_from_file_location('transaction', path)
transaction = importlib.util.module_from_spec(spec)
spec.loader.exec_module(transaction)


class TransactionTests(unittest.TestCase):
    def test_terminal_bar_is_not_embedded_in_stage_text(self):
        rows, pending = transaction.parse_terminal(
            'Installing 2/4… ███████░░░ 72% 10 MB/s\r'.encode())
        self.assertEqual(rows, [{'stage': 'Installing 2/4…', 'percent': 72}])
        self.assertEqual(pending, b'')
        rows, _ = transaction.parse_terminal(b'Installing [======>   ] 42%\r')
        self.assertEqual(rows, [{'stage': 'Installing', 'percent': 42}])

    def test_progress_only_rows_preserve_numeric_updates(self):
        rows, _ = transaction.parse_terminal('████ 57%\r'.encode())
        self.assertEqual(rows, [{'stage': '', 'percent': 57}])
        rows, _ = transaction.parse_terminal(b'Installing 33% 2 MB/s\r')
        self.assertEqual(rows, [{'stage': 'Installing', 'percent': 33}])

    def test_split_utf8_bar_and_osc_still_report_real_percent(self):
        raw = 'Installing… ███ 81%\r'.encode()
        split = raw.index('█'.encode()) + 1
        rows, pending = transaction.parse_terminal(raw[:split])
        self.assertEqual(rows, [])
        rows, pending = transaction.parse_terminal(raw[split:] + b'\x1b]9;4;1;82\x07', pending)
        self.assertEqual(rows, [{'stage': 'Installing…', 'percent': 81},
                                {'stage': '', 'percent': 82}])
        self.assertEqual(pending, b'')

    def test_split_osc_progress_and_step_reset(self):
        rows, pending = transaction.parse_terminal(b'Installing runtime\r\x1b]9;4;1;')
        self.assertEqual(rows, [{'stage': 'Installing runtime', 'percent': None}])
        rows, pending = transaction.parse_terminal(b'67\x1b\\Installing game\r\x1b]9;4;1;12\x1b\\', pending)
        self.assertEqual([row['percent'] for row in rows], [67, None, 12])
        self.assertEqual(pending, b'')

    def test_lifecycle_uncertainty_and_cleanup_failed_never_modify_flatpak(self):
        for state in ['generation=2 app=org.example.Game name=Game phase=CleanupFailed error=uncertain ready=false\n', 'state=Active\n', 'phase=Running\n']:
            with tempfile.TemporaryDirectory() as root, patch.dict(os.environ, {'XDG_RUNTIME_DIR': root}), \
                    patch.object(transaction, 'authorize'), \
                    patch.object(transaction.subprocess, 'Popen') as child, \
                    patch.object(transaction.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0, state, '')):
                with self.assertRaises(transaction.Busy):
                    transaction.run('install', 'org.example.Game')
                child.assert_not_called()

    def test_failed_status_probe_never_starts_a_transaction(self):
        with tempfile.TemporaryDirectory() as root, patch.dict(os.environ, {'XDG_RUNTIME_DIR': root}), \
                patch.object(transaction, 'authorize'), patch.object(transaction.subprocess, 'Popen') as child, \
                patch.object(transaction.subprocess, 'run', return_value=subprocess.CompletedProcess([], 1, '', 'bus unavailable')):
            with self.assertRaises(transaction.Busy):
                transaction.run('uninstall', 'org.example.Game')
            child.assert_not_called()

    def test_real_pty_relay_confirms_postcondition(self):
        original_popen = subprocess.Popen
        app = 'org.openttd.OpenTTD'

        def fake_popen(argv, **kwargs):
            self.assertIn('--assumeyes', argv)
            self.assertNotIn('--noninteractive', argv)
            code = ('import os,time; os.write(1,b"Installing\\r\\x1b]9;4;1;");'
                    'time.sleep(.02); os.write(1,bytes([52,53,27,92]));')
            return original_popen([sys.executable, '-c', code], **kwargs)

        with tempfile.TemporaryDirectory() as root, patch.dict(os.environ, {'XDG_RUNTIME_DIR': root}), \
                patch.object(transaction, 'authorize'), \
                patch.object(transaction.subprocess, 'Popen', side_effect=fake_popen), \
                patch.object(transaction.subprocess, 'run') as verify:
            verify.return_value.returncode = 0
            verify.side_effect = lambda argv, **kwargs: subprocess.CompletedProcess(argv, 0, f'{app}/aarch64/stable\n' if 'list' in argv else 'phase=Exited\nstate=Idle\n' if 'status' in argv else '', '')
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                self.assertEqual(transaction.run('install', app), 0)
            events = [json.loads(line) for line in out.getvalue().splitlines()]
            self.assertIn(45, [event.get('percent') for event in events])
            self.assertTrue(events[-1]['success'])
            verify.side_effect = lambda argv, **kwargs: subprocess.CompletedProcess(argv, 0, 'phase=Exited\nstate=Idle\n' if 'status' in argv else '', '')
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(transaction.run('install', app), 1)


if __name__ == '__main__':
    unittest.main()
