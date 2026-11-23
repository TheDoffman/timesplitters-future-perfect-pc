"""Execute the production CMake wiring for native, Windows and legacy inputs."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import pytest
ROOT=Path(__file__).resolve().parents[2]


@pytest.mark.parametrize('host,vulkan,rhi,pc,native,expected',[
    ('windows',True,False,True,False,'pc'),
    ('windows',False,False,True,False,None),
    ('linux',False,True,False,True,'native'),
    ('mac',False,True,False,True,'native'),
    ('mac',True,False,False,True,None),
    ('linux',False,True,True,True,'pc'),
    ('linux',False,True,False,False,None),
])
def test_common_opengl_source_selection(tmp_path,host,vulkan,rhi,pc,native,expected):
    text=(ROOT/'external/ts3FPrecomp/src/d3d/CMakeLists.txt').read_text()
    opening='if(WIN32 OR (UNIX AND TSFP_NATIVE_RHI))'
    assert opening in text
    block=text.split('# OpenGL startup recovery build',1)[1].split('# End OpenGL startup recovery build',1)[0]
    pc_source=tmp_path/'pc'
    native_source=tmp_path/'native/src'
    for source,label in [(pc_source,'pc'),(native_source,'native')]:
        folder=source/'graphics';folder.mkdir(parents=True)
        (folder/'opengl_backend.cmake').write_text('function(tsfp_add_opengl_backend target runtime)\n'
            'file(WRITE "${CMAKE_BINARY_DIR}/selected.txt" "'+label+';${target}")\nendfunction()\n')
    def flag(value):return 'ON' if value else 'OFF'
    script='cmake_minimum_required(VERSION 3.20)\nproject(wiring NONE)\n'
    script+=f'set(WIN32 {flag(host=="windows")})\nset(UNIX {flag(host!="windows")})\n'
    script+=f'set(APPLE {flag(host=="mac")})\nset(XBOXRECOMP_VULKAN {flag(vulkan)})\nset(TSFP_NATIVE_RHI {flag(rhi)})\n'
    if pc:script+=f'set(TSFP_PC_CONTROLS_DIR "{pc_source.as_posix()}")\n'
    if native:script+=f'set(TSFP_NATIVE_DIR "{(native_source/"native").as_posix()}")\n'
    script+=opening+'\n'+block+'\nendif()\n'
    if not pc:script+='if(DEFINED TSFP_PC_CONTROLS_DIR)\nmessage(FATAL_ERROR "Native wiring enabled unrelated PC features")\nendif()\n'
    (tmp_path/'CMakeLists.txt').write_text(script)
    suffix='.exe' if os.name=='nt' else ''
    tools=Path(sys.executable).parent
    # A global Windows Python installs entry points under Scripts, while a
    # venv places them beside python.exe. Resolve this interpreter's tools first.
    search=os.pathsep.join(map(str,(tools,tools/'Scripts')))
    cmake=shutil.which('cmake'+suffix,path=search)
    ninja=shutil.which('ninja'+suffix,path=search)
    assert cmake and ninja, 'Install the pinned cmake/ninja requirements for this Python'
    result=subprocess.run([cmake,'-S',str(tmp_path),'-B',str(tmp_path/'build'),
        '-G','Ninja','-DCMAKE_MAKE_PROGRAM='+ninja],capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stdout+result.stderr
    selected=tmp_path/'build/selected.txt'
    assert selected.exists()==(expected is not None)
    if expected:assert selected.read_text()==expected+';xbox_d3d8'
