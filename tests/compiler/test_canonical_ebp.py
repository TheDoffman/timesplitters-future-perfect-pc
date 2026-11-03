"""Native helpers must see the real EBP of ordinary translated execution."""
import struct
import pytest
from test_translation_function import translator
from test_translation_continuation import execute,check


def body_code(frame):
    raw=bytes.fromhex('55'+('89e5' if frame else '')+'bd44332211')
    raw+=b'\xe8'+struct.pack('<i',0x2000-(0x1000+len(raw)+5))
    return raw.hex()+'5dc3'


@pytest.mark.parametrize('frame',[False,True])
@pytest.mark.parametrize('refs',[False,True])
def test_native_call_observes_live_ebp_and_return_restores_it(tmp_path,frame,refs):
    t,info=translator(body_code(frame));normal=t.translate_function(0x1000,info)
    check(execute(tmp_path,{0x3000:'c3'},'''
g_ebp=0x1234;g_seh_ebp=0xfeed;
guest_body();
CHECK(observed_bp==0x11223344 && g_ebp==0x1234 && g_esp==1004);
CHECK(MEM32(996)==0x1234);
''',support='''
static uint32_t observed_bp;
void sub_00002000(void){observed_bp=g_ebp;g_esp+=4;}
'''+normal,refs=refs))


def test_fault_helper_observes_ebp_before_any_guest_call_boundary(tmp_path):
    t,info=translator('bd44332211 31c9 f7f1 c3');normal=t.translate_function(0x1000,info)
    check(execute(tmp_path,{0x3000:'c3'},'''
g_ebp=0x1234;g_seh_ebp=0xfeed;
if(!setjmp(fault)){guest_body();CHECK(0);}
CHECK(observed_bp==0x11223344 && fault_site==0x1007 && fault_reason==0);
CHECK(g_ebp==0x11223344 && g_esp==1000);
''',support='''
static jmp_buf fault;static uint32_t observed_bp,fault_site;static unsigned fault_reason;
void tsfp_guest_divide_error(uint32_t site,unsigned reason){
 observed_bp=g_ebp;fault_site=site;fault_reason=reason;longjmp(fault,1);
}
'''+normal))


def test_compiled_cached_ebp_provider_is_rejected(tmp_path):
    t,info=translator(body_code(False));normal=t.translate_function(0x1000,info)
    binding='uint32_t *const _recomp_live_ebp = &g_ebp;'
    assert normal.count(binding)==1
    normal=normal.replace(binding,'uint32_t cached_ebp = g_ebp; uint32_t *const _recomp_live_ebp = &cached_ebp;')
    result=execute(tmp_path,{0x3000:'c3'},'''
g_ebp=0x1234;g_seh_ebp=0xfeed;guest_body();
CHECK(observed_bp==0x11223344 && g_ebp==0x1234 && MEM32(996)==0x1234);
''',support='''
static uint32_t observed_bp;
void sub_00002000(void){observed_bp=g_ebp;g_esp+=4;}
'''+normal)
    assert result.returncode!=0,'Compiled stale EBP cache passed architectural checks'
