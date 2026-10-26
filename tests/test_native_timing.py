"""Compile and execute the native update model and the actual guest hook."""
import os
from pathlib import Path
import subprocess
import sys

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]

class ObjectRockingQuantization(AssertionError):
    """Measured repeated live transforms; fixture/build errors remain failures."""


@pytest.mark.parametrize('level,descriptors,callback,ranges,addresses', [
    ('venice', (0x50b2a8, 0x50b400, 0x50b558), 0x222b60,
     [(0x228870,0x228b35),(0x222b60,0x222bd2),(0x222740,0x22275c)],
     (0x475c78,0x475d30,0x47d5d4)),
    ('subway', (0x508a78,0x508bd0), 0x21af90,
     [(0x228870,0x228b35),(0x21af90,0x21b063),(0x21af50,0x21af81),
      (0x12380,0x123c9),(0x153bb0,0x153bce),(0x153bd0,0x153bff)],
     (0x475c78,0x475d28,0x47862c,0x4788b8,0x4782c4,0x475cbc)),
])
def test_original_level_ambient_callback(tmp_path, level, descriptors, callback, ranges, addresses):
    import hashlib
    import struct
    from test_recomp_float_joins import compile_and_run, config, FunctionTranslator
    path = ROOT / 'private/game/default.xbe'
    if not path.is_file():
        pytest.skip('Supported US XBE required for original level callback')
    image = path.read_bytes()
    assert hashlib.sha256(image).hexdigest() == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    previous = dict(vars(config))
    try:
        config.configure_from_xbe(str(path))
        # Descriptors route +110 through the callback; +11C names its map.
        for descriptor in descriptors:
            offset = config.va_to_file_offset(descriptor)
            assert struct.unpack_from('<I', image, offset + 0x110)[0] == callback
            name = struct.unpack_from('<I', image, offset + 0x11c)[0]
            at = config.va_to_file_offset(name)
            assert f'{level}.raw'.encode() in image[at:at+30].split(b'\0')[0]
        functions = {start: dict(name=f'sub_{start:08X}', end=end) for start, end in ranges}
        translator = FunctionTranslator(image, functions)
        code = '\n'.join(f'void {f["name"]}(void);' for f in functions.values())
        code += '\n' + '\n'.join(translator.translate_function(a, f) for a, f in functions.items())
        constants = '\n'.join(f'MEM32(0x{a:x})=0x{struct.unpack_from("<I", image, config.va_to_file_offset(a))[0]:08x};'
                              for a in addresses)
        source = (ROOT / f'tests/native_{level}_callback.c').read_text()
        source = source.replace('/* POLICY */', f'#define RECOMP_NUMERIC_POLICY_HEADER "{(ROOT / "src/pc_native_numeric.h").as_posix()}"')
        source = source.replace('/* ROUTINES */', code).replace('/* CONSTANTS */', constants)
        print(compile_and_run(tmp_path, source))
        executable = tmp_path / ('float_join.exe' if os.name == 'nt' else 'float_join')
        broken = subprocess.run([str(executable), 'legacy-gate'], capture_output=True, text=True, timeout=15)
        assert broken.returncode != 0 and 'Assertion' in broken.stderr
        marker = 'venice_callback fps=120' if level == 'venice' else 'subway_callback fps=60 native=1 slow=4'
        assert marker in broken.stdout
    finally:
        vars(config).update(previous)


def test_original_actor_voice_clock(tmp_path):
    import hashlib
    import re
    import struct
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    from test_recomp_float_joins import compile_and_run, config, FunctionTranslator
    path = ROOT/'private/game/default.xbe'
    if not path.is_file():
        pytest.skip('Supported US XBE required for original actor voice clock')
    image = path.read_bytes()
    assert hashlib.sha256(image).hexdigest() == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    previous = dict(vars(config))
    try:
        config.configure_from_xbe(str(path))
        ranges = [(0x1ce2e0,0x1ce404),(0x164e00,0x164f5e),(0x1648d0,0x1648fa),
                  (0x164920,0x16495e),(0x164900,0x16491e),(0x164960,0x1649dc),
                  (0x153b40,0x153b7a),(0x3c9ed0,0x3c9f04),(0x153c00,0x153c20)]
        functions = {a:dict(name=f'sub_{a:08X}',end=e) for a,e in ranges}
        translator = FunctionTranslator(image,functions)
        pieces = {a:translator.translate_function(a,f) for a,f in functions.items()}
        code = '\n'.join(f'void {f["name"]}(void);' for f in functions.values())+'\n'+'\n'.join(pieces.values())
        # Mutations disable only the original generation checks, never production.
        for target in ('00164EC6','00164F52'):
            needle = f'if (CMP_NE(_fa, _fb)) goto loc_{target};'
            assert code.count(needle)==1
            code = code.replace(needle,f'if (!ignore_generation && CMP_NE(_fa, _fb)) goto loc_{target};')
        source = (ROOT/'tests/native_actor_voice_clock.c').read_text()
        defined = set(re.findall(r'void (sub_[0-9A-F]+)\(void\)',source+code))
        parent = pieces[0x1ce2e0]
        parent_calls = set(re.findall(r'RECOMP_DIRECT_CALLEE_AT\([^,]+, (sub_[0-9A-F]+)\)',parent))
        parent_calls.update(re.findall(r'\b(sub_[0-9A-F]+)\(\);',parent))
        calls = set(re.findall(r'RECOMP_DIRECT_CALLEE_AT\([^,]+, (sub_[0-9A-F]+)\)',code))|parent_calls
        services = '\n'.join(f'void {n}(void){{frame_service();}}' if n in parent_calls else
                             f'void {n}(void){{fprintf(stderr,"UNEXPECTED {n}\\n");abort();}}'
                             for n in sorted(calls-defined))
        addresses = set(range(0x1ce404,0x1ce420,4))
        decoder = Cs(CS_ARCH_X86,CS_MODE_32)
        for a,e in ranges:
            offset = config.va_to_file_offset(a)
            for instruction in decoder.disasm(image[offset:offset+e-a],a):
                addresses.update(int(x,16) for x in re.findall(r'\[0x([0-9a-f]+)\]',instruction.op_str)
                                 if 0x470000<=int(x,16)<0x4a0000)
        constants = '\n'.join(f'MEM32(0x{a:x})=0x{struct.unpack_from("<I",image,config.va_to_file_offset(a))[0]:08x};' for a in sorted(addresses))
        source = source.replace('/* POLICY */',f'#define RECOMP_NUMERIC_POLICY_HEADER "{(ROOT/"src/pc_native_numeric.h").as_posix()}"')
        source = source.replace('/* ROUTINES */',code).replace('/* FRAME_SERVICES */',services).replace('/* CONSTANTS */',constants)
        print(compile_and_run(tmp_path,source))
        executable = tmp_path/('float_join.exe' if os.name=='nt' else 'float_join')
        for mutation in ('legacy-gate','ignore-generation'):
            result = subprocess.run([str(executable),mutation],capture_output=True,text=True,timeout=15)
            assert result.returncode!=0 and 'Assertion' in result.stderr
            assert 'UNEXPECTED' not in result.stderr
            marker = 'fabsf(MEMF(0x74c3d0)-.75f)' if mutation=='legacy-gate' else 'primary_events==!(stale&1)'
            assert marker in result.stderr
    finally:
        vars(config).update(previous)


def test_original_scripted_yaw_callback(tmp_path):
    import hashlib
    import re
    import struct
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    from test_recomp_float_joins import compile_and_run, config, FunctionTranslator
    path = ROOT / 'private/game/default.xbe'
    if not path.is_file():
        pytest.skip('Supported US XBE required for original object callback')
    image = path.read_bytes()
    assert hashlib.sha256(image).hexdigest() == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    previous = dict(vars(config))
    try:
        config.configure_from_xbe(str(path))
        at = config.va_to_file_offset(0x225430)
        assert image[at:at+8] == bytes.fromhex('68704122006a0356')
        call = config.va_to_file_offset(0x225438)
        assert image[call] == 0xe8 and 0x22543d+struct.unpack_from('<i',image,call+1)[0] == 0xa5ce0
        ranges = [(0xa0c50,0xa0d24),(0x36e430,0x36e43b),(0x224170,0x224285),
                  (0xa5ce0,0xa5e34),(0xa7670,0xa786a),(0xa7870,0xa79a2),
                  (0xa7360,0xa7666),(0xa14b0,0xa17ab),(0x151650,0x15178f),
                  (0x1518f0,0x151916),(0x151e80,0x151f1c),(0x1531d0,0x1531fb),
                  (0x153240,0x15326d),(0x153270,0x1533e0),(0x153440,0x153490),
                  (0x153600,0x153650),(0x153650,0x1536a0)]
        functions = {a:dict(name=f'sub_{a:08X}',end=e) for a,e in ranges}
        translator = FunctionTranslator(image, functions)
        code = '\n'.join(f'void {f["name"]}(void);' for f in functions.values())+'\n'
        code += '\n'.join(translator.translate_function(a,f) for a,f in functions.items())
        source = (ROOT/'tests/native_scripted_yaw.c').read_text()
        defined = set(re.findall(r'void (sub_[0-9A-F]+)\(void\)',source+code))
        calls = set(re.findall(r'RECOMP_DIRECT_CALLEE_AT\([^,]+, (sub_[0-9A-F]+)\)',code))
        unexpected = '\n'.join(f'void {n}(void){{fprintf(stderr,"UNEXPECTED {n}\\n");abort();}}' for n in sorted(calls-defined))
        addresses = set(range(0xa17ac,0xa17bc,4))
        decoder = Cs(CS_ARCH_X86,CS_MODE_32)
        for a,e in ranges:
            offset = config.va_to_file_offset(a)
            for instruction in decoder.disasm(image[offset:offset+e-a],a):
                addresses.update(int(x,16) for x in re.findall(r'\[0x([0-9a-f]+)\]',instruction.op_str)
                                 if 0x470000<=int(x,16)<0x4a0000)
        constants = '\n'.join(f'MEM32(0x{a:x})=0x{struct.unpack_from("<I",image,config.va_to_file_offset(a))[0]:08x};' for a in sorted(addresses))
        source = source.replace('/* POLICY */',f'#define RECOMP_NUMERIC_POLICY_HEADER "{(ROOT/"src/pc_native_numeric.h").as_posix()}"')
        source = source.replace('/* ROUTINES */',f'#include "{(ROOT/"src/pc_model_transform_guest.inc").as_posix()}"\n'+code)
        source = source.replace('/* UNEXPECTED */',unexpected).replace('/* CONSTANTS */',constants)
        print(compile_and_run(tmp_path,source))
        executable = tmp_path/('float_join.exe' if os.name=='nt' else 'float_join')
        for mutation in ('legacy-gate','missing-dirty'):
            result = subprocess.run([str(executable),mutation],capture_output=True,text=True,timeout=15)
            assert result.returncode != 0 and 'Assertion' in result.stderr
            assert 'UNEXPECTED' not in result.stderr
    finally:
        vars(config).update(previous)


