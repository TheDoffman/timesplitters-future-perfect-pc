"""Protected private-host preparation/admission; never registers jobs or routing."""
import argparse
import hashlib
import importlib.util
from itertools import islice
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import time

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from mod_package import ModError,read_json,require
from mod_system_host import ROOT,ROLES


def helper(name):
    require(name in ('system-scan-monitor-mac.py','host_release.py'),'Invalid protected preparation helper')
    spec=importlib.util.spec_from_file_location('prepare_'+name.replace('.','_'),Path(__file__).with_name(name))
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module


LIMITS={'origin.json':8192,'trusted-root.json':65536,'catalog-1.pem':16384}


def read_inputs(directory,uid):
    directory=Path(directory)
    require(directory.is_absolute(),'Use an absolute private input directory')
    fd=helper('host_release.py').directory(directory,uid)
    try:
        info=os.fstat(fd)
        require(info.st_uid==uid and stat.S_IMODE(info.st_mode)==0o700,'Unsafe input directory')
        with os.scandir(fd) as entries:names=[e.name for e in islice(entries,4)]
        require(len(names)==3 and set(names)==set(LIMITS),'Unknown private input entries')
        result={}
        for name,maximum in LIMITS.items():
            child=os.open(name,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK,dir_fd=fd)
            with os.fdopen(child,'rb') as stream:
                before=os.fstat(stream.fileno())
                require(stat.S_ISREG(before.st_mode) and before.st_uid==uid and before.st_nlink==1
                        and stat.S_IMODE(before.st_mode)==0o600 and 0<before.st_size<=maximum,'Unsafe online input file')
                wire=stream.read(maximum+1);after=os.fstat(stream.fileno())
                fields=lambda s:(s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns)
                require(len(wire)==before.st_size and fields(before)==fields(after),'Online input changed')
                result[name]=wire
        return result
    finally:os.close(fd)


def verify_public_inputs(release,payloads):
    monitor=helper('system-scan-monitor-mac.py')
    profile=monitor.root_json(release/'source/config/mods','public-workshop.json')
    require(set(profile)=={'schema','origin','trusted_root_sha256','status'} and type(profile['schema']) is int
            and profile['schema']==1 and profile['status'] in ('pending-public-acceptance','active'),
            'Unknown immutable public profile')
    origin=read_json(payloads['origin.json'])
    require(isinstance(origin,dict) and type(origin.get('schema')) is int
            and origin==dict(schema=1,origin=profile['origin'],trusted_root_sha256=profile['trusted_root_sha256']),
            'Online origin settings differ from the immutable public pin')
    require(hashlib.sha256(payloads['trusted-root.json']).hexdigest()==profile['trusted_root_sha256'],
            'Online public root differs from the immutable pin')
    # RSA key parsing and signature verification happen only in the locked child.
    return profile


def copy_inputs(payloads,account):
    require(set(payloads)==set(LIMITS) and all(isinstance(value,bytes) and 0<len(value)<=LIMITS[name]
            for name,value in payloads.items()),'Invalid fixed online output set')
    release_files=helper('host_release.py')
    parent=release_files.directory(ROOT/'data',account['uid'])
    try:
        info=os.fstat(parent)
        require(info.st_uid==account['uid'] and stat.S_IMODE(info.st_mode)==0o700,'Unsafe locked data directory')
        try:os.mkdir('host-input',mode=0o700,dir_fd=parent)
        except FileExistsError:
            require(read_inputs(ROOT/'data/host-input',account['uid'])==payloads,'Preserving changed existing online inputs')
            return
        fd=os.open('host-input',os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW,dir_fd=parent)
        try:
            os.fchown(fd,account['uid'],account['gid']);os.fchmod(fd,0o700)
            for name,wire in payloads.items():
                child=os.open(name,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600,dir_fd=fd)
                with os.fdopen(child,'wb') as output:
                    os.fchown(output.fileno(),account['uid'],account['gid']);os.fchmod(output.fileno(),0o600)
                    output.write(wire);output.flush();os.fsync(output.fileno())
            os.fsync(fd)
        finally:os.close(fd)
        os.fsync(parent)
    finally:os.close(parent)


