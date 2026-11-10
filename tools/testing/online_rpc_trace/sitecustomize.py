"""Opt-in exception observer for isolated Online companion tests only."""
import json
import os
import sys
import threading
import time

if os.environ.get('TSFP_ONLINE_RPC_TRACE') == '1':
    remaining = 32

    def observe(frame, event, arg):
        global remaining
        if event == 'exception' and remaining:
            kind, error, _ = arg
            remaining -= 1
            print('[ONLINE-RPC] '+json.dumps(dict(time=time.monotonic(),
                function=frame.f_code.co_name, line=frame.f_lineno,
                error=kind.__name__, status=getattr(error, 'status', None),
                message=str(error)[:160])), file=sys.stderr, flush=True)
        return observe

    def select(frame, event, arg):
        filename = frame.f_code.co_filename.replace('\\', '/')
        if event == 'call' and ((filename.endswith('/online_client.py') and frame.f_code.co_name == 'request') or
                (filename.endswith('/online_arcade_client.py') and frame.f_code.co_name == 'heartbeat')):
            return observe
        return None

    # Do not inspect locals, credentials, request bodies or guest memory.
    threading.settrace(select)
