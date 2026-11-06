"""Exercise optional texture hydration with real Git LFS and tiny local files."""
import os
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[2]


def test_clone_skips_textures_and_explicit_pull_restores_them(tmp_path):
    if not shutil.which('git') or subprocess.run(
        ['git', 'lfs', 'version'], capture_output=True).returncode:
        pytest.skip('Git LFS is required for the actual clone check')
    env = {k: v for k, v in os.environ.items()
           if not k.startswith(('GIT_', 'GIT_CONFIG_'))}
    env.update(GIT_CONFIG_NOSYSTEM='1', GIT_CONFIG_GLOBAL=str(tmp_path/'empty-config'))
    (tmp_path/'empty-config').write_text('')

    def git(cwd, *args):
        return subprocess.run(['git', *args], cwd=cwd, env=env,
                              check=True, capture_output=True, text=True).stdout

    source = tmp_path/'source'
    source.mkdir()
    git(source, 'init')
    git(source, 'config', 'user.name', 'Fixture')
    git(source, 'config', 'user.email', 'fixture@example.invalid')
    git(source, 'lfs', 'install', '--local')
    shutil.copyfile(ROOT/'.lfsconfig', source/'.lfsconfig')
    (source/'.gitattributes').write_text('assets/textures/*.dds filter=lfs diff=lfs merge=lfs -text\n')
    texture = source/'assets/textures/sample.dds'
    texture.parent.mkdir(parents=True)
    payload = b'DDS ' + bytes(range(256))*8
    texture.write_bytes(payload)
    (source/'source.txt').write_text('ordinary source')
    git(source, 'add', '.')
    git(source, 'commit', '-m', 'fixture')
    clone = tmp_path/'clone'
    # Install the standard filter for this one command, without skip-smudge.
    git(tmp_path, '-c', 'filter.lfs.process=git-lfs filter-process',
        '-c', 'filter.lfs.required=true', 'clone', source.as_uri(), str(clone))
    copied = clone/'assets/textures/sample.dds'
    assert copied.read_bytes().startswith(b'version https://git-lfs.github.com/spec/v1\n')
    assert not [p for p in (clone/'.git/lfs/objects').rglob('*') if p.is_file()]
    assert (clone/'source.txt').read_text() == 'ordinary source'
    opted_in = tmp_path/'opted-in'
    git(tmp_path, '-c', 'filter.lfs.process=git-lfs filter-process',
        '-c', 'filter.lfs.required=true', '-c', 'lfs.fetchexclude=',
        'clone', source.as_uri(), str(opted_in))
    assert (opted_in/'assets/textures/sample.dds').read_bytes() == payload
    git(clone, 'lfs', 'install', '--local')
    git(clone, 'lfs', 'pull', '--include=assets/textures/**', '--exclude=')
    assert copied.read_bytes() == payload
    # Ordinary fetch/pull does not remove a pack already hydrated by choice.
    git(clone, 'lfs', 'pull')
    assert copied.read_bytes() == payload
