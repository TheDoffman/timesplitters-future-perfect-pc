"""Original CLI contracts using synthetic executables and compiled output."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from types import SimpleNamespace as S

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/compiler'))
import translation_cli as cli
from test_xbe_image import fixture as xbe_fixture
from test_translation_product import _compile


TYPES = '''#undef RECOMP_REGISTER_SITE
#ifndef CLI_FIXTURE_H
#define CLI_FIXTURE_H
#include <stdint.h>
extern uint32_t eax, g_esp;
#define esp g_esp
#define RECOMP_FAST_ENTRY(site) 0
#define RECOMP_DECLARE_GPRS(site) ((void)0)
void recomp_stub_missing(uint32_t);
#endif
'''


def fixture(tmp_path):
    image = xbe_fixture()
    records = []
    for index in range(6):
        start = 0x11000 + index*16
        code = b'\xb8'+(index+1).to_bytes(4, 'little')+b'\xc3'
        image[0x1000+index*16:0x1006+index*16] = code
        records.append(dict(start=hex(start), end=hex(start+6), name=f'sub_{start:08X}', num_instructions=2))
    xbe = tmp_path/'sample.xbe'; xbe.write_bytes(image)
    analysis = tmp_path/'analysis'; analysis.mkdir()
    (analysis/'functions.json').write_text(json.dumps(records))
    (analysis/'labels.json').write_text('[]')
    (analysis/'summary.json').write_text(json.dumps(dict(binary=str(xbe))))
    (analysis/'identified_functions.json').write_text(json.dumps([
        dict(start=row['start'], category=category) for row,category in zip(records,
            ('game_test','crt','data_init','library','game_audio','unknown'))]))
    types = tmp_path/'types.h'; types.write_text(TYPES)
    args = [str(xbe), '--runtime', str(tmp_path/'no-toolkit'), '--disasm-dir', str(analysis),
            '--func-id-dir', str(analysis), '--types-header', str(types), '-o', str(tmp_path/'reports'),
            '--gen-dir', str(tmp_path/'gen')]
    return args, records


def test_address_sources_zero_bounds_and_force_return(tmp_path):
    path = tmp_path/'addresses.json'
    path.write_text(json.dumps([{'start':0,'address':0x1234},'0x10',32]))
    assert cli._load_addrs(path)=={0,16,32}
    path.write_text('{"001000":"named"}')
    assert cli._load_addrs(path)=={0x1000}
    assert cli._parse_force_returns(['0x100=-1','256=0x100000001'])=={256:1}
    for item in ('broken','0x100000000=3','-1=1','0x10=bad'):
        with pytest.raises(ValueError): cli._parse_force_returns([item])
    for value in (-1,2**32,True,{},1.5):
        with pytest.raises((ValueError,KeyError)): cli._address(value)


def test_input_paths_binary_mismatch_and_failure_preserve_output(tmp_path):
    args,_ = fixture(tmp_path)
    files = cli.find_data_files(tmp_path/'analysis',tmp_path/'analysis',runtime=tmp_path/'absent')
    assert files['functions'] and files['abi'] is None
    output = tmp_path/'reports';output.mkdir();sentinel=output/'keep.txt';sentinel.write_text('keep')
    summary=tmp_path/'analysis/summary.json';summary.write_text('{"binary":"other.xbe"}')
    assert cli.main(args+['--all'])==1 and list(output.iterdir())==[sentinel]
    assert cli.main(args+['--header','--skip-binary-check'])==0
    summary.write_text('invalid json')
    assert cli.main(args)==1
    before={p.name:p.read_bytes() for p in output.iterdir()}
    assert cli.main(args+['--split','-1'])==1
    assert before=={p.name:p.read_bytes() for p in output.iterdir()}
    cli.check_data_matches_binary('folder/sample.xbe',None)


def test_modes_categories_limits_and_single_failure(tmp_path,capsys):
    args,_ = fixture(tmp_path)
    assert cli.main(args+['--list-categories'])==0
    assert 'game_audio' in capsys.readouterr().out and not (tmp_path/'reports').exists()
    assert cli.main(args+['--function','11000'])==0
    assert 'eax = 1;' in capsys.readouterr().out
    assert cli.main(args+['--function','9999'])==1
    assert cli.main(args+['--header','--category','library'])==0
    header=(tmp_path/'reports/recomp_functions.h').read_text()
    assert 'sub_00011030' in header and 'sub_00011000' not in header
    assert cli.main(args+['--game-only','--max-funcs','2'])==0
    assert json.loads((tmp_path/'reports/summary.json').read_text())['translated']==2
    assert cli.game_categories({1:{'category':'game_new'},2:{'category':'library'}})=={'game_new','crt','unknown','data_init'}


def test_real_split_generated_functions_and_dispatch_execute(tmp_path):
    args,_=fixture(tmp_path)
    assert cli.main(args+['--game-only','--split','2'])==0
    summary=json.loads((tmp_path/'reports/summary.json').read_text())
    assert summary['translated']==5 and summary['failed']==0 and summary['num_chunks']==3
    gen=tmp_path/'gen'
    paths=[*sorted(gen.glob('recomp_0*.c')),gen/'recomp_dispatch.c',gen/'recomp_hle.c']
    source=gen/'execute.c'
    source.write_text('\n'.join('#include "'+p.name+'"' for p in paths)+'''
uint32_t eax,g_esp;
void recomp_stub_missing(uint32_t address){}
int main(void){g_esp=100;if(recomp_get_count()!=5||recomp_lookup(0x11030))return 1;
recomp_lookup(0x11000)();if(eax!=1||g_esp!=104)return 2;
recomp_lookup(0x11040)();if(eax!=5||g_esp!=108)return 3;
if(!recomp_dispatch_init())return 4;recomp_lookup(0x11050)();return eax!=6||g_esp!=112;}
''')
    binary=_compile(gen,source)
    result=subprocess.run([str(binary)],capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stdout+result.stderr
    # Repeated output retains content and timestamps; only generated files count.
    files=[p for p in gen.iterdir() if p.name.startswith('recomp_')]
    before={p.name:(p.read_bytes(),p.stat().st_mtime_ns) for p in files}
    assert cli.main(args+['--game-only','--split','2'])==0
    assert before=={p.name:(p.read_bytes(),p.stat().st_mtime_ns) for p in files}


def test_manual_pins_wrappers_hook_precedence_and_kept_bodies(tmp_path):
    args,records=fixture(tmp_path)
    manual=tmp_path/'manual.c';manual.write_text('void sub_00011000_gen(void);\nvoid sub_00011000(void) { sub_00011000_gen(); }\n')
    pins=tmp_path/'manual.json';pins.write_text('{"0x11020":"manual_initializer"}')
    hooks=tmp_path/'hooks.c';hooks.write_text('HLE_EXPORT(Example)\nHLE_ORIGINAL(Example)\nHLE_EXPORT(Manual)\nHLE_IMPORT_VAR(Global)\n')
    symbols=tmp_path/'symbols.json';symbols.write_text(json.dumps({'symbols':[
        dict(kind='function',name='Example',address=0x11010),
        dict(kind='function',name='Manual',address=0x11020),
        dict(kind='variable',name='Global',address=0x11100)]}))
    options=['--split','3','--exclude-manual',str(manual),'--manual-functions',str(pins),
             '--hle-impl',str(hooks),'--hle-symbols',str(symbols)]
    assert cli.main(args+options)==0
    gen=tmp_path/'gen';code=''.join(p.read_text() for p in gen.glob('recomp_0*.c'))
    dispatch=(gen/'recomp_dispatch.c').read_text();thunks=(gen/'recomp_hle.c').read_text()
    assert 'void sub_00011000_gen(void)' in code and '(recomp_func_t)sub_00011000 }' in dispatch
    assert '(recomp_func_t)manual_initializer' in dispatch and 'void manual_initializer(void)' not in code
    assert 'void sub_00011010_hle_original(void)' in code
    assert 'hle_Example(); g_esp += 4;' in thunks and 'hle_Manual' not in thunks
    assert 'hle_var_Global = 0x00011100' in thunks and '= sub_00011010_hle_original' in thunks
    before={p.name:(p.read_bytes(),p.stat().st_mtime_ns) for p in gen.iterdir()}
    # Wrapper definitions supplied by the manual file satisfy dispatch during refresh.
    assert cli.main(args+options+['--only-hle-thunks'])==0
    assert before=={p.name:(p.read_bytes(),p.stat().st_mtime_ns) for p in gen.iterdir()}
    paths=[*sorted(gen.glob('recomp_0*.c')),gen/'recomp_dispatch.c',gen/'recomp_hle.c']
    source=gen/'execute.c'
    source.write_text('\n'.join('#include "'+p.name+'"' for p in paths)+'''
uint32_t eax,g_esp;
void recomp_stub_missing(uint32_t address){}
void sub_00011000(void){sub_00011000_gen();eax+=100;}
void manual_initializer(void){eax=20;g_esp+=4;}
void hle_Example(void){uint32_t saved=g_esp;hle_original_Example();g_esp=saved;eax+=10;}
int main(void){g_esp=100;recomp_lookup(0x11000)();if(eax!=101||g_esp!=104)return 1;
g_esp=100;recomp_lookup(0x11010)();if(eax!=12||g_esp!=104)return 2;
recomp_lookup(0x11020)();return eax!=20||g_esp!=108||hle_var_Global!=0x11100;}
''')
    result=subprocess.run([str(_compile(gen,source))],capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stdout+result.stderr


def test_refresh_declines_lost_dispatch_and_missing_types_before_writes(tmp_path):
    gen=tmp_path/'gen';gen.mkdir();thunks=gen/'recomp_hle.c';thunks.write_text('preserve')
    types=gen/'recomp_types.h';types.write_text('preserve types')
    dispatch=gen/'recomp_dispatch.c';dispatch.write_text('(recomp_func_t)sub_00001000')
    batch=S(_runtime_headers=lambda:{'recomp_types.h':'new types'})
    with pytest.raises(ValueError,match='undefined'):cli._refresh(batch,gen,{}, {},set(),set())
    assert thunks.read_text()=='preserve' and types.read_text()=='preserve types'
    def missing(): raise FileNotFoundError('types')
    with pytest.raises(FileNotFoundError):cli._refresh(S(_runtime_headers=missing),gen,{0x1000:('Test',0,{})},{},set(),set())
    assert thunks.read_text()=='preserve' and types.read_text()=='preserve types'


def test_refresh_defers_new_hooks_and_ignores_nondefinitions(tmp_path):
    gen=tmp_path/'gen';gen.mkdir()
    (gen/'recomp_dispatch.c').write_text('(recomp_func_t)sub_00001000')
    (gen/'recomp_0000.c').write_text('''void sub_00001000(void) {}
/* void sub_00002000_hle_original(void) {} */
const char *text="void sub_00002000_hle_original(void) {}";
void sub_00002000_hle_original(void);
#if 0
void sub_00002000_hle_original(void) {}
#endif
#define FAKE \\
void sub_00002000_hle_original(void) {}
''')
    cli._refresh(S(_runtime_headers=lambda:{'recomp_types.h':TYPES,'tsfp_branch_feedback.h':'support'}),gen,{0x1000:('New',0,{}),0x2000:('Kept',0,{})},{}, {'Kept'},set())
    assert (gen/'tsfp_branch_feedback.h').read_text()=='support'
    text=(gen/'recomp_hle.c').read_text()
    assert 'void sub_00001000(void)' not in text and 'void sub_00002000(void)' in text
    assert 'hle_original_Kept)(void) = 0;' in text


def test_toolkit_free_subprocess_cli_emits_and_lists(tmp_path):
    args,_=fixture(tmp_path);copy=tmp_path/'standalone';copy.mkdir()
    sources=list((ROOT/'tools/compiler').glob('translation_*'))
    sources += [ROOT/'tools/compiler'/name for name in ('c_source_scan.py','x86_decode.py','xbe_image.py','xbe_kernel_exports.def','generation_output.py')]
    for path in sources:
        if path.is_file():shutil.copyfile(path,copy/path.name)
    script='import sys;sys.path.insert(0,sys.argv.pop(1));import translation_cli;result=translation_cli.main();assert not any(n.startswith("tools.recomp") for n in sys.modules);raise SystemExit(result)'
    result=subprocess.run([sys.executable,'-I','-c',script,str(copy),*args,'--split','2'],cwd=tmp_path,
                          capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stdout+result.stderr
    assert json.loads((tmp_path/'reports/summary.json').read_text())['translated']==5


def test_unimplemented_diagnostics_report_counts_and_sorted_examples(capsys):
    cli._report_unimplemented({'unimplemented':{'z':[0x40], 'a':[0x20,0x10]}})
    text=capsys.readouterr().err
    assert '3 instruction(s)' in text and text.index('a: 2')<text.index('z: 1') and '0x00000010' in text


def test_trace_force_seh_and_feedback_options_are_isolated(tmp_path,monkeypatch):
    args,_=fixture(tmp_path)
    traces=tmp_path/'traces.json';traces.write_text('[69632]')
    output=tmp_path/'reports';output.mkdir()
    (output/'icall_sites.json').write_text('{"0x11000":{"targets":["0x11010"]}}')
    instances=[];factory=cli.BatchTranslator
    def record(*args,**options):
        result=factory(*args,**options);instances.append(result);return result
    monkeypatch.setattr(cli,'BatchTranslator',record)
    assert cli.main(args+['--function','11000','--trace-functions',str(traces),'--trace-all-entries',
                         '--force-return','0x11000=-1','--seh-prolog','11040','--seh-epilog','11050'])==0
    context=instances[-1].translator
    assert context.trace_functions=={0x11000} and context.trace_all_entries
    assert context.force_returns=={0x11000:0xffffffff}
    assert instances[-1].seh_prolog==0x11040 and instances[-1].seh_epilog==0x11050
    assert instances[-1].icall_sites=={0x11000:[0x11010]}
    alternate=tmp_path/'other-output'
    assert cli.main(args+['-o',str(alternate),'--function','11000'])==0
    assert instances[-1].icall_sites=={} and instances[-1].translator.force_returns=={}


def test_installed_cli_alias_and_module_entry_route_to_project():
    runtime=ROOT/'external/ts3FPrecomp'
    if not (runtime/'.git').exists():pytest.skip('Runtime not prepared')
    source='from tools.recomp import __main__ as bridge;import translation_cli as own;assert bridge is own;assert bridge.main.__module__=="translation_cli"'
    env=dict(os.environ,TSFP_PROJECT_ROOT=str(ROOT))
    for command in ([sys.executable,'-c',source],[sys.executable,'-m','tools.recomp','--help']):
        result=subprocess.run(command,cwd=runtime,env=env,capture_output=True,text=True,timeout=30)
        assert result.returncode==0,result.stdout+result.stderr
