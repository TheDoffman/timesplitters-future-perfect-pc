"""Locked fresh-process factory reader; returns only fixed installed job metadata."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import stat
import sys

ROOT=Path('/Library/Application Support/TSFP Mod Workshop')
SHA=re.compile(r'[0-9a-f]{64}\Z')
PYTHON=Path('Frameworks/Python.framework/Versions/3.12/bin/python3.12')


def need(value,message):
    if not value:raise ValueError(message)


def protected_program(path):
    for parent in path.parents:
        info=parent.lstat()
        need(stat.S_ISDIR(info.st_mode) and info.st_uid==0 and not info.st_mode&0o022,'Unsafe job factory ancestor')
    info=path.lstat()
    need(stat.S_ISREG(info.st_mode) and info.st_uid==0 and info.st_nlink==1 and not info.st_mode&0o6022,
         'Unsafe job factory program')


def closure(folder):
    for name,module in tuple(sys.modules.items()):
        if name.startswith('mod_') and getattr(module,'__file__',None):
            need(Path(module.__file__).resolve().parent==folder,'Job factory module generations differ')


def host_entries(generation,uid,python):
    folder=generation/'source/tools/mods'
    closure(folder)
    spec=importlib.util.spec_from_file_location('installed_job_factories',folder/'hosting/register-installed-host-mac.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    entries=module.jobs(generation,dict(uid=uid,gid=uid),python)
    closure(folder)
    need(len(entries)==9 and len({value['Label'] for _,value in entries})==9,'Installed host job inventory differs')
    return [dict(generated=str(path),specification=value) for path,value in entries]


def active(kind,identity):
    need(kind in ('host','backup') and isinstance(identity,str) and SHA.fullmatch(identity),'Invalid fixed job factory selection')
    need(sys.platform=='darwin' and os.geteuid()>=600 and sys.flags.isolated and sys.flags.no_site
         and sys.dont_write_bytecode,'Use the isolated locked Mac job factory process')
    program=Path(__file__).absolute();current=program.parents[4]
    need(current.parent==ROOT/'releases' and SHA.fullmatch(current.name)
         and program==current/'source/tools/mods/hosting/upgrade-job-specs-mac.py','Use the immutable installed job factory reader')
    protected_program(program)
    if kind=='host':
        generation=ROOT/'releases'/identity;folder=generation/'source/tools/mods'
    else:folder=ROOT/'backup-releases'/identity
    protected_program(folder/'mod_host_identity.py')
    closure(folder);sys.path.insert(0,str(folder))
    from mod_host_identity import kernel_groups,require_locked_process
    uid=require_locked_process()
    if kind=='host':
        python=generation/'runtime'/PYTHON
        need(Path(sys.executable).resolve()==python and Path(sys.prefix).resolve().is_relative_to(generation/'runtime'),
             'Host factory Python binding differs')
        entries=host_entries(generation,uid,python)
        base=identity
    else:
        from mod_backup_release import installed,job
        _,binding=installed(folder)
        need(binding['uid']==uid and binding['gid']==uid and str(Path(sys.executable).resolve())==binding['python']
             and Path(sys.prefix).resolve().is_relative_to(ROOT/'releases'/binding['base_release']/'runtime'),
             'Backup factory Python binding differs')
        entries=[dict(generated=str(ROOT/'backup-jobs'/identity/'worker.plist'),specification=job(folder,binding))]
        base=binding['base_release']
    closure(folder)
    return dict(schema=1,producer='tsfp-upgrade-job-specs-v1',kind=kind,identity=identity,base_release=base,
                uid=uid,gid=os.getegid(),kernel_groups=kernel_groups(),entries=entries)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    select=parser.add_mutually_exclusive_group()
    select.add_argument('--host-release');select.add_argument('--backup-release')
    args=parser.parse_args()
    try:
        if args.host_release is None and args.backup_release is None:
            value=dict(schema=1,phase='plan-only',services_changed=False,data_written=False)
        else:value=active('host' if args.host_release else 'backup',args.host_release or args.backup_release)
        data=json.dumps(value,separators=(',',':'))
        need(len(data.encode())+1<=65536,'Job factory report exceeds budget')
        print(data);return 0
    except (OSError,ValueError):
        print('Installed job factory inspection failed.',file=sys.stderr);return 2


if __name__=='__main__':raise SystemExit(main())
