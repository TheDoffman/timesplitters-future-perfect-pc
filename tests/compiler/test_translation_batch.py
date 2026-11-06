"""Original batch publication and compiled address-dispatch contracts."""
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
from types import SimpleNamespace as S

import pytest

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools/compiler'))
import translation_batch as own
from translation_names import identifier
from test_translation_product import _compile
from test_translation_lifting import installed


TYPES='''#ifndef FIXTURE_TYPES_H
#define FIXTURE_TYPES_H
#include <stdint.h>
extern unsigned seen;
extern uint32_t g_esp;
void recomp_stub_missing(uint32_t);
#endif
'''


class FakeTranslator:
    def __init__(self,data,functions,labels,classifications,abi,**options):
        self.func_db=functions;self.options=options;self.discovery=[]
        self.owned_function_starts={0x1040}
        self.lifter=S(manual_functions=set(),referenced_calls={},unimplemented={})
        self.calls=[]
    def discover_static_indirect_targets(self):self.discovery.append('static')
    def discover_cfg_ownership(self):self.discovery.append('ownership')
    def discover_jump_table_entries(self):self.discovery.append('tables')
    def translate_function(self,address,info):
        self.calls.append((address,info.get('name'),set(self.lifter.manual_functions)))
        if address==0x1030:return None
        return f'void {identifier(address,info.get("name"))}(void) {{ seen += {address-0x1000+1}; }}\n'
    def _stub_ret_bytes(self,address):return 8


def fixture(tmp_path):
    data=bytearray(512);struct.pack_into('<I',data,0x104,0x10000);struct.pack_into('<I',data,0x118,0x10180)
    title='Fixture */ title'.encode('utf-16-le');data[0x18c:0x18c+len(title)]=title
    xbe=tmp_path/'game.xbe';xbe.write_bytes(data)
    records=[dict(start=f'{a:X}',end=f'{a+8:X}',name=name,num_instructions=3,
                  **({'call_name':'guest_wrapper'} if a==0x1020 else {})) for a,name in
             ((0x1000,'guest_a'),(0x1010,'guest_manual'),(0x1020,'guest_wrapped_gen'),(0x1030,'guest_failed'),(0x1040,'guest_owned'))]
    functions=tmp_path/'functions.json';functions.write_text(json.dumps(records),encoding='utf-8')
    types=tmp_path/'runtime-types.h';types.write_text(TYPES,encoding='utf-8')
    batch=own.BatchTranslator(xbe,functions,output_dir=tmp_path/'out',types_header=types,function_translator_type=FakeTranslator)
    return batch,records


def test_loading_classification_discovery_title_and_filters(tmp_path):
    batch,records=fixture(tmp_path)
    assert batch.title=='Fixture */ title'
    assert batch.translator.discovery==['static','ownership','tables']
    assert batch.func_db[0x1000]['end']==0x1008 and batch.func_db[0x1000]['_addr']==0x1000
    assert batch.translator.func_db is batch.func_db
    assert batch.translator.options['setjmp_fn'] is None
    batch.classification_db={0x1000:{'category':'alpha'},0x1010:{'category':'beta'}}
    assert [a for a,_ in batch.get_functions_by_category({'alpha'})]==[0x1000]
    assert [a for a,_ in batch.get_functions_by_category(exclude_categories={'alpha','beta'})]==[0x1020,0x1030]
    assert batch.translate_single(0x9999) is None
    assert 'guest_a' in batch.translate_single(0x1000)
    paths={}
    for name,data in {'labels':[{'address':'1000','name':'label'}],
                      'classes':[{'start':'1000','category':'alpha'}],
                      'abi':[{'address':'1000','estimated_params':2}],
                      'sites':{'1000':{'targets':['1001','1002']},'1004':{'targets':['1008'],'saturated':True}}}.items():
        paths[name]=tmp_path/(name+'.json');paths[name].write_text(json.dumps(data),encoding='utf-8')
    other=own.BatchTranslator(batch.xbe_path,tmp_path/'functions.json',paths['labels'],paths['classes'],paths['abi'],
        seh_prolog=0,seh_epilog=[],icall_sites_json_path=paths['sites'],function_translator_type=FakeTranslator)
    assert other.label_db[0x1000]=='label' and other.classification_db[0x1000]['category']=='alpha'
    assert other.abi_db[0x1000]['estimated_params']==2
    assert other.icall_sites=={0x1000:[0x1001,0x1002]}
    assert other.translator.options['seh_prolog']==0 and other.translator.options['seh_epilog']==[]
    assert own.xbe_title(b'bad','folder/fallback.xbe')=='fallback'
    damaged=bytearray(512);struct.pack_into('<I',damaged,0x104,0x10000)
    assert own.xbe_title(damaged,'fallback.xbe')=='fallback'
    struct.pack_into('<I',damaged,0x118,0xffffffff)
    assert own.xbe_title(damaged,'fallback.xbe')=='fallback'


