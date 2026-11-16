"""Locked-account forced recovery capture using the preserved backup code/runtime."""
import argparse
import json
import os
from pathlib import Path
import re
import sqlite3
import stat
import sys
import time

ROOT=Path('/Library/Application Support/TSFP Mod Workshop')
SHA=re.compile(r'[0-9a-f]{64}\Z')


def need(value,message):
    if not value:raise ValueError(message)


def protected_program(path):
    for parent in path.parents:
        info=parent.lstat()
        need(stat.S_ISDIR(info.st_mode) and info.st_uid==0 and not info.st_mode&0o022,
             'Unsafe forced backup program ancestor')
    info=path.lstat()
    need(stat.S_ISREG(info.st_mode) and info.st_uid==0 and info.st_nlink==1 and not info.st_mode&0o6022,
         'Unsafe forced backup program')


def module_closure(folder):
    # -I -S -B starts without checkout/site modules. Refuse ambient module reuse
    # rather than replacing sys.modules entries and mixing generations.
    for name,module in tuple(sys.modules.items()):
        if name.startswith('mod_') and getattr(module,'__file__',None):
            need(Path(module.__file__).resolve().parent==folder,'Backup module closure differs')


def force_capture(folder,binding,uid):
    """Called only after the locked identity and preserved binding are verified."""
    module_closure(folder)
    from mod_backup import private_directory,verify_snapshot
    from mod_backup_pool import policy,run,entries,locked
    module_closure(folder)
    parent=ROOT/'data/backups';pool=parent/'managed'
    private_directory(parent)
    _,settings=policy(pool)
    need(settings['repository']==str(ROOT/'data/scan/repository')
         and (settings['keep'],settings['interval'],settings['maximum_bytes'])==(7,86400,8*1024**3),
         'Preserved backup policy differs')
    started=int(time.time())
    result=run(pool,force=True)
    need(result.get('status')=='captured','Forced backup did not capture a fresh snapshot')
    with locked(pool):
        current,_=entries(pool,settings)
        matches=[value for value in current if value['path'].name==result['backup']]
        need(len(matches)==1,'Forced backup snapshot is missing or ambiguous')
        verified=verify_snapshot(matches[0]['path'])
    finished=int(time.time())
    need(verified['manifest_sha256']==result['verified_manifest_sha256']
         and started<=verified['created']<=finished,'Forced backup verification differs')
    module_closure(folder)
    return dict(schema=1,producer='tsfp-upgrade-backup-v1',identity=binding['identity'],
        base_release=binding['base_release'],uid=uid,status='captured',backup=result['backup'],
        verified_manifest_sha256=verified['manifest_sha256'],created=verified['created'],
        started=started,finished=finished,bytes=verified['bytes'],forced=True,verified=True)


def active(identity):
    need(isinstance(identity,str) and SHA.fullmatch(identity),'An exact preserved backup release is required')
    need(sys.platform=='darwin' and os.geteuid()>=600 and sys.flags.isolated and sys.flags.no_site
         and sys.dont_write_bytecode,'Use the isolated locked Mac backup process')
    script=Path(__file__).absolute();generation=script.parents[4]
    need(generation.parent==ROOT/'releases' and SHA.fullmatch(generation.name)
         and script==generation/'source/tools/mods/hosting/force-upgrade-backup-mac.py',
         'Use the immutable installed forced backup step')
    protected_program(script)
    folder=ROOT/'backup-releases'/identity
    protected_program(folder/'mod_backup_release.py')
    module_closure(folder)
    sys.path.insert(0,str(folder))
    from mod_backup_release import installed
    from mod_host_identity import require_locked_process
    _,binding=installed(folder)
    uid=require_locked_process()
    need(binding['uid']==uid and binding['gid']==uid and str(Path(sys.executable).resolve())==binding['python']
         and Path(sys.prefix).resolve().is_relative_to(ROOT/'releases'/binding['base_release']/'runtime'),
         'Forced backup process differs from preserved binding')
    import resource
    os.umask(0o077)
    for limit,value in ((resource.RLIMIT_CPU,600),(resource.RLIMIT_CORE,0),
                        (resource.RLIMIT_NOFILE,64),(resource.RLIMIT_DATA,1024**3)):
        resource.setrlimit(limit,(value,value))
    return force_capture(folder,binding,uid)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture',action='store_true')
    parser.add_argument('--backup-release')
    args=parser.parse_args()
    try:
        if args.capture:result=active(args.backup_release)
        else:
            need(args.backup_release is None,'Choose explicit forced capture')
            result=dict(schema=1,phase='plan-only',forced_capture=False,data_written=False)
        data=json.dumps(result,separators=(',',':'))
        need(len(data.encode())<=8192,'Forced backup report exceeds budget')
        print(data);return 0
    except (OSError,ValueError,sqlite3.Error):
        # Private repository/backup exception text is not a root/operator report.
        print('Forced upgrade backup failed; private state is preserved.',file=sys.stderr);return 2


if __name__=='__main__':raise SystemExit(main())
