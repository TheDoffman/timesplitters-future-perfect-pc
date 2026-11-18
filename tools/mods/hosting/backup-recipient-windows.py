#!/usr/bin/env python3
"""Prepare and recover a backup recipient; secrets enter only local UI/TTY."""
import argparse
import getpass
import json
from pathlib import Path
import queue
import sys
import threading

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import mod_backup_custody as custody
from mod_package import require


def show(directory, fingerprint, origin, pin):
    require(sys.platform == 'win32', 'Use the local Windows backup key window')
    custody.opened(directory, fingerprint, origin, pin)
    import tkinter as tk
    from tkinter import ttk
    form = tk.Tk()
    form.title('TimeSplitters backup key recovery')
    form.geometry('720x500')
    form.resizable(False, False)
    frame = ttk.Frame(form, padding=22)
    frame.pack(fill='both', expand=True)
    ttk.Label(frame, text='Create encrypted backup-key recovery', font=('Segoe UI', 16)).pack(anchor='w')
    ttk.Label(frame, text='Choose a recovery passphrase of 16–256 characters and save it separately.\nIt encrypts the key needed to recover workshop backups.',
              wraplength=670, justify='left').pack(anchor='w', pady=(12, 18))
    ttk.Label(frame, text='Recovery passphrase').pack(anchor='w')
    password = ttk.Entry(frame, show='*', width=72)
    password.pack(anchor='w', pady=(4, 12))
    ttk.Label(frame, text='Confirm recovery passphrase').pack(anchor='w')
    confirm = ttk.Entry(frame, show='*', width=72)
    confirm.pack(anchor='w', pady=(4, 16))
    status = tk.StringVar(value='The key is protected by your Windows account. Recovery is not complete yet.')
    ttk.Label(frame, textvariable=status, wraplength=670, justify='left').pack(anchor='w', pady=(0, 12))
    results = queue.Queue(maxsize=1)
    busy = False
    done = False

    def submit():
        nonlocal busy
        if busy or done:
            return
        first, second = password.get(), confirm.get()
        try:
            custody.password_bytes(first)
            require(first == second, 'Recovery passphrases differ')
        except Exception:
            status.set('Use 16–256 characters and enter the same passphrase in both fields.')
            return
        busy = True
        button.configure(state='disabled')
        password.delete(0, 'end')
        confirm.delete(0, 'end')
        password.configure(state='disabled')
        confirm.configure(state='disabled')
        status.set('Creating and checking the encrypted recovery file…')
        def work():
            try:
                custody.finish(directory, fingerprint, origin, pin, first, second)
                results.put(True)
            except Exception:
                results.put(False)
        threading.Thread(target=work, daemon=True).start()

    def poll():
        nonlocal busy, done
        try:
            success = results.get_nowait()
        except queue.Empty:
            form.after(100, poll)
            return
        busy = False
        done = success
        if success:
            status.set('Recovery file created and verified. Keep a protected copy outside this Windows disk:\n' + str(Path(directory) / custody.RECOVERY))
            button.configure(text='Close', command=form.destroy, state='normal')
        else:
            password.configure(state='normal')
            confirm.configure(state='normal')
            button.configure(state='normal')
            status.set('Recovery could not finish. Check the passphrase and private files. If a recovery file already exists, use its original passphrase; it has been preserved.')
        form.after(100, poll)

    button = ttk.Button(frame, text='Create encrypted recovery file', command=submit)
    button.pack(anchor='w')
    form.protocol('WM_DELETE_WINDOW', lambda: None if busy else form.destroy())
    form.after(100, poll)
    password.focus_set()
    form.mainloop()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=('prepare', 'gui', 'status', 'check-portable'))
    parser.add_argument('path', type=Path)
    parser.add_argument('--origin', required=True)
    parser.add_argument('--trusted-root-sha256', required=True)
    parser.add_argument('--preparation-sha256')
    parser.add_argument('--recovery-sha256')
    parser.add_argument('--key-id')
    args = parser.parse_args()
    try:
        if args.operation == 'prepare':
            require(not any((args.preparation_sha256, args.recovery_sha256, args.key_id)), 'Unexpected preparation authority')
            value = custody.prepare(args.path, args.origin, args.trusted_root_sha256)
        elif args.operation == 'check-portable':
            require(not args.preparation_sha256 and args.recovery_sha256 and args.key_id and sys.stdin.isatty(),
                    'Check interactively with recorded file/key fingerprints; keep passphrases out of arguments/pipes/chat')
            password = getpass.getpass('Backup-key recovery passphrase: ')
            value = custody.check_portable(args.path, args.origin, args.trusted_root_sha256, args.recovery_sha256, args.key_id, password)
        else:
            require(args.preparation_sha256 and not any((args.recovery_sha256, args.key_id)), 'Supply only the key preparation SHA256')
            if args.operation == 'gui':
                show(args.path, args.preparation_sha256, args.origin, args.trusted_root_sha256)
                return 0
            value = custody.inspect(args.path, args.preparation_sha256, args.origin, args.trusted_root_sha256)
        print(json.dumps(value, indent=2))
        return 0
    except Exception:
        if sys.stderr is not None:
            print('Backup key custody could not finish; preserve the private store and recovery files.', file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