@pytest.mark.parametrize('source,mutation', [
    (name, None) for name in (
        'native_timing.c', 'native_timing_guest.c', 'native_projectile_scope.c',
        'native_visual_time.c', 'native_movie_pacing.c', 'native_movie_update.c', 'native_material_motion.c', 'native_character_trace.c', 'native_walk_trace.c', 'native_walk_policy.c', 'native_surface_interval.c', 'native_surface_mode.c', 'native_surface_native.c', 'native_surface_render_guest.c', 'native_surface_runtime.c', 'native_surface_gpu_owner.c', 'native_surface_gpu_cache.c',
        'native_mouse_cadence.c', 'native_network_cadence.c', 'native_pose_trace.c', 'native_contact_trace.c', 'native_surface_trace.c', 'native_surface_capture.c', 'native_surface_precision.c', 'native_surface_coupled.c', 'native_surface_basis.c')
] + [('native_network_cadence.c', 'render-pump'),
     ('native_network_cadence.c', 'render-refresh'),
     ('native_surface_interval.c', 'reset-phase')])
def test_native_timing(source, mutation, tmp_path):
    compiler, environment = native_compiler(tmp_path)
    runtime = ROOT / 'external/ts3FPrecomp/src'
    includes = [runtime, runtime / 'config', runtime / 'd3d', ROOT / 'src']
    output = tmp_path / ('native-timing.exe' if os.name == 'nt' else 'native-timing')
    source_path = ROOT / 'tests' / source
    if mutation == 'reset-phase':
        adapter = (ROOT / 'src/graphics/surface_interval.h').read_text(encoding='utf-8')
        needle = 'PcSurfaceInterval result={0};'
        assert adapter.count(needle) == 1
        (tmp_path / 'graphics').mkdir()
        (tmp_path / 'graphics/surface_interval.h').write_text(adapter.replace(needle, needle+' phase=0;'))
        source_path = tmp_path / source
        source_path.write_text((ROOT / 'tests' / source).read_text())
    elif mutation:
        # Reject changing network clock units to rendered-frame counts. Keep
        # production files intact; mutate only this disposable translation unit.
        adapter = (ROOT / 'src/pc_network_guest.inc').read_text(encoding='utf-8')
        address = '0x007497A8u' if mutation == 'render-pump' else '0x007DE318u'
        assert adapter.count(address) == 1
        adapter_path = tmp_path / 'network_mutation.inc'
        adapter_path.write_text(adapter.replace(address, '0x007497B8u'), encoding='utf-8')
        text = source_path.read_text(encoding='utf-8')
        for name, path in (
            ('native_timing_guest.c', ROOT / 'tests/native_timing_guest.c'),
            ('../src/pc_network.h', ROOT / 'src/pc_network.h'),
            ('../src/pc_network_guest.inc', adapter_path)):
            text = text.replace('"' + name + '"', '"' + path.as_posix() + '"')
        source_path = tmp_path / 'native_network_cadence.c'
        source_path.write_text(text, encoding='utf-8')
    if source == 'native_movie_update.c':
        player=(runtime/'hle/hle_xmv_play.c').read_text(encoding='utf-8')
        start=player.index('typedef struct {')
        structure=player[start:player.index('} movie;',start)+len('} movie;')]
        names=('static movie *get(', 'static void feed_audio(', 'static uint32_t movie_clock(',
               'void xmv_play_start(', 'uint32_t xmv_play_update(', 'uint32_t xmv_play_time(',
               'void xmv_play_close(')
        functions=[]
        for name in names:
            start=player.index(name)
            functions.append(player[start:player.index('\n}',start)+2])
        text=source_path.read_text(encoding='utf-8')
        text=text.replace('/* MOVIE_STRUCT */',structure).replace('/* MOVIE_FUNCTIONS */','\n'.join(functions))
        text=text.replace('../src/pc_native_movie_pacing.h',(ROOT/'src/pc_native_movie_pacing.h').as_posix())
        source_path=tmp_path/'native_movie_update.c'
        source_path.write_text(text,encoding='utf-8')
    if os.name == 'nt':
        command = [compiler, '/nologo', '/std:c11', '/W4',
                   *['/I' + str(path) for path in includes], str(source_path),
                   '/Fe:' + str(output), '/Fo:' + str(tmp_path / 'timing.obj')]
    else:
        command = [compiler, '-std=c11', '-Wall', '-Wextra',
                   *['-I' + str(path) for path in includes], str(source_path),
                   '-lm', '-o', str(output)]
    subprocess.run(command, env=environment, cwd=tmp_path, check=True)
    run_environment = {k:v for k,v in os.environ.items()
                       if k not in ('TSFP_NATIVE_TRACE', 'TSFP_NATIVE_POSE_TRACE', 'TSFP_NATIVE_LOCAL_TRACE', 'TSFP_SURFACE_TRACE',
                                    'TSFP_CONTACT_TRACE', 'TSFP_CONTACT_TRACE_SLOT', 'TSFP_CONTACT_TRACE_Z',
                                    'TSFP_CHARACTER_TRACE', 'TSFP_WALK_TRACE',
                                    'TSFP_WALK_TRACE_SCENE', 'TSFP_WALK_TRACE_TICKS')}
    if source == 'native_contact_trace.c':
        import csv
        subprocess.run([str(output), 'disabled'], env=run_environment, check=True, timeout=10)
        for mode in ('bounded', 'bytes', 'invalid'):
            path=tmp_path / (mode+'.csv')
            env=dict(run_environment, TSFP_CONTACT_TRACE=str(path), TSFP_CONTACT_TRACE_SLOT='2',
                     TSFP_CONTACT_TRACE_Z='60,80' if mode!='invalid' else 'nan,80')
            subprocess.run([str(output), mode], env=env, check=True, timeout=10)
            if mode=='invalid':
                assert not path.exists()
                continue
            rows=list(csv.DictReader(path.open()))
            assert len(rows)==(4 if mode=='bytes' else 16000)
            assert path.stat().st_size<=8*1024*1024
            assert [r['source'] for r in rows[:4]]==['summary','generated','generated','selected']
            assert all(None not in row and row['raw_count']=='2' and row['selected_count']=='1' for row in rows)
        # Opening failure is sticky and must leave gameplay unaffected.
        env=dict(run_environment, TSFP_CONTACT_TRACE=str(tmp_path/'absent'/'trace.csv'),
                 TSFP_CONTACT_TRACE_SLOT='2', TSFP_CONTACT_TRACE_Z='60,80')
        subprocess.run([str(output), 'invalid'], env=env, check=True, timeout=10)
        return
    if source == 'native_pose_trace.c':
        run_environment['TSFP_NATIVE_POSE_TRACE'] = str(tmp_path / 'poses.csv')
    if source == 'native_surface_precision.c':
        subprocess.run([str(output), 'disabled'], env=run_environment, check=True, timeout=10)
    if source == 'native_surface_trace.c':
        import csv
        subprocess.run([str(output), 'disabled'], env=run_environment, check=True, timeout=10)
        for mode in ('bounded', 'failure', 'switches'):
            path=tmp_path / (mode+'.csv')
            trace_environment=dict(run_environment, TSFP_SURFACE_TRACE=str(path), TSFP_SURFACE_TRACE_SWITCHES='1' if mode=='switches' else '0')
            subprocess.run([str(output), mode], env=trace_environment, check=True, timeout=10)
            rows=list(csv.DictReader(path.open()))
            assert len(rows)==(2 if mode=='failure' else 360)
            assert Path(str(path)+'.rgba').stat().st_size==len(rows)*128*128*4
            if mode=='switches':
                sys.path.insert(0,str(ROOT/'tools'))
                from procedural_surface_stats import validate_switch_cycle
                validate_switch_cycle(rows)
                continue
            assert rows[0]['executed']=='1' and rows[1]['executed']=='0'
            assert rows[0]['hash']==rows[1]['hash']
        return
    result = subprocess.run([str(output)], env=run_environment, check=not mutation,
                            capture_output=bool(mutation), text=bool(mutation), timeout=10)
    if mutation:
        assert result.returncode != 0
        expected = ('p->begins_tick' if mutation == 'reset-phase' else
                    'sends==120' if mutation == 'render-pump' else 'refreshes[port]==40')
        assert expected in result.stderr, result.stdout + result.stderr