def command(release,python,account,name,arguments=(),root=False,timeout=60,json_output=True):
    allowed={'initialize-private-scan-mac.py':(('--apply',),),'initialize-private-host-mac.py':(('--apply',),),
             'definitions-service-mac.py':(('--system-once',),),'accept-installed-scanner-mac.py':((),),
             'system-scan-monitor-mac.py':(('--generate',),),
             'system-host-monitor-mac.py':tuple(('--role',role,'--generate') for role in ROLES)}
    require(name in allowed and tuple(arguments) in allowed[name]
            and root==(name in ('system-scan-monitor-mac.py','system-host-monitor-mac.py')),
            'Invalid fixed preparation command')
    options={} if root else dict(user=account['uid'],group=account['gid'],extra_groups=[])
    result=subprocess.run([str(python),'-I',*(['-S'] if root else []),'-B',
                           str(release/'source/tools/mods/hosting'/name),*arguments],
                          stdin=subprocess.DEVNULL,capture_output=True,timeout=timeout,cwd=release,
                          env=dict(PATH='/usr/bin:/bin:/usr/sbin:/sbin',HOME=str(ROOT/'data'),TMPDIR=str(ROOT/'data'),LC_ALL='C',TZ='UTC'),
                          **options)
    require(result.returncode==0 and len(result.stdout)<=8192 and len(result.stderr)<=8192,
            'Private preparation step failed: '+name)
    return read_json(result.stdout) if json_output else None


def admission_record(release,account,profile,scanner):
    require(os.geteuid()==0,'Protected admission record requires root')
    value=dict(schema=1,producer='tsfp-private-host-admission-v1',release=str(release),uid=account['uid'],
               origin=profile['origin'],trusted_root_sha256=profile['trusted_root_sha256'],time=int(time.time()),
               scanner=scanner,services_started=False,public_endpoint_started=False)
    wire=json.dumps(value,separators=(',',':')).encode()
    require(len(wire)<=8192,'Protected admission report exceeds budget')
    fd=helper('system-scan-monitor-mac.py').root_directory(ROOT/'monitor')
    try:
        child=os.open('admission.json',os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600,dir_fd=fd)
        with os.fdopen(child,'wb') as output:
            output.write(wire);output.flush();os.fsync(output.fileno())
        os.fsync(fd)
    finally:os.close(fd)


def prepare(release,account,python,directory,operator):
    payloads=read_inputs(directory,operator)
    profile=verify_public_inputs(release,payloads)
    copy_inputs(payloads,account)
    scan=command(release,python,account,'initialize-private-scan-mac.py',['--apply'])
    require(scan.get('phase')=='ready' and scan.get('release')==release.name,'Private scan preparation differs')
    host=command(release,python,account,'initialize-private-host-mac.py',['--apply'])
    require(host.get('phase')=='ready' and host.get('release')==release.name
            and host.get('origin')==profile['origin'] and host.get('trusted_root_sha256')==profile['trusted_root_sha256'],
            'Private host preparation differs')
    command(release,python,account,'definitions-service-mac.py',['--system-once'],timeout=240,json_output=False)
    scanner=command(release,python,account,'accept-installed-scanner-mac.py',timeout=240)
    require(scanner.get('phase')=='scanner-admitted' and scanner.get('release')==release.name
            and scanner.get('uid')==account['uid'] and scanner.get('temporary_state_removed') is True
            and scanner.get('results')==dict(clean='clean',blocked='blocked',empty_database='error',missing_engine='error'),
            'Installed scanner was not admitted')
    generated=command(release,python,account,'system-scan-monitor-mac.py',['--generate'],root=True)
    require(generated.get('phase')=='generated-only' and generated.get('services_started') is False,
            'Unexpected scan monitor generation')
    for role in ROLES:
        generated=command(release,python,account,'system-host-monitor-mac.py',['--role',role,'--generate'],root=True)
        require(generated.get('phase')=='generated-only' and generated.get('services_started') is False,
                'Unexpected host monitor generation')
    admission_record(release,account,profile,scanner)
    return dict(schema=1,phase='configured-and-admitted',release=release.name,origin=profile['origin'],
                trusted_root_sha256=profile['trusted_root_sha256'],scanner=scanner,
                root_monitors_generated=True,services_started=False,public_endpoint_started=False)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apply',action='store_true')
    parser.add_argument('--input',type=Path)
    args=parser.parse_args()
    try:
        if not args.apply:
            value=dict(schema=1,phase='plan-only',official_definition_network_required=True,
                       services_started=False,public_endpoint_started=False)
        else:
            monitor=helper('system-scan-monitor-mac.py')
            release,account=monitor.installed()
            require(Path(__file__).absolute()==release/'source/tools/mods/hosting/prepare-installed-host-mac.py',
                    'Use the immutable installed private host preparer')
            operator=int(os.environ.get('SUDO_UID','0'))
            require(operator>=501 and operator!=account['uid'] and args.input is not None,
                    'Use local administrator sudo with the prepared operator input')
            value=prepare(release,account,Path(sys.executable).resolve(),args.input,operator)
        print(json.dumps(value,indent=2));return 0
    except (OSError,ValueError,ModError,subprocess.SubprocessError) as error:
        print('Protected private host preparation: '+str(error)[:2000],file=sys.stderr);return 2


if __name__=='__main__':raise SystemExit(main())
