"""A dedicated Windows backup worker owns its descendants until process exit.

Call before spawning any helper. Never call in a shared host or test runner:
closing the handle terminates this process too. Keep it open until OS exit.
This is lifetime containment, not a restricted security token or sandbox.
"""
import ctypes
from ctypes import wintypes
import os

from mod_package import require

_handle = None


def attach():
    global _handle
    require(os.name == 'nt' and _handle is None, 'A fresh dedicated Windows worker is required')

    class Basic(ctypes.Structure):
        _fields_ = [('process_time', ctypes.c_int64), ('job_time', ctypes.c_int64),
                    ('flags', wintypes.DWORD), ('minimum', ctypes.c_size_t),
                    ('maximum', ctypes.c_size_t), ('active', wintypes.DWORD),
                    ('affinity', ctypes.c_size_t), ('priority', wintypes.DWORD),
                    ('scheduling', wintypes.DWORD)]

    class Limits(ctypes.Structure):
        _fields_ = [('basic', Basic), ('io', ctypes.c_uint64 * 6),
                    ('process_memory', ctypes.c_size_t), ('job_memory', ctypes.c_size_t),
                    ('peak_process', ctypes.c_size_t), ('peak_job', ctypes.c_size_t)]

    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.CreateJobObjectW.argtypes = [ctypes.c_void_p, wintypes.LPCWSTR]
    kernel.CreateJobObjectW.restype = wintypes.HANDLE
    kernel.SetInformationJobObject.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD]
    kernel.SetInformationJobObject.restype = wintypes.BOOL
    kernel.AssignProcessToJobObject.argtypes = [wintypes.HANDLE, wintypes.HANDLE]
    kernel.AssignProcessToJobObject.restype = wintypes.BOOL
    kernel.GetCurrentProcess.argtypes = []
    kernel.GetCurrentProcess.restype = wintypes.HANDLE
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel.CloseHandle.restype = wintypes.BOOL
    handle = kernel.CreateJobObjectW(None, None)  # Unnamed, non-inheritable handle.
    require(handle, 'Cannot create backup worker lifetime job')
    limits = Limits()
    limits.basic.flags = 0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE, no breakaway flags.
    try:
        require(kernel.SetInformationJobObject(handle, 9, ctypes.byref(limits), ctypes.sizeof(limits)),
                'Cannot set backup worker lifetime policy')
        require(kernel.AssignProcessToJobObject(handle, kernel.GetCurrentProcess()),
                'Cannot contain backup worker process tree')
    except BaseException:
        kernel.CloseHandle(handle)
        raise
    # No finalizer/atexit: only OS process death closes this handle. The parent
    # scheduler can terminate the worker without leaving its SSH process alive.
    _handle = handle