def test_split_manual_kept_wrappers_stubs_and_stale_output_execute(tmp_path):
    batch,_=fixture(tmp_path);out=tmp_path/'out'
    batch.translator.lifter.referenced_calls={0x9000:'guest_missing',0x1010:'guest_manual',0x1020:'guest_wrapper'}
    batch.translator.lifter.unimplemented={'bad':[0x1001]}
    kwargs=dict(chunk_size=2,manual={0x1010},keep_bodies={0x1010:'guest_kept'})
    stats=batch.translate_batch_split(list(batch.func_db.items()),out,**kwargs)
    assert {k:stats[k] for k in ('total','translated','failed','manual_functions','unresolved_stubs','num_chunks')}==dict(total=4,translated=3,failed=1,manual_functions=1,unresolved_stubs=1,num_chunks=2)
    assert stats['unimplemented']=={'bad':[0x1001]} and len(stats['files'])==5
    assert all(manual=={0x1010} for _,_,manual in batch.translator.calls)
    dispatch=(out/'recomp_dispatch.c').read_text(encoding='utf-8')
    assert 'guest_kept' not in dispatch and '(recomp_func_t)guest_wrapper' in dispatch
    header=(out/'recomp_funcs.h').read_text(encoding='utf-8')
    assert 'Fixture * / title' in header and 'void guest_kept(void);' in header
    stamps={p.name:p.stat().st_mtime_ns for p in out.iterdir()}
    batch.translate_batch_split(list(batch.func_db.items()),out,**kwargs)
    assert stamps=={p.name:p.stat().st_mtime_ns for p in out.iterdir()}
    harness=tmp_path/'batch.c';harness.write_text('''#include "recomp_funcs.h"
#include <stddef.h>
unsigned seen;uint32_t g_esp,observed_esp,observed_address;
void recomp_stub_missing(uint32_t address){observed_address=address;observed_esp=g_esp;}
void guest_manual(void){seen+=100;}
void guest_wrapper(void){seen+=200;}
typedef void (*recomp_func_t)(void);
recomp_func_t recomp_lookup(uint32_t);size_t recomp_call_all(void);size_t recomp_get_count(void);
int main(void){if(recomp_get_count()!=4||recomp_lookup(0x1010)!=guest_manual||recomp_lookup(0x1020)!=guest_wrapper)return 1;
if(recomp_call_all()!=4||seen!=301)return 2;
g_esp=100;guest_missing();if(observed_esp!=100||observed_address!=0x9000||g_esp!=112)return 3;
guest_kept();return seen!=318;}
''',encoding='utf-8')
    includes=[f'/I{out}'] if os.name=='nt' else ['-I',str(out)]
    executable=_compile(tmp_path,harness,extra=(*includes,*sorted(out.glob('*.c'))))
    assert subprocess.run([str(executable)],timeout=30).returncode==0
    (out/'recomp_helper.c').write_text('keep',encoding='utf-8')
    (out/'unrelated.c').write_text('keep',encoding='utf-8')
    batch.translator.lifter.referenced_calls.clear()
    batch.translate_batch_split([(0x1000,batch.func_db[0x1000])],out,chunk_size=1)
    assert not (out/'recomp_0001.c').exists() and not (out/'recomp_stubs_unresolved.c').exists()
    assert (out/'recomp_helper.c').read_text()=='keep' and (out/'unrelated.c').read_text()=='keep'
    Path(batch.types_header).write_text(TYPES+'\n/* updated ABI */\n',encoding='utf-8')
    batch.translate_batch_split([],out)
    assert 'updated ABI' in (out/'recomp_types.h').read_text()
    assert not list(out.glob('recomp_0*.c'))


