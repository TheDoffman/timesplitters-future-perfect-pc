"""Execute the real language/EEPROM paths, including section checksum consistency."""
from pathlib import Path
import os
import re
import subprocess
import sys
import pytest
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from project_paths import configure
configure()
from test_translation_product import _compile


def function(source, signature):
    start = source.index(signature)
    begin = source.index("{", start)
    depth = 1
    end = begin + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


@pytest.fixture(scope="module")
def executable(tmp_path_factory):
    folder = tmp_path_factory.mktemp("language-eeprom")
    runtime = ROOT / "external/ts3FPrecomp"
    source = (runtime / "src/kernel/kernel_xbox.c").read_text(encoding="utf-8")
    # Compile the production implementation, not a second language parser.
    parts = [function(source, name) for name in (
        "static ULONG tsfp_game_language(", "static void eeprom_put32(",
        "static uint32_t eeprom_checksum(", "static void eeprom_image(",
        "NTSTATUS __stdcall xbox_ExQueryNonVolatileSetting(")]
    inherited = (ROOT / "vendor/ts3FPrecomp/source/tests/kernel_audio_setting/test_main.c").read_text(encoding="utf-8")
    inherited = inherited.replace("int main(void)", "int inherited_checks(void)")
    # Keep the inherited assertions/checksum oracle, with diagnostics matching
    # the fixed-width Xbox types on LP64 hosts as well as Windows.
    inherited = inherited.replace('%08lx', '%08" PRIx32 "').replace('%lx', '%" PRIx32 "').replace('%lu', '%" PRIu32 "')
    inherited = re.sub(r'\bstatus(?=\s*,)', '(uint32_t)status', inherited)
    c = folder / "language.c"
    c.write_text('''#include "kernel.h"
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#define xbox_log(...) ((void)0)
#define XC_MAX_FACTORY 0x1FF
#define XC_ENCRYPTED_SECTION 0xFFFE
#define XC_MAX_ALL 0xFFFF
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xC0000023L)
static uint32_t console_game_region(void) { return XC_GAME_REGION_NA; }
static uint32_t console_av_region(void) { return XC_AV_STANDARD_NTSC_M; }
static int recomp_config_bool(const char *a,const char *b,int c) { (void)a;(void)b;return c; }
''' + "\n".join(parts) + inherited + '''
int main(int argc,char **argv) {
    if(argc!=2 || single(XC_LANGUAGE)!=(ULONG)atoi(argv[1]))return 20;
    return inherited_checks();
}
''', encoding='utf-8')
    extra = (["/I"+str(runtime/"src/kernel"), "/I"+str(runtime/"src"), "/D_CRT_SECURE_NO_WARNINGS"] if os.name=="nt"
             # Vendor headers retain their production diagnostics; the extracted
             # implementation and oracle still compile with -Werror.
             else ["-isystem", str(runtime/"src/kernel"), "-isystem", str(runtime/"src")])
    return _compile(folder, c, extra=extra)


@pytest.mark.parametrize("code,expected", [(None,1),("",1),("en",1),("fr",4),("de",3),("it",6),("es",5),("ja",1),("FR",1),("../fr",1),("4",1)])
def test_language_and_all_eeprom_views(executable,code,expected):
    env = os.environ.copy()
    env.pop("TSFP_GAME_LANGUAGE",None)
    if code is not None:
        env["TSFP_GAME_LANGUAGE"] = code
    run = subprocess.run([str(executable),str(expected)],env=env,capture_output=True,text=True,timeout=10)
    assert run.returncode == 0, run.stdout + run.stderr