@pytest.mark.parametrize('encoding,helper', [
    ('f30f2a0544794e00', 'RECOMP_I32_TO_F32_AT'),
    ('f20f2a0544794e00', 'RECOMP_I32_TO_F64'),
    ('db0544794e00', 'RECOMP_I32_TO_F64'),
])
def test_generated_numeric_conversion(encoding, helper):
    sys.path.insert(0, str(ROOT / 'external/ts3FPrecomp'))
    from tools.recomp.disasm import Disassembler
    from tools.recomp.lifter import Lifter
    raw = bytes.fromhex(encoding)
    instructions = Disassembler().disassemble_function(raw, 0x1000, 0x1000 + len(raw))
    code = '\n'.join(Lifter().lift_instruction(instructions[0]))
    assert helper + ('(0x1000, 0x4E7944,' if helper.endswith('_AT') else '(0x4E7944,') in code
    assert 'MEM32(0x4E7944)' in code


@pytest.mark.parametrize('encoding,observed', [
    ('a318e37d00', True),             # mov [absolute],eax
    ('c70518e37d0000000000', True),   # mov [absolute],immediate
    ('8903', False),                 # mov [ebx],eax: not a direct global
    ('a218e37d00', False),           # mov [absolute],al: not 32-bit
    ('64a300000000', False),         # mov fs:[0],eax: not title memory
])
def test_global_store_observation_scope(encoding, observed):
    sys.path.insert(0, str(ROOT / 'external/ts3FPrecomp'))
    from tools.recomp.disasm import Disassembler
    from tools.recomp.lifter import Lifter
    raw=bytes.fromhex(encoding)
    instruction=Disassembler().disassemble_function(raw,0x1000,0x1000+len(raw))[0]
    code='\n'.join(Lifter().lift_instruction(instruction))
    assert ('RECOMP_STORE_GLOBAL_U32(0x1000, 0x7DE318,' in code)==observed


@pytest.mark.parametrize('encoding,observed', [
    ('833d44794e0001', True),  # cmp dword [absolute],1
    ('3b0544794e00', True),    # cmp eax,[absolute]
    ('833b01', False),         # cmp dword [ebx],1
    ('803d44794e0001', False), # byte
    ('66833d44794e0001', False), # word
    ('64833d0000000001', False), # fs segment
    ('83f801', False),         # register
    ('f70544794e0001000000', False), # TEST is not CMP
])
def test_global_compare_load_scope(encoding, observed):
    sys.path.insert(0, str(ROOT / 'external/ts3FPrecomp'))
    from tools.recomp.disasm import Disassembler
    from tools.recomp.lifter import Lifter
    raw=bytes.fromhex(encoding)
    instruction=Disassembler().disassemble_function(raw,0x1000,0x1000+len(raw))[0]
    code='\n'.join(Lifter().lift_instruction(instruction))
    # Count executable expressions only, not the generated explanatory comment.
    expressions='\n'.join(line.split('/*')[0] for line in code.splitlines())
    assert expressions.count('RECOMP_LOAD_GLOBAL_U32(')==int(observed)


@pytest.mark.parametrize('separated', [False, True])
@pytest.mark.parametrize('mutation', [None, 'whole-tick-policy', 'unhooked-compare'])
def test_reload_compare_gate(tmp_path, separated, mutation):
    """Asset-free generated CMP/JL plus the production policy, also in CI."""
    from test_recomp_float_joins import compile_and_run
    from tools.recomp import config
    from tools.recomp.translator import FunctionTranslator
    previous=dict(vars(config))
    base=0x1c2f09
    image=bytes.fromhex('833d44794e0001'+('90' if separated else '')+'7c06b801000000c331c0c3')
    try:
        config._install([config.Section('.text',base,len(image),0,len(image),True)],base,base,'reload-gate-test')
        info={'end':base+len(image),'name':'reload_gate'}
        code=FunctionTranslator(image,{base:info}).translate_function(base,info)
    finally:
        vars(config).update(previous)
    policy=(ROOT/'src/pc_native_numeric.h').read_text(encoding='utf-8')
    policy=policy.replace('"pc_gpu_ack.h"','"'+(ROOT/'src/pc_gpu_ack.h').as_posix()+'"')
    policy=policy.replace('"pc_native_material.h"','"'+(ROOT/'src/pc_native_material.h').as_posix()+'"')
    policy=policy.replace('"pc_native_statistics_guest.h"','"'+(ROOT/'src/pc_native_statistics_guest.h').as_posix()+'"')
    policy=policy.replace('"pc_native_trap_guest.h"','"'+(ROOT/'src/pc_native_trap_guest.h').as_posix()+'"')
    policy=policy.replace('"pc_native_wind_clock.h"','"'+(ROOT/'src/pc_native_wind_clock.h').as_posix()+'"')
    if mutation=='whole-tick-policy':
        assert policy.count('site == 0x1C2F09u')==1
        policy=policy.replace('site == 0x1C2F09u','site == 0xFFFFFFFFu')
    if mutation=='unhooked-compare':
        assert 'RECOMP_LOAD_GLOBAL_U32(0x1C2F09, 0x4E7944, MEM32(0x4E7944))' in code
        code=code.replace('RECOMP_LOAD_GLOBAL_U32(0x1C2F09, 0x4E7944, MEM32(0x4E7944))','MEM32(0x4E7944)')
    (tmp_path/'numeric.h').write_text(policy,encoding='utf-8')
    harness=(ROOT/'tests/native_reload_gate.c').read_text(encoding='utf-8')
    harness=harness.replace('/* NUMERIC_POLICY */',f'#define RECOMP_NUMERIC_POLICY_HEADER "{(tmp_path / "numeric.h").as_posix()}"')
    source=(harness.replace('/* GENERATED_GATE */',code)
            .replace('/* TIMING_HEADER */',f'#include "{(ROOT / "src/pc_native_timing.h").as_posix()}"')
            .replace('/* PHASE_SWEEP */',(ROOT/'tests/reload_phase_sweep.h').read_text()))
    if mutation:
        with pytest.raises(AssertionError,match='reload phase mismatch'):
            compile_and_run(tmp_path,source)
    else:
        output=compile_and_run(tmp_path,source)
        assert 'reload_sweep trials=864 presses=1728' in output,output


def test_native_update_statistics():
    sys.path.insert(0,str(ROOT/'tools'))
    from native_update_stats import summarize
    rows=[dict(seconds=i/144,scene=2,flags=0,game_delta=60/144,raw_delta=60/144)
          for i in range(145)]
    result=summarize(rows,0)
    assert result['mean_update_fps']==144
    arcade=[dict(row,scene=48) for row in rows]
    with pytest.raises(ValueError): summarize(arcade,0)
    arcade_result=summarize(arcade,0,48)
    assert arcade_result['mean_update_fps']==144 and 'Arcade' in arcade_result['scope']
    assert result['game_seconds_per_wall_second']==1
    assert result['raw_seconds_per_wall_second']==1
    assert result['frame_ms']['p99']==6.9444
    slow=[dict(row,game_delta=row['game_delta']/4) for row in rows]
    assert summarize(slow,0)['game_seconds_per_wall_second']==.25
    assert summarize(slow,0)['raw_seconds_per_wall_second']==1
    rows[72]['game_delta']=0
    assert summarize(rows,0)['intervals']==142
    rows.append(dict(seconds='1.1',scene=None,flags=None,game_delta=None,raw_delta=None))
    assert summarize(rows,0)['intervals']==142


def test_gameplay_checks_reject_frozen_opening():
    from native_refresh_game import gameplay_checks
    frozen=[dict(seconds=t,position=[1,2,3],
                 hand=dict(clip=836,time=[1.14885,1.14885])) for t in (86,90,100,110)]
    assert not any(gameplay_checks(frozen).values())
    assert not any(gameplay_checks([]).values())
    moving=[dict(seconds=t,position=[t-86,2,3],equipped=[0xffffffff,14],
                 ammo=[dict(type=1,reserve=100,loaded=120-t)],
                 hand=dict(clip=clip,time=[.2,.19]))
            for t,clip in [(86,832),(90,834),(100,838),(110,836)]]
    assert all(gameplay_checks(moving).values())
    for sample in moving:sample['equipped']=[0xffffffff,0]
    assert not gameplay_checks(moving)['opening_firearm_equipped']
    # A changing camera/actor does not establish advancing hand animation.
    for sample in moving:sample['hand']=None
    assert gameplay_checks(moving)==dict(replayed_movement=True,
        hand_animation_advanced=False,hand_animation_changed=False,
        opening_firearm_equipped=False,ammunition_consumed=True)
    for sample in moving:
        sample['ammo']=[dict(type=1,reserve=110-sample['seconds'],
                            loaded=sample['seconds'])]
    assert not gameplay_checks(moving)['ammunition_consumed']  # reload only


