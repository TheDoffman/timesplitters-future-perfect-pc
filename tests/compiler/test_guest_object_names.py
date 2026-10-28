"""Project counted namespace and guest ABI contracts."""
from pathlib import Path
import os
import subprocess
import sys
import pytest
HERE=Path(__file__).resolve().parent
ROOT=next(p for p in HERE.parents if (p/'tools/project_paths.py').exists())
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(ROOT/'tests/compiler'))
from test_translation_product import _compile

def run_contract(tmp_path,mutation=None):
    for name in ('guest_object_names.c','guest_object_names.h','guest_object_names_contracts.c'):
        (tmp_path/name).write_bytes(source_file(name).read_bytes())
    for name in ('guest_objects.c','guest_objects.h'):
        (tmp_path/name).write_bytes((ROOT/'src/runtime'/name).read_bytes())
    if mutation:
        old,new=mutation;path=tmp_path/'guest_object_names.c';text=path.read_text()
        assert text.count(old)==1
        path.write_text(text.replace(old,new),encoding='utf-8')
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra','-pthread']
    exe=_compile(tmp_path,tmp_path/'guest_object_names_contracts.c',extra=[
        tmp_path/'guest_object_names.c',tmp_path/'guest_objects.c',*flags])
    return subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)

def test_counted_namespace_lifetimes_and_real_threads(tmp_path):
    result=run_contract(tmp_path)
    assert result.returncode==0,result.stdout+result.stderr
    assert 'PASS:' in result.stdout

@pytest.mark.parametrize('mutation',[
    ('if(node->length!=length) return 0;','if(node->length<length) return 0;'),
    ('if(flags&TSFP_NAME_IGNORE_ASCII_CASE)','if(flags<=1)'),
    ('if(status==TSFP_NAME_OK && s->count==s->limit)','if(status==TSFP_NAME_OK && s->count>s->limit)'),
    ('if(status==TSFP_NAME_OK && !tsfp_guest_objects_retain(s->objects,key))',
     'if(status==TSFP_NAME_OK && !key)'),
    ('if(tsfp_guest_objects_retain(s->objects,node->key))','if(node->key)'),
    ('(void)tsfp_guest_objects_dereference(s->objects,key);','(void)key;'),
    ('if(expected_key && expected_key!=(*link)->key) break;','if(expected_key==UINT64_MAX) break;'),
    ('if(s->closed) status=TSFP_NAME_CLOSED;\n    else {','if(s->closed==2) status=TSFP_NAME_CLOSED;\n    else {'),
])
def test_broken_namespace_is_rejected(tmp_path,mutation):
    result=run_contract(tmp_path,mutation)
    assert result.returncode!=0,'Broken namespace passed ownership/name contracts'

def run_service(tmp_path,mutation=None,existing=False,adapter_mutation=None):
    names=('guest_kernel_names.inc','guest_object_names.c','guest_object_names.h','guest_name_service.c','guest_name_service.h',
           'guest_object_services.c','guest_object_services.h')
    for name in names:(tmp_path/name).write_bytes(source_file(name).read_bytes())
    for name in ('guest_objects.c','guest_objects.h'):
        (tmp_path/name).write_bytes((ROOT/'src/runtime'/name).read_bytes())
    fixture=(ROOT/'tests/compiler/guest_object_services_contracts.c') if existing else HERE/'guest_name_service_contracts.c'
    (tmp_path/'contract.c').write_bytes(fixture.read_bytes())
    if mutation:
        old,new=mutation;path=tmp_path/'guest_name_service.c';text=path.read_text()
        assert text.count(old)==1;path.write_text(text.replace(old,new),encoding='utf-8')
    if adapter_mutation:
        old,new=adapter_mutation;path=tmp_path/'guest_kernel_names.inc';text=path.read_text()
        assert text.count(old)==1;path.write_text(text.replace(old,new),encoding='utf-8')
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra','-pthread']
    exe=_compile(tmp_path,tmp_path/'contract.c',extra=[tmp_path/name for name in
        ('guest_object_names.c','guest_objects.c','guest_object_services.c','guest_name_service.c')]+flags)
    return subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)

@pytest.mark.parametrize('existing',[False,True],ids=['named-abi','existing-reference-abi'])
def test_name_and_existing_reference_services(tmp_path,existing):
    result=run_service(tmp_path,existing=existing)
    assert result.returncode==0,result.stdout+result.stderr
    assert 'PASS:' in result.stdout

@pytest.mark.parametrize('mutation',[
    ('source=input(s,descriptor,8);','source=input(s,descriptor,4);'),
    ('length>capacity','length>UINT32_MAX-capacity'),
    ('name=input(s,address,length);','name=input(s,address,1);'),
    ('attributes&~UINT32_C(0x40)','attributes&UINT32_C(0)'),
    ('(void)tsfp_guest_objects_dereference(s->references->objects,key);','(void)key;'),
    ('s->references,key,expected_type,output','s->references,key,expected_type==UINT32_MAX?expected_type:0,output'),
    ('s->names->objects!=s->references->objects','s->names->objects==NULL'),
])
def test_broken_name_abi_is_rejected(tmp_path,mutation):
    result=run_service(tmp_path,mutation)
    assert result.returncode!=0,'Broken named-object ABI passed'

@pytest.mark.parametrize('mutation',[
    ('descriptor,attributes,','descriptor?descriptor+1:0,attributes,'),
    ('descriptor,attributes,','descriptor,attributes&~UINT32_C(0x40),'),
    ('type,parse,output','type==UINT32_MAX?type:0,parse,output'),
    ('type,parse,output','type,parse==UINT32_MAX?parse:0,output'),
    ('type,parse,output','type,parse,output?output+1:0'),
    ('g_eax=tsfp_guest_name_reference','(void)tsfp_guest_name_reference'),
])
def test_broken_guest_name_bridge_is_rejected(tmp_path,mutation):
    result=run_service(tmp_path,adapter_mutation=mutation)
    assert result.returncode!=0,'Broken guest name bridge passed'


def source_file(name):
    fixture=HERE/name
    return fixture if fixture.is_file() else ROOT/"src/runtime"/name
