"""Original runtime-hook planning fixtures and execution of generated ABI glue."""

from project_paths import source_file as repo_file

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import translation_hooks as hooks


def symbol(name, address=0x2000, kind='function'):
    return dict(name=name, address=address, kind=kind)


def test_source_markers_exclude_decoys_and_known_disabled_branches(tmp_path):
    source = tmp_path / 'hooks.c'
    source.write_text('''/*
HLE_EXPORT(Comment)
*/
const char *s = "HLE_IMPORT_VAR(String)";
#define HLE_EXPORT(Name) void hle_##Name(void)
#if 0
HLE_EXPORT(Disabled)
HLE_ORIGINAL(Disabled)
#else
HLE_EXPORT(Live)
#endif
#if UNKNOWN_HOST
HLE_IMPORT_VAR(HostA)
#else
HLE_IMPORT_VAR(HostB)
#endif
HLE_ORIGINAL(\
  Live)
''')
    assert hooks.implemented_names([tmp_path, source]) == {'Live'}
    assert hooks.imported_variables([source]) == {'HostA', 'HostB'}
    assert hooks.wanted_originals([source]) == {'Live'}


def test_catalog_selects_records_and_resolves_distinct_addresses(tmp_path):
    path = tmp_path / 'symbols.json'
    path.write_text(json.dumps(dict(symbols=[symbol('F'), symbol('Data', 0x3000, 'variable')])))
    assert hooks.load_symbols(path) == [symbol('F')]
    assert hooks.load_variables(path) == [symbol('Data', 0x3000, 'variable')]
    values, notes = hooks.resolve_variables({'F', 'Data', 'Missing'},
        [symbol('F'), symbol('F'), symbol('Data', 0x3000), symbol('Data', 0x4000)])
    assert values == {'Data': 0, 'F': 0x2000, 'Missing': 0}
    assert len(notes) == 2


@pytest.mark.parametrize('entry', [symbol('not valid'), symbol('Valid', -1), symbol('Valid', 2**32),
                                  symbol('Valid', True), 'not a record'])
def test_invalid_catalog_is_rejected_before_emitting_c(tmp_path, entry):
    path = tmp_path / 'bad.json'
    path.write_text(json.dumps(dict(symbols=[entry])))
    with pytest.raises(ValueError):
        hooks.load_symbols(path)


def test_cleanup_uses_instructions_not_opcode_bytes_in_immediates():
    assert hooks.stack_cleanup(bytes.fromhex('b8c2080000c3'), 0x2000) == 0
    assert hooks.stack_cleanup(bytes.fromhex('c20800c20800'), 0x2000) == 8
    assert hooks.stack_cleanup(bytes.fromhex('c20800c20400'), 0x2000) is None
    assert hooks.stack_cleanup(bytes.fromhex('ebfe'), 0x2000) is None


@pytest.mark.parametrize('suffix', ['eax0', 'esp1', 'eax1_eax2', 'eax1_ecx1', 'bad1', 'eax25', 'eax1_junk'])
def test_malformed_register_abi_names_are_not_partially_accepted(suffix):
    name = 'Draw_4__LTCG_' + suffix
    assert hooks.parse_ltcg(name) == (name, {})


def test_planning_requires_unique_known_nonmanual_binding():
    records = [symbol('Duplicate'), symbol('Duplicate'), symbol('Manual', 0x3000),
               symbol('Unknown', 0x4000), symbol('AliasA', 0x5000), symbol('AliasB', 0x5000)]
    replacements, notes = hooks.plan(records, {s['name'] for s in records},
                                    {0x2000, 0x3000, 0x5000}, {0x3000}, lambda _: 4)
    assert replacements == {0x2000: ('Duplicate', 4, {})}
    assert any('conflicting' in note for note in notes)
    assert any('hand-written' in note for note in notes)
    assert any('not a detected' in note for note in notes)


def test_register_plan_prefers_plain_name_and_measured_cleanup():
    name = 'Draw_4__LTCG_ecx1_eax3'
    assert hooks.parse_ltcg(name) == ('Draw', {1: 'ecx', 3: 'eax'})
    replacements, _ = hooks.plan([symbol(name)], {'Draw'}, {0x2000}, set(), lambda _: 4)
    assert replacements == {0x2000: ('Draw', 4, {1: 'ecx', 3: 'eax'})}
    replacements, _ = hooks.plan([symbol(name), symbol('Draw', 0x3000)], {'Draw'},
                                {0x2000, 0x3000}, set(), lambda _: 8)
    assert replacements == {0x3000: ('Draw', 8, {})}


