"""Creator linking/ZIP contracts; real VM execution is in test_script_worker.py."""
import json
from pathlib import Path
import subprocess
import sys
import zipfile

import pytest

from mod_lua_bundle import compose, MAX_SCRIPTS
from mod_package import ModError, MAX_SCRIPT, build_package, inspect_package

ROOT = Path(__file__).resolve().parents[2]
EXAMPLE = ROOT / 'examples/mods/modular-first-to-three'


def test_original_module_project_links_without_changing_creator_files(tmp_path):
    before = {p.relative_to(EXAMPLE).as_posix():p.read_bytes() for p in EXAMPLE.rglob('*') if p.is_file()}
    first, second = tmp_path/'one.zip', tmp_path/'two.zip'
    manifest = build_package(EXAMPLE, first, bundle_scripts=True)
    build_package(EXAMPLE, second, bundle_scripts=True)
    assert first.read_bytes() == second.read_bytes()
    assert set(manifest['files']) == {'scripts/main.lua'}
    assert inspect_package(first) == manifest
    with zipfile.ZipFile(first) as archive:
        source = archive.read('scripts/main.lua')
        assert b'__tsfp_loaders["rules.scores"]' in source and b'__tsfp_loaders["rules.settings"]' in source
        assert str(EXAMPLE).encode() not in source
    assert before == {p.relative_to(EXAMPLE).as_posix():p.read_bytes() for p in EXAMPLE.rglob('*') if p.is_file()}


def test_cli_modules_and_legacy_single_source_bytes(tmp_path):
    assert compose({'scripts/main.lua':b'return {api=1}\n'}) == b'return {api=1}\n'
    target = tmp_path/'cli.zip'
    result = subprocess.run([sys.executable,str(ROOT/'tools/mods/mod-workshop.py'),'pack',str(EXAMPLE),str(target),'--modules'],capture_output=True,text=True)
    assert result.returncode == 0, result.stderr
    assert set(json.loads(result.stdout)['files']) == {'scripts/main.lua'}
    ordinary = tmp_path/'ordinary.zip'
    manifest = build_package(EXAMPLE, ordinary)
    assert len(manifest['files']) == 3  # No implicit transformation without the flag.


def test_module_order_is_ordinal_and_insertion_independent():
    sources = {'scripts/main.lua':b'return {api=1}', 'scripts/z.lua':b'return 1','scripts/A.lua':b'return 2'}
    first = compose(sources)
    assert first == compose(dict(reversed(list(sources.items()))))
    assert first.index(b'__tsfp_loaders["A"]') < first.index(b'__tsfp_loaders["z"]')


def test_package_payload_order_is_ordinal_on_every_host(tmp_path):
    source = tmp_path/'project'
    (source/'scripts').mkdir(parents=True)
    (source/'mod.json').write_bytes((EXAMPLE/'mod.json').read_bytes())
    for name in ('main.lua','a.lua','B.lua'):
        (source/'scripts'/name).write_bytes(b'return {api=1}\n')
    target = tmp_path/'ordered.zip'
    build_package(source,target)
    with zipfile.ZipFile(target) as archive:
        assert archive.namelist() == ['mod.json','scripts/B.lua','scripts/a.lua','scripts/main.lua']


@pytest.mark.parametrize('extra', [
    {'scripts/rules.lua':b''}, {'scripts/rules.lua':b'\xff'},
    {'scripts/rules.lua':b'\x1bLua'}, {'scripts/rules.lua':b'return "\0"'},
    {'scripts/x.y.lua':b'return 1','scripts/x/y.lua':b'return 2'},
    {'scripts/a..b.lua':b'return 1'}, {'scripts/../escape.lua':b'return 1'},
    {'scripts/A.lua':b'return 1','scripts/a.lua':b'return 2'},
    {'scripts/a.lua':b'return 1','scripts/a.lua/b.lua':b'return 2'},
    {'../host.lua':b'return 1'},
])
def test_invalid_module_projects_are_refused(extra):
    with pytest.raises(ModError):
        compose({'scripts/main.lua':b'return {api=1}', **extra})


def test_linked_source_and_file_count_keep_existing_worker_limits():
    with pytest.raises(ModError):
        compose({'scripts/main.lua':b' '*MAX_SCRIPT,'scripts/rules.lua':b'return true'})
    with pytest.raises(ModError):
        compose({'scripts/main.lua':b'return {api=1}', **{f'scripts/m{i}.lua':b'return true' for i in range(MAX_SCRIPTS)}})
    with pytest.raises(ModError):
        compose({'scripts/rules.lua':b'return true'})
