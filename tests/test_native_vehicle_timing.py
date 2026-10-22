"""Native vehicle/rigid clock tests; original guest routines require the US XBE."""
import hashlib
import re
import struct
import subprocess

import pytest

from test_recomp_float_joins import ROOT, compile_and_run, config, FunctionTranslator


@pytest.fixture(autouse=True)
def restore_sections():
    previous = dict(vars(config))
    yield
    vars(config).update(previous)


@pytest.mark.parametrize('harness,ranges,mutations', [
    ('native_vehicle_timing.c', [(0x17e680, 0x17f2d2), (0x175150, 0x175399),
                               (0x1515e0, 0x15163b), (0x1521d0, 0x15225d),
                               (0x176530, 0x1766de)],
     ['legacy-gate', 'legacy-linear', 'legacy-rest']),
    ('native_rigid_clock.c', [(0x171fb0, 0x172161)], ['legacy-clock']),
])
def test_original_vehicle_timing(tmp_path, harness, ranges, mutations):
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    path = ROOT / 'private/game/default.xbe'
    if not path.is_file():
        pytest.skip('Supported US XBE required for original vehicle routines')
    image = path.read_bytes()
    assert hashlib.sha256(image).hexdigest() == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    config.configure_from_xbe(str(path))
    functions = {start: dict(name=f'sub_{start:08X}', end=end) for start, end in ranges}
    translator = FunctionTranslator(image, functions, trace_all_entries=harness == 'native_vehicle_timing.c')
    code = '\n'.join(f'void {info["name"]}(void);' for info in functions.values()) + '\n'
    code += '\n'.join(translator.translate_function(start, info) for start, info in functions.items())
    source = (ROOT / 'tests' / harness).read_text()
    calls = set(re.findall(r'RECOMP_DIRECT_CALLEE_AT\([^,]+, (sub_[0-9A-F]+)\)', code))
    defined = set(re.findall(r'void (sub_[0-9A-F]+)\(void\)', source + code))
    unexpected = '\n'.join(f'void {name}(void) {{ fprintf(stderr,"UNEXPECTED: {name}\\n");abort(); }}'
                           for name in sorted(calls - defined))
    addresses = set(range(0x17f2d4, 0x17f2e8, 4)) if harness == 'native_vehicle_timing.c' else set()
    for start, end in ranges:
        offset = config.va_to_file_offset(start)
        for instruction in Cs(CS_ARCH_X86, CS_MODE_32).disasm(image[offset:offset + end - start], start):
            for value in re.findall(r'\[0x([0-9a-f]+)\]', instruction.op_str):
                address = int(value, 16)
                if 0x470000 <= address < 0x490000:
                    addresses.add(address)
    constants = '\n'.join(f'MEM32(0x{a:x})=0x{struct.unpack_from("<I", image, config.va_to_file_offset(a))[0]:08x};'
                          for a in sorted(addresses))
    policy = f'#define RECOMP_NUMERIC_POLICY_HEADER "{(ROOT / "src/pc_native_numeric.h").as_posix()}"'
    source = source.replace('/* POLICY */', policy).replace('/* ROUTINES */', code)
    source = source.replace('/* UNREACHED */', unexpected).replace('/* CONSTANTS */', constants)
    print(compile_and_run(tmp_path, source))
    executable = next(tmp_path.glob('float_join.exe'), tmp_path / 'float_join')
    for mutation in mutations:
        result = subprocess.run([str(executable), mutation], capture_output=True, text=True, timeout=15)
        assert result.returncode != 0 and 'Assertion' in result.stderr, result.stdout + result.stderr
        assert 'UNEXPECTED' not in result.stderr
        assert 'fps=120' in result.stdout, result.stdout + result.stderr


def test_byte_timer_load_and_default_semantics(tmp_path):
    # Authored synthetic x86: MOV AL,[EDX]; DEC AL; MOV [EDX],AL; RET.
    # Exercise the real generic byte-load hook, with no game assets.
    image = bytes.fromhex('8a02fec88802c3')
    config._install([config.Section('.text', 0x176673, len(image), 0, len(image), True)],
                    0x176673, 0, 'synthetic-byte-timer')
    info = dict(name='tick', end=0x176673 + len(image))
    code = FunctionTranslator(image, {0x176673: info}).translate_function(0x176673, info)
    assert code.count('RECOMP_LOAD_U8_AT(') == 1
    source = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#define RECOMP_GENERATED_CODE
/* POLICY */
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
/* ROUTINES */
int main(void) {
    unsigned char *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    for(unsigned value=0;value<256;value++) {
        g_tsfp_native_timing_active=0;MEM8(0x1000)=value;g_edx=0x1000;g_esp=0x2000;
        tick();assert(g_esp==0x2004 && MEM8(0x1000)==(uint8_t)(value-1));
        g_tsfp_native_timing_active=1;
        assert(pc_native_load_u8(0x123,value)==value);
        MEM8(0x1000)=value;g_esp=0x2000;tick();
        assert(g_esp==0x2004 && MEM8(0x1000)==value);
    }
    for(unsigned rate=120;rate<=144;rate+=24) for(unsigned mode=0;mode<3;mode++) {
        unsigned divisor=mode==0?4:1, multiplier=mode==2?3:1;
        unsigned frames=rate*divisor;
        MEM8(0x1000)=60;
        for(unsigned frame=0;frame<frames;frame++) {
            MEMF(0x4e7948)=60.f/rate;MEMF(0x4e794c)=60.f*multiplier/(rate*divisor);
            MEM32(0x4e7940)=(frame+1)*60/rate-frame*60/rate;
            MEM32(0x4e7944)=(frame+1)*60*multiplier/frames-frame*60*multiplier/frames;
            g_edx=0x1000;g_esp=0x2000;tick();
            assert(g_esp==0x2004);
            if(frame+1<frames)assert(MEM8(0x1000)>0);
        }
        assert(MEM8(0x1000)==0);
    }
    /* A phase-shifted slow-game tick remains eligible even with no raw tick. */
    MEMF(0x4e7948)=.5f;MEMF(0x4e794c)=.25f;
    MEM32(0x4e7940)=0;MEM32(0x4e7944)=1;
    assert(pc_native_load_u8(0x176673,5)==5);
    free(memory);puts("PASS: real byte-load hook, all original byte values, native timer cadence and shifted clocks");return 0;
}
'''
    policy = f'#define RECOMP_NUMERIC_POLICY_HEADER "{(ROOT / "src/pc_native_numeric.h").as_posix()}"'
    print(compile_and_run(tmp_path, source.replace('/* POLICY */', policy).replace('/* ROUTINES */', code)))
