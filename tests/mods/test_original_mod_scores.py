"""Local-only original score/ranking/end routines with the real isolated bridge."""
import hashlib
import os
from pathlib import Path
import re
import subprocess
import sys

import pytest
from test_script_process import process_tools

ROOT = Path(__file__).resolve().parents[2]


def test_supported_title_routines_with_original_custom_mode(process_tools, tmp_path):
    xbe = ROOT / 'private/game/default.xbe'
    if not xbe.is_file():
        pytest.skip('Local supported US XBE required; not distributed')
    image = xbe.read_bytes()
    assert hashlib.sha256(image).hexdigest() == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    runtime = ROOT / 'external/ts3FPrecomp'
    sys.path.insert(0, str(runtime))
    from tools.recomp import config
    from tools.recomp.translator import FunctionTranslator
    previous = dict(vars(config))
    try:
        config.configure_from_xbe(str(xbe))
        bounds = {0x640c0: 0x6415c, 0x641c0: 0x64218, 0x64810: 0x64c56, 0x67450: 0x674c0}
        infos = {a: {'end': b, 'name': f'sub_{a:08X}'} for a, b in bounds.items()}
        translator = FunctionTranslator(image, infos)
        routines = '\n'.join(translator.translate_function(a, info) for a, info in infos.items())
        offset = config.va_to_file_offset(0x64218)
        table = image[offset:offset+0x1c]
    finally:
        vars(config).update(previous)
    for name in ('641C0', '64810', '67450'):
        routines = routines.replace(f'void sub_000{name}(void)', f'void sub_000{name}_gen(void)')
    source = (ROOT / 'tests/mods/native_mod_guest.c').read_text()
    source = source.replace('static uintptr_t g_xbox_mem_offset;', '''
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0;
RECOMP_TLS uint32_t g_icall_saved_esp,g_icall_dispatch_form;
recomp_func_t recomp_lookup(uint32_t address){(void)address;abort();return NULL;}
recomp_func_t recomp_lookup_manual(uint32_t address){(void)address;abort();return NULL;}
recomp_func_t recomp_lookup_kernel(uint32_t address){(void)address;abort();return NULL;}
void recomp_icall_fail_log(uint32_t address){(void)address;abort();}
''')
    source = source.replace('static uint32_t g_eax,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_ecx,g_edx;',
                            'RECOMP_TLS uint32_t g_eax,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_ecx,g_edx;')
    source = re.sub(r'static void stock_score\(void\)\{[^\n]*',
                    'void sub_000641C0_gen(void);\nstatic void stock_score(void){++original_scores;sub_000641C0_gen();}\nvoid sub_000641C0(void){pc_mod_game_score(stock_score);}', source)
    source = re.sub(r'static void stock_ranking\(void\)\{[^\n]*',
                    'void sub_00064810_gen(void);\nstatic void stock_ranking(void){++original_scores;sub_00064810_gen();}', source)
    source = re.sub(r'void sub_00067450\(void\)\{[^\n]*', '''
void sub_00067450_gen(void);
void sub_00067450(void){sub_00067450_gen();}
void sub_00066FC0(void){++finished;WORD_AT(0x7de30c)=8;g_esp+=4;}
void sub_003533F0(void){g_eax=0;g_esp+=4;}
void sub_003568C0(void){g_eax=WORD_AT(g_esp+4)<2;g_esp+=4;}
void sub_00355F10(void){g_eax=0;g_esp+=4;}
void sub_00357690(void){abort();}
void sub_00066E90(void){g_esp+=4;}
void sub_000646B0(void){abort();}
void sub_000D6C90(void){abort();}
void sub_00231A20(void){abort();}
''', source)
    source = source.replace('g_esp=0x70000;g_ebx=11;',
                            'static const unsigned char table[]={' + ','.join(map(str, table)) + '};memcpy(memory+0x64218,table,sizeof(table));\n    g_esp=0x70000;g_ebx=11;', 1)
    source = source.replace('TsfpModWorld world;CHECK(pc_mod_game_capture(&world));', '''
    /* First prove the unbound original score calculation and ranking path. */
    WORD_AT(0x10000+0x84)=1;WORD_AT(0x10000+0xea0+0x84)=2;
    MEM16(0x10000+0xa12)=7;MEM16(0x10000+0xea0+0xa10)=2;
    g_esp=0x70000;WORD_AT(g_esp+4)=0;pc_mod_game_score(stock_score);
    CHECK(g_eax==7 && g_esp==0x70004);
    g_esp=0x70000;WORD_AT(g_esp+4)=0;pc_mod_game_ranking(stock_ranking);
    CHECK(g_esp==0x70004 && WORD_AT(0x10000+0xa6c)==7 && WORD_AT(0x10000+0xea0+0xa6c)==2);
    CHECK(WORD_AT(0x10000+0xa68)==1 && WORD_AT(0x10000+0xea0+0xa68)==2);
    CHECK(MEM16(0x10000+0xa12)==7 && MEM16(0x10000+0xea0+0xa10)==2);
    memset(memory+0x10000+0xa10,0,32);memset(memory+0x10000+0xea0+0xa10,0,32);
    TsfpModWorld world;CHECK(pc_mod_game_capture(&world));''')
    source += '\n' + routines + '\n' + (ROOT / 'src/runtime/guest_integer_fault.c').read_text()
    (tmp_path / 'original.c').write_text(source, encoding='utf-8')
    worker = process_tools['worker']
    libdir = worker.parent
    libraries = [libdir / (name+'.lib') if os.name == 'nt' else libdir / ('lib'+name+'.a')
                 for name in ('tsfp_mod_match', 'tsfp_mod_session', 'tsfp_mod_process')]
    cmake = Path(sys.executable).with_name('cmake.exe' if os.name == 'nt' else 'cmake')
    def quoted(path):
        return '"' + str(path).replace('\\', '/') + '"'
    project = '''cmake_minimum_required(VERSION 3.20)
project(original_mod C CXX)
add_executable(original original.c)
set_property(TARGET original PROPERTY LINKER_LANGUAGE CXX)
set_property(TARGET original PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_include_directories(original PRIVATE ''' + quoted(runtime / 'templates/runtime') + ' ' + quoted(ROOT / 'src/mods') + ''')
target_link_libraries(original PRIVATE ''' + ' '.join(map(quoted, libraries)) + ''')
if(WIN32)
 target_link_libraries(original PRIVATE userenv advapi32 psapi ole32)
else()
 target_link_libraries(original PRIVATE pthread m)
endif()
'''
    if os.environ.get('TSFP_MOD_HOST_SANITIZE') == '1':
        project += 'target_compile_options(original PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)\ntarget_link_options(original PRIVATE -fsanitize=address,undefined)\n'
    (tmp_path / 'CMakeLists.txt').write_text(project, encoding='utf-8')
    subprocess.run([str(cmake), '-S', str(tmp_path), '-B', str(tmp_path / 'build'), '-DCMAKE_BUILD_TYPE=Release'], check=True, capture_output=True)
    build = subprocess.run([str(cmake), '--build', str(tmp_path / 'build'), '--config', 'Release'], capture_output=True)
    assert build.returncode == 0, (build.stdout[-8000:].decode(errors='replace'), build.stderr[-4000:].decode(errors='replace'))
    exe = tmp_path / 'build' / ('Release/original.exe' if os.name == 'nt' else 'original')
    cache = tmp_path / 'cache'
    cache.mkdir()
    run = subprocess.run([str(exe), str(worker), str(cache), 'game'],
                         input=(ROOT / 'examples/mods/first-to-three/scripts/main.lua').read_bytes(), capture_output=True, timeout=15)
    assert run.returncode == 0, (run.returncode, run.stdout, run.stderr)
    assert b'PASS' in run.stdout and run.stderr == b'' and not list(cache.iterdir())
