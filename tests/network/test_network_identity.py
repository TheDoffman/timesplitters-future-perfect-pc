

import importlib.util
import pytest
from pathlib import Path
import os
import shutil
import subprocess
import sys


def test_portable_probe_preserves_interpreter_search_path():
    """The bundled runner must not bootstrap development-tool directories."""
    runner = Path(__file__).with_name('portable_online.py')
    check = (
        "import runpy, sys; before=list(sys.path); "
        "runpy.run_path(sys.argv[1], run_name='portable_probe_fixture'); "
        "assert sys.path==before, (before,sys.path)"
    )
    result = subprocess.run([sys.executable, '-I', '-c', check, str(runner)],
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr

spec = importlib.util.spec_from_file_location('network_identity', Path(__file__).resolve().parents[2] / 'tools/build/network-identity.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def test_explicit_same_host_runtime_is_part_of_identity(tmp_path):
    root = tmp_path/'source'; root.mkdir()
    game = tmp_path/'game'; game.mkdir(); (game/'default.xbe').write_bytes(b'fixture')
    runtime = tmp_path/'cache/runtime'; (runtime/'src').mkdir(parents=True)
    source = runtime/'src/network.c'; source.write_text('int version=1;\n')
    before, entries = module.identity(root, game, runtime)
    assert any(name == 'runtime/src/network.c' for name, _ in entries)
    assert before != module.identity(root, game)[0]
    source.write_text('int version=2;\n')
    assert before != module.identity(root, game, runtime)[0]


def test_online_theme_is_bundled_and_part_of_runtime_identity(tmp_path):
    bundle_spec = importlib.util.spec_from_file_location('bundle_online', Path(__file__).resolve().parents[2]/'tools/network/bundle_online.py')
    bundle = importlib.util.module_from_spec(bundle_spec)
    bundle_spec.loader.exec_module(bundle)
    assert 'online_theme.py' in bundle.COMPANION
    game=tmp_path/'private/game';game.mkdir(parents=True)
    (game/'default.xbe').write_bytes(b'fixture')
    (tmp_path/'tools/network').mkdir(parents=True)
    theme=tmp_path/'tools/network/online_theme.py';theme.write_text('dark = False\n')
    before, entries=module.identity(tmp_path,game)
    # Manifest identities stay stable while repository source paths are grouped.
    assert any(name=='tools/online_theme.py' for name,_ in entries)
    theme.write_text('dark = True\n')
    assert module.identity(tmp_path,game)[0]!=before


def test_identity_is_relocatable_and_rejects_gameplay_changes(tmp_path):
    roots = [tmp_path / 'a', tmp_path / 'b']
    for root in roots:
        (root / 'src').mkdir(parents=True)
        (root / 'patches').mkdir()
        (root / 'private/game').mkdir(parents=True)
        (root / 'src/example.c').write_bytes(b'int version = 1;\n')
        (root / 'private/game/default.xbe').write_bytes(b'executable')
        (root / 'private/game/pak').mkdir()
        (root / 'private/game/pak/map.pak').write_bytes(b'map')
    def digest(root):
        return module.identity(root, root / 'private/game')[0]
    original = digest(roots[0])
    manifest = dict(protocol=module.PROTOCOL, files=module.identity(roots[0], roots[0] / 'private/game')[1])
    module.verify_content(roots[0] / 'private/game', manifest)
    (roots[1] / 'src/example.c').write_bytes(b'int version = 1;\r\n')
    assert digest(roots[1]) == original
    (roots[0] / 'private/game/default_analysis.json').write_text('diagnostic metadata')
    assert digest(roots[0]) == original
    (roots[0] / 'private/game/pak/map.pak').write_bytes(b'modded map')
    assert digest(roots[0]) != original
    with pytest.raises(ValueError, match='content differs'):
        module.verify_content(roots[0] / 'private/game', manifest)
    (roots[1] / 'src/example.c').write_bytes(b'int version = 2;\n')
    assert digest(roots[1]) != original


def test_import_layout_ignores_updaters_and_runtime_cache_but_checks_assets(tmp_path):
    game = tmp_path/'game'
    (game/'PAK').mkdir(parents=True)
    (game/'xmv').mkdir()
    (game/'default.xbe').write_bytes(b'executable')
    (game/'PAK/map.pak').write_bytes(b'map')
    (game/'xmv/intro.xmv').write_bytes(b'movie')
    (game/'dashupdate.xbe').write_bytes(b'Xbox dashboard updater')
    (game/'update.xbe').write_bytes(b'Xbox updater')
    (game/'CACHE').mkdir()
    cache = game/'CACHE/localcache00.bin'
    cache.write_bytes(b'old cache')
    before = module.identity(tmp_path, game)[0]
    entries = module.game_entries(game)
    assert len(entries) == 3
    # A folder import omits the updater/cache files; launching creates cache.
    cache.write_bytes(b'new runtime cache')
    (game/'dashupdate.xbe').unlink()
    (game/'update.xbe').unlink()
    assert module.identity(tmp_path, game)[0] == before
    manifest = dict(protocol=module.PROTOCOL, files=list(reversed(entries)))
    module.verify_content(game, manifest)  # Enumeration order is irrelevant.
    (game/'PAK/map.pak').write_bytes(b'modded map')
    with pytest.raises(ValueError, match='Changed: pak/map.pak'):
        module.verify_content(game, manifest)
    (game/'xmv/intro.xmv').unlink()
    with pytest.raises(ValueError, match='Missing: xmv/intro.xmv'):
        module.verify_content(game, manifest)


@pytest.mark.skipif(os.name != 'nt', reason='Windows release importer')
def test_actual_folder_import_is_accepted_without_copying_full_game(tmp_path):
    root = Path(__file__).resolve().parents[2]
    executable = root/'private/game/default.xbe'
    if not executable.is_file():
        pytest.skip('Requires the supported extracted executable')
    source, imported = tmp_path/'source', tmp_path/'Data'
    (source/'pak').mkdir(parents=True)
    (source/'xmv').mkdir()
    shutil.copyfile(executable, source/'default.xbe')
    (source/'pak/test.pak').write_bytes(b'fixture asset')
    (source/'xmv/test.xmv').write_bytes(b'fixture movie')
    (source/'update.xbe').write_bytes(b'not imported')
    (source/'CACHE').mkdir()
    (source/'CACHE/localcache00.bin').write_bytes(b'not imported')
    manifest = dict(protocol=module.PROTOCOL, files=module.game_entries(source))
    def quote(path):
        return "'"+str(path).replace("'", "''")+"'"
    result = subprocess.run(['powershell.exe', '-NoProfile', '-Command',
        "$ErrorActionPreference='Stop'; Add-Type -Path "+quote(root/'tools/release/ImportGame.cs')+
        '; [TsfpImport]::ImportFolder('+quote(source)+','+quote(imported)+')'],
        capture_output=True, text=True)
    assert result.returncode == 0, result.stdout+result.stderr
    assert not (imported/'CACHE').exists() and not (imported/'update.xbe').exists()
    module.verify_content(imported, manifest)
    (imported/'CACHE').mkdir()
    (imported/'CACHE/localcache00.bin').write_bytes(b'created after game startup')
    module.verify_content(imported, manifest)


def test_runtime_edits_change_identity(tmp_path):
    game = tmp_path / 'private/game'
    game.mkdir(parents=True)
    (game / 'default.xbe').write_bytes(b'xbe')
    runtime = tmp_path / 'external/ts3FPrecomp/src'
    runtime.mkdir(parents=True)
    source = runtime / 'kernel.c'
    source.write_text('int value = 1;\n')
    before = module.identity(tmp_path, game)[0]
    source.write_text('int value = 2;\n')
    assert module.identity(tmp_path, game)[0] != before
    companions = tmp_path / 'tools/network'
    companions.mkdir(parents=True)
    bridge = companions / 'online_transport.py'
    bridge.write_text('protocol = 1\n')
    before = module.identity(tmp_path, game)[0]
    bridge.write_text('protocol = 2\n')
    assert module.identity(tmp_path, game)[0] != before


def test_incomplete_or_replaced_build_is_not_accepted(tmp_path):
    executable = tmp_path / 'game.exe'
    executable.write_bytes(b'completed executable')
    manifest = {'compatibility': 'a' * 64}
    stamp = dict(compatibility=manifest['compatibility'], executable_sha256=module.executable_hash(executable))
    module.verify_build(executable, manifest, stamp)
    with pytest.raises(ValueError, match='completed multiplayer build'):
        module.verify_build(executable, {'compatibility': 'b' * 64}, stamp)
    executable.write_bytes(b'replaced executable')
    with pytest.raises(ValueError, match='completed multiplayer build'):
        module.verify_build(executable, manifest, stamp)
