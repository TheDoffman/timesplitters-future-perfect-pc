"""Activate a completed private original-upscale pack, preserving prior artwork.

Explicit local activation only. Optionally install same-volume hard links into
a launcher installation, enabling only its enhanced-textures preference. This
never publishes assets or changes saves.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re

HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('upscale_audit',HERE/'texture-worklist.py')
audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)


def validate(root,candidate):
    if not candidate.resolve().is_relative_to((root/'authored').resolve()):
        raise ValueError('Candidate must be inside the private authored directory')
    plan=json.loads((candidate.parent/'plan.json').read_text(encoding='utf-8'))
    planned={r['key']:r for r in plan['records']}
    expected=set(planned)
    entries={}
    for entry in json.loads((root/'catalog.json').read_text(encoding='utf-8'))['textures']:
        if 'key' in entry:entries.setdefault(entry['key'],entry)
    if hashlib.sha256((root/'catalog.json').read_bytes()).hexdigest()!=plan['catalog_sha256']:
        raise ValueError('Candidate catalog changed')
    files=list(candidate.iterdir())
    if any(not p.is_file() or p.is_symlink() or not re.fullmatch(r'[0-9a-f]{16}\.(dds|json)',p.name) for p in files):
        raise ValueError('Unexpected candidate contents')
    if {p.stem for p in files if p.suffix=='.dds'}!=expected or len(files)!=len(expected)*2:
        raise ValueError('Candidate scope is incomplete')
    total=0
    for key in sorted(expected):
        dds=candidate/f'{key}.dds';meta=json.loads(dds.with_suffix('.json').read_text(encoding='utf-8'))
        total+=audit.check_asset(dds,meta,entries[key])
        if (meta['original_payload_sha256']!=entries[key]['sha256'] or meta['decoded_alpha_max_error']!=0
                or meta['output_size']!=planned[key]['output_size']
                or meta['original_alpha_resampling']!=planned[key]['alpha_resampling']):
            raise ValueError('Candidate provenance/alpha differs from verified originals')
    allocated=sum(audit.allocation(p) for p in files)
    logical=sum(p.stat().st_size for p in files)
    if logical>6144*1024*1024:raise ValueError('Pack exceeds 6 GiB')
    if expected & set(plan['retained_original_keys']):raise ValueError('Retained character icon replaced')
    return files,dict(keys=len(expected),dds_bytes=total,logical_bytes=logical,allocated_bytes=allocated)


def _activate(root,candidate,installation=None):
    root=root.resolve();candidate=candidate.resolve()
    files,summary=validate(root,candidate)
    active=root/'pack';previous=candidate.parent/'previous-enhanced-pack'
    if any(not path.resolve().is_relative_to(root) for path in (active,previous)):
        raise ValueError('Pack/recovery path escapes private texture workspace')
    if previous.exists():raise ValueError('Recovery pack already exists; inspect before activation')
    receipt=candidate.parent/'activation.json'
    if receipt.exists():raise ValueError('Activation receipt already exists')
    staging=None;destination=None;installed_previous=None;settings=None;original_controls=None
    controls_payload=None; controls_changed=False
    if installation:
        installation=installation.resolve()
        if not (installation/'TimeSplitters Launcher.exe').is_file() or not (installation/'BUILD.json').is_file():
            raise ValueError('Not a launcher installation')
        destination=installation/'Textures'
        staging=installation/'Textures.original-upscale-pending'
        installed_previous=installation/'Backups/original-upscale-2x/Textures'
        if any(not path.resolve().is_relative_to(installation) for path in (destination,staging,installed_previous)):
            raise ValueError('Texture installation/recovery path escapes launcher directory')
        if staging.exists() or installed_previous.exists():raise ValueError('Existing installation staging/recovery requires inspection')
        if installation.stat().st_dev!=candidate.stat().st_dev:raise ValueError('Hard-link installation requires the same volume')
        # A target game's open files must not be changed during play. The
        # installation's launcher provides its own session record; PID existence
        # is checked on Windows without sending any signal to the process.
        if os.name=='nt':
            import subprocess
            path=str(installation/'Bin/futureperfect_recomp.exe').replace("'","''")
            script=f"Get-CimInstance Win32_Process -Filter \"Name='futureperfect_recomp.exe'\" | Where-Object {{ $_.ExecutablePath -eq '{path}' }} | Select-Object -ExpandProperty ProcessId"
            running=subprocess.check_output(['powershell','-NoProfile','-Command',script],text=True).strip()
            if running:raise ValueError('Target installed game is running; close it before activation')
        settings=installation/'Settings/pc-controls.json'
        original_controls=settings.read_bytes()
        controls=json.loads(original_controls.decode('utf-8-sig'))
        if not isinstance(controls,dict) or not isinstance(controls.get('launcher',{}),dict):raise ValueError('Invalid launcher controls document')
        controls.setdefault('launcher',{})['enhanced_textures']=True
        controls_payload=(json.dumps(controls,indent=2)+'\n').encode('utf-8')
        staging.mkdir()
        try:
            for p in files:os.link(p,staging/p.name)
        except Exception:
            # Only our flat links are removed; authoritative candidates remain.
            for p in staging.iterdir():
                if p.resolve().parent!=staging.resolve():raise ValueError('Redirected staging file')
                p.unlink()
            staging.rmdir();raise
    moved_old=False;activated=False;installed=False;old_installed=False
    try:
        if active.exists():active.rename(previous);moved_old=True
        candidate.rename(active);activated=True
        if installation:
            if destination.exists():
                installed_previous.parent.mkdir(parents=True,exist_ok=True)
                destination.rename(installed_previous);old_installed=True
            staging.rename(destination);installed=True
            backup=candidate.parent/'previous-launcher-controls.json'
            with backup.open('xb') as f:f.write(original_controls)
            if settings.read_bytes()!=original_controls:raise ValueError('Launcher controls changed during activation; preserving concurrent edits')
            temp=settings.with_suffix('.original-upscale.tmp')
            with temp.open('xb') as f:f.write(controls_payload)
            temp.replace(settings);controls_changed=True
        record=dict(summary,active_pack=str(active),previous_pack=str(previous) if moved_old else None,
                    launcher=str(installation) if installation else None,
                    installed_previous=str(installed_previous) if old_installed else None,
                    installation_storage='same-volume hard links; no second texture payload copy' if installation else None,
                    enhanced_enabled=bool(installation),scene_validation='pending')
        with receipt.open('x',encoding='utf-8') as f:json.dump(record,f,indent=2);f.write('\n')
        return record
    except Exception:
        # Roll back moves, preserving every candidate and previous pack.
        if installed:destination.rename(staging)
        if old_installed:installed_previous.rename(destination)
        if activated:active.rename(candidate)
        if moved_old:previous.rename(active)
        if controls_changed:
            settings.write_bytes(original_controls)
        raise


def activate(root,candidate,installation=None):
    root=root.resolve();candidate=candidate.resolve()
    if not candidate.is_relative_to(root/'authored'):
        raise ValueError('Candidate must be inside the private authored directory')
    lock=candidate.parent/'production.lock'
    try:
        with lock.open('x',encoding='utf-8') as f:f.write(str(os.getpid()))
    except FileExistsError:
        raise ValueError('Production or activation is already running; inspect '+str(lock))
    try:return _activate(root,candidate,installation)
    finally:lock.unlink()


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,required=True)
    p.add_argument('--candidate',type=Path,required=True)
    p.add_argument('--installation',type=Path)
    a=p.parse_args()
    try:print(json.dumps(activate(a.root,a.candidate,a.installation),indent=2))
    except (ValueError,OSError,KeyError) as e:p.exit(2,str(e)+'\n')


if __name__=='__main__':main()