def test_opening_handoff_rejects_unarmed_natural_intro():
    from native_refresh_game import opening_firearm_check
    assert opening_firearm_check([])=={}
    samples=[dict(seconds=157.6,equipped=[0xffffffff,0xffffffff]),
             dict(seconds=180.6,equipped=[0xffffffff,37])]
    assert opening_firearm_check(samples)=={}
    samples.append(dict(seconds=299.7,equipped=[0xffffffff,37]))
    assert opening_firearm_check(samples)==dict(opening_firearm_equipped_at_end=False)
    samples[-1]['equipped'][1]=14
    assert opening_firearm_check(samples)==dict(opening_firearm_equipped_at_end=True)
    from native_refresh_game import gameplay_checks
    late=[dict(seconds=t,position=[t,0,0],equipped=[0xffffffff,weapon],
               hand=dict(clip=t,time=[.2,.1]))
          for t,weapon in [(50,37),(86,37),(95,14),(115,14)]]
    checks=gameplay_checks(late)
    checks.update(opening_firearm_check(late))
    assert checks['opening_firearm_equipped_at_end']
    assert not checks['opening_firearm_equipped']
    assert not all(checks.values())


def test_live_observer_waits_for_guest_mapping_with_bounded_deadline():
    from native_refresh_game import guest_memory_ready
    memory = {}
    def read(address, size):
        assert address == 0x79094c and size == 4
        return memory.get(address)
    assert not guest_memory_ready(read, 0)
    assert not guest_memory_ready(read, 4.99)
    memory[0x79094c] = b'\0\0'  # A partial read is not ready either.
    assert not guest_memory_ready(read, 1)
    with pytest.raises(RuntimeError, match='startup deadline'):
        guest_memory_ready(read, 5)
    memory[0x79094c] = b'\0' * 4  # Mapped menu state, before Story.
    assert guest_memory_ready(read, .05)
    assert guest_memory_ready(read, 30)
    del memory[0x79094c]
    with pytest.raises(RuntimeError, match='startup deadline'):
        guest_memory_ready(read, 30)


def test_native_present_kinds_and_errors():
    from native_refresh_game import native_present_checks
    assert not any(native_present_checks('').values())
    valid='[PRESENT] backend=d3d11 total=3000 guest=3000 interpolated=0 other=0 skipped=0 failed=0'
    assert all(native_present_checks(valid).values())
    mixed=valid.replace('guest=3000','guest=2999').replace('interpolated=0','interpolated=1')
    assert not native_present_checks(valid+'\n'+mixed)['interpolation_disabled']
    assert not native_present_checks(valid.replace('failed=0','failed=1'))['present_failures_absent']
    assert not native_present_checks(valid.replace('3000','0'))['guest_presents_observed']


def test_capture_cleanup_preserves_other_evidence(tmp_path):
    from native_refresh_game import recycle_captures
    for i in range(24):
        (tmp_path/f'native-144-frame{i:03}.bmp').write_bytes(b'frame')
    other=tmp_path/'reference-60-frame000.bmp'
    other.write_bytes(b'reference')
    recycle_captures(tmp_path,'native-144')
    assert [p.name for p in sorted(tmp_path.glob('native-144-*'))]==[
        f'native-144-frame{i:03}.bmp' for i in (21,22,23)]
    assert other.read_bytes()==b'reference'
    for i in (720,1440,2160):
        (tmp_path/f'native-144-frame_s{i:08}.bmp').write_bytes(b'rolling')
    recycle_captures(tmp_path,'native-144')
    assert [p.name for p in sorted(tmp_path.glob('native-144-*'))]==[
        f'native-144-frame_s{i:08}.bmp' for i in (720,1440,2160)]
    with pytest.raises(ValueError):
        recycle_captures(tmp_path,'../reference-60')
    target=tmp_path/'outside.bmp'
    target.write_bytes(b'keep')
    link=tmp_path/'native-144-frame000.bmp'
    try:
        link.symlink_to(target)
    except OSError:
        return  # Link creation may require developer mode on Windows.
    with pytest.raises(ValueError):
        recycle_captures(tmp_path,'native-144')
    assert target.read_bytes()==b'keep'


