"""Reproduce launcher device changes against the real runtime parser/sampler."""


import json
import os
from pathlib import Path
import subprocess
import re

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
RUNTIME = ROOT / "external/ts3FPrecomp/src"


def test_pre_preset_factory_defaults():
    config=json.loads((ROOT / "config/pc-controls.default.json").read_text())
    assert "custom_binding_semantics" not in config
    assert all("keyboard_preset" not in c and "controller_preset" not in c for c in config["controllers"])
    keys=config["controllers"][0]["bindings"]
    assert keys["a"]==["key:E","key:SPACE"]
    assert keys["b"]==["key:F","key:MBUTTON"]
    assert keys["dpad_up"]==["key:UP","key:PAGEUP"]
    assert keys["dpad_down"]==["key:DOWN","key:PAGEDOWN"]
    assert keys["dpad_left"]==["key:LEFT","key:F24"]
    assert keys["dpad_right"]==["key:RIGHT","key:F23"]


def test_retired_metadata_does_not_override_custom_pad(controller_exe,tmp_path):
    config=json.loads((ROOT / "config/pc-controls.default.json").read_text())
    profile=config["controllers"][0];profile.update(device="gamepad:0",keyboard_preset=1,controller_preset=1)
    config["custom_binding_semantics"]=1
    profile["bindings"]["a"]=["pad:b","none"]
    profile["bindings"]["rtrigger"]=["none","none"]
    path=tmp_path/"retired-mode.json";path.write_text(json.dumps(config));before=path.read_bytes()
    result=subprocess.run([str(controller_exe),str(path),"0","custom"],capture_output=True,text=True,timeout=15)
    assert result.returncode==0,result.stdout+result.stderr
    assert path.read_bytes()==before


