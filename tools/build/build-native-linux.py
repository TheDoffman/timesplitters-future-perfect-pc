#!/usr/bin/env python3
"""Build the native Linux x64 game from the shared pinned runtime and patch stack."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
import linux_vulkan_sdk
import linux_ffmpeg
from vulkan_sdk import sha256

def run(*args,cwd=ROOT,env=None):
    subprocess.run([str(a) for a in args],cwd=cwd,env=dict(os.environ if env is None else env,TSFP_PROJECT_ROOT=str(ROOT)),check=True)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir',type=Path,required=True,help='Existing supported game folder; no duplicate asset extract')
    parser.add_argument('--jobs',type=int,default=2,help='Compiler workers (default2 limits WSL memory use)')
    parser.add_argument('--setup',action='store_true',help='Initialize pinned submodules and build the symbol scanner')
    parser.add_argument('--generate',action='store_true',help='Generate current guest C from the verified XBE')
    parser.add_argument('--test',action='store_true',help='Run protected-MMIO regression after building')
    parser.add_argument('--release-media',action='store_true',help='Compile/link against the verified decoder-only media build for packaging')
    parser.add_argument('--host-optimization',choices=('optimized','debug'),default='optimized',
                        help='Optimize host libraries while retaining assertions/symbols; debug restores unoptimized host code')
    args=parser.parse_args()
    if platform.system()!='Linux' or platform.machine().lower() not in ('x86_64','amd64'):
        parser.error('Requires Linux x64 and a Linux Python environment')
    if sys.version_info<(3,12):parser.error('Use Python 3.12+ with the shared requirements')
    if not 1<=args.jobs<=8:parser.error('--jobs must be 1â€“8')
    pins=(ROOT/'tsfp.ps1').read_text()
    revision=re.search(r"\$revision = '([a-f0-9]{40})'",pins)[1]
    expected=re.search(r"\$expectedHash = '([A-F0-9]{64})'",pins)[1].lower()
    game=args.game_dir.resolve();xbe=game/'default.xbe'
    if not xbe.is_file() or sha256(xbe)!=expected:parser.error('Missing/unsupported US Xbox executable')
    if not (game/'pak').is_dir() or not (game/'xmv').is_dir():parser.error('Game folder must contain pak and xmv')
    if shutil.disk_usage(ROOT).free<4*1024**3:parser.error('Build needs at least 4 GiB free')
    sdk=linux_vulkan_sdk.verify_install(ROOT)
    tools=Path(sys.executable).parent;cmake=tools/'cmake';ninja=tools/'ninja'
    if not cmake.is_file() or not ninja.is_file():parser.error('Install shared requirements.txt in this Linux environment')
    runtime=ROOT/'external/ts3FPrecomp';build=ROOT/'build/linux/game'
    if args.setup:run(sys.executable,ROOT/'tools/build/prepare-runtime.py')
    actual=subprocess.check_output(['git','-C',str(runtime),'rev-parse','HEAD'],text=True).strip()
    if actual!=revision:parser.error('Runtime differs from shared pin')
    run(sys.executable,ROOT/'tools/build/apply-patches.py',runtime,ROOT/'patches')
    if args.setup:
        run('git','-C',runtime,'submodule','update','--init','--depth','1','third_party/SDL',
            'third_party/Vulkan-Headers','third_party/volk','third_party/VulkanMemoryAllocator','third_party/XbSymbolDatabase')
        db=runtime/'third_party/XbSymbolDatabase'
        run(cmake,'-S',db,'-B',db/'build','-G','Ninja','-DCMAKE_BUILD_TYPE=Release',
            '-DCMAKE_MAKE_PROGRAM='+str(ninja),'-DXBSDB_BUILD_UNITTEST=OFF')
        run(cmake,'--build',db/'build','--target','XbSymbolDatabaseCLI','--parallel','4')
    if args.generate:
        run(sys.executable,'-m','tools.xdk_symbols',xbe,cwd=runtime)
        run(sys.executable,'scripts/recompile.py',xbe,'--work-dir',ROOT/'private/pipeline',
            '--project','titles/futureperfect','--game-name','TimeSplitters: Future Perfect','--trace-all-entries',cwd=runtime)
        run(sys.executable,ROOT/'tools/build/recomp-generation.py',runtime,'--record')
    run(sys.executable,ROOT/'tools/build/recomp-generation.py',runtime)
    run(sys.executable,ROOT/'tools/build/network-identity.py','--runtime',runtime,'--game',game,'--output',build/'generated')
    (build/'generated/online-python.h').write_text('#define TSFP_ONLINE_PYTHON '+json.dumps(sys.executable)+'\n')
    # Keep Debug assertions and symbols, plus the title's existing precise
    # -O2/no-vectorization policy. Host libraries need optimization as well.
    host_flags='-O2 -g' if args.host_optimization=='optimized' else '-g'
    command=[cmake,'-S',runtime/'titles/futureperfect','-B',build,'-G','Ninja',
        '-DCMAKE_BUILD_TYPE=Debug','-DCMAKE_MAKE_PROGRAM='+str(ninja),
        '-DCMAKE_C_FLAGS_DEBUG='+host_flags,'-DCMAKE_CXX_FLAGS_DEBUG='+host_flags,
        '-DTSFP_NATIVE_DIR='+str(ROOT/'src/native'),'-DTSFP_NATIVE_RHI=ON','-DTSFP_VULKAN_SDK='+str(sdk)]
    media=None;configure_env=dict(os.environ)
    command.append('-U*XMV_FFMPEG*')
    if args.release_media:
        media_root,media=linux_ffmpeg.verify_install(ROOT)
        configure_env['PKG_CONFIG_PATH']=str(media_root/'install/lib/pkgconfig')+(':'+configure_env['PKG_CONFIG_PATH'] if configure_env.get('PKG_CONFIG_PATH') else '')
    run(*command,env=configure_env)
    run(cmake,'--build',build,'--target','futureperfect_recomp','tsfp_native_mmio_test','--parallel',args.jobs)
    exe=build/'futureperfect_recomp'
    run(sys.executable,ROOT/'tools/build/network-identity.py','--output',build/'generated','--seal-executable',exe)
    sources=[p for area in ('src','patches','vendor/smaa') for p in (ROOT/area).rglob('*')
             if p.is_file() and 'legacy' not in p.relative_to(ROOT).parts]
    manifest=dict(platform='linux-x64',renderer='vulkan',build_type='Debug',release_media=media,
        host_optimization=args.host_optimization,host_debug_flags=host_flags,assertions_enabled=True,
        source_revision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
        working_tree_changes=subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True),
        runtime_revision=revision,xbe_sha256=expected,game_directory=str(game),
        host=platform.platform(),compiler=subprocess.check_output(['clang','--version'],text=True).splitlines()[0],
        sdk_version=linux_vulkan_sdk.pin()['version'],executable_sha256=sha256(exe),
        compatibility_identity=json.loads((build/'generated/network-identity.json').read_text())['compatibility'],
        source_sha256={p.relative_to(ROOT).as_posix():sha256(p) for p in sorted(sources)},
        configure_command=[str(a) for a in command],
        runtime_libraries=subprocess.check_output(['ldd',str(exe)],text=True),
        capabilities=dict(shared_input=True,shared_audio=True,settings_dialog=True,debug_dialog=True,
                          source_online_adapter=True,movie_decode=True))
    (build/'BUILD.json').write_text(json.dumps(manifest,indent=2)+'\n')
    if args.test:run(build/'tsfp_native_mmio_test')
    print('Linux game built:',exe)
    print('Build success does not establish gameplay; use the isolated Linux runner.')

if __name__=='__main__':main()
