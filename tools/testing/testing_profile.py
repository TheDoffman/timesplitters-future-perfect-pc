"""Create and clone a local unlocked US test profile; never use personal saves."""

# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[1]
_layout_root = _layout_tools.parent
if (_layout_tools / 'project_paths.py').is_file():
    _layout_sys.path.insert(0, str(_layout_tools))
    from project_paths import configure as _configure_paths, source_file as repo_file
    _configure_paths()
else:
    # Standalone source fixtures retain their original flat-folder behavior.
    _layout_root = _LayoutPath(__file__).resolve().parents[1]
    def repo_file(root, area, name):
        return _LayoutPath(root) / area / name

import argparse
import hashlib
import hmac
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import threading
import tempfile
import time

ROOT=_layout_root
XBE_SHA256='3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
TITLE='45410066'
BODY_SIZE=0x1F54
PROGRESS=0x1290
FIXTURE='private/test-fixtures/unlocked'
# Directory produced by the supported US game when creating the name Test Lab.
# Reusing the seed Player 1 directory makes XCreateSaveGame reject that name.
TEST_SLOT='1AD19156651F'


def contained(root,path,area):
    root=Path(root).resolve();path=Path(path).absolute();allowed=root/area
    if not path.is_relative_to(allowed) or path==allowed:
        raise ValueError(f'Expected a child of {allowed}: {path}')
    for part in (path,*path.parents):
        if part==root:break
        if part.is_symlink() or getattr(part,'is_junction',lambda:False)():
            raise ValueError(f'Refusing linked test path: {part}')
    if not path.resolve().is_relative_to(allowed.resolve()):
        raise ValueError(f'Test path escapes {allowed}')
    return path.resolve()


def signature(body):
    # The pinned PC/native kernel initializes XboxSignatureKey to sixteen zero
    # bytes. This is a runtime test save, not a save signed for a retail Xbox.
    return hmac.new(bytes(16),body,hashlib.sha1).digest()


def unlock_profile(raw):
    if len(raw)!=BODY_SIZE+20 or not hmac.compare_digest(signature(raw[:BODY_SIZE]),raw[BODY_SIZE:]):
        raise ValueError('Expected an intact save signed by the pinned runtime')
    if struct.unpack_from('<I',raw,0x1EE0)[0]!=2:
        raise ValueError('Unsupported profile version')
    body=bytearray(raw[:BODY_SIZE])
    # US sub_001CD200 uses sub_00069900's progress base (profile + 0x1290).
    # Six story completion masks cover single/co-op at three difficulties.
    # Four 128-bit award masks and 48 platinum grades unlock Arcade/Challenge
    # content and character rewards. Leave +0xBF0 feature/pickup flags intact:
    # unlike the game's broad debug unlock, collectibles remain available.
    body[PROGRESS+0xBF4:PROGRESS+0xC4C]=b'\xff'*(0xC4C-0xBF4)
    for index in range(48):
        struct.pack_into('<I',body,PROGRESS+0x49C+index*0x18,4)
    return bytes(body)+signature(body)


