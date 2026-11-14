#!/usr/bin/env python3
"""Prepare a relocatable Linux x64 package from verified binaries, without game data."""
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
import tempfile
import zipfile
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
from bundle_online import COMPANION, repo_file
import linux_vulkan_sdk
import linux_ffmpeg
from runtime_licenses import prepare as prepare_runtime_licenses
from linux_dependency_sources import collect as collect_dependency_sources
from release_channels import identity as release_identity
from build_textures import optional_identity, write_source_files

# These are the host ABI, not distributable application dependencies. Keep the
# host GPU/GL/Vulkan drivers and loader selection; never ship a hardware ICD.
# Mesa can require newer Wayland symbols than the packaging host provides, even
# for X11 windows. Keep the whole Wayland ABI with the user's graphics stack.
WAYLAND_HOST=re.compile(r"^libwayland-(?:(?:client|cursor)\.so\.0|egl\.so\.1)(?:\.[0-9]+)*$")
HOST=re.compile(r"^(?:ld-linux-x86-64\.so\.2|lib(?:c|m|pthread|dl|rt|util|resolv)\.so\.[0-9]+|libnss_[^.]+\.so\.[0-9]+|libwayland-(?:client|cursor)\.so\.0|libwayland-egl\.so\.1)$")
def digest(path):
    with Path(path).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def run(*args):return subprocess.check_output(list(map(str,args)),text=True).strip()
def elf(path):
    with Path(path).open('rb') as f:h=f.read(20)
    return len(h)==20 and h[:6]==b'\x7fELF\x02\x01' and h[18:20]==b'\x3e\x00'
def dependencies(path,search=None):
    result=[]
    output=run('ldd',path) if search is None else subprocess.check_output(['ldd',str(path)],text=True,env=dict(os.environ,LD_LIBRARY_PATH=str(search)))
    for line in output.splitlines():
        if '=> not found' in line:raise ValueError('Unresolved dependency: '+line.strip())
        match=re.match(r'\s*(\S+) => (/\S+) \(',line)
        if match:result.append((match[1],Path(match[2]).resolve()))
    return result

def bundle_libraries(pairs,stage):
    """Copy complete direct ELF closures; apply local RPATH without glibc/ICDs."""
    pending=list(pairs);visited=set();origins={};records={}
    bin_dir=stage/'Bin';bin_dir.mkdir(exist_ok=True)
    for member in stage.rglob('*'):
        if member.is_file() and WAYLAND_HOST.fullmatch(member.name):
            raise ValueError('Host Wayland library in Linux package: '+str(member))
    while pending:
        source,target=pending.pop(0)
        if target in visited:continue
        visited.add(target)
        if not elf(source):raise ValueError('Expected Linux x64 ELF: '+str(source))
        records[target.relative_to(stage).as_posix()]=dict(source=str(source.resolve()),source_sha256=digest(source))
        for name,origin in dependencies(source,bin_dir):
            if HOST.fullmatch(name):continue
            if name in origins and origins[name]!=origin and origin!=(bin_dir/name).resolve():raise ValueError('Conflicting ELF dependency: '+name)
            if name not in origins:
                origins[name]=origin;destination=bin_dir/name
                if destination.exists() and digest(destination)!=digest(origin):raise ValueError('Duplicate ELF name: '+name)
                if origin!=destination.resolve():shutil.copy2(origin,destination)
                pending.append((origin,destination))
        # .NET single-file AppHost is itself checksummed and contains a bundle
        # overlay. Preserve it byte-for-byte; Launch.sh supplies its local search.
        if target.name!='TimeSplitters.Launcher':
            relative=os.path.relpath(bin_dir,target.parent)
            subprocess.run(['patchelf','--set-rpath','$ORIGIN'+('/'+relative if relative!='.' else ''),str(target)],check=True)
    for target in visited:
        for name,path in dependencies(target,bin_dir):
            if not HOST.fullmatch(name) and not path.is_relative_to(stage.resolve()):
                raise ValueError('External application dependency remains: '+name)
    return records

def copy(source,stage,name):
    target=stage/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target);return target

