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