def create_fixture(root,seed):
    root=Path(root).resolve();seed=contained(root,seed,'private')
    if seed.is_relative_to(root/'private/saves'):
        raise ValueError('Personal saves cannot be used as test seeds')
    target=contained(root,root/FIXTURE,'private/test-fixtures')
    if target.exists():raise ValueError('Fixture already exists; check/reuse it instead')
    xbe=root/'private/game/default.xbe'
    if hashlib.sha256(xbe.read_bytes()).hexdigest()!=XBE_SHA256:
        raise ValueError('Unsupported game executable')
    title=seed/'UserData'/TITLE
    profiles=list(title.glob('*/tsftrpft'))
    if len(profiles)!=1:raise ValueError('Seed must contain exactly one fresh disposable profile')
    profile=contained(root,profiles[0],'private')
    raw=profile.read_bytes()
    result=unlock_profile(raw)
    if struct.unpack_from('<I',raw,PROGRESS+0xBF0)[0]:
        raise ValueError('Seed already has collectible/feature flags; use a fresh profile')
    files={}
    for name in ('TitleMeta.xbx','TitleImage.xbx','SaveImage.xbx'):
        source=contained(root,title/name,'private')
        files[Path('UserData')/TITLE/name]=source.read_bytes()
    slot=profile.parent.name
    if len(slot)!=12 or any(c not in '0123456789ABCDEFabcdef' for c in slot):
        raise ValueError('Unexpected Xbox save slot')
    base=Path('UserData')/TITLE/TEST_SLOT
    files[base/'SaveMeta.xbx']='\ufeffName=Test Lab\r\n'.encode('utf-16-le')
    files[base/'tsftrpft']=result
    manifest=dict(schema=1,xbe_sha256=XBE_SHA256,seed_sha256=hashlib.sha256(raw).hexdigest(),
                  collectibles='fresh seed preserved; pickup flags not unlocked',
                  files={p.as_posix():hashlib.sha256(b).hexdigest() for p,b in files.items()})
    for relative,data in files.items():
        destination=target/relative;destination.parent.mkdir(parents=True,exist_ok=True)
        destination.write_bytes(data)
    (target/'fixture.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return target


def check_fixture(root):
    root=Path(root).resolve();fixture=contained(root,root/FIXTURE,'private/test-fixtures')
    manifest=json.loads(contained(root,fixture/'fixture.json','private/test-fixtures').read_text())
    if manifest.get('schema')!=1 or manifest.get('xbe_sha256')!=XBE_SHA256:
        raise ValueError('Unsupported test fixture manifest')
    profiles=[name for name in manifest['files'] if name.endswith('/tsftrpft')]
    if len(profiles)!=1 or len(manifest['files'])!=5:
        raise ValueError('Expected one complete test profile')
    base=Path(profiles[0]).parent
    expected={f'UserData/{TITLE}/{name}' for name in ('TitleMeta.xbx','TitleImage.xbx','SaveImage.xbx')}
    expected.update((base/name).as_posix() for name in ('SaveMeta.xbx','tsftrpft'))
    if (len(base.parts)!=3 or base.parts[:2]!=('UserData',TITLE)
            or len(base.name)!=12 or any(c not in '0123456789abcdefABCDEF' for c in base.name)
            or set(manifest['files'])!=expected):
        raise ValueError('Unexpected fixture file paths')
    for name,digest in manifest['files'].items():
        path=contained(root,fixture/name,'private/test-fixtures')
        if not path.is_relative_to(fixture) or hashlib.sha256(path.read_bytes()).hexdigest()!=digest:
            raise ValueError(f'Fixture content changed: {name}')
    return fixture,manifest


def feedback_dump_path(root,run):
    """Keep both optional recorder outputs inside the disposable test tree."""
    for name in ('icall_targets.dump','icall_sites.dump'):
        contained(root,Path(run)/name,'private/test-saves')
    return str(Path(run)/'icall_targets.dump')


def prepare_run(root,run):
    root=Path(root).resolve();run=contained(root,run,'private/test-saves')
    fixture,manifest=check_fixture(root)
    marker=contained(root,run/'test-run.json','private/test-saves')
    if run.exists() and any(run.iterdir()) and not marker.is_file():
        raise ValueError('Refusing to replace a directory not owned by the test runner')
    if marker.exists() and json.loads(marker.read_text())!=dict(schema=1,fixture=str(fixture.relative_to(root))):
        raise ValueError('Unrecognized test directory marker')
    if (run/'run.lock').exists():
        raise ValueError('Test directory is in use; close its game before preparing again')
    run.mkdir(parents=True,exist_ok=True)
    old_base=Path(next(name for name in manifest['files'] if name.endswith('/tsftrpft'))).parent
    new_base=Path('UserData')/TITLE/TEST_SLOT
    # Legacy fixtures renamed only SaveMeta, retaining the seed's directory ID.
    # Preserve an untouched old test copy outside UserData instead of leaving
    # a hidden name collision. Never move a profile changed since fixture copy.
    if old_base!=new_base:
        old=contained(root,run/'saves'/old_base,'private/test-saves')
        if old.exists():
            expected={Path(name).name:digest for name,digest in manifest['files'].items() if Path(name).parent==old_base}
            if set(p.name for p in old.iterdir())!=set(expected) or any(
                hashlib.sha256((old/name).read_bytes()).hexdigest()!=digest for name,digest in expected.items()):
                raise ValueError('Legacy test profile was modified; use a fresh isolated test directory')
            backup=contained(root,run/'legacy-fixture-backup'/old_base.name,'private/test-saves')
            if backup.exists():raise ValueError('Legacy fixture backup exists; use a fresh test directory')
            backup.parent.mkdir(parents=True,exist_ok=True)
            old.rename(backup)
    for name in manifest['files']:
        relative=Path(name)
        if relative.parent==old_base:relative=new_base/relative.name
        destination=contained(root,run/'saves'/relative,'private/test-saves')
        destination.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(fixture/name,destination)
    for source,name in ((root/'tests/fixtures/pc-controls.json','controls.json'),):
        destination=contained(root,run/name,'private/test-saves')
        if not destination.exists():shutil.copyfile(source,destination)
    for name,default in (('display.ini','TSFP_DISPLAY_V1 0 0\n'),('runtime.ini','# Isolated test runtime\n')):
        destination=contained(root,run/name,'private/test-saves')
        if not destination.exists():destination.write_text(default)
    marker.write_text(json.dumps(dict(schema=1,fixture=str(fixture.relative_to(root))))+'\n')
    environment={k:v for k,v in os.environ.items() if not k.startswith(('RECOMP_','TSFP_'))}
    environment.update(RECOMP_GAME_DIR=str(root/'private/game'),RECOMP_SAVE_DIR=str(run/'saves'),
        RECOMP_ICALL_FEEDBACK_PATH=feedback_dump_path(root,run),
        TSFP_SAVE_LOCATION_LOCK='1',
        TSFP_CRASH_DIR=str(run/'crashes'),
        TSFP_TEXTURE_PACK_DIR=str(root/'assets/textures'),TSFP_TEXTURE_MODE='classic',
        RECOMP_INPUT_CONFIG=str(run/'controls.json'),TSFP_DISPLAY_CONFIG=str(run/'display.ini'),
        RECOMP_DISPLAY_CONFIG=str(run/'runtime.ini'),RECOMP_XMV_PLAY='0',RECOMP_TRACE_BUDGET='0')
    return environment


def play(root,run,seconds,*,allow_save_drive_changes=False):
    """Bounded Windows session, with capped logs and an exclusive run marker."""
    if os.name!='nt':raise ValueError('Use the isolated native runner on macOS')
    if not 1<=seconds<=600:raise ValueError('Duration must be between 1 and 600 seconds')
    root=Path(root).resolve();run=contained(root,run,'private/test-saves')
    if hashlib.sha256((root/'private/game/default.xbe').read_bytes()).hexdigest()!=XBE_SHA256:
        raise ValueError('Unsupported game executable')
    env=prepare_run(root,run)
    if allow_save_drive_changes:
        # Explicit human drive-selection testing only. Automated sessions stay
        # locked even when the parent process has disabled the lock.
        env['TSFP_SAVE_LOCATION_LOCK']='0'
    lock=run/'run.lock'
    with lock.open('x') as stream:stream.write(str(os.getpid()))
    child=None
    threads=[]
    def drain(pipe,destination):
        with destination.open('wb') as output:
            remaining=4*1024*1024
            while chunk:=pipe.read(65536):
                output.write(chunk[:remaining]);remaining=max(0,remaining-len(chunk))
    try:
        child=subprocess.Popen([str(root/'build/windows/Release/futureperfect_recomp.exe')],
            cwd=root,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        for pipe,name in ((child.stdout,'game.out'),(child.stderr,'game.err')):
            destination=contained(root,run/name,'private/test-saves')
            thread=threading.Thread(target=drain,args=(pipe,destination),daemon=True)
            thread.start();threads.append(thread)
        print(f'Test Lab session: PID {child.pid}, limit {seconds}s, default saves {run / "saves"}',flush=True)
        if allow_save_drive_changes:
            print(f'Manual drive selection enabled. Selected drives activate immediately and persist; '
                  f'preference: {run / "save-location.txt"}. Existing saves are not moved.',flush=True)
        try:child.wait(timeout=seconds)
        except (subprocess.TimeoutExpired,KeyboardInterrupt):
            child.terminate();child.wait(timeout=10)
            return 0
        return child.returncode
    finally:
        if child is not None and child.poll() is None:
            child.kill();child.wait()
        for thread in threads:thread.join(timeout=10)
        lock.unlink()
        recycle_disks(root,run)


def recycle_disks(root,run):
    """Recycle only the six known disposable Xbox disks, never the profile."""
    import ctypes
    from ctypes import wintypes
    class FileOperation(ctypes.Structure):
        _fields_=[('hwnd',wintypes.HWND),('operation',wintypes.UINT),('source',wintypes.LPCWSTR),
                  ('destination',wintypes.LPCWSTR),('flags',wintypes.WORD),
                  ('aborted',wintypes.BOOL),('mapping',ctypes.c_void_p),('title',wintypes.LPCWSTR)]
    paths=[]
    for candidate in (run/'saves').glob('Partition*.img'):
        path=contained(root,candidate,'private/test-saves')
        if path.name not in {f'Partition{i}.img' for i in range(6)}:
            raise ValueError(f'Refusing unknown virtual disk: {path}')
        paths.append(str(path))
    if paths and sys.platform == 'darwin':
        # Keep the same validated six-file boundary as Windows recycling.
        trash=Path.home()/'.Trash'/f'TSFP-test-disks-{time.time_ns()}'
        trash.mkdir(parents=True,exist_ok=False)
        for name in paths:
            source=Path(name)
            source.rename(trash/source.name)
        return
    if paths:
        operation=FileOperation(None,3,'\0'.join(paths)+'\0\0',None,0x454,False,None,None)
        result=ctypes.windll.shell32.SHFileOperationW(ctypes.byref(operation))
        if result or operation.aborted:raise OSError(f'Test disk recycling failed: {result}')



def publish_input_state(root, path, text):
    """Publish one complete input state; never expose a truncated held key."""
    path = contained(root, path, 'private/test-saves')
    descriptor, temporary = tempfile.mkstemp(prefix=path.name + '.', suffix='.tmp', dir=path.parent)
    try:
        with os.fdopen(descriptor, 'w', encoding='utf-8', newline='\n') as stream:
            stream.write(text)
        # Windows CRT readers may briefly deny replacement while fopen holds
        # the old file. Keep the previous complete state and retry boundedly.
        deadline = time.monotonic() + .5
        while True:
            try:
                os.replace(temporary, path)
                return
            except PermissionError:
                if time.monotonic() >= deadline:
                    raise
                time.sleep(.002)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seed',type=Path,help='Fresh disposable save directory used only for initial creation')
    parser.add_argument('--prepare',type=Path,help='Restore a test copy beneath private/test-saves')
    parser.add_argument('--play',action='store_true',help='Run a bounded Windows test with a disposable copy')
    parser.add_argument('--seconds',type=int,default=120,help='Play duration, 1–600 seconds (default 120)')
    parser.add_argument('--allow-save-drive-changes',action='store_true',
        help='Manual play only: allow selecting and immediately using another save drive')
    args=parser.parse_args()
    if args.allow_save_drive_changes and not args.play:
        parser.error('--allow-save-drive-changes requires --play')
    if args.seed:print('Created',create_fixture(ROOT,args.seed))
    if args.play:
        raise SystemExit(play(ROOT,args.prepare or ROOT/'private/test-saves/quick-test',args.seconds,
                             allow_save_drive_changes=args.allow_save_drive_changes))
    elif args.prepare:
        env=prepare_run(ROOT,args.prepare)
        print(json.dumps({k:v for k,v in env.items() if k.startswith(('RECOMP_','TSFP_'))},indent=2))
    elif not args.seed:
        fixture,_=check_fixture(ROOT);print('Verified',fixture)


if __name__=='__main__':main()