def verify_launcher(directory):
    manifest=json.loads((directory/'Support/Launcher/manifest.json').read_text())
    files=manifest.get('files',{})
    if 'TimeSplitters.Launcher' not in files:raise ValueError('Incomplete launcher manifest')
    for name,sha in files.items():
        part=PurePosixPath(name)
        if not name or part.is_absolute() or '..' in part.parts or '\\' in name or ':' in name:raise ValueError('Unsafe launcher path')
        path=directory/part
        if path.is_symlink() or not path.resolve().is_relative_to(directory.resolve()) or digest(path)!=sha:raise ValueError('Launcher checksum mismatch: '+name)
    actual={p.relative_to(directory).as_posix() for p in directory.rglob('*') if p.is_file()}
    if actual!=set(files)|{'Support/Launcher/manifest.json'}:raise ValueError('Unlisted launcher member')
    return manifest

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
        if item.is_file() and elf(item):
            original=standard/item.relative_to(library)
            pairs.append((original if original.is_file() else item,item))
    return pairs,dict(version=version,wheels=pins)

def verify_payload(stage,manifest):
    required={'Bin/libdxcompiler.so','Bin/futureperfect_recomp','TimeSplitters.Launcher','Launch.sh','Settings/pc-controls.json',
        'Support/Python/bin/python3','Support/Online/online.py','Support/Online/network-identity.json','Source/pc-port-source.zip',
        'Bin/libavcodec.so.60','Bin/libavutil.so.58','Source/FFmpeg/pin.json','Source/FFmpeg/linux_ffmpeg.py',
        'Licenses/SDL.txt','Licenses/DXC.txt','Licenses/DXC-ThirdParty.txt','Licenses/Runtime-GPL-3.0.txt',
        'Licenses/FFmpeg-COPYING.LGPLv2.1.txt','Licenses/Keyboard-mouse-icons.txt','Support/Runtime/dependency-audit.json',
        'Bin/libICE.so.6','Bin/libSM.so.6','Support/Desktop/gdk-pixbuf-query-loaders',
        'Support/Desktop/pixbuf/libpixbufloader-svg.so','Support/Desktop/fonts.conf',
        'Support/Desktop/share/mime/mime.cache','Support/Desktop/share/mime/packages/freedesktop.org.xml'}
    media=manifest.get('media',{})
    archive=media.get('archive','')
    if not archive or Path(archive).name!=archive:raise ValueError('Incomplete media source metadata')
    required.add('Source/FFmpeg/'+archive)
    files=manifest['files']
    if not required<=files.keys():raise ValueError('Incomplete Linux game package')
    for name,sha in files.items():
        p=PurePosixPath(name)
        if WAYLAND_HOST.fullmatch(p.name):raise ValueError('Host Wayland library in Linux package: '+name)
        if ":" in name or not p.parts or p.is_absolute() or '..' in p.parts or '\\' in name or p.parts[0].lower() in {'data','saves','private','textures'} or p.suffix.lower() in {'.xbe','.pak','.iso','.img','.xmv'}:
            raise ValueError('Protected package path: '+name)
        if p.parts[0]=='Settings' and name!='Settings/pc-controls.json':raise ValueError('Personal settings in package')
        path=stage/p
        if path.is_symlink() or not path.resolve().is_relative_to(stage.resolve()) or digest(path)!=sha:raise ValueError('Package checksum mismatch: '+name)
    actual={p.relative_to(stage).as_posix() for p in stage.rglob('*') if p.is_file()}
    if actual!=set(files)|{'BUILD.json'}:raise ValueError('Unlisted package file')
    if digest(stage/'Source/FFmpeg'/archive)!=media.get('sha256'):raise ValueError('FFmpeg source archive checksum mismatch')
    audit=json.loads((stage/'Support/Runtime/dependency-audit.json').read_text())
    if set(audit.get('components',{}))!={name for name in files if elf(stage/name)}:raise ValueError('Incomplete ELF provider audit')
    data=audit.get('data_components',{})
    if set(data)!={'Support/Desktop/share/mime/packages/freedesktop.org.xml'}:raise ValueError('Incomplete desktop data audit')
    for name,record in data.items():
        if record.get('sha256')!=files.get(name):raise ValueError('Desktop source data checksum mismatch')
    for source in audit.get('source_packages',{}).values():
        for name,sha in source['files'].items():
            if files.get(name)!=sha:raise ValueError('Incomplete corresponding sources')
    if (manifest.get('textures') or {}).get('delivery')!='optional':
        raise ValueError('Linux release requires the optional texture policy')

