"""Opt-in exception observer for isolated Online companion tests only."""
import json
import os
import sys
import threading
import time
import importlib.machinery
import importlib.util
from pathlib import Path

# Preserve the environment's own bootstrap (including native Tk extensions).
# Only exclude this observer's directory to avoid importing ourselves again.
observer_dir = Path(__file__).resolve().parent
bootstrap = importlib.machinery.PathFinder.find_spec('sitecustomize',
    [entry for entry in sys.path if Path(entry or '.').resolve() != observer_dir])
if bootstrap and bootstrap.loader:
    bootstrap.loader.exec_module(importlib.util.module_from_spec(bootstrap))

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
