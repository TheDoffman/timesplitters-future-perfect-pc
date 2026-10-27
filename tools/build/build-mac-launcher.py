#!/usr/bin/env python3
"""Build the native Apple Silicon launcher from shared Windows launcher policy."""

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
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import struct
import tarfile
import urllib.request
import zipfile
ROOT=_layout_root
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def sign_bundle(app):
 # A generated bundle reused from Finder/cloud storage can retain metadata
 # that codesign refuses. Normalize this owned generated output only.
 subprocess.run(['/usr/bin/xattr','-cr',str(app)],check=True)
 # Sign nested native code first, then seal the final plist/resources. A signed
 # command-line host alone is not a signed application bundle.
 for library in sorted((app/'Contents/MacOS').glob('*.dylib')):
  subprocess.run(['/usr/bin/codesign','--force','--sign','-','--timestamp=none',str(library)],check=True)
 subprocess.run(['/usr/bin/codesign','--force','--sign','-','--timestamp=none',str(app)],check=True)
 subprocess.run(['/usr/bin/codesign','--verify','--deep','--strict','--verbose=2',str(app)],check=True)
def ico_to_icns(source,destination):
 data=source.read_bytes()
 if len(data)<6 or data[:4]!=b'\0\0\1\0':raise ValueError('Invalid launcher icon')
 chunks=[];types={16:b'icp4',32:b'icp5',64:b'icp6',128:b'ic07',256:b'ic08'}
 for i in range(struct.unpack_from('<H',data,4)[0]):
  offset=6+16*i;w,h,_,_,_,_,size,start=struct.unpack_from('<BBBBHHII',data,offset);w=w or 256;h=h or 256
  if w not in types or w!=h:continue
  payload=data[start:start+size]
  if len(payload)!=size or not payload.startswith(b'\x89PNG'):raise ValueError('Expected original PNG icon entry')
  chunks.append(types[w]+struct.pack('>I',8+len(payload))+payload)
 if not chunks:raise ValueError('Launcher icon contains no supported sizes')
 payload=b''.join(chunks);destination.write_bytes(b'icns'+struct.pack('>I',8+len(payload))+payload)
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--dotnet',type=Path);p.add_argument('--install-sdk',action='store_true');p.add_argument('--output',type=Path,default=ROOT/'build/macos-launcher');p.add_argument('--artwork-dir',type=Path,default=ROOT/'private/launcher-art');p.add_argument('--require-artwork',action='store_true');p.add_argument('--test',action='store_true');p.add_argument('--package',action='store_true')
 a=p.parse_args();pin=json.loads((ROOT/'config/build/mac-launcher-sdk.json').read_text());sdk=(a.dotnet or ROOT/'external/dotnet/dotnet').resolve()
 if a.install_sdk and not sdk.exists():
  if shutil.disk_usage(ROOT).free<2*1024**3:raise SystemExit('At least 2 GiB free is required for the SDK and build.')
  sdk.parent.mkdir(parents=True,exist_ok=True);archive=sdk.parent/'sdk.tar.gz'
  urllib.request.urlretrieve(pin['url'],archive)
  if hashlib.sha512(archive.read_bytes()).hexdigest()!=pin['sha512']:raise SystemExit('SDK checksum mismatch; archive retained for inspection.')
  with tarfile.open(archive) as t:
   for item in t.getmembers():
    if item.name.startswith('/') or '..' in Path(item.name).parts:raise SystemExit('Unsafe SDK archive path.')
   t.extractall(sdk.parent)
  archive.unlink()
 if not sdk.exists():raise SystemExit('Pinned SDK missing. Use --install-sdk or --dotnet path/to/dotnet.')
 env=dict(os.environ,DOTNET_CLI_TELEMETRY_OPTOUT='1',DOTNET_SKIP_FIRST_TIME_EXPERIENCE='1',DOTNET_GENERATE_ASPNET_CERTIFICATE='false',DOTNET_NOLOGO='1')
 if subprocess.check_output([str(sdk),'--version'],env=env,text=True).strip()!=pin['version']:raise SystemExit('Use the SDK version pinned in config/build/mac-launcher-sdk.json.')
 output=a.output.resolve();output.mkdir(parents=True,exist_ok=True)
 if shutil.disk_usage(output).free<1024**3:raise SystemExit('At least 1 GiB free is required for publishing.')
 artwork=a.artwork_dir.resolve()
 for name in ('launcher-banner.png','launcher-logo.png','launcher-backdrop.png'):
  if a.require_artwork and not (artwork/name).is_file():raise SystemExit('Required original artwork missing: '+name)
  if (artwork/name).exists() and (artwork/name).stat().st_size>8*1024**2:raise SystemExit('Artwork exceeds 8 MiB: '+name)
 source=ROOT/'tools/launcher/macos';spec=importlib.util.spec_from_file_location('layout',source/'generate_layout.py');layout=importlib.util.module_from_spec(spec);spec.loader.exec_module(layout)
 layout.generate(ROOT/'tools/launcher/Launcher.xaml',source/'NativeWindow.axaml')
 for theme in ('PageTheme','TabsTheme'):layout.generate_theme(ROOT/f'tools/launcher/{theme}.xaml',source/f'{theme}.axaml')
 app=output/'TimeSplitters Launcher.app';binary_dir=app/'Contents/MacOS';binary_dir.mkdir(parents=True,exist_ok=True)
 publish_dir=output/'publish';publish_dir.mkdir(parents=True,exist_ok=True)
 command=[str(sdk),'publish',str(source/'NativeLauncher.csproj'),'-p:RestoreLockedMode=true','-c','Release','-r','osx-arm64','--self-contained','true','-p:PublishSingleFile=true','-p:_EnableMacOSCodeSign=false','-p:DebugType=None','-p:DebugSymbols=false','-p:IncludeNativeLibrariesForSelfExtract=true','-p:EnableCompressionInSingleFile=true',f'-p:BaseIntermediateOutputPath={output}/obj/',f'-p:BaseOutputPath={output}/bin/',f'-p:ArtworkDir={artwork}','-o',str(publish_dir)]
 # Regenerate the owned bundle executable with a fresh inode. Reusing a
 # previously signed single-file host can retain a stale macOS signature.
 generated_binary=publish_dir/'TimeSplitters.Launcher'
 if generated_binary.exists():generated_binary.unlink()
 intermediate_host=output/'obj/Release/net8.0/osx-arm64/singlefilehost'
 if intermediate_host.exists():intermediate_host.unlink()
 subprocess.run(command,env=env,check=True,cwd=ROOT)
 # Assemble the complete bundle before signing; the resource envelope must
 # cover the final plist, icon, notices and nested native libraries.
 for generated in publish_dir.iterdir():
  if generated.is_file():os.replace(generated,binary_dir/generated.name)
 version=re.search(r'Value = "([^"]+)"',(ROOT/'tools/launcher/LauncherVersion.cs').read_text()).group(1)
 with (app/'Contents/Info.plist').open('wb') as f:plistlib.dump({'CFBundleName':'TimeSplitters Launcher','CFBundleDisplayName':'TimeSplitters Launcher','CFBundleIdentifier':'community.timesplitters.launcher','CFBundleExecutable':'TimeSplitters.Launcher','CFBundlePackageType':'APPL','CFBundleShortVersionString':version,'CFBundleVersion':version,'LSMinimumSystemVersion':'12.0','NSHighResolutionCapable':True,**({'CFBundleIconFile':'launcher.icns'} if (artwork/'launcher.ico').exists() else {})},f)
 licenses=app/'Contents/Resources/Licenses';licenses.mkdir(parents=True,exist_ok=True)
 if (artwork/'launcher.ico').is_file():ico_to_icns(artwork/'launcher.ico',app/'Contents/Resources/launcher.icns')
 for name in ('LICENSE.txt','ThirdPartyNotices.txt'):
  if (sdk.parent/name).exists():shutil.copyfile(sdk.parent/name,licenses/('dotnet-'+name))
 (licenses/'Avalonia.txt').write_text('Avalonia '+ '11.3.22'+'\nMIT License\nCopyright (c) AvaloniaUI\n\nPermission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:\n\nThe above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.\n\nTHE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.\n')
 # Preserve shipped runtime dependency notices from the locked NuGet graph.
 assets=json.loads((output/'obj/project.assets.json').read_text())
 for package,metadata in assets.get('libraries',{}).items():
  for base in assets.get('packageFolders',{}):
   package_dir=Path(base)/metadata.get('path','')
   for notice in package_dir.glob('*.txt'):
    if any(word in notice.name.lower() for word in ('license','notice','copying')):
     shutil.copyfile(notice,licenses/(package.replace('/','-')+'-'+notice.name))
 sign_bundle(app)
 files={str(f.relative_to(output)):digest(f) for f in app.rglob('*') if f.is_file()}
 manifest=output/'Support/Launcher/manifest.json';manifest.parent.mkdir(parents=True,exist_ok=True);manifest.write_text(json.dumps({'schema':1,'version':version,'files':files},indent=2)+'\n')
 receipt={'source_revision':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'working_tree_changes':subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True),'source_sha256':{str(f.relative_to(ROOT)):digest(f) for f in [ROOT/'tools/build/build-mac-launcher.py',ROOT/'config/build/mac-launcher-sdk.json',*sorted((ROOT/'tools/launcher').glob('*.xaml')),ROOT/'tests/launcher/launcher_shared_ui.cs',ROOT/'tests/launcher/launcher_restart.cs',ROOT/'tools/launcher/macos/NativeLauncher.csproj',ROOT/'tools/launcher/macos/packages.lock.json',ROOT/'tools/launcher/macos/generate_layout.py',*sorted((ROOT/'tools/launcher').glob('*.cs')),*sorted((ROOT/'tools/launcher/macos').glob('*.cs'))]},'sdk':pin,'avalonia':'11.3.22','artwork':{n:digest(artwork/n) for n in ('launcher-banner.png','launcher-logo.png','launcher-backdrop.png') if (artwork/n).exists()},'files':files}
 (output/'BUILD-launcher.json').write_text(json.dumps(receipt,indent=2)+'\n')
 binary=binary_dir/'TimeSplitters.Launcher'
 if a.test:
  for mode in ('--test','--test-ui'):subprocess.run([str(binary),mode],check=True,cwd=ROOT)
 if a.package:
  archive=output/f'TimeSplitters-Launcher-v{version}-macos-arm64.zip'
  with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
   for name in list(files)+['Support/Launcher/manifest.json']:z.write(output/name,name)
  checksum=digest(archive);archive.with_suffix('.zip.sha256').write_text(f'{checksum}  {archive.name}\n')
  (output/'launcher-update-macos-arm64.json').write_text(json.dumps({'schema':1,'version':version,'asset_name':archive.name,'size':archive.stat().st_size,'sha256':checksum},indent=2)+'\n')
 print(app)
if __name__=='__main__':main()
