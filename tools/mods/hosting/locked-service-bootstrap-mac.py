"""Fixed installed-service entry points; drop credentials before loading any worker.

launchd supplies root only for this trusted, isolated, site-disabled prelude.
There is no supplied command, executable, configuration or user identity.
"""
import argparse
import importlib.util
import os
from pathlib import Path
import re
import sys
import time

HERE=Path(__file__).absolute().parent
spec=importlib.util.spec_from_file_location('bootstrap_monitor',HERE/'system-scan-monitor-mac.py')
monitor=importlib.util.module_from_spec(spec);spec.loader.exec_module(monitor)
from mod_host_identity import require_locked_process
from mod_package import ModError, require


def arguments(release,role,nonce=None):
    if role in ('scan','definitions'):
        require(nonce is None,'Production bootstrap accepts no proof nonce')
        name,mode=(('scan-service-mac.py','--system-worker') if role=='scan'
                   else ('definitions-service-mac.py','--system-once'))
        return ['-I','-B',str(release/'source/tools/mods/hosting'/name),mode]
    require(role=='proof-target' and isinstance(nonce,str) and re.fullmatch('[0-9a-f]{32}',nonce),
            'Invalid fixed bootstrap role')
    fixture=monitor.ROOT/'releases'/('system-monitor-probe-'+nonce)
    marker=monitor.root_json(fixture,'probe.json')
    require(marker.get('producer')=='tsfp-system-monitor-proof-v1' and marker.get('release')==str(release),
            'Unknown inert bootstrap proof')
    worker=fixture/'worker.py'
    import stat
    info=worker.lstat()
    require(stat.S_ISREG(info.st_mode) and info.st_uid==0 and info.st_nlink==1 and not info.st_mode&0o022,
            'Unsafe inert bootstrap worker')
    return ['-I','-S','-B',str(worker),'target',str(fixture/'state'),
            str(release/'source/tools/mods/hosting/system-scan-monitor-mac.py'),str(fixture)]


def drop(uid,gid):
    require(os.geteuid()==0 and type(uid) is int and uid>=600 and type(gid) is int and gid==uid,
            'Bootstrap requires root and the fixed restricted identity')
    os.setgroups([])
    os.setregid(gid,gid)
    os.setreuid(uid,uid)
    require_locked_process()
    try:os.seteuid(0)
    except PermissionError:pass
    else:raise ModError('Service bootstrap retained recoverable root privilege')


def proof_start(release,nonce,uid):
    fixture=monitor.ROOT/'releases'/('system-monitor-probe-'+nonce)
    marker=monitor.root_json(fixture,'probe.json')
    require(marker.get('uid')==uid and marker.get('release')==str(release),'Bootstrap proof identity changed')
    # The proof uses a root-only readiness record and a bounded startup barrier;
    # production entry points have no waiting/clock override option.
    value=dict(schema=1,uid=os.getuid(),euid=os.geteuid(),pid=os.getpid(),phase='root-bootstrap')
    temporary=fixture/'.bootstrap.json'
    with os.fdopen(os.open(temporary,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600),'w') as output:
        import json
        json.dump(value,output);output.flush();os.fsync(output.fileno())
    os.replace(temporary,fixture/'bootstrap.json')
    deadline=time.monotonic()+8
    while True:
        value=monitor.root_json(fixture/'monitor','scan.json')
        if (value.get('status')=='starting' and value.get('service')==marker.get('service')
                and value.get('uid')==uid and value.get('policy')==monitor.POLICY):return
        require(time.monotonic()<deadline,'Root monitor did not observe bootstrap before deadline')
        time.sleep(0.05)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--role',required=True,choices=('scan','definitions','proof-target'))
    parser.add_argument('--nonce')
    args=parser.parse_args()
    try:
        release,account=monitor.installed()
        require(Path(__file__).absolute()==release/'source/tools/mods/hosting/locked-service-bootstrap-mac.py',
                'Use the immutable installed bootstrap')
        child=arguments(release,args.role,args.nonce)
        if args.role=='proof-target':proof_start(release,args.nonce,account['uid'])
        python=Path(sys.executable).resolve()
        drop(account['uid'],account['gid'])
        # No worker code, config, repository or network has been opened as root.
        environment=dict(PATH='/usr/bin:/bin:/usr/sbin:/sbin',HOME=str(monitor.ROOT/'data'),
                         TMPDIR=str(monitor.ROOT/'data'),LC_ALL='C',TZ='UTC')
        os.execve(python,[str(python),*child],environment)
        raise ModError('Restricted service exec returned unexpectedly')
    except (OSError,ValueError,ModError) as error:
        print('Locked service bootstrap: '+str(error),file=sys.stderr);return 2


if __name__=='__main__':
    raise SystemExit(main())
