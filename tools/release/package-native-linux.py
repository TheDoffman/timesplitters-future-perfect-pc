#!/usr/bin/env python3
"""Build a relocatable Linux x64 preview from verified binaries, without game data."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import shutil
import ssl
import subprocess
import sys
import sysconfig
import zipfile
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
from bundle_online import COMPANION, repo_file
import linux_vulkan_sdk

# These are the host ABI, not distributable application dependencies. Keep the
# host GPU/GL/Vulkan drivers and loader selection; never ship a hardware ICD.
HOST=re.compile(r"^(?:ld-linux-x86-64\.so\.2|lib(?:c|m|pthread|dl|rt|util|resolv)\.so\.[0-9]+|libnss_[^.]+\.so\.[0-9]+)$")
def digest(path):
    with Path(path).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def run(*args):return subprocess.check_output(list(map(str,args)),text=True).strip()
def elf(path):
    with Path(path).open('rb') as f:h=f.read(20)
    return len(h)==20 and h[:6]==b'\x7fELF\x02\x01' and h[18:20]==b'\x3e\x00'
def dependencies(path):
    result=[]
    for line in run('ldd',path).splitlines():
        if '=> not found' in line:raise ValueError('Unresolved dependency: '+line.strip())
        match=re.match(r'\s*(\S+) => (/\S+) \(',line)
        if match:result.append((match[1],Path(match[2]).resolve()))
    return result

def bundle_libraries(pairs,stage):
    """Copy complete direct ELF closures; apply local RPATH without glibc/ICDs."""
    pending=list(pairs);visited=set();origins={};records={}
    bin_dir=stage/'Bin';bin_dir.mkdir(exist_ok=True)
    while pending:
        source,target=pending.pop(0)
        if target in visited:continue
        visited.add(target)
        if not elf(source):raise ValueError('Expected Linux x64 ELF: '+str(source))
        for name,origin in dependencies(source):
            if HOST.fullmatch(name):continue
            if name in origins and origins[name]!=origin and origin!=(bin_dir/name).resolve():raise ValueError('Conflicting ELF dependency: '+name)
            if name not in origins:
                origins[name]=origin;destination=bin_dir/name
                if destination.exists() and digest(destination)!=digest(origin):raise ValueError('Duplicate ELF name: '+name)
                shutil.copy2(origin,destination);pending.append((origin,destination))
                records[name]=dict(source=str(origin),source_sha256=digest(origin))
        relative=os.path.relpath(bin_dir,target.parent)
        subprocess.run(['patchelf','--set-rpath','$ORIGIN'+('/'+relative if relative!='.' else ''),str(target)],check=True)
    for target in visited:
        for name,path in dependencies(target):
            if not HOST.fullmatch(name) and not path.is_relative_to(stage.resolve()):
                raise ValueError('External application dependency remains: '+name)
    return records

def copy(source,stage,name):
    target=stage/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target);return target

def bundle_python(root,stage):
    import tkinter,_tkinter
    version=f'{sys.version_info.major}.{sys.version_info.minor}'
    runtime=stage/'Support/Python';standard=Path(sysconfig.get_path('stdlib'));library=runtime/f'lib/python{version}'
    shutil.copytree(standard,library,ignore=shutil.ignore_patterns('site-packages','dist-packages','__pycache__','*.pyc','test','tests','ensurepip','idlelib','turtledemo','venv'))
    executable=Path(sys._base_executable).resolve();target=copy(executable,stage,'Support/Python/bin/python-real')
    tk_target=copy(Path(_tkinter.__file__),stage,'Support/Python/lib/python'+version+'/lib-dynload/'+Path(_tkinter.__file__).name)
    tcl=tkinter.Tcl();tcl_library=Path(tcl.eval('info library'));window=tkinter.Tk();window.withdraw()
    try:tk_library=Path(window.tk.eval('set tk_library'))
    finally:window.destroy()
    for origin,name in ((tcl_library,'tcl'),(tk_library,'tk')):shutil.copytree(origin,runtime/'lib'/name)
    certificate=ssl.get_default_verify_paths().cafile
    if not certificate:raise ValueError('Build host has no CA certificate bundle')
    copy(certificate,stage,'Support/Python/certificates.pem')
    wheels=root/'build/release-wheels-linux';wheels.mkdir(parents=True,exist_ok=True)
    subprocess.run([sys.executable,'-m','pip','download','--only-binary=:all:','--dest',str(wheels),'-r',str(root/'requirements-release.txt')],check=True)
    pins=[];site=library/'site-packages'
    for requirement in (root/'requirements-release.txt').read_text().splitlines():
        if not requirement or requirement.startswith('#'):continue
        name,pin=requirement.split('==');candidates=list(wheels.glob(name.replace('-','_')+'-'+pin+'-*.whl'))
        if len(candidates)!=1:raise ValueError('Expected one pinned wheel: '+requirement)
        wheel=candidates[0]
        with zipfile.ZipFile(wheel) as archive:
            for entry in archive.infolist():
                if entry.is_dir():continue
                path=Path(entry.filename)
                if path.is_absolute() or '..' in path.parts:raise ValueError('Unsafe wheel member')
                destination=site/path;destination.parent.mkdir(parents=True,exist_ok=True);destination.write_bytes(archive.read(entry))
        pins.append(dict(requirement=requirement,sha256=digest(wheel)))
    wrapper=runtime/'bin/python3'
    wrapper.write_text("#!/bin/sh\nset -eu\nhere=$(CDPATH= cd -- \"$(dirname -- \"$0\")/..\" && pwd)\nexport PYTHONHOME=\"$here\" PYTHONNOUSERSITE=1 PYTHONDONTWRITEBYTECODE=1\nexport PYTHONPATH=\"$here/lib/python@VERSION@/site-packages\"\nexport TCL_LIBRARY=\"$here/lib/tcl\" TK_LIBRARY=\"$here/lib/tk\" SSL_CERT_FILE=\"$here/certificates.pem\"\nexec \"$here/bin/python-real\" -s \"$@\"\n".replace("@VERSION@",version))
    wrapper.chmod(0o755)
    pairs=[(executable,target)]
    for item in library.rglob('*'):
        if item.is_file() and elf(item):pairs.append((item,item))
    return pairs,dict(version=version,wheels=pins)

def verify_payload(stage,manifest):
    required={'Bin/libdxcompiler.so','Bin/futureperfect_recomp','TimeSplitters.Launcher','Launch.sh','Settings/pc-controls.json',
        'Support/Python/bin/python3','Support/Online/online.py','Support/Online/network-identity.json','Source/pc-port-source.zip'}
    files=manifest['files']
    if not required<=files.keys():raise ValueError('Incomplete Linux game package')
    for name,sha in files.items():
        p=PurePosixPath(name)
        if ":" in name or not p.parts or p.is_absolute() or '..' in p.parts or '\\' in name or p.parts[0].lower() in {'data','saves','private','textures'} or p.suffix.lower() in {'.xbe','.pak','.iso','.img','.xmv'}:
            raise ValueError('Protected package path: '+name)
        if p.parts[0]=='Settings' and name!='Settings/pc-controls.json':raise ValueError('Personal settings in package')
        path=stage/p
        if path.is_symlink() or not path.resolve().is_relative_to(stage.resolve()) or digest(path)!=sha:raise ValueError('Package checksum mismatch: '+name)
    actual={p.relative_to(stage).as_posix() for p in stage.rglob('*') if p.is_file()}
    if actual!=set(files)|{'BUILD.json'}:raise ValueError('Unlisted package file')

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--version',required=True)
    parser.add_argument('--output',type=Path,default=ROOT/'build/releases');parser.add_argument('--launcher',type=Path,default=ROOT/'build/linux-launcher/package')
    args=parser.parse_args()
    if platform.system()!='Linux' or platform.machine()!='x86_64':parser.error('Package on the Linux x64 build host')
    if not re.fullmatch(r'v\d+\.\d+\.\d+(?:-[A-Za-z0-9.-]+)?',args.version):parser.error('Use a release-style version tag')
    if not shutil.which('patchelf'):parser.error('Install patchelf on the build host')
    build=ROOT/'build/linux/game';native=json.loads((build/'BUILD.json').read_text());executable=build/'futureperfect_recomp'
    if digest(executable)!=native['executable_sha256']:parser.error('Native executable differs from BUILD.json')
    for name,sha in native['source_sha256'].items():
        if digest(ROOT/name)!=sha:parser.error('Runtime source changed; rebuild: '+name)
    if not all(native['capabilities'].get(k) for k in ('movie_decode','settings_dialog','debug_dialog')):parser.error('Build the complete desktop/media runtime first')
    name='TimeSplitters-Future-Perfect-PC-'+args.version+'-linux-x64';stage=args.output.resolve()/name
    if stage.exists() or stage.with_name(stage.name+'.zip').exists():parser.error('Use a new output directory; existing packages are preserved')
    stage.mkdir(parents=True);game=copy(executable,stage,'Bin/futureperfect_recomp');game.chmod(0o755)
    if digest(args.launcher/'TimeSplitters.Launcher')!=json.loads((args.launcher/'Support/Launcher/manifest.json').read_text())['files']['TimeSplitters.Launcher']:parser.error('Launcher differs from its manifest')
    for source in args.launcher.rglob('*'):
        if source.is_symlink():raise ValueError('Linked launcher member')
        if source.is_file():copy(source,stage,source.relative_to(args.launcher))
    copy(ROOT/'config/pc-controls.default.json',stage,'Settings/pc-controls.json')
    copy(ROOT/'LICENSE',stage,'Licenses/Project.txt')
    for file in COMPANION:copy(repo_file(ROOT,'tools',file),stage,'Support/Online/'+file)
    copy(ROOT/'build/online-icon/multiplayer.png',stage,'Support/Online/multiplayer.png')
    for file in ('network-identity.json','network-build.json'):copy(build/'generated'/file,stage,'Support/Online/'+file)
    python_pairs,python=bundle_python(ROOT,stage)
    # FFmpeg is opened dynamically, so include it explicitly in the closure.
    dynamic=[]
    for stem in ('libavcodec.so','libavutil.so'):
        path=Path(run('cc','-print-file-name='+stem)).resolve()
        if not path.is_file():raise ValueError('Missing FFmpeg development library: '+stem)
        soname=run('patchelf','--print-soname',path);dynamic.append((path,copy(path,stage,'Bin/'+soname)))
    sdk=linux_vulkan_sdk.verify_install(ROOT)
    dxc=sdk/'lib/libdxcompiler.so'
    dynamic.append((dxc,copy(dxc,stage,'Bin/libdxcompiler.so')))
    copy(sdk.parent/'LICENSE.txt',stage,'Support/Runtime/Vulkan-SDK-LICENSE.txt')
    records=bundle_libraries([(executable,game),*python_pairs,*dynamic],stage)
    subprocess.run([sys.executable,str(repo_file(ROOT,'tools','network-identity.py')),'--output',str(build/'generated'),'--seal-executable',str(game)],check=True)
    # Preserve package-owner copyright/source information for every copied ELF.
    notices=stage/'Support/Runtime';notices.mkdir(parents=True,exist_ok=True)
    (notices/'dependencies.json').write_text(json.dumps(records,indent=2)+'\n')
    for number,record in enumerate(records.values()):
        owner=''
        if record['source'].startswith('/usr/'):
            found=subprocess.run(['dpkg-query','-S',record['source']],capture_output=True,text=True)
            if found.returncode:found=subprocess.run(['dpkg-query','-S',record['source'].removeprefix('/usr')],capture_output=True,text=True)
            if not found.returncode:owner=found.stdout.split(': /',1)[0].split(':',1)[0]
        notice=Path('/usr/share/doc')/owner/'copyright'
        if owner and notice.is_file():copy(notice,stage,f'Licenses/Runtime-{number}-{owner}.txt')
    source=stage/'Source/pc-port-source.zip';source.parent.mkdir()
    with zipfile.ZipFile(source,'x',zipfile.ZIP_DEFLATED) as archive:
        for file in run('git','ls-files','-z').split('\0'):
            if file and (ROOT/file).is_file():archive.write(ROOT/file,file)
    (stage/'Launch.sh').write_text((ROOT/'tools/release/linux-launch.sh').read_text());(stage/'Launch.sh').chmod(0o755)
    (stage/'README.txt').write_text('Linux x64 experimental preview. Extract into a writable folder and run ./Launch.sh.\nImport your supported US Xbox files using the launcher. Game data is not included.\nRequires a Vulkan-capable system driver and the build host glibc baseline (Ubuntu24.04).\nNo development checkout, system Python or Vulkan SDK is needed.\nFor a read-only game installation use ./Launch.sh --game --data /path/to/game.\nSource, runtime notices and exact build provenance are included. This local preview is not a published release.\n')
    manifest=dict(version=args.version,platform='linux-x64',renderer='vulkan',source_commit=native['source_revision'],packaging_commit=run('git','rev-parse','HEAD'),
        minimum_glibc=run('getconf','GNU_LIBC_VERSION').split()[-1],native_build=native,python=python,
        runtime_libraries=records,files={p.relative_to(stage).as_posix():digest(p) for p in stage.rglob('*') if p.is_file()})
    (stage/'BUILD.json').write_text(json.dumps(manifest,indent=2)+'\n');verify_payload(stage,manifest)
    env=dict(os.environ);env.pop('PYTHONPATH',None)
    subprocess.run([str(stage/'Support/Python/bin/python3'),'-c','import ssl,tkinter,cryptography,cffi; t=tkinter.Tk();t.withdraw();t.update();t.destroy();assert ssl.create_default_context().cert_store_stats()["x509_ca"]>0;print("PASS relocated Python/Tk/TLS")'],env=env,check=True,timeout=30)
    archive_path=stage.with_name(name+'.zip')
    with zipfile.ZipFile(archive_path,'x',zipfile.ZIP_DEFLATED,compresslevel=1) as archive:
        for path in sorted(stage.rglob('*')):
            if path.is_file():archive.write(path,name+'/'+path.relative_to(stage).as_posix())
    with zipfile.ZipFile(archive_path) as archive:
        if archive.testzip():raise ValueError('Corrupt Linux package')
    subprocess.run([str(stage/'TimeSplitters.Launcher'),'--verify-game-package',str(archive_path),args.version],check=True,timeout=60)
    archive_path.with_suffix('.zip.sha256').write_text(digest(archive_path)+'  '+archive_path.name+'\n');print(archive_path)
if __name__=='__main__':main()
