"""Trusted resource-limit bootstrap, replaced by the sandboxed scanner via exec."""
import os
import resource
import signal
import sys


def main():
    if sys.platform != 'darwin' or len(sys.argv) < 3 or sys.argv[1] != '/usr/bin/sandbox-exec':
        return 2
    for kind, maximum in ((resource.RLIMIT_CPU, 150), (resource.RLIMIT_CORE, 0),
                          (resource.RLIMIT_FSIZE, 512 * 1024 * 1024), (resource.RLIMIT_NOFILE, 128)):
        resource.setrlimit(kind, (maximum, maximum))
    # Survives exec: a controller crash cannot leave an ordinary scan running
    # past its deadline. A production supervisor remains required as well.
    signal.alarm(180)
    os.execv(sys.argv[1], sys.argv[1:])
    return 2


if __name__ == '__main__':
    raise SystemExit(main())
