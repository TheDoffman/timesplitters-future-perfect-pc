"""Plan or stage a sealed backup-only release, then run its protected registrar."""
import argparse
import json
import os
from pathlib import Path
import sys

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import mod_backup_release as release


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle',required=True,type=Path)
    parser.add_argument('--sha256',required=True)
    parser.add_argument('--apply',action='store_true')
    args=parser.parse_args()
    try:
        plan=release.plan(args.bundle,args.sha256)
        if not args.apply:
            print(json.dumps(dict(schema=1,phase='plan-only',identity=plan['identity'],target=str(plan['target']),
                                  source_revision=plan['descriptor']['source_revision'],base_release=plan['descriptor']['base_release'],
                                  files=len(plan['descriptor']['files']),jobs=1,root_modified=False,services_started=False)));return 0
        release.need(sys.platform=='darwin' and os.geteuid()==0,'Use local Mac administrator authentication')
        release.need(int(os.environ.get('SUDO_UID','-1'))==plan['descriptor']['owner_uid'],'Prepared bundle belongs to another operator')
        from mod_backup_install import base_binding,stage
        binding=base_binding(plan);folder=stage(plan,binding)
        environment=dict(PATH='/usr/bin:/bin:/usr/sbin:/sbin',HOME=str(release.ROOT/'data'),TMPDIR=str(release.ROOT/'data'),LC_ALL='C',TZ='UTC')
        os.execve(binding['python'],[binding['python'],'-I','-S','-B',str(folder/'mod_backup_install.py'),'--activate'],environment)
        raise ValueError('Backup installer exec returned unexpectedly')
    except (OSError,ValueError) as error:
        print('Backup installation: '+str(error)[:2000],file=sys.stderr);return 2


if __name__=='__main__':raise SystemExit(main())
