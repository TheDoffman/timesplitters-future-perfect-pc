"""Bounded one-shot backup supervision after irreversible privilege dropping."""
import json
import os
from pathlib import Path
import resource
import selectors
import signal
import sqlite3
import subprocess
import sys
import time
import uuid

sys.path.insert(0,str(Path(__file__).absolute().parent))
from mod_backup_release import ROOT,decode,installed,read,wire
from mod_host_identity import kernel_groups,require_locked_process

WALL_SECONDS=1800
OUTPUT_LIMIT=8192


def capture(python,program,mode,*,timeout=WALL_SECONDS):
    # No new session: the trusted child remains in the launchd job group. It
    # never starts processes or executes uploaded content. launchd also cleans
    # the group if this supervisor is killed before its finally block.
    child=subprocess.Popen([str(python),'-I','-S','-B',str(program),mode],stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,stderr=subprocess.PIPE,close_fds=True)
    output={};selector=selectors.DefaultSelector();started=time.monotonic()
    try:
        for stream in (child.stdout,child.stderr):
            os.set_blocking(stream.fileno(),False);selector.register(stream,selectors.EVENT_READ);output[stream]=bytearray()
        while selector.get_map() or child.poll() is None:
            if time.monotonic()-started>timeout:raise TimeoutError('Backup exceeded its wall-time limit')
            for key,_ in selector.select(min(.2,max(.001,timeout-(time.monotonic()-started)))):
                data=os.read(key.fileobj.fileno(),4096)
                if not data:selector.unregister(key.fileobj);continue
                output[key.fileobj]+=data
                if len(output[key.fileobj])>OUTPUT_LIMIT:raise ValueError('Backup output budget exceeded')
        return child.wait(),bytes(output[child.stdout]),bytes(output[child.stderr])
    finally:
        selector.close()
        if child.poll() is None:
            child.terminate()
            try:child.wait(timeout=5)
            except subprocess.TimeoutExpired:child.kill();child.wait(timeout=5)
        child.stdout.close();child.stderr.close()


def checked_status(value):
    import re
    fields={'schema','producer','identity','base_release','uid','time','status'}
    if (not isinstance(value,dict) or value.get('status') not in ('ok','error')
        or set(value)!=fields|({'result'} if value.get('status')=='ok' else {'error'})
        or type(value.get('schema')) is not int or value['schema']!=1
        or value.get('producer')!='tsfp-backup-status-v1'
        or type(value.get('uid')) is not int or value['uid']<600
        or type(value.get('time')) is not int or not 0<=value['time']<10**12
        or any(not isinstance(value.get(key),str) or not re.fullmatch('[0-9a-f]{64}',value[key]) for key in ('identity','base_release'))
        or (value['status']=='ok' and not isinstance(value['result'],dict))
        or (value['status']=='error' and (not isinstance(value['error'],str) or len(value['error'])>1000))):
        raise ValueError('Preserving unknown backup status')
    return value


def load_status(parent):return checked_status(decode(read(parent/'status.json',os.geteuid(),16384)))


def atomic_status(parent,value):
    from mod_backup import private_directory,sync_directory
    private_directory(parent);target=parent/'status.json'
    checked_status(value);data=wire(value)
    if len(data)>16384:raise ValueError('Backup status exceeded its byte budget')
    if target.exists() or target.is_symlink():
        old=load_status(parent)
        if any(old[key]!=value[key] for key in ('identity','base_release','uid')):raise ValueError('Preserving differently bound backup status')
    temporary=parent/('.status-'+uuid.uuid4().hex)
    try:
        with os.fdopen(os.open(temporary,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600),'wb') as stream:
            stream.write(data);stream.flush();os.fsync(stream.fileno())
        os.replace(temporary,target);sync_directory(parent)
    finally:
        if temporary.exists():temporary.unlink()


def main():
    try:
        mode=sys.argv[1:]
        if len(mode)!=1 or mode[0] not in ('--once','--initialize','--probe','--status','--capture','--initialize-capture'):raise ValueError('Invalid backup worker operation')
        uid=require_locked_process();folder=Path(__file__).absolute().parent
        _,binding=installed(folder)
        if binding['uid']!=uid or str(Path(sys.executable).resolve())!=binding['python']:raise ValueError('Backup worker binding changed')
        parent=ROOT/'data/backups';pool=parent/'managed';repository=ROOT/'data/scan/repository'
        from mod_backup import private_directory
        private_directory(parent)
        if mode==['--probe']:
            denied=False
            try:
                with (ROOT/'identity.json').open('rb'):pass
            except PermissionError:denied=True
            if not denied:raise ValueError('Locked backup worker can read root-only metadata')
            result=dict(status='probe',root_metadata_denied=True)
        elif mode in (['--capture'],['--initialize-capture']):
            for limit,value in ((resource.RLIMIT_CPU,600),(resource.RLIMIT_CORE,0),(resource.RLIMIT_NOFILE,64),(resource.RLIMIT_DATA,1024**3)):
                resource.setrlimit(limit,(value,value))
            from mod_backup_pool import initialize,policy,run
            if mode==['--initialize-capture'] and not pool.exists():initialize(repository,pool)
            _,settings=policy(pool)
            if settings['repository']!=str(repository) or (settings['keep'],settings['interval'],settings['maximum_bytes'])!=(7,86400,8*1024**3):raise ValueError('Installed backup policy differs')
            result=run(pool)
        elif mode==['--status']:
            status=load_status(parent)
            if status['identity']!=folder.name or status['base_release']!=binding['base_release'] or status['uid']!=uid:raise ValueError('Backup status binding differs')
            print(json.dumps(status));return 0
        else:
            def interrupted(signum,frame):raise InterruptedError('Backup supervisor interrupted')
            signal.signal(signal.SIGTERM,interrupted);signal.signal(signal.SIGINT,interrupted)
            action='--initialize-capture' if mode==['--initialize'] else '--capture'
            status=dict(schema=1,producer='tsfp-backup-status-v1',identity=folder.name,base_release=binding['base_release'],uid=uid,time=int(time.time()))
            try:
                code,output,error=capture(sys.executable,Path(__file__).absolute(),action)
                if code!=0 or error:raise ValueError('Backup capture failed (exit '+str(code)+'): '+error.decode('utf-8','replace')[:500])
                report=json.loads(output)
                if (not isinstance(report,dict) or report.get('schema')!=1 or report.get('producer')!='tsfp-backup-worker-v1'
                    or report.get('uid')!=uid or report.get('gid')!=uid or not isinstance(report.get('kernel_groups'),list)
                    or any(type(group) is not int or group!=uid for group in report['kernel_groups'])
                    or not isinstance(report.get('result'),dict)
                    or report['result'].get('status') not in ('captured','not-due')):raise ValueError('Invalid backup capture report')
                status.update(status='ok',result=report['result']);atomic_status(parent,status)
                print(json.dumps(report));return 0
            except (OSError,ValueError,TimeoutError) as error:
                status.update(status='error',error=str(error)[:1000]);atomic_status(parent,status);raise
        print(json.dumps(dict(schema=1,producer='tsfp-backup-worker-v1',uid=uid,gid=os.getegid(),kernel_groups=kernel_groups(),result=result)));return 0
    except (OSError,ValueError,sqlite3.Error) as error:
        print('Backup worker: '+str(error),file=sys.stderr);return 2


if __name__=='__main__':raise SystemExit(main())