def test_simple_batches_categories_and_invalid_output_preservation(tmp_path):
    batch,_=fixture(tmp_path)
    batch.classification_db={0x1000:{'category':'alpha'},0x1030:{'category':'beta'}}
    result=batch.translate_by_category(['alpha','beta'],output_prefix='bundle')
    assert set(result)=={'alpha','beta'}
    assert result['alpha']['total_insns']==3 and result['beta']['failed']==1 and result['beta']['total_insns']==0
    assert (tmp_path/'out/bundle_alpha.c').exists() and (tmp_path/'out/bundle_beta.c').exists()
    before={p.name:p.read_bytes() for p in (tmp_path/'out').iterdir()}
    for kwargs in ({'chunk_size':0},{'chunk_size':-1},{'chunk_size':True},{'header_name':'../oops.h'},
                   {'header_name':'recomp_types.h'},{'header_name':'tsfp_branch_feedback.h'},
                   {'header_name':'TSFP_BRANCH_FEEDBACK.H'},{'header_name':'RECOMP_TYPES.H'},
                   {'prefix':'RECOMP','header_name':'recomp_dispatch.c'},
                   {'prefix':'../outside'},{'header_name':'recomp_0000.c'},{'header_name':'Recomp_0000.C'}):
        with pytest.raises(ValueError):batch.translate_batch_split([],tmp_path/'out',**kwargs)
    batch.types_header=tmp_path/'missing.h'
    with pytest.raises(FileNotFoundError):batch.translate_batch_split([],tmp_path/'out')
    assert before=={p.name:p.read_bytes() for p in (tmp_path/'out').iterdir()}


@pytest.mark.parametrize('addresses',[[],[0],[0xffffffff],[0x1000,0x1004,0x1010,0x1100],[0,0xffffffff]])
def test_compiled_dispatch_search_flat_failure_and_edge_addresses(tmp_path,addresses):
    batch=object.__new__(own.BatchTranslator)
    header=tmp_path/'functions.h'
    header.write_text('#include <stdint.h>\n'+'\n'.join(f'void guest_{i}(void);' for i in range(len(addresses))),encoding='utf-8')
    source=batch._dispatch_source([(a,f'guest_{i}',None) for i,a in reversed(list(enumerate(addresses)))],'functions.h')
    # Allocation failure is deterministic, even for a full 32-bit guest span.
    prefix='''#include <stdlib.h>
static int fail_allocate=1;static unsigned allocations;
static void *fixture_calloc(size_t n,size_t size){++allocations;return fail_allocate?NULL:calloc(n,size);}
#define calloc fixture_calloc
'''
    code=prefix+source+'\n#undef calloc\nstatic unsigned calls;\n'
    code+='\n'.join(f'void guest_{i}(void){{calls+={i+1};}}' for i in range(len(addresses)))
    expected='\n'.join(f'if(recomp_lookup(0x{a:08X}u)!=guest_{i})return 10;' for i,a in enumerate(addresses))
    holes={0,0xffffffff,0x7fffffff,0x80000000,1,0xffff,0x1001}-set(addresses)
    expected+='\n'+'\n'.join(f'if(recomp_lookup(0x{a:08X}u)!=NULL)return 11;' for a in holes)
    span=addresses[-1]-addresses[0]+1 if addresses else 0
    code+='\nint main(void){\n'+expected+'\nif(recomp_dispatch_init()!=0||recomp_dispatch_flat_bytes()!=0)return 12;\n'+expected
    code+=f'\nif(recomp_get_count()!={len(addresses)}||recomp_call_all()!={len(addresses)}||calls!={len(addresses)*(len(addresses)+1)//2})return 13;\n'
    if addresses and span<4096:
        code+='fail_allocate=0;if(recomp_dispatch_init()!=1)return 14;\n'+expected
        code+=f'\nif(recomp_dispatch_flat_bytes()!=(size_t){span}*sizeof(recomp_func_t))return 15;\n'
        code+='if(recomp_dispatch_init()!=1||allocations!=2)return 16;\n'
    code+='return 0;}\n'
    path=tmp_path/'dispatch.c';path.write_text(code,encoding='utf-8')
    assert subprocess.run([str(_compile(tmp_path,path))],timeout=30).returncode==0


