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
    samples = 512
    output_lock = threading.Lock()

    def record(value):
        line='[ONLINE-RPC] '+json.dumps(value)
        destination=os.environ.get('TSFP_ONLINE_RPC_LOG')
        if destination:
            with output_lock, open(destination,'a',encoding='utf-8') as stream:
                stream.write(line+'\n')
        elif sys.stderr is not None:
            print(line,file=sys.stderr,flush=True)

    original_thread_error=threading.excepthook

    def thread_error(args):
        record(dict(time=time.monotonic(),error=args.exc_type.__name__,
                    message=str(args.exc_value)[:160],thread_failure=True))
        original_thread_error(args)

    threading.excepthook=thread_error

    def observe(frame, event, arg):
        global remaining,samples
        if event == 'exception' and remaining:
            kind, error, _ = arg
            remaining -= 1
            record(dict(time=time.monotonic(),
                function=frame.f_code.co_name, line=frame.f_lineno,
                error=kind.__name__, status=getattr(error, 'status', None),
                message=str(error)[:160]))
        if event=='return' and frame.f_code.co_name=='request' and samples:
            body=frame.f_locals.get('body')
            if isinstance(body,dict) and body.get('active') is True:
                samples-=1
                record(dict(time=time.monotonic(),counter=body.get('native_counter'),
                    epoch=arg.get('epoch') if isinstance(arg,dict) else None,
                    phase=arg.get('phase') if isinstance(arg,dict) else None))
        return observe

    def select(frame, event, arg):
        filename = frame.f_code.co_filename.replace('\\', '/')
        if event == 'call' and ((filename.endswith('/online_client.py') and frame.f_code.co_name == 'request') or
                (filename.endswith('/online_arcade_client.py') and frame.f_code.co_name == 'heartbeat')):
            return observe
        return None

    # Only whitelist native counter/active fields, never credentials/checkpoints.
    threading.settrace(select)
