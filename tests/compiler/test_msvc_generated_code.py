"""Compiler exceptions follow definitions when generated chunks are regrouped."""
import json
import shutil
import subprocess
from pathlib import Path
import pytest
from msvc_generated_code import publish,resolve


def policy():
    return {'version':1,'title_id':'45410066','rules':[
        {'symbol':'sub_00001000','options':['/Od','/Ob0']},
        {'symbol':'tsfp_context_run_00002000','options':['/Ob1']}]}


def test_regrouped_chunks_keep_rules_and_source_bytes(tmp_path):
    a=tmp_path/'recomp_0042.c';b=tmp_path/'recomp_context_0012.c'
    a.write_text('void sub_00001000(void) { }\n',encoding='utf-8')
    b.write_text('int tsfp_context_run_00002000(unsigned pc,unsigned *next) {return 1;}\n',encoding='utf-8')
    snapshots={p.name:p.read_bytes() for p in (a,b)}
    rows=publish(tmp_path,policy(),tmp_path/'policy.cmake')
    assert {r['source']:r['options'] for r in rows}=={a.name:['/Od','/Ob0'],b.name:['/Ob1']}
    assert {p.name:p.read_bytes() for p in (a,b)}==snapshots
    renamed=tmp_path/'recomp_9999.c';a.rename(renamed)
    rows=publish(tmp_path,policy(),tmp_path/'policy.cmake')
    assert any(r['source']==renamed.name and r['symbols']==['sub_00001000'] for r in rows)
    text=(tmp_path/'policy.cmake').read_text(encoding='utf-8')
    assert a.name not in text and renamed.name in text
    assert json.loads((tmp_path/'policy.json').read_text())==rows


def test_shared_unit_uses_stronger_rule_without_duplicate_options(tmp_path):
    (tmp_path/'recomp_0000.c').write_text('void sub_00001000(void){}\nint tsfp_context_run_00002000(unsigned pc,unsigned *next){return 1;}\n')
    assert resolve(tmp_path,policy())==[{'source':'recomp_0000.c',
        'symbols':['sub_00001000','tsfp_context_run_00002000'],'options':['/Od','/Ob0']}]


def test_missing_or_comment_only_definition_preserves_previous_policy(tmp_path):
    out=tmp_path/'policy.cmake';out.write_text('previous')
    (tmp_path/'recomp_0000.c').write_text('/*\nvoid sub_00001000(void) {}\n*/\nint tsfp_context_run_00002000(unsigned pc,unsigned *next){return 1;}\n')
    with pytest.raises(ValueError,match='one definition'):publish(tmp_path,policy(),out)
    assert out.read_text()=='previous'


def test_duplicate_definition_and_arbitrary_flags_are_rejected(tmp_path):
    for n in (1,2):(tmp_path/f'recomp_{n:04}.c').write_text('void sub_00001000(void){}\n')
    with pytest.raises(ValueError,match='one definition'):resolve(tmp_path,policy())
    manifest=policy();manifest['rules'][0]['options']=['/O2','/w']
    with pytest.raises(ValueError,match='compiler rule'):resolve(tmp_path,manifest)


@pytest.mark.parametrize('configuration',['Debug','Release'])
def test_cmake_resolves_release_rules_without_changing_debug(tmp_path,configuration):
    root=Path(__file__).resolve().parents[2]
    manifest=json.loads((root/'config/build/msvc-generated-code.json').read_text())
    generated=tmp_path/'gen';generated.mkdir()
    bodies=[]
    for rule in manifest['rules']:
        symbol=rule['symbol']
        bodies.append(f'void {symbol}(void){{}}' if symbol.startswith('sub_') else
                      f'int {symbol}(unsigned pc,unsigned *next){{return 1;}}')
    (generated/'recomp_0000.c').write_text('\n'.join(bodies)+'\n',encoding='utf-8')
    (tmp_path/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.20)
project(policy NONE)
add_custom_target(probe)
include("{(root/'src/runtime/msvc_generated_code.cmake').as_posix()}")
tsfp_configure_generated_msvc(probe "{generated.as_posix()}")
get_source_file_property(options "{(generated/'recomp_0000.c').as_posix()}" COMPILE_OPTIONS)
file(GENERATE OUTPUT "${{CMAKE_BINARY_DIR}}/effective-$<CONFIG>.txt" CONTENT "${{options}}")
''',encoding='utf-8')
    cmake=shutil.which('cmake')
    if not cmake:
        candidate=root/'.venv/Scripts/cmake.exe'
        assert candidate.is_file(),'Install the pinned CMake requirement'
        cmake=str(candidate)
    result=subprocess.run([cmake,'-S',str(tmp_path),'-B',str(tmp_path/'build'),
                           '-DCMAKE_BUILD_TYPE='+configuration],capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    options=(tmp_path/f'build/effective-{configuration}.txt').read_text().strip(';\n ')
    assert options==('' if configuration=='Debug' else '/Od;/Ob0')