def test_dispatch_rejects_duplicate_addresses_and_publication_symlinks(tmp_path):
    batch,_=fixture(tmp_path)
    with pytest.raises(ValueError):batch._dispatch_source([(1,'a',None),(1,'b',None)],'functions.h')
    for address in (-1,2**32):
        with pytest.raises(ValueError):batch._dispatch_source([(address,'a',None)],'functions.h')
    out=tmp_path/'out';out.mkdir();outside=tmp_path/'preserved.h';outside.write_text('preserve',encoding='utf-8')
    try:(out/'recomp_funcs.h').symlink_to(outside)
    except OSError:pytest.skip('Host does not permit symbolic-link creation')
    with pytest.raises(ValueError):batch.translate_batch_split([],out)
    assert outside.read_text()=='preserve'


def test_installed_adapter_and_toolkit_free_batch_import(tmp_path):
    installed('''
from tools.recomp.translator import BatchTranslator,FunctionTranslator,xbe_title
import translation_batch as own
assert BatchTranslator.__bases__==(own.BatchTranslator,)
assert BatchTranslator.function_translator_type is FunctionTranslator
assert xbe_title is own.xbe_title
''')
    names=['translation_batch.py','translation_helpers.py','translation_layout.py','translation_names.py',
           'translation_host_symbols.txt','generation_output.py','xbe_image.py']
    for name in names:shutil.copyfile(ROOT/'tools/compiler'/name,tmp_path/name)
    result=subprocess.run([sys.executable,'-I','-c',
        'import sys;sys.path.insert(0,sys.argv[1]);import translation_batch as b;'
        'assert b.xbe_title(b"","game.xbe")=="game";'
        'assert not any(n.startswith("tools.recomp") for n in sys.modules)',str(tmp_path)],
        capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stderr


def test_default_support_header_publication_and_missing_input_preservation(tmp_path, monkeypatch):
    from translation_cli import _refresh
    batch,_=fixture(tmp_path)
    batch.types_header=None
    project=tmp_path/'own-project'
    runtime=project/'src/runtime';runtime.mkdir(parents=True)
    (runtime/'recomp_types.h').write_text(TYPES)
    support=runtime/'tsfp_branch_feedback.h';support.write_text('original support')
    monkeypatch.setattr(own,'__file__',str(project/'tools/compiler/translation_batch.py'))
    output=tmp_path/'generated'
    batch.translate_batch_split([],output)
    assert (output/support.name).read_text()=='original support'
    before={p.name:p.read_bytes() for p in output.iterdir()}
    support.unlink()
    with pytest.raises(FileNotFoundError):batch.translate_batch_split([],output)
    assert before=={p.name:p.read_bytes() for p in output.iterdir()}
    with pytest.raises(FileNotFoundError):_refresh(batch,output,{}, {},set(),set())
    assert before=={p.name:p.read_bytes() for p in output.iterdir()}
    support.write_text('updated support')
    _refresh(batch,output,{}, {},set(),set())
    assert (output/support.name).read_text()=='updated support'
