"""Freeze committed backup-only code; creates no protected files or services."""
import argparse
import json
import os
from pathlib import Path
import sys

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from mod_backup_release import prepare


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base-release',required=True)
    parser.add_argument('--output',required=True,type=Path)
    args=parser.parse_args()
    try:
        if sys.platform!='darwin' or os.getuid()==0:raise ValueError('Prepare as the ordinary Mac user without sudo')
        print(json.dumps(prepare(Path(__file__).resolve().parents[3],args.output,args.base_release),sort_keys=True));return 0
    except (OSError,ValueError) as error:
        print('Backup preparation: '+str(error),file=sys.stderr);return 2


if __name__=='__main__':raise SystemExit(main())