def test_native_developer_player_keys(tmp_path):
    production = (RUNTIME / 'input/input_bindings.c').read_text()
    native = (ROOT / 'src/native/input_sdl.c').read_text()
    guards = '\n'.join(re.search(pattern, native, re.S).group() for pattern in (
        r'int pc_controls_accept_pad\(.*?\n\}', r'int pc_controls_key_down\(.*?\n\}'))
    dispatch = re.search(r'static int key_down\(.*?\n\}', production, re.S).group()
    fixture = (ROOT / 'tests/input/developer_player_keys.c').read_text()
    source = tmp_path / 'developer-keys.c'
    source.write_text(fixture.replace('/* PRODUCTION_GUARDS */', guards+'\n'+dispatch))
    compiler, env = native_compiler(tmp_path)
    exe = tmp_path / ('developer-keys.exe' if os.name == 'nt' else 'developer-keys')
    sources = [str(source), str(ROOT / 'src/input/pc_input.c')]
    args = ([compiler, '/nologo', '/std:c11', *sources, '/Fe:'+str(exe)] if os.name == 'nt'
            else [compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', *sources, '-lm', '-o', str(exe)])
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stdout + result.stderr


@pytest.mark.parametrize("adapter", ["TSFP_PC_CONTROLS", "TSFP_NATIVE_INPUT"])
@pytest.mark.parametrize("mutation", ["fixed", "steal-player-two", "shared-keyboard-layout"])
def test_local_player_host_routing(tmp_path, adapter, mutation):
    production = (RUNTIME / "input/input_bindings.c").read_text()
    sampler = re.search(r'int recomp_bindings_sample\(.*?\n\}', production, re.S).group()
    sampler = sampler.replace('recomp_bindings_sample(', 'test_sample(', 1)
    reload_helper = re.search(r'static int reload_action\(.*?\n\}', production, re.S).group()
    if mutation == "steal-player-two":
        sampler, changed = re.subn(r'!\(reserved_pads\s*&\s*\(1u\s*<<\s*slot\)\)', '1', sampler)
        assert changed == 2, 'Exercise both real host fallback loops'
    if mutation == "shared-keyboard-layout":
        sampler,changed=re.subn(r'int keyboard_actions=.*?;', 'int keyboard_actions=0;',sampler)
        assert changed==1
    fixture = (ROOT / "tests/input/local_player_routing.c").read_text()
    prompts=(ROOT / 'src/input/pc_prompts.c').read_text()
    prompts=prompts[prompts.index('static SRWLOCK prompt_lock'):prompts.index('static void key_label(')]
    fixture=fixture.replace('/* PRODUCTION_PROMPTS */',prompts)
    axis_helper = re.search(r'static int control_magnitude\(.*?\n\}', production, re.S).group()
    fixture = fixture.replace('/* PRODUCTION_SAMPLER */', '#define '+adapter+'\n'+axis_helper+'\n'+reload_helper+'\n'+sampler)
    source = tmp_path / "routing.c"; source.write_text(fixture)
    compiler, env = native_compiler(tmp_path)
    exe = tmp_path / ("routing.exe" if os.name == "nt" else "routing")
    includes = [ROOT / 'tests/input', ROOT / 'src', RUNTIME, RUNTIME / 'input']
    sources = [str(source), str(ROOT / 'src/input/pc_input.c')]
    args = ([compiler, '/nologo', '/std:c11', *['/I'+str(p) for p in includes], *sources, '/Fe:'+str(exe), 'user32.lib']
            if os.name == 'nt' else [compiler, '-std=gnu11', *['-I'+str(p) for p in includes], *sources, '-lm', '-o', str(exe)])
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=15)
    assert (result.returncode == 0) == (mutation == 'fixed'), result.stdout + result.stderr
    if mutation == 'shared-keyboard-layout':
        assert 'one.bAnalogButtons[XBOX_BUTTON_X]' in result.stderr
    if mutation == 'steal-player-two':
        assert '!memcmp(&one,&neutral,sizeof one)' in result.stderr, result.stderr



@pytest.fixture(scope="module")
def controller_exe(tmp_path_factory):
    folder = tmp_path_factory.mktemp("controller-profile")
    compiler, env = native_compiler(folder)
    exe = folder / ("controller.exe" if os.name == "nt" else "controller")
    includes = [RUNTIME, RUNTIME / "input"]
    source = ROOT / "tests/input/controller_profile.c"
    args = ([compiler, "/nologo", "/std:c11", "/D_CRT_SECURE_NO_WARNINGS",
             *["/I" + str(p) for p in includes], str(source), "/Fe:" + str(exe), "user32.lib"]
            if os.name == "nt" else
            [compiler, "-std=gnu11", *["-I" + str(p) for p in includes], str(source), "-o", str(exe)])
    result = subprocess.run(args, cwd=folder, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    return exe


@pytest.mark.parametrize("device", ["gamepad:0", "gamepad:2", "xinput:1"])
@pytest.mark.parametrize("mode", ["standard", "custom", "unbound", "none", "defaults"])
@pytest.mark.parametrize("reordered", [False, True])
def test_controller_profile(controller_exe, tmp_path, device, mode, reordered):
    config = json.loads((ROOT / "config/pc-controls.default.json").read_text())
    player = config["controllers"][0]
    player["device"] = device
    if mode == "custom":
        player["bindings"]["a"] = ["pad:b", "none"]
        player["bindings"]["rtrigger"] = ["none", "none"]
    elif mode == "unbound":
        player["bindings"] = {key: "none" for key in player["bindings"]}
    elif mode == "none":
        player["device"] = "none"
    elif mode == "defaults":
        del player["bindings"]
    if reordered:
        # Explicit physical assignments must not become a port default just
        # because the profile's array index matches its selected pad number.
        config["controllers"].remove(player)
        config["controllers"].insert(int(device[-1]), player)
    path = tmp_path / "controls.json"
    path.write_text(json.dumps(config))
    before = path.read_bytes()
    result = subprocess.run([str(controller_exe), str(path), device[-1],
                             "standard" if mode == "defaults" else mode],
                            capture_output=True, text=True, timeout=15)
    assert result.returncode == 0, result.stdout + result.stderr
    assert path.read_bytes() == before


def test_independent_local_players(controller_exe, tmp_path):
    config = json.loads((ROOT / "config/pc-controls.default.json").read_text())
    config["controllers"][0]["device"] = "gamepad:0"
    config["controllers"][1]["device"] = "gamepad:1"
    path = tmp_path / "two-players.json"
    path.write_text(json.dumps(config))
    before = path.read_bytes()
    result = subprocess.run([str(controller_exe), str(path), "0", "two"],
                            capture_output=True, text=True, timeout=15)
    assert result.returncode == 0, result.stdout + result.stderr
    assert path.read_bytes() == before


def test_controller_hotplug_keeps_player_slots(tmp_path):
    source = (RUNTIME / "input/recomp_pad.c").read_text()
    slot = source[source.index("typedef struct {"):source.index("} SdlSlot;")+len("} SdlSlot;")]
    scan = source[source.index("static void sdl_scan("):source.index("static int16_t flip_y(")]
    fixture = r"""
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
typedef int SDL_JoystickID;
typedef struct { int connected; const char *path; const char *serial; const char *name; } SDL_Gamepad;
static SDL_Gamepad pads[10];static int ids[4],count;
#define RECOMP_PAD_SLOTS 4
#define SCAN_MS 250
static int SDL_GamepadConnected(SDL_Gamepad *p){return p->connected;}
static void SDL_CloseGamepad(SDL_Gamepad *p){(void)p;}
static int *SDL_GetGamepads(int *n){*n=count;return ids;}
static const char *SDL_GetGamepadPathForID(int id){return pads[id].path;}
static SDL_Gamepad *SDL_OpenGamepad(int id){return &pads[id];}
static const char *SDL_GetGamepadName(SDL_Gamepad *p){return p->name?p->name:p->path;}
static const char *SDL_GetGamepadSerial(SDL_Gamepad *p){return p->serial;}
static const char *SDL_GetGamepadGUIDForID(int id){return pads[id].name?pads[id].name:pads[id].path;}
static void SDL_GUIDToString(const char *guid,char *out,int n){snprintf(out,n,"%s",guid?guid:"");}
static void SDL_free(void *p){(void)p;}
""" + slot + "\nstatic SdlSlot g_sdl[4];static uint64_t g_scan_at;\n" + scan + r"""
int main(void){
 pads[1]=(SDL_Gamepad){1,"pad-a"};pads[2]=(SDL_Gamepad){1,"pad-b"};
 ids[0]=1;ids[1]=2;count=2;sdl_scan(0);
 assert(g_sdl[0].pad==&pads[1] && g_sdl[1].pad==&pads[2]);
 pads[1].connected=0;pads[3]=(SDL_Gamepad){1,"pad-c"};ids[0]=2;ids[1]=3;sdl_scan(250);
 assert(!g_sdl[0].pad && g_sdl[1].pad==&pads[2] && g_sdl[2].pad==&pads[3]);
 pads[4]=(SDL_Gamepad){1,"pad-a"};ids[0]=3;ids[1]=4;ids[2]=2;count=3;sdl_scan(500);
 assert(g_sdl[0].pad==&pads[4] && g_sdl[1].pad==&pads[2] && g_sdl[2].pad==&pads[3]);
 /* A reconnecting controller can have a changed USB/Bluetooth path. */
 memset(g_sdl,0,sizeof g_sdl);count=2;ids[0]=1;ids[1]=2;
 pads[1]=(SDL_Gamepad){1,"old-path","serial-a","same-model"};
 pads[2]=(SDL_Gamepad){1,"other-path","serial-b","same-model"};sdl_scan(750);
 pads[1].connected=0;pads[5]=(SDL_Gamepad){1,"new-path","serial-a","same-model"};ids[0]=5;
 sdl_scan(1000);assert(g_sdl[0].pad==&pads[5] && g_sdl[1].pad==&pads[2]);
 /* Pathless devices must not consume a new reserved slot on every reconnect. */
 memset(g_sdl,0,sizeof g_sdl);count=1;ids[0]=6;
 pads[6]=(SDL_Gamepad){1,NULL,NULL,"pathless-model"};sdl_scan(1250);
 for(int cycle=0;cycle<8;cycle++) {
  int old=ids[0],next=old==6?7:6;pads[old].connected=0;
  pads[next]=(SDL_Gamepad){1,NULL,NULL,"pathless-model"};ids[0]=next;
  sdl_scan(1500+cycle*250);assert(g_sdl[0].pad==&pads[next]);
  for(int j=1;j<4;j++)assert(!g_sdl[j].pad);
 }
 puts("PASS: stable/changed/pathless reconnect restores seats without stealing another controller");return 0;
}
"""
    path=tmp_path / "hotplug.c";path.write_text(fixture)
    compiler, env=native_compiler(tmp_path)
    exe=tmp_path / ("hotplug.exe" if os.name=="nt" else "hotplug")
    args=([compiler,"/nologo","/std:c11",str(path),"/Fe:"+str(exe)] if os.name=="nt" else
          [compiler,"-std=c11",str(path),"-o",str(exe)])
    result=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15)
    assert result.returncode==0,result.stdout+result.stderr


def test_start_to_join(tmp_path):
    source = tmp_path / "join.c"
    source.write_text(r'''#include <assert.h>
#include "pc_local_join.h"
int main(void) {
 PcLocalJoin s={0}; int d[4]={-2,-1,-1,-1};
 int preferred[4]={-1,-1,-1,-1};
 pc_local_detect(d,preferred,5);assert(d[0]==-2 && d[1]==0 && d[2]==2 && d[3]==-1);
 /* Disabling a session port restores its same controller when available. */
 preferred[1]=0;d[1]=-1;pc_local_detect(d,preferred,5);assert(d[1]==0 && d[2]==2);
 pc_local_detect(d,preferred,4);assert(d[1]==0); /* disconnected seat retained */
 d[1]=d[2]=d[3]=-1;
 /* P1 already owns keyboard port 0; controller connection order is irrelevant. */
 assert(pc_local_join(&s,d,0,1)==-1);
 assert(pc_local_join(&s,d,4,1)==1); assert(d[0]==-2 && d[1]==2);
 assert(pc_local_join(&s,d,4,3)==-1); /* held cannot join twice */
 pc_local_join(&s,d,0,3);
 assert(pc_local_join(&s,d,1,3)==2); assert(d[2]==0);
 pc_local_join(&s,d,0,7);
 assert(pc_local_join(&s,d,8,7)==3); assert(d[3]==3);
 pc_local_join(&s,d,0,15);assert(pc_local_join(&s,d,2,15)==-1);
 /* Existing configuration does not dictate guest player order. */
 s=(PcLocalJoin){0};d[0]=3;d[1]=0;d[2]=1;d[3]=2;
 pc_local_join(&s,d,0,1);
 assert(pc_local_join(&s,d,4,1)==3); /* first Start -> guest P2, input port 3 */
 pc_local_join(&s,d,0,9);
 assert(pc_local_join(&s,d,8,9)==-1); /* P1 controller cannot join again */
 assert(pc_local_join(&s,d,1,9)==1); /* next Start -> guest P3 */
 /* Simultaneous unused pads each receive one seat, even after release. */
 s=(PcLocalJoin){0};d[0]=-2;d[1]=d[2]=d[3]=-1;
 pc_local_join(&s,d,0,1);
 assert(pc_local_join(&s,d,6,1)==1);
 assert(pc_local_join(&s,d,0,3)==2);
 assert(d[1]==1 && d[2]==2);
 return 0;
}
''')
    compiler, env = native_compiler(tmp_path)
    exe = tmp_path / ("join.exe" if os.name == "nt" else "join")
    inc = str(ROOT / "src/input")
    args = ([compiler, "/nologo", "/std:c11", "/I"+inc, str(source), "/Fe:"+str(exe)]
            if os.name == "nt" else [compiler, "-std=c11", "-I"+inc, str(source), "-o", str(exe)])
    result = subprocess.run(args, cwd=tmp_path, env=env, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=15)
    assert result.returncode == 0, result.stdout + result.stderr


def test_presence_detects_unconfigured_controllers(tmp_path):
    bindings=(ROOT / "src/input/pc_bindings.inc").read_text()
    detect=re.search(r"static void pc_settings_players_detect\(.*?\n\}",bindings,re.S).group()
    runtime=(RUNTIME / "input/input_bindings.c").read_text()
    present=re.search(r"unsigned recomp_bindings_present_mask\(.*?\n\}",runtime,re.S).group()
    source=tmp_path / "presence.c"
    source.write_text(r'''#include <assert.h>
#include <stdio.h>
#include "pc_local_join.h"
#define TSFP_NATIVE_INPUT
#define DEV_NONE 0
#define DEV_KEYBOARD 1
#define DEV_PAD 2
#define SRC_NONE 0
#define SRC_KEY 1
#define CONTROL_COUNT 1
#define PORTS 4
#define AcquireSRWLockExclusive(x) ((void)0)
#define ReleaseSRWLockExclusive(x) ((void)0)
typedef struct {int count;struct {int kind;} src[2];} Binding;
typedef struct {int device,pad,has_key,pad_fallback;Binding bind[1];char label[32];} Controller;
typedef struct {int unused;} RecompPadState;
static unsigned g_players_auto_ports;
static Controller g_ctl[4];static int g_players_preferred[4]={-1,-1,-1,-1};
static unsigned connected;static unsigned long long tick;
static int recomp_pad_read(int slot,RecompPadState *p){(void)p;return !!(connected&(1u<<slot));}
static void describe(Controller *c,char *s,unsigned n){(void)c;(void)s;(void)n;}
static void recomp_bindings_init(void){}
static unsigned long long now_ms(void){return tick+=251;}
''' + detect + "\n" + present + r'''
int main(void) {
 g_ctl[0].device=DEV_KEYBOARD;
 connected=5;assert(recomp_bindings_present_mask()==7);
 assert(g_ctl[1].pad==0 && g_ctl[2].pad==2 && g_players_auto_ports==6);
 connected=4;assert(recomp_bindings_present_mask()==5);
 connected=5;assert(recomp_bindings_present_mask()==7);
 g_ctl[1].device=DEV_NONE;g_ctl[1].pad=-1;
 assert(recomp_bindings_present_mask()==7 && g_ctl[1].pad==0);
 return 0;
}
''')
    compiler,env=native_compiler(tmp_path)
    exe=tmp_path / ("presence.exe" if os.name=="nt" else "presence")
    inc=str(ROOT / "src/input")
    args=([compiler,"/nologo","/std:c11","/I"+inc,str(source),"/Fe:"+str(exe)] if os.name=="nt" else
          [compiler,"-std=c11","-I"+inc,str(source),"-o",str(exe)])
    result=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15)
    assert result.returncode==0,result.stdout+result.stderr


