import importlib.util
import pytest
from pathlib import Path

spec = importlib.util.spec_from_file_location('network_identity', Path(__file__).resolve().parents[1] / 'tools/network-identity.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def test_identity_is_relocatable_and_rejects_gameplay_changes(tmp_path):
    roots = [tmp_path / 'a', tmp_path / 'b']
    for root in roots:
        (root / 'src').mkdir(parents=True)
        (root / 'patches').mkdir()
        (root / 'private/game').mkdir(parents=True)
        (root / 'src/example.c').write_bytes(b'int version = 1;\n')
        (root / 'private/game/default.xbe').write_bytes(b'executable')
        (root / 'private/game/map.pak').write_bytes(b'map')
    def digest(root):
        return module.identity(root, root / 'private/game')[0]
    original = digest(roots[0])
    manifest = dict(protocol=module.PROTOCOL, files=module.identity(roots[0], roots[0] / 'private/game')[1])
    module.verify_content(roots[0] / 'private/game', manifest)
    (roots[1] / 'src/example.c').write_bytes(b'int version = 1;\r\n')
    assert digest(roots[1]) == original
    (roots[0] / 'private/game/default_analysis.json').write_text('diagnostic metadata')
    assert digest(roots[0]) == original
    (roots[0] / 'private/game/map.pak').write_bytes(b'modded map')
    assert digest(roots[0]) != original
    with pytest.raises(ValueError, match='content differs'):
        module.verify_content(roots[0] / 'private/game', manifest)
    (roots[1] / 'src/example.c').write_bytes(b'int version = 2;\n')
    assert digest(roots[1]) != original


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
    companions = tmp_path / 'tools'
    companions.mkdir()
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
