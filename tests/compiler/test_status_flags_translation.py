"""Execute production translations with canonical register storage and flags ABI."""
import os
from pathlib import Path
import subprocess
import pytest
from test_translation_function import translator
from test_translation_product import _compile
from test_guest_status_flags import model, STATUS
from test_guest_integer_flags import CASES as INTEGER_CASES

ROOT=Path(__file__).resolve().parents[2]


def execute(tmp_path,raw,initial,check,callee=None,support=""):
    t,info=translator(raw)
    prefix=''
    if callee:
        data=bytes.fromhex(callee)
        t.func_db[0x2000]={'end':0x2000+len(data),'name':'guest_callee'}
        t._read_func_bytes=lambda start,end:bytes.fromhex(raw) if start==0x1000 else data
        prefix=t.translate_function(0x2000,t.func_db[0x2000])
    generated=t.translate_function(0x1000,info)
    for name in ('recomp_types.h','guest_status_flags.h','guest_state.c'):
        (tmp_path/name).write_bytes((ROOT/'src/runtime'/name).read_bytes())
    source=tmp_path/'translated.c'
    source.write_bytes(('''static int fast_entry;
#define RECOMP_FAST_ENTRY(site) fast_entry
#define RECOMP_GENERATED_CODE 1
#include "recomp_types.h"
#include <stdio.h>
ptrdiff_t g_xbox_mem_offset;
static union { uint64_t alignment; unsigned char bytes[8192]; } storage;
#define ram storage.bytes
#define CHECK(x) do { if(!(x)) {fprintf(stderr,"line%d: %s\\n",__LINE__,#x);return 1;} } while(0)
'''+support+prefix+generated+'\nint main(void) {\ng_xbox_mem_offset=(ptrdiff_t)ram;\n'+initial+'\nguest_body();\n'+check+'\nreturn 0; }\n').encode())
    flag='/DTSFP_RECORD_STATUS_FLAGS=1' if os.name=='nt' else '-DTSFP_RECORD_STATUS_FLAGS=1'
    exe=_compile(tmp_path,source,extra=[tmp_path/'guest_state.c',flag])
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)
    assert result.returncode==0,result.stdout+result.stderr


@pytest.mark.parametrize('raw,op,a,b,carry',[
    ('11d8c3',2,0xffffffff,0,1),
    ('19d8c3',3,0,0xffffffff,1),
    ('40c3',4,0x7fffffff,0,1),
    ('48c3',5,0x80000000,0,1),
    ('f7d8c3',6,0x80000000,0,0),
    ('85c0c3',11,0x80000000,0x80000000,1),
])
def test_translated_producers_publish_actual_flags(tmp_path,raw,op,a,b,carry):
    result,value,known,_=model((op,32,a,b,0,2|carry,STATUS|2))
    execute(tmp_path,raw,f'g_eax={a}u;g_ebx={b}u;g_esp=100;g_status_flags.value={2|carry};g_status_flags.known={STATUS|2};',
            f'CHECK(g_eax=={result}u);CHECK(g_esp==104);CHECK(g_status_flags.value=={value}u);CHECK(g_status_flags.known=={known}u);')


@pytest.mark.parametrize('raw,initial,check',[
    ('f97206b801000000c3b802000000c3','g_eax=0;g_status_flags.value=2;g_status_flags.known=2;',
     'CHECK(g_eax==2);CHECK(g_status_flags.value&1);'),
    ('0f92c0c3','g_eax=0;g_status_flags.value=3;g_status_flags.known=3;',
     'CHECK(g_eax==1);'),
    ('9e9fc3','g_eax=0xd500;g_status_flags.value=2;g_status_flags.known=2;',
     'CHECK(g_eax==0xd700);CHECK((g_status_flags.known&0xd5)==0xd5);CHECK(!(g_status_flags.known&2048));'),
    ('e102b001c3','g_eax=0;g_ecx=2;g_status_flags.value=66;g_status_flags.known=66;',
     'CHECK(g_eax==0);CHECK(g_ecx==1);CHECK(g_status_flags.value==66);'),
    ('e002b001c3','g_eax=0;g_ecx=2;g_status_flags.value=2;g_status_flags.known=66;',
     'CHECK(g_eax==0);CHECK(g_ecx==1);CHECK(g_status_flags.value==2);'),
    ('c1e000c3','g_eax=42;g_status_flags.value=0x8d7;g_status_flags.known=0x8d7;',
     'CHECK(g_eax==42);CHECK(g_status_flags.known==0x8d7);'),
    ('d1e0c3','g_eax=42;g_status_flags.value=0x8d7;g_status_flags.known=0x8d7;',
     'CHECK(g_eax==84);CHECK(g_status_flags.value==2);CHECK(g_status_flags.known==0x8c7);'),
    ('39d87c06b901000000c3b902000000c3','g_eax=0x80000000u;g_ebx=1;g_status_flags.value=2;g_status_flags.known=2;',
     'CHECK(g_ecx==2);CHECK(g_status_flags.known==0x8d7);CHECK(g_status_flags.value&2048);'),
    ('11d8c3','g_eax=42;g_ebx=0;g_status_flags.value=0x8d7;g_status_flags.known=2;',
     'CHECK(g_eax==42);CHECK(g_status_flags.known==2);'),
])
def test_translated_consumers_and_conservative_invalidation(tmp_path,raw,initial,check):
    execute(tmp_path,raw,'g_esp=100;'+initial,check+'CHECK(g_esp==104);')


