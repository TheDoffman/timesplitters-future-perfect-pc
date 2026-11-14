"""Trusted resource-limit bootstrap, replaced by the sandboxed scanner via exec."""
import os
import resource
import sys


def main():
    if sys.platform != 'darwin' or len(sys.argv) < 3 or sys.argv[1] != '/usr/bin/sandbox-exec':
        return 2
    for kind, maximum in ((resource.RLIMIT_CPU, 150), (resource.RLIMIT_CORE, 0),
                          (resource.RLIMIT_FSIZE, 1024 * 1024), (resource.RLIMIT_NOFILE, 128)):
        resource.setrlimit(kind, (maximum, maximum))
    os.execv(sys.argv[1], sys.argv[1:])
    return 2


if __name__ == '__main__':
    raise SystemExit(main())
