"""New-only private scan configuration, executed as the locked service account.

Never starts a job, downloads definitions or initializes/migrates a repository.
Interrupted/foreign state is preserved. A completed unchanged layout is reusable.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import stat
import sys

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from mod_backup import private_directory
from mod_host_identity import require_locked_process
from mod_package import ModError, require
from mod_runtime_files import runtime_access

ROOT=Path('/Library/Application Support/TSFP Mod Workshop')
PRODUCER='tsfp-private-scan-v1'
DIRECTORIES=('repository','database','worker-state','definitions-state')


def wire(value):
    return (json.dumps(value,sort_keys=True,separators=(',',':'))+'\n').encode()


def plan(release):
    require(release.parent==ROOT/'releases' and re.fullmatch('[0-9a-f]{64}',release.name),
            'Use an installed release identity')
    root=ROOT/'data/scan'
    configs={
        'service.json':dict(schema=1,repository=str(root/'repository'),database=str(root/'database'),
                            state=str(root/'worker-state'),scanner=str(release/'runtime/bin/clamscan')),
        'definitions.json':dict(schema=1,database=str(root/'database'),state=str(root/'definitions-state'),
                                freshclam=str(release/'runtime/bin/freshclam'))}
    return configs,dict(schema=1,producer=PRODUCER,release=release.name,
                        configs={name:hashlib.sha256(wire(value)).hexdigest() for name,value in configs.items()})


def read_member(fd,name):
    require(name in ('initialization.json','service.json','definitions.json'),'Invalid private scan member')
    child=os.open(name,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK,dir_fd=fd)
    with os.fdopen(child,'rb') as stream:
        before=os.fstat(stream.fileno())
        require(stat.S_ISREG(before.st_mode) and before.st_uid==os.geteuid() and before.st_nlink==1
                and stat.S_IMODE(before.st_mode)==0o600 and 0<before.st_size<=8192,'Unsafe private scan record')
        value=stream.read(8193)
        after=os.fstat(stream.fileno())
        require(len(value)==before.st_size and (before.st_ino,before.st_size,before.st_mtime_ns,before.st_ctime_ns)
                ==(after.st_ino,after.st_size,after.st_mtime_ns,after.st_ctime_ns),'Private scan record changed')
    return value


def new_member(fd,name,value):
    require(name in ('initialization.json','service.json','definitions.json','.initialization-complete'),
            'Invalid private scan output member')
    encoded=wire(value)
    require(len(encoded)<=8192,'Private scan record exceeds budget')
    child=os.open(name,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600,dir_fd=fd)
    with os.fdopen(child,'wb') as stream:
        stream.write(encoded);stream.flush();os.fsync(stream.fileno())


def check(release):
    configs,marker=plan(release)
    root=private_directory(ROOT/'data/scan')
    fd=os.open(root,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
    try:
        require(read_member(fd,'initialization.json')==wire(dict(marker,phase='complete')),
                'Preserving incomplete or different private scan generation')
        require(set(os.listdir(fd))=={'initialization.json',*configs,*DIRECTORIES},
                'Preserving unknown private scan entries')
        for name,value in configs.items():
            require(read_member(fd,name)==wire(value),'Preserving changed private scan configuration')
        for name in DIRECTORIES:private_directory(root/name)
    finally:os.close(fd)
    return dict(schema=1,phase='ready',release=release.name,root=str(root),services_started=False,network_started=False)


def initialize(release):
    configs,marker=plan(release)
    data=private_directory(ROOT/'data')
    parent=os.open(data,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
    try:
        try:os.mkdir('scan',mode=0o700,dir_fd=parent)
        except FileExistsError:return check(release)
        fd=os.open('scan',os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW,dir_fd=parent)
        try:
            new_member(fd,'initialization.json',dict(marker,phase='preparing'))
            for name in DIRECTORIES:os.mkdir(name,mode=0o700,dir_fd=fd)
            for name,value in configs.items():new_member(fd,name,value)
            require(read_member(fd,'initialization.json')==wire(dict(marker,phase='preparing')),
                    'Private initialization changed before commit')
            new_member(fd,'.initialization-complete',dict(marker,phase='complete'))
            os.replace('.initialization-complete','initialization.json',src_dir_fd=fd,dst_dir_fd=fd)
            os.fsync(fd)
        finally:os.close(fd)
    finally:os.close(parent)
    return check(release)


def installed_release():
    require(sys.platform=='darwin','Run private scan initialization on the Mac')
    require_locked_process()
    script=Path(__file__).absolute()
    release=script.parents[4]
    plan(release)
    require(script==release/'source/tools/mods/hosting/initialize-private-scan-mac.py',
            'Run the immutable installed initializer')
    spec=importlib.util.spec_from_file_location('private_scan_monitor',script.with_name('system-scan-monitor-mac.py'))
    monitor=importlib.util.module_from_spec(spec);spec.loader.exec_module(monitor)
    fd=monitor.root_directory(script.parent);os.close(fd)
    info=script.lstat()
    require(stat.S_ISREG(info.st_mode) and info.st_uid==0 and info.st_nlink==1 and not info.st_mode&0o022,
            'Unsafe installed initializer')
    require(Path(sys.executable).resolve().is_relative_to(release/'runtime'),'Use installed isolated Python')
    for name in ('clamscan','freshclam'):
        require(runtime_access(release/'runtime/bin'/name) is not None,'Use the verified installed scanner runtime')
    return release


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    mode=parser.add_mutually_exclusive_group()
    mode.add_argument('--apply',action='store_true')
    mode.add_argument('--check',action='store_true')
    args=parser.parse_args()
    try:
        if args.apply or args.check:
            release=installed_release()
            result=initialize(release) if args.apply else check(release)
        else:
            result=dict(schema=1,phase='plan-only',account='_tsfpworkshop',root=str(ROOT/'data/scan'),
                        services_started=False,network_started=False)
        print(json.dumps(result,indent=2));return 0
    except (OSError,ValueError,ModError) as error:
        print('Private scan initialization: '+str(error),file=sys.stderr);return 2


if __name__=='__main__':
    raise SystemExit(main())