@pytest.mark.parametrize('pop,registers', [(-4, {}), (True, {}), (65536, {}), (2, {1:'eax'}),
    (0, {3:'eax'}), (96, {1:'eax'}), (0, {1:'esp'}), (0, {1:'eax', 2:'eax'})])
def test_bad_or_oversized_register_frames_cannot_be_emitted(pop, registers):
    with pytest.raises(ValueError):
        hooks.render_thunks({0x2000: ('Draw', pop, registers)})


def test_original_bindings_and_imports_are_emitted_explicitly():
    replacements = {0x2000: ('Draw', 4, {})}
    assert hooks.keep_originals(replacements, {'Draw', 'Absent'}) == {0x2000: 'sub_00002000_hle_original'}
    text = hooks.render_thunks(replacements, {'Data': 0x5000}, {'Draw': 0x2000, 'Absent': None})
    assert 'const uint32_t hle_var_Data = 0x00005000u;' in text
    assert 'hle_original_Draw)(void) = sub_00002000_hle_original;' in text
    assert 'hle_original_Absent)(void) = 0;' in text


def test_copied_hook_tooling_does_not_import_the_toolkit(tmp_path):
    for name in ('translation_hooks.py', 'x86_decode.py', 'c_source_scan.py'):
        shutil.copyfile(repo_file(ROOT, 'tools', name), tmp_path / name)
    code = '''import sys
sys.path.insert(0,sys.argv[1])
import translation_hooks as hooks
assert hooks.stack_cleanup(bytes.fromhex('c20400'),0x2000)==4
assert hooks.parse_ltcg('Call_0__LTCG_eax1')==('Call',{1:'eax'})
assert 'tools.recomp.hle' not in sys.modules
'''
    result = subprocess.run([sys.executable, '-I', '-c', code, str(tmp_path)],
                            cwd=tmp_path, capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr


def test_generated_thunks_execute_with_production_register_frame_macros(tmp_path):
    path = ROOT / 'external/ts3FPrecomp/templates/runtime/recomp_types.h'
    if not path.is_file():
        pytest.skip('Pinned runtime macro consumer unavailable')
    runtime = path.read_text()
    macros = runtime[runtime.index('#define RECOMP_HLE_STACK'):runtime.index('#define RECOMP_ICALL_SAFE')]
    header = '''#include <stdint.h>
extern uint32_t g_esp,g_eax,g_ecx,memory[256];
#define MEM32(address) memory[(address)/4]
'''
    (tmp_path/'recomp_types.h').write_text(header + macros)
    generated = hooks.render_thunks({0x1000: ('Plain', 8, {}), 0x2000: ('Mixed', 8, {1:'eax',4:'ecx'})},
                                    {'Data': 0x5000}, {'Mixed': 0x2000, 'Missing': None})
    body = '''
#include <assert.h>
uint32_t g_esp,g_eax,g_ecx,memory[256];
void hle_Plain(void) { assert(g_esp==128); assert(MEM32(g_esp+4)==7); }
void hle_Mixed(void) {
    assert(g_esp==224 && MEM32(g_esp)==0xABCDEF01u);
    assert(MEM32(g_esp+4)==11 && MEM32(g_esp+8)==22);
    assert(MEM32(g_esp+12)==33 && MEM32(g_esp+16)==44);
    g_esp=4; g_ecx=999;
}
void sub_00002000_hle_original(void) { g_eax=123; }
int main(void) {
    g_esp=128; MEM32(132)=7; sub_00001000(); assert(g_esp==140);
    g_esp=256; MEM32(256)=0xABCDEF01u; MEM32(260)=22; MEM32(264)=33;
    g_eax=11; g_ecx=44; sub_00002000(); assert(g_esp==268 && g_ecx==999);
    assert(hle_var_Data==0x5000 && hle_original_Missing==0);
    hle_original_Mixed(); assert(g_eax==123);
    return 0;
}
'''
    source, executable = tmp_path/'thunks.c', tmp_path/('thunks.exe' if os.name=='nt' else 'thunks')
    source.write_text(generated + body)
    compiler, environment = native_compiler(tmp_path)
    command = ([compiler, '/nologo', '/std:c11', '/O2', str(source), '/Fe:'+str(executable)]
               if os.name=='nt' else [compiler, '-std=c11', '-O2', str(source), '-o', str(executable)])
    result = subprocess.run(command, cwd=tmp_path, env=environment, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stdout + result.stderr
