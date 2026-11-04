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


@pytest.mark.parametrize("adapter", ["TSFP_PC_CONTROLS", "TSFP_NATIVE_INPUT"])
@pytest.mark.parametrize("mutation", ["fixed", "steal-player-two"])
def test_local_player_host_routing(tmp_path, adapter, mutation):
    production = (RUNTIME / "input/input_bindings.c").read_text()
    sampler = re.search(r'int recomp_bindings_sample\(.*?\n\}', production, re.S).group()
    sampler = sampler.replace('recomp_bindings_sample(', 'test_sample(', 1)
    reload_helper = re.search(r'static int reload_action\(.*?\n\}', production, re.S).group()
    if mutation == "steal-player-two":
        sampler, changed = re.subn(r'!\(reserved_pads\s*&\s*\(1u\s*<<\s*slot\)\)', '1', sampler)
        assert changed == 2, 'Exercise both real host fallback loops'
    fixture = (ROOT / "tests/input/local_player_routing.c").read_text()
    fixture = fixture.replace('/* PRODUCTION_SAMPLER */', '#define '+adapter+'\n'+reload_helper+'\n'+sampler)
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
    if mutation != 'fixed':
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
typedef struct { int connected; const char *path; } SDL_Gamepad;
static SDL_Gamepad pads[10];static int ids[4],count;
#define RECOMP_PAD_SLOTS 4
#define SCAN_MS 250
static int SDL_GamepadConnected(SDL_Gamepad *p){return p->connected;}
static void SDL_CloseGamepad(SDL_Gamepad *p){(void)p;}
static int *SDL_GetGamepads(int *n){*n=count;return ids;}
static const char *SDL_GetGamepadPathForID(int id){return pads[id].path;}
static SDL_Gamepad *SDL_OpenGamepad(int id){return &pads[id];}
static const char *SDL_GetGamepadName(SDL_Gamepad *p){return p->path;}
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
 puts("PASS: reconnect restores its seat; another device cannot steal it");return 0;
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
