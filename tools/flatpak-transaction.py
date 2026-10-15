#!/usr/bin/env python3
"""Relay a curated Flatpak transaction's terminal progress as JSON lines."""
import argparse
import ctypes
import fcntl
import json
import os
import pty
import re
import select
import signal
import subprocess
import sys
import time

APPS = {'org.supertuxproject.SuperTux', 'net.supertuxkart.SuperTuxKart',
        'org.wesnoth.Wesnoth', 'org.openttd.OpenTTD', 'org.opensurge2d.OpenSurge'}
OSC = re.compile(rb'\x1b\]9;4;([01])(?:;([0-9]{1,3}))?(?:\x1b\\|\x07)')
ANSI = re.compile(rb'\x1b\[[0-9;?]*[ -/]*[@-~]')
PERCENT = re.compile(rb'(?<!\d)([0-9]{1,3})\s*%')
TEXT_PERCENT = re.compile(r'(?<!\d)[0-9]{1,3}\s*%')
TEXT_BAR = re.compile(r'[\u2580-\u259f\u2800-\u28ff]|\[[#=\-> .]{3,}\]')
PR_SET_PDEATHSIG = 1


def emit(**fields):
    print(json.dumps(fields, ensure_ascii=False), flush=True)


def terminal_row(raw):
    """Keep phase text and numeric progress separate from terminal graphics.

    Flatpak can print a Unicode bar, percent and transfer stats in the same
    row. The QML progress track is the only bar we display.
    """
    clean = ANSI.sub(b'', raw)
    text = clean.decode('utf-8', 'replace').strip()
    match = PERCENT.search(clean)
    value = int(match.group(1)) if match else None
    percent = value if value is not None and value <= 100 else None
    bar, number = TEXT_BAR.search(text), TEXT_PERCENT.search(text)
    cutoff = min((match.start() for match in (bar, number) if match), default=len(text))
    stage = ''.join(character for character in text[:cutoff]
                    if character.isprintable() or character == '\t').strip()
    stage = ' '.join(stage.split())[:180]
    return {'stage': stage, 'percent': percent} if stage or percent is not None else None


def parse_terminal(data, pending=b''):
    """Yield terminal rows; retain partial OSC and UTF-8 fragments for next read."""
    pending += data
    rows = []
    while True:
        osc = OSC.search(pending)
        newline = min((n for n in (pending.find(b'\r'), pending.find(b'\n')) if n >= 0), default=-1)
        if osc and (newline < 0 or osc.start() <= newline):
            if osc.start():
                row = terminal_row(pending[:osc.start()])
                if row:
                    rows.append(row)
            value = int(osc.group(2)) if osc.group(1) == b'1' and osc.group(2) else None
            rows.append({'stage': '', 'percent': value if value is not None and value <= 100 else None})
            pending = pending[osc.end():]
        elif newline >= 0:
            raw, pending = pending[:newline], pending[newline + 1:]
            row = terminal_row(raw)
            if row:
                rows.append(row)
        else:
            break
    if len(pending) > 8192:
        pending = pending[-4096:]
    return rows, pending


def child_setup():
    libc = ctypes.CDLL(None, use_errno=True)
    if libc.prctl(PR_SET_PDEATHSIG, signal.SIGTERM, 0, 0, 0):
        raise OSError(ctypes.get_errno(), 'prctl(PR_SET_PDEATHSIG) failed')
    if os.getppid() == 1:
        os.kill(os.getpid(), signal.SIGTERM)


def run(operation, app):
    if app not in APPS:
        raise ValueError('Application is outside the curated catalog')
    runtime = os.environ.get('XDG_RUNTIME_DIR', f'/run/user/{os.getuid()}')
    root = os.path.join(runtime, 'zaman-installed-game')
    os.makedirs(root, mode=0o700, exist_ok=True)
    stat = os.stat(root, follow_symlinks=False)
    if not os.path.isdir(root) or stat.st_uid != os.getuid() or stat.st_mode & 0o077:
        raise RuntimeError('Unsafe managed-game runtime directory')
    lock_fd = os.open(os.path.join(root, 'transaction.lock'),
                      os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    try:
        fcntl.flock(lock_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        os.close(lock_fd)
        raise RuntimeError('A managed game or Flatpak transaction is already active')
    if operation == 'install':
        argv = ['/usr/bin/flatpak', 'install', '--user', '--assumeyes',
                '--arch=aarch64', 'flathub', f'app/{app}/aarch64/stable']
    else:
        argv = ['/usr/bin/flatpak', 'uninstall', '--user', '--assumeyes',
                f'app/{app}/aarch64/stable']
    env = os.environ.copy()
    env.update(TERM='xterm', FLATPAK_TTY_PROGRESS='1', LC_ALL='C.UTF-8')
    env.pop('FLATPAK_DEBUG', None)
    master, slave = pty.openpty()
    child = subprocess.Popen(argv, stdin=subprocess.DEVNULL, stdout=slave, stderr=slave,
                             env=env, preexec_fn=child_setup, start_new_session=True)
    os.close(slave)
    stopping = False
    stop_deadline = None

    def stop(_signum, _frame):
        nonlocal stopping, stop_deadline
        stopping = True
        stop_deadline = time.monotonic() + 5
        if child.poll() is None:
            child.terminate()

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    pending = b''
    stage = 'Preparing transaction…'
    percent = None
    emit(type='progress', stage=stage, percent=percent)
    try:
        while True:
            ready, _, _ = select.select([master], [], [], 0.2)
            if ready:
                try:
                    chunk = os.read(master, 4096)
                except OSError:
                    chunk = b''
                rows, pending = parse_terminal(chunk, pending)
                for row in rows:
                    if row['stage']:
                        stage = row['stage']
                        # A new operation starts indeterminate until its own percent arrives.
                        percent = row['percent']
                    else:
                        percent = row['percent']
                    emit(type='progress', stage=stage, percent=percent)
                if not chunk and child.poll() is not None:
                    break
            if child.poll() is not None and not ready:
                break
            if stopping and stop_deadline is not None and time.monotonic() >= stop_deadline and child.poll() is None:
                child.kill()
        code = child.wait()
        success = code == 0 and not stopping
        if success:
            check = subprocess.run(['/usr/bin/flatpak', 'list', '--user', '--app', '--columns=ref:f'],
                                   capture_output=True, text=True, timeout=8)
            expected = f'{app}/aarch64/stable'
            found = expected in check.stdout.splitlines() or f'app/{expected}' in check.stdout.splitlines()
            success = check.returncode == 0 and (found if operation == 'install' else not found)
        emit(type='result', success=success, exit_code=code,
             message='Completed' if success else ('Cancelled' if stopping else
                     'Post-transaction verification failed' if code == 0 else stage))
        return 0 if success else 1
    finally:
        os.close(master)
        if child.poll() is None:
            child.terminate()
            try:
                child.wait(timeout=3)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait()
        os.close(lock_fd)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('operation', choices=('install', 'uninstall'))
    parser.add_argument('app')
    args = parser.parse_args()
    try:
        return run(args.operation, args.app)
    except Exception as error:
        emit(type='result', success=False, exit_code=-1, message=str(error))
        return 1


if __name__ == '__main__':
    sys.exit(main())