def test_translated_callee_carry_reaches_caller_without_local_snapshot(tmp_path):
    execute(tmp_path,'e8fb0f00000f92c0c3',
            'g_esp=100;g_eax=0;g_status_flags.value=2;g_status_flags.known=2;',
            'CHECK(g_eax==1);CHECK(g_esp==104);CHECK(g_status_flags.known&1);',callee='f9c3')


def test_translated_callee_carry_is_used_by_adc(tmp_path):
    execute(tmp_path,'e8fb0f000011d8c3',
            'g_esp=100;g_eax=0xffffffffu;g_ebx=0;g_status_flags.value=2;g_status_flags.known=2;',
            'CHECK(g_eax==0);CHECK(g_esp==104);CHECK(g_status_flags.value==0x57);CHECK(g_status_flags.known==0x8d7);',callee='f9c3')


@pytest.mark.parametrize('raw,width',[('80d001c3',8),('6683d001c3',16)])
def test_translated_partial_register_adc_preserves_upper_bits(tmp_path,raw,width):
    a=0xabcdefff
    result,value,known,_=model((2,width,a,1,0,3,STATUS|2))
    expected=(a&~(2**width-1))|result
    execute(tmp_path,raw,f'g_esp=100;g_eax={a}u;g_status_flags.value=3;g_status_flags.known=0x8d7;',
            f'CHECK(g_eax=={expected}u);CHECK(g_status_flags.value=={value}u);CHECK(g_status_flags.known=={known}u);')


def test_fast_entry_invalidates_unmodeled_host_status(tmp_path):
    execute(tmp_path,'c3','fast_entry=1;g_esp=100;g_status_flags.value=0x8d7;g_status_flags.known=0x8d7;',
            'CHECK(g_esp==100);CHECK(!(g_status_flags.known&TSFP_STATUS_FLAGS));')


@pytest.mark.parametrize('remainder',['d9f8','d9f5'])
def test_complete_remainder_exits_guest_reduction_loop(tmp_path,remainder):
    # FPREM/FPREM1; FNSTSW AX; SAHF; JP back to the operation; RET.
    # Stale C2 from an earlier unordered comparison must not spin forever.
    execute(tmp_path,remainder+'dfe09e7af9c3',
            'g_esp=100;g_fp_top=0;g_fp_stack[0]=7.;g_fp_stack[1]=2.;g_fp_cc=0x4500;',
            'CHECK(!(g_fp_cc&0x400));CHECK(!(g_status_flags.value&TSFP_FLAG_PF));CHECK(g_esp==104);')