@pytest.mark.parametrize('harness_name,ranges', [
    ('native_analog_input.c', [(0x18ff70,0x190334),(0x426e0,0x4270d)]),
    ('native_locomotion.c', [(0x258f50,0x2597ff)]),
    ('native_walk_contact.c', [(0xc54a0,0xc5a4a)]),
    ('native_network_correction.c', [(0x356e20,0x3570c7)]),
    ('native_surface_fractional.c', [(0x1950b0,0x195110),(0x195110,0x195277),(0x1956a0,0x19576d)]),
    ('native_character_scale.c', [(0x125fe0,0x12609c),(0x120ab0,0x120c0f),(0x153440,0x153490),
                                  (0x153600,0x153650),(0x153650,0x1536a0),(0x153270,0x1533e0)]),
    ('native_character_effects.c', [(0x1260c0,0x126236),(0x11fec0,0x120102),
                                    (0x151650,0x15178f),(0x151e80,0x151f1c)]),
    ('native_surface_cache.c', [(0x127400,0x127494),(0x127500,0x12782c),
                               (0x194b00,0x194bb0),(0x1956a0,0x19576d)]),
    ('native_visual_time_guest.c', [(0x43595,0x4359c),(0x670e8,0x670ef),
                                    (0x688bb,0x688c1),(0x68f8e,0x68f94),
                                    (0x691f3,0x691f9),(0x69239,0x6923f),
                                    (0x69265,0x6926b),(0x69325,0x6932c),
                                    (0x357939,0x35793f)]),
    ('native_player_force.c', [(0xd4820,0xd4aff),(0x225c70,0x225dee),
                               (0xd2ce0,0xd2d16),(0xd4100,0xd4609)]),
    ('native_player_fall.c', [(0xd4100,0xd4609),(0xd4610,0xd481d)]),
    ('native_aim_easing.c', [(0x25ab82,0x25acdf),(0x25b0f3,0x25b182)]),
    ('native_animation_clock.c', [(0x5adc0,0x5afab),(0x61570,0x6160f),
                                  (0x585e0,0x5861b),(0x59cf0,0x59d14)]),
    ('native_weapon_sway.c', [(0x1c4400,0x1c4a4b),(0x153c20,0x153c57),(0x12010,0x12017),
                              (0x1c3659,0x1c3681),(0x1c37cf,0x1c37ea)]),
    ('native_strip_distortion.c', [(0xf9b10,0xf9caf),(0x153c20,0x153c57),(0x12010,0x12017)]),
    ('native_screen_distortion.c', [(0xf9700,0xf9b0b),(0xf8aa0,0xf8ac4),
                                    (0x153c20,0x153c57),(0x12010,0x12017)]),
    ('native_light_preview.c', [(0x116130,0x1163f5)]),
    ('native_script_events.c', [(0x228870,0x228b35),(0x218910,0x218aa6),
                               (0x61570,0x6160f),(0x585e0,0x5861b),(0x59cf0,0x59d14)]),
    ('native_npc_dispatch.c', [(0x15eae0,0x15f0ca)]),
    ('native_player_dispatch.c', [(0xd5eb0,0xd683d),
                                  (0xb9510,0xb9711),(0xb9270,0xb92f9),
                                  (0x259344,0x259389)]),
    ('native_hud_messages.c', [(0x14a5a0,0x14aad6),(0x12df00,0x12df5d),(0x12dec0,0x12def8),(0x12d1e0,0x12d29c)]),
    ('native_escort_recovery.c', [(0x68830,0x69587),(0x681c0,0x685e3),(0x654e0,0x65588)]),
    ('native_arcade_countdown.c', [(0x68830,0x69587),(0x42830,0x4286a)]),
    ('native_camera_motion.c', [(0xc73c0,0xc760e),(0xc7610,0xc77bc)]),
    ('native_camera_effects.c', [(0x151e80,0x151f1c),(0x1533e0,0x153431),
                               (0x153440,0x153490),(0xc79c0,0xc8114),
                               (0xcea8d,0xceaa6)]),
    ('native_camera_track.c', [(0xcd691,0xcdd3e)]),
    ('native_menu_motion.c', [(0x2dab30,0x2dab77),(0x2daf90,0x2db005),
                            (0x2da0e0,0x2da440)]),
    ('native_entity_velocity.c', [(0xa7360,0xa7666),(0xa7870,0xa79a2)]),
    ('native_tree_motion.c', [(0x1d88b0,0x1d89d9)]),
    ('native_trap_motion.c', [(0x12010,0x12017),(0x153c20,0x153c57),(0x1d8ab0,0x1d8c40)]),
    ('native_trap_transform.c', [(0x12010,0x12017),(0x153c20,0x153c57),
        (0x1d8ab0,0x1d8c40),(0x1d8c40,0x1d8c8b),
        (0xa7360,0xa7666),(0xa14b0,0xa17ab),
        (0x151650,0x15178f),(0x1518f0,0x151916),(0x151e80,0x151f1c),
        (0x1531d0,0x1531fb),(0x153240,0x15326d),(0x153270,0x1533e0),
        (0x153440,0x153490),(0x153600,0x153650),(0x153650,0x1536a0)]),
    ('native_player_statistics.c', [(0xd1fd0,0xd2188),(0x150200,0x150226),
                                   (0x2382b0,0x23833b),(0x238510,0x23854c)]),
    ('native_weapon_sound.c', [(0x45390,0x453d0),(0x21780,0x217a2),
                              (0x21fb0,0x2201f),(0x1b6ff0,0x1b7258)]),
    ('native_reload_input.c', [(0x18ff70,0x190334),(0x426e0,0x4270d),(0x1c2e80,0x1c30f3),(0x42760,0x427ac),(0x427b0,0x427e8)]),
    ('native_weapon_cooldown.c', [(0x1c320c,0x1c3274)]),
    ('native_weapon_fire.c', [(0x45390,0x453d0),(0x21780,0x217a2),
                             (0x21fb0,0x2201f),(0x1b6ff0,0x1b7258),
                             (0x1b5660,0x1b567b),(0x1bc890,0x1bc988),
                             (0x244f30,0x24598e)]),
    ('native_npc_weapon_fire.c', [(0x45390,0x453d0),(0x21780,0x217a2),
                                 (0x21fb0,0x2201f),(0x1b6ff0,0x1b7258),
                                 (0x1b5660,0x1b567b),(0x1bb5c0,0x1bc64f),
                                 (0x174e0,0x17510),(0x17510,0x17531),
                                 (0x1b6790,0x1b67d5),(0x1b66e0,0x1b673f),
                                 (0x1b8530,0x1b8688),(0x11f780,0x11f7df),
                                 (0x45cf0,0x45d5e),(0xa5a60,0xa5aa9),
                                 (0x238510,0x23854c),(0x2382b0,0x23833b),
                                 (0x3533f0,0x3533f6),(0x358560,0x358590),
                                 (0x35de60,0x35de66),(0x61570,0x6160f),
                                 (0x2b9bce,0x2b9c0d),(0x27ae14,0x27ae84)]),
    ('native_object_bounce.c', [(0xa84f0,0xa8a97),
                              (0x151650,0x15178f),(0x153550,0x1535a1),
                              (0x153650,0x1536a0),(0x153270,0x1533e0),
                              (0x45390,0x453d0),(0x21910,0x21967)]),
    ('native_mothership.c', [(0x217050,0x21862f),(0x2157b0,0x215982),
                           (0x1515e0,0x15163b),(0x151e80,0x151f1c),
                           (0x152090,0x15211d)]),
    ('native_sniper_damage.c', [(0xe36d0,0xe3725),(0xf5d30,0xf5fec),(0xf5ff0,0xf61d1),
                              (0xb91f0,0xb921b),(0xb9220,0xb9264),(0xbd0f0,0xbd18d),
                              (0xb9790,0xb991d),(0xb9720,0xb9789),
                              (0xbb6c0,0xbb842),(0xbb490,0xbb6bf),(0xba310,0xba6f7)]),
    ('native_projectile_motion.c', [(0xf4a50,0xf51af),(0xdcef0,0xdcf85),
                                    (0xb9920,0xb995a),(0xb9720,0xb9789)]),
    ('native_projectile_coupled.c', [(0xf08d0,0xf1a4b),
                 (0xe78f0,0xe799a),(0xe1fa0,0xe2370),(0xe2370,0xe2514),
                 (0xdcef0,0xdcf85),(0xb9920,0xb995a),(0xb9720,0xb9789)]),
    ('native_npc_aim.c', [(0x2b93d5,0x2b9793),(0x2b9381,0x2b938a),
                                     (0x2b9389,0x2b938a),(0x2b938d,0x2b938e),
                                     (0x123d0,0x123f3),(0x124f0,0x12533),
                                     (0x126c0,0x12717)]),
    ('native_beam_lifetime.c', [(0x10a760,0x10ad71)]),
    ('native_beam_geometry.c', [(0x104f30,0x1053ca),(0x1053d0,0x105615),
                                (0x141a0,0x14532),(0x10a760,0x10ad71),
                                (0x10bf5c,0x10bfca)]),
    ('native_ship_debris.c', [(0x2168c0,0x216ca5)]),
    ('native_object_rocking.c', [(0xa84f0,0xa8a97),(0xa8aa0,0xa8c37),
                                            (0x151650,0x15178f),(0x153550,0x1535a1),
                                            (0x153650,0x1536a0),(0x153270,0x1533e0)]),
])
def test_original_fractional_dispatch(tmp_path, harness_name, ranges):
    import hashlib
    from test_recomp_float_joins import compile_and_run
    from tools.recomp import config
    from tools.recomp.translator import FunctionTranslator
    xbe=ROOT/'private/game/default.xbe'
    if not xbe.is_file():
        pytest.skip('Supported US XBE required for script-event integration')
    image=xbe.read_bytes()
    assert hashlib.sha256(image).hexdigest()=='3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    previous=dict(vars(config))
    constants=''
    try:
        config.configure_from_xbe(str(xbe))
        if harness_name in ('native_analog_input.c','native_reload_input.c'):
            import struct
            addresses=[0x475d24,0x4783a8,*range(0x47df00,0x47df80,4)]
            constants='\n'.join(f'MEM32(0x{address:x})=0x{struct.unpack_from("<I",image,config.va_to_file_offset(address))[0]:08x};'
                                for address in addresses)
        if harness_name=='native_network_correction.c':
            import struct
            constants='\n'.join(f'MEM32(0x{address:x})=0x{struct.unpack_from("<I",image,config.va_to_file_offset(address))[0]:08x};'
                                for address in (0x4785e4,0x4782c4))
        if harness_name=='native_hud_messages.c':
            import struct
            constants='\n'.join(f'MEM32(0x{address:x})=0x{struct.unpack_from("<I",image,config.va_to_file_offset(address))[0]:08x};'
                                for address in list(range(0x4e72e0,0x4e7348,4)))
        if harness_name in ('native_arcade_countdown.c','native_escort_recovery.c'):
            import struct
            constants='\n'.join(f'MEM32(0x{address:x})=0x{struct.unpack_from("<I",image,config.va_to_file_offset(address))[0]:08x};'
                                for address in [*range(0x69588,0x695b8,4),
                                    *(list(range(0x685e4,0x6860c,4))+[0x475c78,0x478864]
                                      if harness_name=='native_escort_recovery.c' else [])])
        if harness_name=='native_visual_time_guest.c':
            # Execute each real direct clock store. Append RET at the next
            # instruction; this tests store dispatch/ABI, not enclosing paths.
            expected={0x43595:'891518e37d00',0x670e8:'891d18e37d00',
                      0x688bb:'a318e37d00',0x68f8e:'a318e37d00',
                      0x691f3:'a318e37d00',0x69239:'a318e37d00',
                      0x69265:'a318e37d00',0x69325:'893518e37d00',
                      0x357939:'a318e37d00'}
            for start,end in ranges:
                a,b=config.va_to_file_offset(start),config.va_to_file_offset(end-1)
                assert image[a:b].hex()==expected[start]
                image=image[:b]+b'\xc3'+image[b+1:]
        if harness_name in ('native_walk_contact.c','native_locomotion.c','native_player_force.c','native_player_fall.c','native_aim_easing.c','native_animation_clock.c','native_weapon_sway.c','native_player_dispatch.c','native_surface_cache.c','native_strip_distortion.c','native_screen_distortion.c','native_character_effects.c','native_character_scale.c','native_light_preview.c','native_mothership.c','native_ship_debris.c','native_projectile_motion.c','native_projectile_coupled.c','native_sniper_damage.c','native_beam_lifetime.c','native_beam_geometry.c','native_npc_aim.c'):
            import re, struct
            # Original scalar constants referenced by the complete routines.
            # Include switch-table data; production generation reads this XBE too.
            from capstone import Cs, CS_ARCH_X86, CS_MODE_32
            addresses=set(range(0x1163f8,0x116408,4) if harness_name=='native_light_preview.c'
                          else range(0x218630,0x218648,4) if harness_name=='native_mothership.c'
                          else range(0x507d88,0x507db8,4) if harness_name=='native_ship_debris.c'
                          else range(0xf51b0,0xf51e8,4) if harness_name=='native_projectile_motion.c'
                          else range(0xf1a4c,0xf1a90,4) if harness_name=='native_projectile_coupled.c'
                          else [0x478674]+list(range(0x10ad74,0x10ade8,4))+list(range(0x4e11bc,0x4e143c,4))
                          if harness_name in ('native_beam_lifetime.c','native_beam_geometry.c')
                          else [] if harness_name in ('native_walk_contact.c','native_locomotion.c','native_player_force.c','native_player_fall.c','native_aim_easing.c','native_animation_clock.c','native_weapon_sway.c','native_player_dispatch.c','native_npc_aim.c','native_strip_distortion.c','native_screen_distortion.c','native_surface_cache.c')
                          else range(0x4dad48,0x4dafb8,4))
            if harness_name=='native_character_scale.c':
                addresses.update(range(0x12609c,0x1260b4,4))
            if harness_name in ('native_projectile_motion.c','native_projectile_coupled.c'):
                # Captured projectile kinds and damage profiles. These are
                # original table entries, not made-up falloff coefficients.
                addresses.update(0x4deb54+kind*64 for kind in (24,28,31,35,45,46))
                for profile in (2,8,9,11,12):
                    addresses.update((0x4dad4c+profile*60,0x4dad78+profile*60))
            if harness_name=='native_player_dispatch.c':
                addresses.update(range(0x4dad48,0x4dafb8,4))
            decoder=Cs(CS_ARCH_X86,CS_MODE_32)
            for start,end in ranges:
                offset=config.va_to_file_offset(start)
                for instruction in decoder.disasm(image[offset:offset+end-start],start):
                    for literal in re.findall(r'0x[0-9a-f]+',instruction.op_str):
                        address=int(literal,16)
                        if 0x470000<=address<0x4e0000: addresses.add(address)
            constants='\n'.join(
                f'MEM32(0x{address:x})=0x{struct.unpack_from("<I",image,config.va_to_file_offset(address))[0]:08x};'
                for address in sorted(addresses))
        if harness_name=='native_projectile_coupled.c':
            # Original CRT pow adapter marshals ST(1) base and ST(0) exponent.
            offset=config.va_to_file_offset(0x3c8c64)
            expected=bytes.fromhex('eb0083ec14d9c9dd1c24dd5424088b44240ce80d00000083c414c3')
            assert image[offset:offset+len(expected)]==expected
        if harness_name=='native_aim_easing.c':
            # End immediately after the original easing stores at each join.
            for address,expected in ((0x25acde,'8b0d7c0c7b00'),(0x25b181,'a17c0c7b00')):
                end=config.va_to_file_offset(address)
                assert image[end:end+len(bytes.fromhex(expected))].hex()==expected
                image=image[:end]+b'\xc3'+image[end+1:]
        if harness_name=='native_weapon_sway.c':
            # Original visual consumers through the model C8 stores. Return
            # at their outgoing joins before unrelated translation/copy work.
            for address,expected in ((0x1c3680,'8b4554'),(0x1c37e9,'8b4558')):
                end=config.va_to_file_offset(address)
                assert image[end:end+3].hex()==expected
                image=image[:end]+b'\xc3'+image[end+1:]
        if harness_name=='native_player_dispatch.c':
            # Execute all three original hit-displacement additions/stores.
            # Their incoming world position is supplied by the world-step spy;
            # stop at the outgoing join before unrelated collision processing.
            end=config.va_to_file_offset(0x259388)
            assert image[end:end+5].hex()=='a140194d00'
            image=image[:end]+b'\xc3'+image[end+1:]
        if harness_name=='native_beam_geometry.c':
            # Execute the entire type-32 render-list branch. Its outgoing
            # join begins the next unrelated effect list; terminate there.
            end=config.va_to_file_offset(0x10bfc9)
            assert image[end:end+5].hex()=='a18cbb7300'
            image=image[:end]+b'\xc3'+image[end+1:]
        if harness_name=='native_npc_aim.c':
            # Keep the original aim branch and its zero-result store. End
            # before unrelated eligibility/firing work at each outgoing join.
            for site,expected in [(0x2b9389,'8b7c2460'),(0x2b938d,'396c241c'),
                                  (0x2b9792,'8b4e2c')]:
                end=config.va_to_file_offset(site)
                assert image[end:end+len(bytes.fromhex(expected))].hex()==expected
                image=image[:end]+b'\xc3'+image[end+1:]
        if harness_name=='native_object_bounce.c':
            # The CRT register-argument adapter enters the asin kernel:
            # x / sqrt((1+x)*(1-x)), evaluated with FPATAN. Only this
            # mathematical service is replaced by host asin in the fixture.
            for address,expected in [
                (0x3c9df8,'83ec0cdd1424e87d300000e80d00000083c40cc3'),
                (0x3c9e29,'3d0000f03f732bd9e8d8c1d9e8d8e2dec9d9fad9f3'),
            ]:
                offset=config.va_to_file_offset(address)
                assert image[offset:offset+len(bytes.fromhex(expected))].hex()==expected
        if harness_name in ('native_object_rocking.c','native_object_bounce.c'):
            import struct
            constants='\n'.join(
                f'MEM32(0x{address:x})=0x{struct.unpack_from("<I",image,config.va_to_file_offset(address))[0]:08x};'
                for address in (0x475c78,0x475cc4,0x475cc8,0x475cd4,0x475cdc,
                                0x475ce8,0x475dbc,0x4783dc,0x47d3a4,0x47d564,
                                0x47d568,0x47d56c,0x47d570,0x47d574,0x47d578,
                                0x475d28,0x4785cc,0x4785d0,0x4785d4,0x475ccc,0x475d40))
        if harness_name=='native_player_statistics.c':
            import struct
            constants=f'MEM32(0x475c78)=0x{struct.unpack_from("<I",image,config.va_to_file_offset(0x475c78))[0]:08x};'
        if harness_name in ('native_trap_motion.c','native_trap_transform.c'):
            import struct
            constants='\n'.join(
                f'MEM32(0x{address:x})=0x{struct.unpack_from("<I",image,config.va_to_file_offset(address))[0]:08x};'
                for address in (0x478628,0x475c78,0x475d0c,0x475ce0,0x475cd4,
                                0x475d28,0x4788fc,0x47cfdc,0x49dedc,
                                0x47dc80,0x475c94))
            if harness_name=='native_trap_transform.c':
                constants+='\n'+'\n'.join(
                    f'MEM32(0x{address:x})=0x{struct.unpack_from("<I",image,config.va_to_file_offset(address))[0]:08x};'
                    for address in range(0xa17ac,0xa17bc,4))
        if harness_name=='native_tree_motion.c':
            import struct
            # Full doubles are loaded by the original x87 exponential path.
            constants='\n'.join(
                f'MEM32(0x{address:x})=0x{struct.unpack_from("<I",image,config.va_to_file_offset(address))[0]:08x};'
                for address in (0x475c78,0x49ded8,0x49ded0,0x49ded4,
                                0x49dec8,0x49decc,0x47d7c8,0x47d7cc,
                                0x47d1f4,0x47cfdc))
        if harness_name=='native_weapon_sound.c':
            import struct
            constants='\n'.join(
                f'MEM32(0x{address:x})=0x{struct.unpack_from("<I",image,config.va_to_file_offset(address))[0]:08x};'
                for address in (0x475c78,0x475d28,0x475ccc,0x4785d0,0x475cd4,
                                0x47d1f8,0x478674,0x47d01c,0x47864c))
        if harness_name=='native_weapon_fire.c':
            import struct
            constants='\n'.join(
                f'MEM32(0x{address:x})=0x{struct.unpack_from("<I",image,config.va_to_file_offset(address))[0]:08x};'
                for address in (0x475c78,0x475ce8,0x475ccc,0x4785d0,0x475cd4,
                                0x47666c,0x47cfd8,0x475d10,0x475d28,
                                0x47d1f8,0x478674,0x47d01c,0x47864c))
        if harness_name=='native_npc_weapon_fire.c':
            import struct
            constants='\n'.join(
                f'MEM32(0x{address:x})=0x{struct.unpack_from("<I",image,config.va_to_file_offset(address))[0]:08x};'
                for address in (0x475c78,0x475ccc,0x475cd0,0x475cd4,0x475cdc,
                                0x475ce8,0x475d04,0x475d18,0x475d1c,0x475d24,
                                0x475db4,0x4765f4,0x4785d0,0x47d55c,0x47d670,
                                0x47e2b4,0x47e2b8,0x47e2bc,0x475d28,
                                0x47d1f8,0x478674,0x47d01c,0x47864c,0x475cc8,0x47d6a4))
        if harness_name=='native_camera_track.c':
            import struct
            # Execute this complete switch branch. Redirect only its three
            # exits to a terminal RET, rather than the enclosing camera tail.
            for site in (0xcd816,0xcd9d7,0xcdd38):
                offset=config.va_to_file_offset(site)
                assert image[offset]==0xe9
                assert site+5+struct.unpack_from('<i',image,offset+1)[0]==0xce18c
                image=image[:offset+1]+struct.pack('<i',0xcdd3d-site-5)+image[offset+5:]
            end=config.va_to_file_offset(0xcdd3d)
            assert image[end:end+5].hex()=='a17c0c7b00'
            image=image[:end]+b'\xc3'+image[end+1:]
        if harness_name=='native_camera_effects.c':
            # Isolate the original caller's load/test/branch/call/join. The
            # appended RET replaces only the first instruction after that
            # slice; all gate/call bytes and the complete callee stay original.
            start=config.va_to_file_offset(0xcea8d)
            end=config.va_to_file_offset(0xceaa5)
            assert image[start:end].hex()=='8b0d44794e0085c97e0e8d742414e8208fffffa1c00c7b00'
            image=image[:end]+b'\xc3'+image[end+1:]
        if harness_name=='native_npc_weapon_fire.c':
            # Complete animation-event branch, ending before the next state
            # transition. The event test, call and flag cleanup remain original.
            end=config.va_to_file_offset(0x2b9c0c)
            assert image[end:end+3].hex()=='8b4f04'
            image=image[:end]+b'\xc3'+image[end+1:]
            end=config.va_to_file_offset(0x27ae83)
            assert image[end]==0x56
            image=image[:end]+b'\xc3'+image[end+1:]
        if harness_name=='native_weapon_cooldown.c':
            # Execute the complete deferred-action cooldown branch. All its
            # exits join here; replace only the following instruction by RET.
            end=config.va_to_file_offset(0x1c3273)
            assert image[end:end+6].hex()=='8b0d7c0c7b00'
            image=image[:end]+b'\xc3'+image[end+1:]
        functions={start:dict(end=end,name=f'sub_{start:08X}') for start,end in ranges}
        if harness_name=='native_light_preview.c':
            functions[0x116130]['name']+='_gen'
        if harness_name=='native_weapon_fire.c':
            functions[0x244f30]['name']='sub_00244F30_gen'
        if harness_name=='native_npc_weapon_fire.c':
            functions[0x1bb5c0]['name']='sub_001BB5C0_gen'
        if harness_name=='native_projectile_coupled.c':
            for start in (0xf08d0,0xe78f0):
                functions[start]['name']+='_gen'
        if harness_name in ('native_object_rocking.c','native_object_bounce.c'):
            functions[0xa84f0]['name']+='_gen'
        if harness_name=='native_object_rocking.c':
            functions[0xa8aa0]['name']+='_gen'
        if harness_name=='native_surface_fractional.c':
            functions[0x1950b0]['name']+='_gen'
            functions[0x195110]['name']+='_gen'
            functions[0x1956a0]['name']+='_gen'
        translator=FunctionTranslator(image,functions)
        code='\n'.join(f'void {info["name"]}(void);' for info in functions.values())+'\n'
        code+='\n'.join(translator.translate_function(start,info)
                       for start,info in functions.items())
        if harness_name=='native_object_rocking.c':
            assert code.count('sub_000A8AA0_gen)')==1
            code='void sub_000A8AA0(void);\n'+code.replace('sub_000A8AA0_gen)', 'sub_000A8AA0)')
        if harness_name=='native_npc_weapon_fire.c':
            # Match production's manual NPC entry wrapper at both caller sites.
            assert code.count('sub_001BB5C0_gen)')==2
            code='void sub_001BB5C0(void);\n'+code.replace(
                'sub_001BB5C0_gen)', 'sub_001BB5C0)')

        if harness_name=='native_surface_fractional.c':
            assert code.count('sub_001950B0_gen)')==1
            code='void sub_001950B0(void);\n'+code.replace('sub_001950B0_gen)', 'sub_001950B0)')
            assert code.count('sub_00195110_gen)')==1
            code='void sub_00195110(void);\n'+code.replace('sub_00195110_gen)', 'sub_00195110)')
    finally:
        vars(config).update(previous)
    harness=(ROOT/'tests'/harness_name).read_text()
    if harness_name in ('native_analog_input.c','native_reload_input.c'):
        wrapper=(ROOT/'src/pc_native_timing_guest.inc').read_text(encoding='utf-8')
        start=wrapper.index('static void pc_native_input_guest(')
        end=wrapper.index('static float pc_native_save_animation_delta(',start)
        harness=harness.replace('/* INPUT_WRAPPER */',wrapper[start:end])
    policy=f'#define RECOMP_NUMERIC_POLICY_HEADER "{(ROOT / "src/pc_native_numeric.h").as_posix()}"'
    source=(harness.replace('/* NUMERIC_POLICY */',policy)
                           .replace('/* PHASE_SWEEP */',(ROOT/'tests/reload_phase_sweep.h').read_text())
                           .replace('/* TIMING_HEADER */',f'#include "{(ROOT / "src/pc_native_timing.h").as_posix()}"')
                           .replace('/* FIRE_POLICY */',
                                    f'#include "{(ROOT / "src/pc_native_weapon_guest.h").as_posix()}"')
                           .replace('/* PROJECTILE_POLICY */',
                                    f'#include "{(ROOT / "src/pc_native_projectile_guest.h").as_posix()}"')
                           .replace('/* ROCKING_POLICY */',
                                    f'#include "{(ROOT / "src/pc_native_rocking_guest.h").as_posix()}"')
                           .replace('/* LIGHT_POLICY */',
                                    f'#include "{(ROOT / "src/pc_native_light_guest.h").as_posix()}"')
                           .replace('/* VISUAL_TIME_POLICY */',
                                    f'#include "{(ROOT / "src/pc_native_visual_time_guest.inc").as_posix()}"')
                           .replace('/* SURFACE_POLICY */', f'#include "{(ROOT / "src/pc_native_surface_guest.h").as_posix()}"')
                           .replace('/* CHARACTER_POLICY */', f'#include "{(ROOT / "src/pc_native_character_guest.h").as_posix()}"')
                           .replace('/* WALK_POLICY */', f'#include "{(ROOT / "src/pc_native_walk_guest.h").as_posix()}"')
                           .replace('/* CHARACTER_TRACE_POLICY */', f'#include "{(ROOT / "src/pc_native_character_trace_guest.inc").as_posix()}"')
                           .replace('/* ORIGINAL_CONSTANTS */',constants)
                           .replace('/* GENERATED_ROUTINES */',
                                    (f'#include "{(ROOT / "src/pc_model_transform_guest.inc").as_posix()}"\n'
                                     if any(start == 0x151e80 for start, _ in ranges) else '') + code))
    source=source.replace('"pc_input.c"', '"'+(ROOT/'src/pc_input.c').as_posix()+'"')
    output=compile_and_run(tmp_path,source)
    if harness_name=='native_walk_contact.c':
        for needle,replacement in (
            ('RECOMP_FLOAT_BRANCH_AT(0xC596C,','RECOMP_FLOAT_BRANCH_AT(0xFFFFFFFF,'),
            ('if(candidate)RECOMP_DIRECT_CALLEE_AT(0xc5ad6u,sub_000C54A0)();',
             'if(candidate)sub_000C54A0();'),
        ):
            assert source.count(needle)==1,needle
            with pytest.raises(AssertionError,match='distance>.001f'):
                compile_and_run(tmp_path,source.replace(needle,replacement))
        print(output)
    if harness_name=='native_trap_transform.c':
        for needle,replacement in (
            ('MEM32(esi + 0x28) = eax;\n    RECOMP_AFTER_STORE32(0xA74F4,',
             '(void)eax;\n    RECOMP_AFTER_STORE32(0xA74F4,'),
            ('RECOMP_F32_MUL_AT(0xA15C6, xmm1.f[0], xmm0.f[0])','0.f'),
            ('RECOMP_DIRECT_CALLEE_AT(0xA1645, sub_001531D0)','missing_matrix_copy'),
            ('sub_00151E80();','g_esp+=4;'),
        ):
            assert source.count(needle)==1,needle
            with pytest.raises(AssertionError,match='trap transform'):
                compile_and_run(tmp_path,source.replace(needle,replacement))
        print(output)
    if harness_name=='native_player_statistics.c':
        for needle,replacement in (
            ('RECOMP_DIRECT_CALLEE_AT(0xD2CB7,sub_000D1FD0)','sub_000D1FD0'),
            ('RECOMP_LOAD_GLOBAL_U32(0xD2079, 0x4E7944, MEM32(0x4E7944))','MEM32(0x4E7944)'),
        ):
            assert source.count(needle)==1
            with pytest.raises(AssertionError,match='legacy_stats'):
                compile_and_run(tmp_path,source.replace(needle,replacement))
    if harness_name=='native_trap_motion.c':
        for needle,replacement in (
            ('RECOMP_LOAD_GLOBAL_U32(0x1D8AB0, 0x4E7944, MEM32(0x4E7944))','MEM32(0x4E7944)'),
            ('RECOMP_F32_ADD_AT(0x1D8B46,','RECOMP_F32_ADD_AT(0xFFFFFFFF,'),
            ('RECOMP_F32_ADD_AT(0x1D8B7A,','RECOMP_F32_ADD_AT(0xFFFFFFFF,'),
            ('RECOMP_AFTER_STORE32(0x1D8BFD,','RECOMP_AFTER_STORE32(0xFFFFFFFF,'),
            ('RECOMP_I32_TO_F32_AT(0x1D8AF5, 0x4E7944, MEM32(0x4E7944))','(float)(int32_t)MEM32(0x4E7944)'),
            ('RECOMP_I32_TO_F32_AT(0x1D8B6A, 0x4E7944, MEM32(0x4E7944))','(float)(int32_t)MEM32(0x4E7944)'),
        ):
            assert source.count(needle)==1
            with pytest.raises(AssertionError,match='trap fractional'):
                compile_and_run(tmp_path,source.replace(needle,replacement))
    if harness_name=='native_tree_motion.c':
        needle='RECOMP_LOAD_GLOBAL_U32(0x1D88B0, 0x4E7944, MEM32(0x4E7944))'
        assert source.count(needle)==1
        with pytest.raises(AssertionError,match='legacy_tree'):
            compile_and_run(tmp_path,source.replace(needle,'MEM32(0x4E7944)'))
    if harness_name=='native_character_scale.c':
        import csv
        trace=tmp_path/'character-pairs.csv'
        executable=tmp_path/('float_join.exe' if os.name=='nt' else 'float_join')
        traced=subprocess.run([str(executable)],env=dict(os.environ,TSFP_CHARACTER_TRACE=str(trace)),
                              capture_output=True,text=True,timeout=20)
        assert traced.returncode==0 and traced.stdout==output,traced.stdout+traced.stderr
        rows=list(csv.DictReader(trace.open()))
        assert 20<len(rows)<=8192 and trace.stat().st_size<=8*1024*1024
        assert {'0','1'}=={r['native'] for r in rows}
        assert all(int(r['state'])==0x10000 and len(r)==45 for r in rows)
        failed=subprocess.run([str(executable)],env=dict(os.environ,TSFP_CHARACTER_TRACE=str(tmp_path)),
                              capture_output=True,text=True,timeout=20)
        assert failed.returncode==0 and failed.stdout==output
        assert failed.stderr.count('[CHARACTER-TRACE] Unable to open requested trace')==1
    if harness_name=='native_surface_cache.c':
        for sites in [(0x1276fd,0x127705),(0x12765b,),(0x12773b,0x127791)]:
            mutated=source
            for site in sites:
                needle=f'RECOMP_F32_MUL_AT(0x{site:X},'
                assert mutated.count(needle)==1
                mutated=mutated.replace(needle,'RECOMP_F32_MUL_AT(0xFFFFFFFF,')
            with pytest.raises(AssertionError,match='forced_surface'):
                compile_and_run(tmp_path,mutated)
    if harness_name=='native_reload_input.c':
        assert 'reload_sweep trials=864 presses=1728' in output,output
        needle='RECOMP_LOAD_GLOBAL_U32(0x1C2F09, 0x4E7944, MEM32(0x4E7944))'
        expressions='\n'.join(line.split('/*')[0] for line in source.splitlines())
        assert expressions.count(needle)==1
        mutated=source.replace(needle,'MEM32(0x4E7944)').replace('/* SWEEP_ONLY */','goto temporal;')
        with pytest.raises(AssertionError,match='reload phase mismatch'):
            compile_and_run(tmp_path,mutated)
        print(output)
    if harness_name=='native_escort_recovery.c':
        executable=tmp_path/('float_join.exe' if os.name=='nt' else 'float_join')
        mutation=subprocess.run([str(executable),'legacy-recovery'],capture_output=True,text=True,timeout=10)
        assert mutation.returncode != 0 and 'fps=120 charge=0.359' in mutation.stdout
    if harness_name=='native_locomotion.c':
        executable=tmp_path/('float_join.exe' if os.name=='nt' else 'float_join')
        mutation=subprocess.run([str(executable),'legacy-boost'],capture_output=True,text=True,timeout=10)
        assert mutation.returncode != 0 and 'charge=99.280' in mutation.stdout
    if harness_name=='native_surface_fractional.c':
        print(output)
        executable=tmp_path/('float_join.exe' if os.name=='nt' else 'float_join')
        subprocess.run([str(executable),'disabled'],check=True,timeout=10)
        bias_env=dict(os.environ,TSFP_SURFACE_BIAS_TEST='1',TSFP_SURFACE_FLOAT_TEST='1')
        subprocess.run([str(executable)],env=bias_env,check=True,timeout=10)
        subprocess.run([str(executable)],env=dict(bias_env,TSFP_SURFACE_LIVE_TEST='1'),check=True,timeout=10)
    if harness_name in ('native_analog_input.c','native_network_correction.c','native_locomotion.c','native_player_force.c','native_player_fall.c','native_aim_easing.c','native_animation_clock.c','native_weapon_sway.c','native_player_dispatch.c','native_hud_messages.c','native_light_preview.c','native_strip_distortion.c','native_screen_distortion.c','native_surface_cache.c'):print(output)
    if harness_name in ('native_mothership.c','native_ship_debris.c','native_projectile_motion.c','native_projectile_coupled.c','native_sniper_damage.c','native_beam_lifetime.c','native_beam_geometry.c','native_npc_aim.c','native_object_rocking.c','native_object_bounce.c'):
        print(output)
    if harness_name=='native_escort_recovery.c':print(output)
    if harness_name=='native_script_events.c':
        rows=[dict(field.split('=',1) for field in line.split()[1:])
              for line in output.splitlines() if line.startswith('script_events ')]
        assert len(rows)==10,output
        assert {(int(r['fps']),int(r['slow'])) for r in rows}=={
            (fps,slow) for fps in (60,120,144,180,240) for slow in (1,4)},output
        print(output)
    if harness_name=='native_projectile_coupled.c':
        import math
        rows=[dict(field.split('=',1) for field in line.split()[1:])
              for line in output.splitlines() if line.startswith('coupled_projectile ')]
        assert len(rows)==24,output
        assert {int(r["fps"]) for r in rows}=={60,120,144,180,240},output
        for homing in ('0','1'):
            reference=next(r for r in rows if r['native']=='0' and r['homing']==homing)
            for r in rows:
                if r['homing']==homing and r['native']=='1' and r['opened']=='0':
                    distance=math.hypot(float(r['x'])-float(reference['x']),
                                        float(r['y'])-float(reference['y']))
                    if (int(r['fresh'])!=int(r['frames']) or
                        distance>float(reference['speed'])/60+.00001 or
                        abs(float(r['speed'])-float(reference['speed']))>.001 or
                        r['rng']!=reference['rng'] or r['scans']!=reference['scans'] or
                        int(r['fx'])!=4*int(r['frames'])):
                        raise AssertionError(output)
    if harness_name=='native_npc_aim.c':
        rows=[dict(field.split('=',1) for field in line.split()[1:])
              for line in output.splitlines() if line.startswith('npc_aim ')]
        assert len(rows)==49,output
        assert {int(r["fps"]) for r in rows}=={60,120,144,180,240},output
        for mode in range(3):
            for slow in (1,4):
                base=next(r for r in rows if int(r['mode'])==mode and int(r['slow'])==slow and r['fps']=='60' and r['opened']=='0' and r['rare']=='0')
                for r in rows:
                    if int(r['mode'])==mode and int(r['slow'])==slow and r['opened']=='0' and r['fps']!='60' and r['rare']=='0':
                        if abs(float(r['offset'])-float(base['offset']))>.00001 or int(r['changed'])!=int(r['frames']):
                            raise AssertionError(output)
                        assert r['rng']==base['rng'],output
        for slow in (1,4):
            base=next(r for r in rows if r['rare']=='1' and r['fps']=='60' and int(r['slow'])==slow)
            for r in rows:
                if r['rare']=='1' and int(r['slow'])==slow:
                    assert r['rng']==base['rng'],output
                    # Instant random changes can land at a different subframe.
                    # Bound that phase difference by one original 5% blend of
                    # the full [-1,1] range; random draws/ticks stay exact.
                    assert abs(float(r['offset'])-float(base['offset']))<=.10001,output

    if harness_name=='native_object_rocking.c':
        rows=[dict(field.split('=',1) for field in line.split()[1:])
              for line in output.splitlines() if line.startswith('rocking_audit ')]
        assert len(rows)==12,output
        assert {int(r["fps"]) for r in rows}=={60,120,144,180,240},output
        if any(int(r['frozen']) for r in rows if r['native']=='1'):
            raise ObjectRockingQuantization(output)
    if harness_name=='native_npc_weapon_fire.c':
        print(output)
        rows=[dict(field.split('=',1) for field in line.split()[1:])
              for line in output.splitlines() if line.startswith('npc_fire_audit ')]
        assert len(rows)==10,output
        assert {int(row['fps']) for row in rows}=={60,120,144,180,240},output
        reference=next(r for r in rows if r['fps']=='60' and r['scoped']=='0')
        failures=[r for r in rows if r['scoped']=='1' and
                  (r['shots']!=reference['shots'] or r['sounds']!=reference['sounds'] or
                   abs(float(r['timer'])-float(reference['timer']))>.00001)]
        if failures:
            raise AssertionError(output)
    if harness_name=='native_weapon_fire.c':
        rows=[dict(field.split('=',1) for field in line.split()[1:])
              for line in output.splitlines() if line.startswith('fire_audit ')]
        assert len(rows)==20,output
        failures=[]
        for row in rows:
            if row['native']=='0' or row['open_gate']=='1':
                continue
            reference=next(r for r in rows if r['native']=='0' and
                           r['open_gate']=='0' and r['interval']==row['interval'])
            if (row['shots']!=reference['shots'] or
                abs(float(row['timer'])-float(reference['timer']))>.00001):
                failures.append(f"{row['fps']} FPS / {row['interval']} s interval: "
                                f"shots {row['shots']} vs {reference['shots']}, "
                                f"timer {row['timer']} vs {reference['timer']}")
        if failures:
            raise AssertionError('\n'.join(failures)+'\n'+output)
