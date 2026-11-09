"""Real ELF loading proves staged providers win and single-file launchers stay intact."""
import importlib.util,os,shutil,subprocess,sys
from pathlib import Path
import pytest
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('elf_package',ROOT/'tools/release/package-native-linux.py')
package=importlib.util.module_from_spec(spec);spec.loader.exec_module(package)

@pytest.mark.skipif(sys.platform!='linux',reason='Actual Linux ELF loader contract')
@pytest.mark.parametrize('launcher',[False,True])
def test_staged_provider_and_original_bundle(tmp_path,launcher):
    if not shutil.which('patchelf'):pytest.skip('patchelf required')
    old=tmp_path/'old';new=tmp_path/'new';stage=tmp_path/'package';(stage/'Bin').mkdir(parents=True);old.mkdir();new.mkdir()
    for folder,answer in ((old,1),(new,2)):
        source=folder/'choice.c';source.write_text(f'int answer(void){{return {answer};}}')
        subprocess.run(['cc','-shared','-fPIC','-Wl,-soname,libchoice.so',str(source),'-o',str(folder/'libchoice.so')],check=True)
    main=tmp_path/'main.c';main.write_text('#include <stdio.h>\nint answer(void);int main(void){printf("%d\\n",answer());return 0;}')
    binary=tmp_path/'app';subprocess.run(['cc',str(main),'-L'+str(old),'-lchoice','-Wl,-rpath,'+str(old),'-o',str(binary)],check=True)
    if launcher:
        with binary.open('ab') as output:output.write(b'PRESERVE-SINGLE-FILE-OVERLAY')
    destination=stage/('TimeSplitters.Launcher' if launcher else 'Bin/game')
    library=stage/'Bin/libchoice.so';shutil.copy2(binary,destination);shutil.copy2(new/'libchoice.so',library)
    before=binary.read_bytes();records=package.bundle_libraries([(binary,destination),(new/'libchoice.so',library)],stage)
    result=subprocess.check_output([str(destination)],text=True,env=dict(os.environ,LD_LIBRARY_PATH=str(stage/'Bin')))
    assert result.strip()=='2' and binary.read_bytes()==before
    assert records['Bin/libchoice.so']['source']==str((new/'libchoice.so').resolve())
    if launcher:assert destination.read_bytes()==before


@pytest.mark.skipif(sys.platform!='linux',reason='Actual Linux ELF loader contract')
@pytest.mark.parametrize('modern_host',[False,True])
def test_host_wayland_with_old_and_new_graphics_driver(tmp_path,monkeypatch,modern_host):
    """A newer ICD must see host symbols, while the Ubuntu-era ABI still works."""
    if not shutil.which('patchelf'):pytest.skip('patchelf required')
    old=tmp_path/'build-host';host=tmp_path/'user-host';stage=tmp_path/'package'
    for folder in (old,host,stage/'Bin'):folder.mkdir(parents=True)
    soname='libwayland-client.so.0'
    for folder,modern in ((old,False),(host,modern_host)):
        source=folder/'wayland.c'
        source.write_text('int wl_baseline(void){return 1;}\n'+
                          ('int wl_display_dispatch_queue_timeout(void){return 2;}\n' if modern else ''))
        subprocess.run(['cc','-shared','-fPIC','-Wl,-soname,'+soname,str(source),'-o',str(folder/soname)],check=True)
    # The application was built on the older distro. Its system graphics driver
    # is loaded later, just as SDL loads Mesa when creating a Vulkan window.
    symbol='wl_display_dispatch_queue_timeout' if modern_host else 'wl_baseline'
    driver=host/'driver.c';driver.write_text(f'int {symbol}(void);int driver(void){{return {symbol}();}}')
    subprocess.run(['cc','-shared','-fPIC',str(driver),'-L'+str(host),'-l:'+soname,
                    '-o',str(host/'driver.so')],check=True)
    main=tmp_path/'main.c'
    main.write_text('#include <dlfcn.h>\n#include <stdio.h>\nint wl_baseline(void);\n'
                    'int main(int argc,char **argv){if(wl_baseline()!=1)return 1;'
                    'void *p=dlopen(argv[1],RTLD_NOW);if(!p){puts(dlerror());return 2;}'
                    'int (*f)(void)=dlsym(p,"driver");return !f || f()!=EXPECTED;}')
    binary=tmp_path/'game'
    subprocess.run(['cc',str(main),'-DEXPECTED='+('2' if modern_host else '1'),'-ldl',
                    '-L'+str(old),'-l:'+soname,'-Wl,-rpath,'+str(old),'-o',str(binary)],check=True)
    destination=stage/'Bin/game';shutil.copy2(binary,destination)
    # Give ldd the same simulated system directory used for execution below.
    # Do not install fixture libraries into the machine's real /usr/lib.
    dependencies=package.dependencies
    monkeypatch.setattr(package,'dependencies',lambda path,search=None:
        dependencies(path,str(search)+os.pathsep+str(host) if search else str(host)))
    records=package.bundle_libraries([(binary,destination)],stage)
    assert not (stage/'Bin'/soname).exists()
    assert 'Bin/'+soname not in records
    # Stand in for the user's default system library directory, after package
    # RUNPATH. This tests resolution without LD_PRELOAD or LD_LIBRARY_PATH.
    subprocess.run(['patchelf','--add-rpath',str(host),str(destination)],check=True)
    env={k:v for k,v in os.environ.items() if k not in ('LD_LIBRARY_PATH','LD_PRELOAD')}
    result=subprocess.run([str(destination),str(host/'driver.so')],env=env,capture_output=True,text=True)
    assert result.returncode==0,result.stdout+result.stderr
    if modern_host:
        # Negative control reproduces the Deck failure if the old bundled
        # provider is allowed back in front of the modern system provider.
        shutil.copy2(old/soname,stage/'Bin'/soname)
        failed=subprocess.run([str(destination),str(host/'driver.so')],env=env,capture_output=True,text=True)
        assert failed.returncode==2 and 'wl_display_dispatch_queue_timeout' in failed.stdout
