"""Fixed root prelude; existing installed helper drops credentials before backups."""
import importlib.util
import os
from pathlib import Path
import re
import stat
import sys

ROOT=Path('/Library/Application Support/TSFP Mod Workshop')


def main():
    try:
        if sys.platform!='darwin' or os.geteuid()!=0:raise ValueError('Use the installed Mac backup job')
        folder=Path(__file__).absolute().parent
        if folder.parent!=ROOT/'backup-releases' or not re.fullmatch('[0-9a-f]{64}',folder.name):raise ValueError('Use immutable backup code')
        for path in (folder,*folder.parents):
            info=path.lstat()
            if not stat.S_ISDIR(info.st_mode) or info.st_uid!=0 or info.st_mode&0o022:raise ValueError('Unsafe backup prelude path')
        info=(folder/'mod_backup_release.py').lstat()
        if not stat.S_ISREG(info.st_mode) or info.st_uid!=0 or info.st_nlink!=1 or info.st_mode&0o022:raise ValueError('Unsafe backup release helper')
        sys.path.insert(0,str(folder))
        from mod_backup_release import installed,read
        _,binding=installed(folder)
        if sys.argv[1:] not in (['--once'],['--initialize'],['--probe'],['--status']):raise ValueError('Invalid fixed backup operation')
        python=Path(sys.executable).resolve()
        if str(python)!=binding['python']:raise ValueError('Use the bound protected Python')
        base=ROOT/'releases'/binding['base_release']
        helper=base/'source/tools/mods/hosting/locked-service-bootstrap-mac.py'
        read(helper,0)
        spec=importlib.util.spec_from_file_location('backup_locked_bootstrap',helper)
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        current,account=module.monitor.installed()
        if current!=base or account['uid']!=binding['uid'] or account['gid']!=binding['gid']:raise ValueError('Installed backup identity changed')
        module.drop(account['uid'],account['gid'])
        # Root has opened only protected code/metadata. Private config, SQLite,
        # backup payloads and status are first opened in the unprivileged exec.
        environment=dict(PATH='/usr/bin:/bin:/usr/sbin:/sbin',HOME=str(ROOT/'data'),TMPDIR=str(ROOT/'data'),LC_ALL='C',TZ='UTC')
        os.execve(python,[str(python),'-I','-S','-B',str(folder/'mod_backup_service.py'),sys.argv[1]],environment)
        raise ValueError('Backup exec returned unexpectedly')
    except (OSError,ValueError) as error:
        print('Backup bootstrap: '+str(error),file=sys.stderr);return 2


if __name__=='__main__':raise SystemExit(main())
