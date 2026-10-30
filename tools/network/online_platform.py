"""OS process lifetime and error presentation for the shared Online controller."""
import ctypes
import os
import sys


class GameProcess:
    def __init__(self, pid):
        self.pid = pid
        self.handle = None
        if sys.platform == 'win32':
            self.kernel = ctypes.WinDLL('kernel32', use_last_error=True)
            self.kernel.OpenProcess.argtypes = [ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
            self.kernel.OpenProcess.restype = ctypes.c_void_p
            self.kernel.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
            self.kernel.CloseHandle.argtypes = [ctypes.c_void_p]
            self.handle = self.kernel.OpenProcess(0x00100000, False, pid)
            if not self.handle:
                raise ValueError('Game process is no longer running')
        elif pid != os.getppid():
            # The game launches us directly. Reparenting detects crashes without
            # confusing a recycled PID for the original game process.
            raise ValueError('Online must be opened by its game process')

    def exited(self):
        if sys.platform == 'win32':
            return self.kernel.WaitForSingleObject(self.handle, 0) != 0x102
        return os.getppid() != self.pid

    def close(self):
        if self.handle:
            self.kernel.CloseHandle(self.handle)
            self.handle = None


def show_error(message):
    if sys.platform == 'win32':
        ctypes.windll.user32.MessageBoxW(None, str(message), 'Online connection failed', 0x10)
    else:
        import tkinter as tk
        from tkinter import messagebox
        window = tk.Tk()
        window.withdraw()
        try:
            messagebox.showerror('Online connection failed', str(message), parent=window)
        finally:
            window.destroy()
