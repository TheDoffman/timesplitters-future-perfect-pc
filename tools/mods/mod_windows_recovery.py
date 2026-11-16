"""Account-scoped Windows DPAPI and strict private NTFS recovery files."""
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import stat
import subprocess

from mod_package import require

ENTROPY = b'TSFP-WORKSHOP-ROOT-RECOVERY-V1\0'
MAX_SECRET = 65536


def crypt(value, decrypt=False):
    # No LOCAL_MACHINE flag: recovery is bound to the current Windows account.
    # https://learn.microsoft.com/windows/win32/api/dpapi/nf-dpapi-cryptprotectdata
    require(os.name == 'nt' and isinstance(value, bytes) and 0 < len(value) <= MAX_SECRET,
            'Windows account recovery requires bounded bytes on Windows')
    class Blob(ctypes.Structure):
        _fields_ = [('cbData', wintypes.DWORD), ('pbData', ctypes.POINTER(ctypes.c_ubyte))]
    def blob(data):
        buffer = (ctypes.c_ubyte*len(data)).from_buffer_copy(data)
        return Blob(len(data), buffer), buffer
    incoming, incoming_buffer = blob(value)
    entropy, entropy_buffer = blob(ENTROPY)
    output = Blob()
    library = ctypes.WinDLL('crypt32', use_last_error=True)
    free = ctypes.WinDLL('kernel32', use_last_error=True).LocalFree
    free.argtypes = [ctypes.c_void_p]; free.restype = ctypes.c_void_p
    function = library.CryptUnprotectData if decrypt else library.CryptProtectData
    function.argtypes = [ctypes.POINTER(Blob), ctypes.c_void_p if decrypt else wintypes.LPCWSTR,
                         ctypes.POINTER(Blob), ctypes.c_void_p, ctypes.c_void_p, wintypes.DWORD, ctypes.POINTER(Blob)]
    function.restype = wintypes.BOOL
    # Description contains no secret and is not requested during decryption.
    result = function(ctypes.byref(incoming), None if decrypt else 'TSFP workshop root recovery',
                      ctypes.byref(entropy), None, None, 1, ctypes.byref(output))
    try:
        require(result and 0 < output.cbData <= MAX_SECRET, 'Windows account recovery protection failed')
        return ctypes.string_at(output.pbData, output.cbData)
    finally:
        if output.pbData: free(ctypes.cast(output.pbData, ctypes.c_void_p))


def private(path, create=False):
    require(os.name == 'nt', 'Manage this recovery store on Windows')
    path = Path(path).absolute()
    require(all(not p.is_symlink() and not p.is_junction() for p in (path, *path.parents)),
            'Aliased Windows recovery path refused')
    executable = Path(os.environ['SystemRoot'])/'System32/WindowsPowerShell/v1.0/powershell.exe'
    script = Path(__file__).with_name('hosting')/'private-recovery-windows.ps1'
    result = subprocess.run([str(executable), '-NoProfile', '-NonInteractive', '-File', str(script),
                             '-Path', str(path), '-Mode', 'Create' if create else 'Check'],
                            stdin=subprocess.DEVNULL, capture_output=True, timeout=30)
    require(result.returncode == 0 and len(result.stdout) <= 4096 and len(result.stderr) <= 8192,
            'Windows recovery ACL creation or verification failed')
    value = json.loads(result.stdout)
    require(value.get('schema') == 1 and value.get('private') is True, 'Windows recovery ACL differs')
    return path


def read_file(path, maximum=MAX_SECRET):
    path = private(path)
    with path.open('rb') as source:
        before = os.fstat(source.fileno())
        require(stat.S_ISREG(before.st_mode) and before.st_nlink == 1 and 0 < before.st_size <= maximum,
                'Unsafe Windows recovery file')
        value = source.read(maximum+1)
        after = os.fstat(source.fileno())
        fields = lambda s: (s.st_dev, s.st_ino, s.st_size, s.st_mtime_ns, s.st_ctime_ns)
        require(len(value) == before.st_size and fields(before) == fields(after), 'Windows recovery file changed')
    return value


def new_file(path, value):
    require(isinstance(value, bytes) and 0 < len(value) <= MAX_SECRET, 'Recovery output exceeds budget')
    path = Path(path)
    private(path.parent)
    with path.open('xb') as output:
        output.write(value); output.flush(); os.fsync(output.fileno())
    private(path)
