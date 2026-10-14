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
    def test_split_osc_progress_and_step_reset(self):
        rows, pending = transaction.parse_terminal(b'Installing runtime\r\x1b]9;4;1;')
        self.assertEqual(rows, [{'stage': 'Installing runtime', 'percent': None}])
        rows, pending = transaction.parse_terminal(b'67\x1b\\Installing game\r\x1b]9;4;1;12\x1b\\', pending)
        self.assertEqual([row['percent'] for row in rows], [67, None, 12])
        self.assertEqual(pending, b'')

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
                patch.object(transaction.subprocess, 'Popen', side_effect=fake_popen), \
                patch.object(transaction.subprocess, 'run') as verify:
            verify.return_value.returncode = 0
            verify.return_value.stdout = f'{app}/aarch64/stable\n'
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                self.assertEqual(transaction.run('install', app), 0)
            events = [json.loads(line) for line in out.getvalue().splitlines()]
            self.assertIn(45, [event.get('percent') for event in events])
            self.assertTrue(events[-1]['success'])
            verify.return_value.stdout = ''
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(transaction.run('install', app), 1)


if __name__ == '__main__':
    unittest.main()
