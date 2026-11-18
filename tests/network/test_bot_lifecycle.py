"""Run the production lifecycle against the supported title's actual roster code."""
import hashlib
from pathlib import Path
import pytest

ROOT=Path(__file__).resolve().parents[2]

@pytest.mark.parametrize('host',['windows','native'])
@pytest.mark.parametrize('registry',['fixture','original'])
@pytest.mark.parametrize('mutation',[None,'retained_roster','missing_register'])
def test_bot_body_and_roster_lifetime(tmp_path,host,registry,mutation):
    from test_recomp_float_joins import config,FunctionTranslator,compile_and_run
    xbe=ROOT/'private/game/default.xbe'
    if registry=='original':
        if not xbe.is_file():pytest.skip('Supported US XBE required for original actor roster')
        image=xbe.read_bytes()
        assert hashlib.sha256(image).hexdigest()=='3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
        previous=dict(vars(config))
        try:
            config.configure_from_xbe(str(xbe))
            functions={a:dict(name=f'sub_{a:08X}',end=b) for a,b in
                       [(0x15bca0,0x15bcbe),(0x15bcc0,0x15bcfd),(0xd8c00,0xd8c2f)]}
            translator=FunctionTranslator(image,functions)
            code='\n'.join(translator.translate_function(a,f) for a,f in functions.items())
        finally:vars(config).update(previous)
    else:
        # Engine boundary doubles keep ownership/retry tests active without
        # private game assets on CI. The original route verifies the contract.
        code='''
void sub_0015BCA0(void){
    uint32_t n=MEM32(0x74c37c);MEM32(0x7a3580+n*4)=MEM32(MEM32(g_esp+4)+0x7c);
    MEM32(0x74c37c)=n+1;g_esp+=4;
}
void sub_0015BCC0(void){
    uint32_t actor=MEM32(MEM32(g_esp+4)+0x7c),n=MEM32(0x74c37c);
    for(unsigned i=0;i<n;i++)if(MEM32(0x7a3580+i*4)==actor){
        MEM32(0x7a3580+i*4)=MEM32(0x7a3580+(n-1)*4);MEM32(0x74c37c)=n-1;break;
    }g_esp+=4;
}
void sub_000D8C00(void){
    for(unsigned i=0;i<MEM32(0x74c37c);i++)guest_call(sub_0009A060,1,MEM32(MEM32(0x7a3580+i*4)));
    g_esp+=4;
}
'''
    production=(ROOT/'src/network/pc_network_bot_lifecycle.inc').read_text()
    menu=(ROOT/'src/interface/pc_display_menu.c').read_text()
    for name in ('pc_network_bots_revive','pc_network_bots_pose_weapons'):
        at=menu.index(('uint32_t ' if name.endswith('revive') else 'void ')+name+'(')
        production+='\n'+menu[at:menu.index('\n}',at)+2]
    source=(ROOT/'tests/network/bot_lifecycle.c').read_text()
    source=source.replace('/* HOST */','#define _WIN32 1' if host=='windows' else '#undef _WIN32\n#define TSFP_NATIVE_NETWORK 1')
    source=source.replace('/* ORIGINAL_ROSTER */',code).replace('/* PRODUCTION */',production)
    if mutation=='retained_roster':
        needle='guest_call(sub_0015BCC0,1,body);'
        assert needle in source
        source=source.replace(needle,'/* deliberately retain stale actor */')
        diagnostic='!MEM32(ACTOR) && !MEM32(0x74c37c)'
    elif mutation=='missing_register':
        needle='pc_network_bots_register_body(actor,entity);'
        assert needle in source
        source=source.replace(needle,'/* deliberately omit revived actor */')
        diagnostic='MEM32(0x74c37c)==1'
    if mutation:
        with pytest.raises(AssertionError) as failure:compile_and_run(tmp_path,source)
        assert diagnostic in str(failure.value)
    else:
        assert 'PASS:' in compile_and_run(tmp_path,source)