def write_release_assets(manifest,archive_path):
    """Prepare reviewable upload assets; never create a tag or contact GitHub."""
    archive_path.with_suffix('.zip.sha256').write_text(digest(archive_path)+'  '+archive_path.name+'\n')
    archive_path.with_suffix('.BUILD.json').write_text(json.dumps(manifest,indent=2)+'\n')
    channel=dict(manifest['release_channel'],source_commit=manifest['packaging_commit'],platforms=['linux-x64'])
    notes=('<!-- tsfp-release '+json.dumps(channel,separators=(',',':'))+' -->\n\n'
        '# Linux x64 experimental support\n\n'
        'Requires Ubuntu 24.04 / glibc 2.39 or newer, a desktop session and a working Vulkan driver. '
        'Extract into a writable folder and run `./Launch.sh`; import your supported US Xbox game files. '
        'Game data is not included. Start with Original 60 FPS.\n\n'
        'The SDL Linux host shares gameplay, settings and the Vulkan renderer with Windows and Mac. '
        'The package includes its launcher, movie decoder, Online runtime, corresponding source and notices.\n\n'
        'Experimental limitations: physical Linux desktops and Steam Deck acceptance remain pending. '
        'WSL results do not establish native driver, audio, controller or suspend/resume behavior. '
        'Linux Online supports Arcade; Story co-op transport is unavailable. '
        'Use the launcher or dropdowns to edit bindings; native GTK capture-to-bind remains pending. '
        'Higher frame-rate modes remain experimental. Full-campaign and external-network acceptance are pending.\n')
    archive_path.with_suffix('.notes.md').write_text(notes)

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--version',required=True)
    parser.add_argument('--output',type=Path,default=ROOT/'build/releases');parser.add_argument('--launcher',type=Path,default=ROOT/'build/linux-launcher/package')
    args=parser.parse_args()
    if platform.system()!='Linux' or platform.machine()!='x86_64':parser.error('Package on the Linux x64 build host')
    if run('git','status','--porcelain'):parser.error('Commit source changes before preparing a release candidate')
    try:release=release_identity(args.version,publishing=True)
    except ValueError as error:parser.error(str(error))
    if not shutil.which('patchelf'):parser.error('Install patchelf on the build host')
    build=ROOT/'build/linux/game';native=json.loads((build/'BUILD.json').read_text());executable=build/'futureperfect_recomp'
    if digest(executable)!=native['executable_sha256']:parser.error('Native executable differs from BUILD.json')
    for name,sha in native['source_sha256'].items():
        if digest(ROOT/name)!=sha:parser.error('Runtime source changed; rebuild: '+name)
    if not all(native['capabilities'].get(k) for k in ('movie_decode','settings_dialog','debug_dialog')):parser.error('Build the complete desktop/media runtime first')
    name='TimeSplitters-Future-Perfect-PC-'+args.version+'-linux-x64';stage=args.output.resolve()/name
    if stage.exists() or stage.with_name(stage.name+'.zip').exists():parser.error('Use a new output directory; existing packages are preserved')
    stage.mkdir(parents=True);game=copy(executable,stage,'Bin/futureperfect_recomp');game.chmod(0o755)
    launcher_manifest=verify_launcher(args.launcher)
    release['minimum_launcher']=launcher_manifest['version']
    for source in args.launcher.rglob('*'):
        if source.is_symlink():raise ValueError('Linked launcher member')
        if source.is_file():copy(source,stage,source.relative_to(args.launcher))
    copy(ROOT/'config/pc-controls.default.json',stage,'Settings/pc-controls.json')
    copy(ROOT/'LICENSE',stage,'Licenses/Project.txt')
    for file in COMPANION:copy(repo_file(ROOT,'tools',file),stage,'Support/Online/'+file)
    copy(ROOT/'build/online-icon/multiplayer.png',stage,'Support/Online/multiplayer.png')
    for file in ('network-identity.json','network-build.json'):copy(build/'generated'/file,stage,'Support/Online/'+file)
    python_pairs,python=bundle_python(ROOT,stage)
    # Package only our verified decoder-only media build, never the host's
    # GPL/external-codec dependency closure. Include exact sources and recipe.
    # Avalonia loads session libraries through P/Invoke, and GTK loads its SVG
    # decoder as a module: ldd alone cannot discover these desktop dependencies.
    dynamic=[]
    for soname in ('libICE.so.6','libSM.so.6'):
        matches=[Path(line.split(' => ',1)[1]) for line in run('ldconfig','-p').splitlines()
                 if line.strip().startswith(soname+' ') and 'x86-64' in line]
        if len(matches)!=1:raise ValueError('Missing unique desktop dependency: '+soname)
        origin=matches[0].resolve();dynamic.append((origin,copy(origin,stage,'Bin/'+soname)))
    pixbuf=Path('/usr/lib/x86_64-linux-gnu/gdk-pixbuf-2.0')
    for origin,target in [(pixbuf/'gdk-pixbuf-query-loaders','Support/Desktop/gdk-pixbuf-query-loaders'),
                          (pixbuf/'2.10.0/loaders/libpixbufloader-svg.so','Support/Desktop/pixbuf/libpixbufloader-svg.so')]:
        dynamic.append((origin,copy(origin,stage,target)))
    copy(ROOT/'config/build/linux-fonts.conf',stage,'Support/Desktop/fonts.conf')
    # The host cache merges unrelated installed packages and has no dpkg owner.
    # Generate ours from the single authenticated shared-mime-info source input.
    mime=Path('/usr/share/mime/packages/freedesktop.org.xml')
    mime_target=copy(mime,stage,'Support/Desktop/share/mime/packages/freedesktop.org.xml')
    with tempfile.TemporaryDirectory(prefix='tsfp-mime-') as temporary:
        database=Path(temporary)/'mime';(database/'packages').mkdir(parents=True)
        shutil.copy2(mime,database/'packages/freedesktop.org.xml')
        subprocess.run(['update-mime-database',str(database)],check=True)
        copy(database/'mime.cache',stage,'Support/Desktop/share/mime/mime.cache')
    media,media_receipt=linux_ffmpeg.verify_install(ROOT)
    if native.get('release_media')!=media_receipt:raise ValueError('Rebuild the game with --release-media before packaging')
    for stem in media_receipt['pin']['libraries']:
        path=(media/'install/lib'/stem).resolve()
        soname=run('patchelf','--print-soname',path);dynamic.append((path,copy(path,stage,'Bin/'+soname)))
    copy(media/media_receipt['pin']['archive'],stage,'Source/FFmpeg/'+media_receipt['pin']['archive'])
    copy(ROOT/'tools/build/linux_ffmpeg.py',stage,'Source/FFmpeg/linux_ffmpeg.py')
    copy(ROOT/'config/build/ffmpeg-linux.json',stage,'Source/FFmpeg/pin.json')
    for name in ('COPYING.LGPLv2.1','LICENSE.md'):
        copy(media/media_receipt['pin']['archive_prefix']/name,stage,'Licenses/FFmpeg-'+name+'.txt')
    sdk=linux_vulkan_sdk.verify_install(ROOT)
    dxc=sdk/'lib/libdxcompiler.so'
    dynamic.append((dxc,copy(dxc,stage,'Bin/libdxcompiler.so')))
    copy(sdk.parent/'LICENSE.txt',stage,'Support/Runtime/Vulkan-SDK-LICENSE.txt')
    licenses=ROOT/'external/native-runtime-licenses'
    prepare_runtime_licenses(ROOT,licenses,{'DXC.txt','DXC-ThirdParty.txt'})
    for name in ('DXC.txt','DXC-ThirdParty.txt'):copy(licenses/name,stage,'Licenses/'+name)
    upstream=ROOT/'external/ts3FPrecomp'
    for origin,target in [('LICENSE','Runtime-GPL-3.0.txt'),('LICENSE.upstream-MIT','Runtime-upstream-MIT.txt'),('NOTICE','Runtime-NOTICE.txt'),('LICENSES/LGPL-2.1.txt','Runtime-LGPL-2.1.txt'),('third_party/SDL/LICENSE.txt','SDL.txt'),('third_party/volk/LICENSE.md','volk.txt'),('third_party/VulkanMemoryAllocator/LICENSE.txt','VulkanMemoryAllocator.txt')]:copy(upstream/origin,stage,'Licenses/'+target)
    for source in (upstream/'third_party/Vulkan-Headers/LICENSES').iterdir():
        if source.is_file():copy(source,stage,'Licenses/Vulkan-Headers-'+source.name)
    if (ROOT/'vendor/smaa/LICENSE.txt').is_file():copy(ROOT/'vendor/smaa/LICENSE.txt',stage,'Licenses/SMAA.txt')
    copy(ROOT/'assets/ui/input-icons/ATTRIBUTION.md',stage,'Licenses/Keyboard-mouse-icons.txt')
    for origin,name in [('assets/ui/fonts/Selawik-OFL.txt','Selawik-OFL.txt'),('assets/ui/fonts/Cousine-OFL.txt','Cousine-OFL.txt'),('vendor/stb/LICENSE','stb.txt')]:
        copy(ROOT/origin,stage,'Licenses/'+name)
    roots=[(executable,game),*python_pairs,*dynamic];known={p.resolve() for _,p in roots}
    for member in stage.rglob('*'):
        if member.is_file() and elf(member) and member.resolve() not in known:
            origin=args.launcher/member.relative_to(stage)
            roots.append((origin if origin.is_file() else member,member))
    records=bundle_libraries(roots,stage)
    if digest(stage/'TimeSplitters.Launcher')!=json.loads((stage/'Support/Launcher/manifest.json').read_text())['files']['TimeSplitters.Launcher']:
        raise ValueError('Relocation changed the signed launcher bundle')
    subprocess.run([sys.executable,str(repo_file(ROOT,'tools','network-identity.py')),'--output',str(build/'generated'),'--seal-executable',str(game)],check=True)
    # Preserve package-owner copyright/source information for every copied ELF.
    notices=stage/'Support/Runtime';notices.mkdir(parents=True,exist_ok=True)
    audit=collect_dependency_sources(ROOT,stage,records,args.launcher,media,sdk,python,
        {mime_target.relative_to(stage).as_posix():dict(source=str(mime),source_sha256=digest(mime))})
    (notices/'dependencies.json').write_text(json.dumps(audit['components'],indent=2)+'\n')
    source=stage/'Source/pc-port-source.zip';source.parent.mkdir(exist_ok=True)
    with zipfile.ZipFile(source,'x',zipfile.ZIP_DEFLATED) as archive:
        write_source_files(archive,ROOT,run('git','ls-files','-z').split('\0'))
    (stage/'Launch.sh').write_text((ROOT/'tools/release/linux-launch.sh').read_text());(stage/'Launch.sh').chmod(0o755)
    (stage/'README.txt').write_text('Linux x64 experimental support. Extract into a writable folder and run ./Launch.sh.\nImport your supported US Xbox files using the launcher. Game data is not included.\nRequires a Vulkan-capable system driver, system Wayland client/cursor/EGL runtimes (also on X11), and the build host glibc baseline (Ubuntu24.04).\nNo development checkout, system Python or Vulkan SDK is needed.\nFor a read-only game installation use ./Launch.sh --game --data /path/to/game.\nSource, runtime notices and exact build provenance are included. Native desktop and Steam Deck acceptance remain pending.\n')
    public_native={k:v for k,v in native.items() if k not in ('game_directory','configure_command','runtime_libraries','working_tree_changes')}
    manifest=dict(display_settings_version=native.get('display_settings_version',3),version=args.version,platform='linux-x64',renderer='vulkan',release_channel=release,source_commit=native['source_revision'],packaging_commit=run('git','rev-parse','HEAD'),
        launcher_version=launcher_manifest['version'],textures=optional_identity(ROOT) or dict(delivery='optional'),
        minimum_glibc=run('getconf','GNU_LIBC_VERSION').split()[-1],native_build=public_native,python=python,
        runtime_libraries=audit['components'],media=media_receipt['pin'],files={p.relative_to(stage).as_posix():digest(p) for p in stage.rglob('*') if p.is_file()})
    (stage/'BUILD.json').write_text(json.dumps(manifest,indent=2)+'\n');verify_payload(stage,manifest)
    env=dict(os.environ);env.pop('PYTHONPATH',None)
    subprocess.run([str(stage/'Support/Python/bin/python3'),'-c','import ssl,tkinter,cryptography,cffi; t=tkinter.Tk();t.withdraw();t.update();t.destroy();assert ssl.create_default_context().cert_store_stats()["x509_ca"]>0;print("PASS relocated Python/Tk/TLS")'],env=env,check=True,timeout=30)
    archive_path=stage.with_name(stage.name+'.zip')
    with zipfile.ZipFile(archive_path,'x',zipfile.ZIP_DEFLATED,compresslevel=1) as archive:
        for path in sorted(stage.rglob('*')):
            if path.is_file():archive.write(path,stage.name+'/'+path.relative_to(stage).as_posix())
    with zipfile.ZipFile(archive_path) as archive:
        if archive.testzip():raise ValueError('Corrupt Linux package')
    subprocess.run([str(stage/'TimeSplitters.Launcher'),'--verify-game-package',str(archive_path),args.version],check=True,timeout=60)
    write_release_assets(manifest,archive_path);print(archive_path)
if __name__=='__main__':main()