def test_supported_us_guest_selects_each_archive(tmp_path):
    """Run original XGetLanguage -> game language init -> archive-name lookup."""
    import hashlib
    import struct
    from test_recomp_float_joins import compile_and_run, config, FunctionTranslator
    xbe = ROOT / "private/game/default.xbe"
    if not xbe.is_file():
        pytest.skip("Supported US XBE required for original pack-selection path")
    image = xbe.read_bytes()
    assert hashlib.sha256(image).hexdigest() == "3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc"
    previous = dict(vars(config))
    try:
        config.configure_from_xbe(str(xbe))
        ranges = [(0x37d3bc,0x37d3e9),(0x229c0,0x22a02),(0x70360,0x703e3),(0x350cc0,0x350ceb),(0x3c2d0,0x3c325)]
        functions = {a:dict(name=f"sub_{a:08X}",end=b) for a,b in ranges}
        translator = FunctionTranslator(image,functions)
        routines = "\n".join(f"void {f['name']}(void);" for f in functions.values())
        routines += "\n" + "\n".join(translator.translate_function(a,f) for a,f in functions.items())
        # Include the original branch tables and five pack-name pointers/strings.
        regions = [(0x22a04,24),(0x703e4,24),(0x5373e4,40)]
        table = image[config.va_to_file_offset(0x5373e4):config.va_to_file_offset(0x5373e4)+40]
        for i in range(5):
            address=struct.unpack_from("<I",table,i*8)[0]
            regions.append((address,17))
        constants=[]
        for address,size in regions:
            start=config.va_to_file_offset(address)
            data=",".join(str(v) for v in image[start:start+size])
            constants.append(f"{{const unsigned char b[]={{ {data} }};memcpy(memory+0x{address:x},b,sizeof b);}}")
        source = '''#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "GAME_LANGUAGE_HEADER"
#define RECOMP_LOAD_GLOBAL_U32(site,address,value) pc_game_language_gate(site,address,value)
#define RECOMP_STORE_GLOBAL_U32(site,address,value) pc_game_language_store(site,address,value)
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1;
RECOMP_TLS int g_df;
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) assert(!"unexpected jump-table target")
/* LANGUAGE */
void sub_0038486A(void) {
    assert(MEM32(g_esp+4)==7 && MEM32(g_esp+16)==4);
    MEM32(MEM32(g_esp+12))=tsfp_game_language();
    MEM32(MEM32(g_esp+8))=4;
    g_eax=0;g_esp+=24;
}
static unsigned expected_language;
void sub_001B5300(void) {g_esp+=4;}
void sub_000658D0(void) {MEM8(0x7eb354)=0;g_esp+=4;}
void sub_0003DD20(void) {assert(MEM32(0x4c0454)==expected_language);g_esp+=4;}
void sub_000389B0(void) {g_esp+=4;}
void sub_0003D6A0(void) {g_esp+=4;}
/* ROUTINES */
int main(void) {
    unsigned char *memory=(unsigned char *)calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* CONSTANTS */
    const char *codes[]={"en","fr","de","it","es"};
    const char *packs[]={"pak/sounds_e.pak","pak/sounds_f.pak","pak/sounds_g.pak","pak/sounds_i.pak","pak/sounds_s.pak"};
    for(unsigned i=0;i<5;i++) for(unsigned region=0;region<2;region++) {
#ifdef _WIN32
        _putenv_s("TSFP_GAME_LANGUAGE",codes[i]);
#else
        setenv("TSFP_GAME_LANGUAGE",codes[i],1);
#endif
        MEM32(0x74cf60)=region;
        g_esp=0x9000;MEM32(g_esp)=0x12345678;
        sub_00070360();assert(MEM32(0x4c0454)==i && g_esp==0x9004);
        g_esp=0x9000;MEM32(g_esp)=0x12345678;MEM32(g_esp+4)=0xa000;
        sub_00350CC0();assert(g_esp==0x9004);
        assert(strcmp((const char *)memory+0xa000,packs[i])==0);
        assert(MEM32(0x74cf60)==region); // Region stays unchanged.
        expected_language=i;g_esp=0x9000;MEM32(g_esp)=0x12345678;
        sub_0003C2D0();assert(MEM32(0x4c0454)==i && g_esp==0x9004);
        assert(pc_game_language_gate(0x37000,0x74cf60,region)==region);
        assert(pc_game_language_store(0x704e0,0x4c0454,9)==9);
    }
    free(memory);puts("PASS: original US language pack selection");return 0;
}
'''
        runtime = (ROOT / "external/ts3FPrecomp/src/kernel/kernel_xbox.c").read_text(encoding="utf-8")
        language = function(runtime,"static ULONG tsfp_game_language(").replace("static ULONG","static uint32_t")
        source=source.replace("GAME_LANGUAGE_HEADER",(ROOT/"src/interface/pc_game_language_guest.h").as_posix()).replace("/* LANGUAGE */",language).replace("/* ROUTINES */",routines).replace("/* CONSTANTS */","\n".join(constants))
        compile_and_run(tmp_path,source)
    finally:
        vars(config).update(previous)
