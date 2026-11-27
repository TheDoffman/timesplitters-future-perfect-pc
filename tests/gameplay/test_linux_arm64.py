"""ARM64 build orchestration and executable host-context regressions."""
import importlib.util
import os
import platform
from pathlib import Path
import subprocess
import shutil
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tests/compiler'))
from test_translation_product import _compile


def builder():
    spec = importlib.util.spec_from_file_location('arm64_builder', ROOT / 'tools/build/build-native-linux-arm64.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_linux_register_context_and_device_policy(tmp_path):
    headers = {
        'signal.h': '#pragma once\ntypedef struct { void *si_addr; } siginfo_t;\n',
        'sys/ucontext.h': '#pragma once\n#include <stdint.h>\ntypedef struct { uint64_t regs[32], sp, pc; } mcontext_t;\ntypedef struct { mcontext_t uc_mcontext; } ucontext_t;\n',
        'platform/win32_compat.h': '#pragma once\n#include <stdint.h>\n#include <stddef.h>\n',
        'apu/apu.h': '#pragma once\n#include <stdint.h>\ntypedef struct MCPXAPUState MCPXAPUState;\nextern MCPXAPUState *g_apu_state;\nuint64_t mcpx_apu_mmio_read(MCPXAPUState *,uint64_t,unsigned);\nvoid mcpx_apu_mmio_write(MCPXAPUState *,uint64_t,uint64_t,unsigned);\n',
        'guest_arena_service.h': '#pragma once\n#include <stddef.h>\nvoid *tsfp_guest_mmio_alias(void *,size_t);\n',
    }
    for name, text in headers.items():
        path = tmp_path / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
    source = tmp_path / 'context.c'
    source.write_bytes((ROOT / 'tests/gameplay/linux_arm64_context.c').read_bytes())
    prefix = '/I' if os.name == 'nt' else '-I'
    exe = _compile(tmp_path, source, extra=[prefix + str(tmp_path), prefix + str(ROOT / 'src/native')])
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=15)
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'PASS: Linux ARM64' in result.stdout


@pytest.mark.parametrize('setup', [False, True])
def test_fresh_and_incremental_build_apply_ledger_before_configure(tmp_path, monkeypatch, setup):
    module = builder()
    monkeypatch.setattr(module, 'ROOT', tmp_path)
    monkeypatch.setattr(module, 'SYSROOT', tmp_path / 'sysroot')
    monkeypatch.setattr(module, 'DXC', tmp_path / 'dxc')
    monkeypatch.setattr(module, 'BUILD', tmp_path / 'build')
    game = tmp_path / 'game'; game.mkdir()
    for name in ('pak', 'xmv'): (game / name).mkdir()
    (game / 'default.xbe').write_bytes(b'fixture')
    digest = module.sha256(game / 'default.xbe')
    (tmp_path / 'tsfp.ps1').write_text("$revision = '" + 'a' * 40 + "'\n$expectedHash = '" + digest.upper() + "'\n")
    for file in (module.SYSROOT / 'usr/include/stdio.h', module.DXC / 'usr/lib/libdxcompiler.so'):
        file.parent.mkdir(parents=True); file.touch()
    generated = tmp_path / 'external/ts3FPrecomp/titles/futureperfect/src/recomp/gen'
    generated.mkdir(parents=True)
    calls = []
    monkeypatch.setattr(module, 'setup', lambda jobs: calls.append('dependencies'))
    monkeypatch.setattr(module.spirv_cross, 'setup', lambda root: None)
    monkeypatch.setattr(module.spirv_cross, 'verify', lambda root: None)
    monkeypatch.setattr(module.shutil, 'which', lambda name: '/tools/' + name)
    def output(args, **kwargs):
        if args[-1] == 'HEAD' and '-C' in args:
            if setup: assert 'prepare-runtime.py' in calls
            return 'a' * 40
        return 'fixture'
    monkeypatch.setattr(module.subprocess, 'check_output', output)
    class Configured(Exception): pass
    def run(*args, **kwargs):
        names = [Path(str(a)).name for a in args]
        for name in ('prepare-runtime.py', 'apply-patches.py'):
            if name in names: calls.append(name)
        if 'network-identity.py' in names:
            (module.BUILD / 'generated').mkdir(parents=True)
        if '-S' in args and str(module.BUILD) in [str(a) for a in args]:
            assert 'apply-patches.py' in calls
            assert kwargs['env']['PKG_CONFIG_PATH'] == ''
            assert str(module.SYSROOT) in kwargs['env']['PKG_CONFIG_LIBDIR']
            raise Configured
    monkeypatch.setattr(module, 'run', run)
    from types import SimpleNamespace
    with pytest.raises(Configured):
        module.build(SimpleNamespace(game_dir=game, jobs=2, setup=setup, generate=False))