def test_real_sdl_controller_reconnect(tmp_path):
    library=ROOT / "build/windows/xboxrecomp/third_party/SDL/Release/SDL3-static.lib"
    if os.name!="nt" or not library.is_file():
        pytest.skip("Real SDL lifecycle test requires the Windows build's SDL static library")
    compiler,env=native_compiler(tmp_path)
    exe=tmp_path / "hotplug.exe"
    args=[compiler,"/nologo","/std:c11","/MD",
          "/I"+str(ROOT / "external/ts3FPrecomp/third_party/SDL/include"),
          "/I"+str(RUNTIME / "input"),str(ROOT / "tests/input/controller_hotplug.c"),
          str(RUNTIME / "input/recomp_pad.c"),"/Fe:"+str(exe),str(library)]
    args += [x+".lib" for x in ["user32","gdi32","shell32","ole32","oleaut32","uuid",
              "advapi32","setupapi","dinput8","imm32","version","winmm","xinput","cfgmgr32"]]
    env=dict(env,RECOMP_PAD_API="sdl")
    result=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    result=subprocess.run([str(exe)],cwd=tmp_path,env=env,capture_output=True,text=True,timeout=20)
    if "SKIP:" in result.stdout:pytest.skip(result.stdout.strip())
    assert result.returncode==0,result.stdout+result.stderr
    assert "six real SDL detach/reattach cycles" in result.stdout


@pytest.mark.parametrize("value,expected", [(0,0),(1638,1638),(32767,32767),(-1,-1),(32768,-1),(0.5,-1),("5",-1),(True,-1),(10**40,-1)])
def test_deadzone_configuration(controller_exe,tmp_path,value,expected):
    path=tmp_path/"deadzones.json"
    path.write_text(json.dumps({"controllers":[{"port":1,"device":"gamepad:0","deadzone":4321,"move_deadzone":value,"aim_deadzone":value}]}))
    result=subprocess.run([str(controller_exe),str(path),str(expected),"deadzone-config"],capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stdout+result.stderr
