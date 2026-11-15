"""Effective process credentials, distinct from macOS directory memberships.

Modern Python os.getgroups() on macOS uses the unlimited directory-membership
variant, which ignores setgroups(). The fixed libSystem getgroups symbol reads
the process group list; see the host's getgroups(2) manual and Python os docs.
"""
import ctypes
import os
import sys

from mod_package import require


def _read_groups(function):
    function.argtypes = [ctypes.c_int,ctypes.POINTER(ctypes.c_uint32)]
    function.restype = ctypes.c_int
    count = function(0,None)
    require(0 <= count <= 256,'Cannot read bounded kernel group list')
    groups = (ctypes.c_uint32 * max(1,count))()
    actual = function(count,groups)
    require(0 <= actual <= count,'Kernel group list changed or is unavailable')
    return list(groups)[:actual]


def kernel_groups():
    if sys.platform == 'darwin':
        library = ctypes.CDLL('/usr/lib/libSystem.B.dylib',use_errno=True)
        return _read_groups(library.getgroups)
    require(hasattr(os,'getgroups'),'Kernel group inspection requires a Unix host')
    return os.getgroups()


def require_locked_process():
    uid = os.geteuid()
    require(uid >= 600 and os.getuid() == uid and os.getgid() == os.getegid() == uid
            and set(kernel_groups()) <= {uid},'Use the locked system worker identity')
    # Directory membership is a separate policy: never admit root/admin access.
    require(not {0,80}.intersection(os.getgroups()),'Service directory membership is privileged')
    return uid