@pytest.mark.parametrize('op,width',[(op,width) for op in ('shl','shr','sar','shld','shrd','rol','ror','rcl','rcr') for width in (8,16,32) if width!=8 or op not in ('shld','shrd')])
def test_production_shift_routes_and_dynamic_counts(tmp_path,op,width):
    from test_guest_shift_flags import COMBOS,model as shift_model
    if op in ('shld','shrd'):raw='0f'+('a5' if op=='shld' else 'ad')+'d0'
    else:raw=('d2' if width==8 else 'd3')+dict(shl='e0',shr='e8',sar='f8',rol='c0',ror='c8',rcl='d0',rcr='d8')[op]
    if width==16:raw='66'+raw
    index=COMBOS.index((op,width));a=0xf080807f;b=0x89abcdef
    def seed(count):return f'g_esp=100;g_eax={a}u;g_edx={b}u;g_ecx={count};g_status_flags.value=0x8d7;g_status_flags.known=0x8d7;'
    checks=[]
    for count in (0,1,width,width+1,32,33,255):
        result,value,known,_=shift_model((index,width,a,b,count,0x8d7,0x8d7))
        if count:checks += [seed(count),'guest_body();']
        checks += [f'CHECK(g_eax=={result}u);CHECK(g_status_flags.value=={value}u);CHECK(g_status_flags.known=={known}u);CHECK(g_esp==104);']
    execute(tmp_path,raw+'c3',seed(0),'\n'.join(checks))


@pytest.mark.parametrize('raw,a,result',[('d1d0',0x80000000,1),('d1d8',1,0x80000000)])
def test_carry_rotate_consumes_callee_status(tmp_path,raw,a,result):
    execute(tmp_path,'e8fb0f0000'+raw+'c3',f'g_esp=100;g_eax={a}u;g_status_flags.value=2;g_status_flags.known=2;',
            f'CHECK(g_eax=={result}u);CHECK(g_status_flags.value==0x803);CHECK(g_status_flags.known==0x803);CHECK(g_esp==104);',callee='f9c3')


@pytest.mark.parametrize('op,width,form',INTEGER_CASES)
def test_production_remaining_integer_flag_routes(tmp_path,op,width,form):
    from test_guest_integer_flags import CASES,model as integer_model,MULTIPLIER
    if op in ('mul','imul'):raw=('f6' if width==8 else 'f7')+('e1' if op=='mul' else 'e9')
    elif op=='imul2':raw='0fafc1'
    elif op=='imul3':raw='6bc1f9'
    elif op.startswith('bt'):raw='0f'+dict(bt='a3',bts='ab',btr='b3',btc='bb')[op]+('0b' if form=='mem' else 'c8')
    elif op in ('bsf','bsr'):raw='0f'+('bc' if op=='bsf' else 'bd')+'c1'
    elif op=='xchg':raw=('86' if width==8 else '87')+('0b' if form in ('mem','lock') else 'c8')
    else:
        instruction=dict(xadd=('c0' if width==8 else 'c1'),cmpxchg=('b0' if width==8 else 'b1'))[op]
        modrm='0b' if form in ('mem','lock') else 'e0' if form=='overlap' else 'c9' if form=='alias' and op=='xadd' else 'c8' if form=='alias' or op=='xadd' else 'ca'
        raw='0f'+instruction+modrm
    if width==16:raw='66'+raw
    if form=='lock':raw='f0'+raw
    index=CASES.index((op,width,form))
    checks=[]
    def seed(a,d,c,value,known):
        memory=''.join(f'MEM32({1024+i*4})={(d^((i*MULTIPLIER)&0xffffffff))}u;' for i in range(-8,9))
        return f'g_esp=100;g_eax={a}u;g_edx={d}u;g_ecx={c}u;g_ebx=1024;g_status_flags.value={value}u;g_status_flags.known={known}u;'+memory
    inputs=[(0x80000001,0x7fffffff,0xffffffff,0x8d7,0x8d7),(0,0,0,2,2),(0x7f,1,1,2,0x8d7)]
    for n,(a,d,c,value,known) in enumerate(inputs):
        if op.startswith('bt') and form=='mem':c=((c%129)-64)&0xffffffff
        expected=integer_model((index,width,a,d,c,value,known))
        if n:checks += [seed(a,d,c,value,known),'guest_body();']
        checks += [f'CHECK(g_eax=={expected[0]}u);CHECK(g_edx=={expected[1]}u);CHECK(g_ecx=={expected[2]}u);CHECK(g_esp==104);',
                   f'CHECK(g_status_flags.value=={expected[20]}u);CHECK(g_status_flags.known=={expected[21]}u);']
        checks += [f'CHECK(MEM32({992+i*4})=={v}u);' for i,v in enumerate(expected[3:20])]
    a,d,c,value,known=inputs[0]
    if op.startswith('bt') and form=='mem':c=((c%129)-64)&0xffffffff
    execute(tmp_path,raw+'c3',seed(a,d,c,value,known),'\n'.join(checks))
