"""Fresh-process backup core fixture; deliberately separate from pytest imports."""
import importlib.util
import json
import os
from pathlib import Path
import sys
from types import SimpleNamespace

SOURCE=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(SOURCE/'tools/mods'))
import mod_backup

spec=importlib.util.spec_from_file_location('isolated_force_backup_fixture',SOURCE/'tools/mods/hosting/force-upgrade-backup-mac.py')
force=importlib.util.module_from_spec(spec);spec.loader.exec_module(force)
root,action=Path(sys.argv[1]),sys.argv[2]
assert root.is_absolute() and action in ('force','policy','mixed','verification')
force.ROOT=root
if action=='mixed':sys.modules['mod_foreign_fixture']=SimpleNamespace(__file__=str(root/'foreign.py'))
if action=='verification':
    def failed(_):raise ValueError('injected verification failure')
    mod_backup.verify_snapshot=failed
try:
    result=force.force_capture(SOURCE/'tools/mods',dict(identity='c'*64,base_release='a'*64),os.geteuid())
except ValueError as error:
    print(json.dumps(dict(error=str(error))));raise SystemExit(2)
print(json.dumps(result))