def test_corrupt_sysroot_stops_before_extraction(tmp_path, monkeypatch):
    module = builder()
    archive = tmp_path / 'sysroot.tar.gz'; archive.write_bytes(b'corrupt archive')
    monkeypatch.setattr(module, 'download', lambda *args: archive)
    monkeypatch.setattr(module, 'install_dxc', lambda: pytest.fail('corrupt archive accepted'))
    with pytest.raises(SystemExit, match='checksum'): module.setup(2)


def test_existing_dxc_headers_do_not_bypass_package_verification(tmp_path, monkeypatch):
    module = builder()
    monkeypatch.setattr(module, 'DXC', tmp_path)
    header = tmp_path / 'usr/include/dxc/dxcapi.h'
    header.parent.mkdir(parents=True); header.write_text('previous package')
    archive = tmp_path / module.DXC_PIN['package']; archive.write_bytes(b'corrupt download')
    monkeypatch.setattr(module, 'download', lambda *args: archive)
    with pytest.raises(SystemExit, match='checksum'): module.install_dxc()
    assert header.read_text() == 'previous package'


def test_gtk_package_mismatch_never_extracts_into_sysroot(tmp_path, monkeypatch):
    module = builder()
    archive = tmp_path / 'gtk.deb'; archive.write_bytes(b'corrupt package')
    monkeypatch.setattr(module, 'download', lambda *args: archive)
    monkeypatch.setattr(module, 'run', lambda *args, **kwargs: pytest.fail('unverified package extraction'))
    with pytest.raises(SystemExit, match='GTK package.*checksum'): module.install_gtk()


def test_wrong_host_rejected_before_setup(tmp_path, monkeypatch):
    module = builder()
    monkeypatch.setattr(module.platform, 'system', lambda: 'Windows')
    monkeypatch.setattr(sys, 'argv', ['build-native-linux-arm64.py', '--game-dir', str(tmp_path), '--setup'])
    monkeypatch.setattr(module, 'build', lambda args: pytest.fail('wrong-host mutation'))
    with pytest.raises(SystemExit) as error: module.main()
    assert error.value.code == 2


@pytest.mark.skipif(sys.platform != 'linux', reason='Linux signal delivery')
def test_real_arm64_protected_page_recovery(tmp_path):
    native = platform.machine().lower() in ('aarch64', 'arm64')
    compiler = shutil.which('clang' if native else 'aarch64-linux-gnu-gcc')
    emulator = shutil.which('qemu-aarch64')
    if not compiler or (not native and not emulator):
        pytest.skip('Needs native ARM64 or aarch64-linux-gnu-gcc and qemu-aarch64')
    # Reuse the real protected-page fixture and project arena/alias services.
    # Only VirtualProtect spelling and APU declarations are supplied here.
    platform_header = tmp_path / 'platform/win32_compat.h'
    platform_header.parent.mkdir()
    platform_header.write_text('''#pragma once
#include <stdint.h>
#include <stddef.h>
#include <sys/mman.h>
typedef uint32_t DWORD;
#define PAGE_READONLY 2
static inline int VirtualProtect(void *p,size_t n,DWORD flags,DWORD *old) {
    (void)flags; *old=0; return mprotect(p,n,PROT_READ)==0;
}
''')
    apu = tmp_path / 'apu/apu.h'; apu.parent.mkdir()
    apu.write_text('''#pragma once
#include <stdint.h>
typedef struct MCPXAPUState MCPXAPUState;
extern MCPXAPUState *g_apu_state;
uint64_t mcpx_apu_mmio_read(MCPXAPUState *,uint64_t,unsigned);
void mcpx_apu_mmio_write(MCPXAPUState *,uint64_t,uint64_t,unsigned);
''')
    sources = [ROOT / 'tests/gameplay/native_mmio.c', ROOT / 'src/native/mmio_linux_arm64.c',
               ROOT / 'src/native/shared_host.c']
    sources += [ROOT / 'src/runtime' / name for name in
                ('shared_views.c', 'owned_arena.c', 'mmio_views.c', 'guest_arena_service.c')]
    exe = tmp_path / 'arm64-mmio'
    subprocess.run([compiler, '-static', '-std=c11', '-D_GNU_SOURCE', '-O2', '-g',
                    '-Wall', '-Wextra', '-Werror', '-I' + str(tmp_path),
                    '-I' + str(ROOT / 'src/native'), '-I' + str(ROOT / 'src/runtime'),
                    *map(str, sources), '-o', str(exe)], check=True, capture_output=True, text=True)
    result = subprocess.run(([emulator] if not native else []) + [str(exe)],
                            capture_output=True, text=True, timeout=45)
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'PASS: ARM64 decoder, protected register writes' in result.stdout
