"""Synthetic format fixtures and public importer entry points, no toolkit import."""

from project_paths import source_file as repo_file

import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import disc_image as disc


def disc_bytes(xbe=b'unit-test executable'):
    # Fixed asymmetric directory trees, independent payload offsets. Left/right
    # pointers use DWORDs while extents use sectors; deliberately mix both.
    sector = 2048
    movie_sector = 44 + (len(xbe) + sector - 1) // sector
    data = bytearray((movie_sector + 2) * sector)
    data[65536:65556] = disc.MAGIC
    data[65536 + 2028:65536 + 2048] = disc.MAGIC
    struct.pack_into('<II', data, 65556, 40, sector)

    def entry(table, node, left, right, block, size, flags, name):
        offset = table * sector + node
        struct.pack_into('<HHIIBB', data, offset, left, right, block, size, flags, len(name))
        data[offset + 14:offset + 14 + len(name)] = name

    entry(40, 0, 8, 16, 41, sector, 0x10, b'pak')
    entry(40, 32, 0, 0xffff, 43, len(xbe), 0x20, b'default.xbe')
    entry(40, 64, 0, 24, 42, sector, 0x10, b'xmv')
    entry(40, 96, 0, 0, 0, 0, 0x20, b'z.empty')
    entry(41, 0, 0, 0, movie_sector - 1, 13, 0x20, b'asset.pak')
    entry(42, 0, 0, 0, movie_sector, 12, 0x20, b'intro.xmv')
    data[43 * sector:43 * sector + len(xbe)] = xbe
    data[(movie_sector - 1) * sector:(movie_sector - 1) * sector + 13] = b'asset payload'
    data[movie_sector * sector:movie_sector * sector + 12] = b'movie frames'
    return data


@pytest.fixture
def image_file(tmp_path):
    path = tmp_path / 'input.iso'
    path.write_bytes(disc_bytes())
    return path


def test_catalog_streamed_extraction_and_identical_reuse(image_file, tmp_path, monkeypatch):
    monkeypatch.setattr(disc, 'CHUNK', 7)
    output = tmp_path / 'game'
    before = image_file.read_bytes()
    with disc.DiscImage(image_file) as image:
        assert [e.path for e in image.entries] == [
            'default.xbe', 'pak', 'pak/asset.pak', 'xmv', 'xmv/intro.xmv', 'z.empty']
        result = disc.extract(image, output, verify_supported=False)
        assert result['copied'] == 4 and result['reused'] == 0
        assert (output / 'default.xbe').read_bytes() == b'unit-test executable'
        assert (output / 'pak/asset.pak').read_bytes() == b'asset payload'
        assert (output / 'xmv/intro.xmv').read_bytes() == b'movie frames'
        assert (output / 'z.empty').read_bytes() == b''
        (output / 'personal.txt').write_bytes(b'keep')
        result = disc.extract(image, output, verify_supported=False)
        assert result['copied'] == 0 and result['reused'] == 4
    assert image_file.read_bytes() == before
    assert (output / 'personal.txt').read_bytes() == b'keep'
    assert not list(output.rglob('.tsfp-import-*'))


def test_partition_relative_offsets(image_file, tmp_path, monkeypatch):
    prefix = 16 * 2048
    image_file.write_bytes(b'\0' * prefix + image_file.read_bytes())
    monkeypatch.setattr(disc, 'PARTITION_BASES', (0, prefix))
    with disc.DiscImage(image_file) as image:
        assert image.base == prefix
        disc.extract(image, tmp_path / 'game', verify_supported=False)
    assert (tmp_path / 'game/pak/asset.pak').read_bytes() == b'asset payload'


@pytest.mark.parametrize('name', [b'../x', b'a\\b', b'C:x', b'CON', b'nul.txt', b'a.', b'a ', b'a\0b', b'\xff'])
def test_unsafe_names_fail_before_writes(image_file, tmp_path, name):
    data = bytearray(image_file.read_bytes())
    data[40 * 2048 + 13] = len(name)
    data[40 * 2048 + 14:40 * 2048 + 14 + len(name)] = name
    image_file.write_bytes(data)
    with pytest.raises(disc.DiscError):
        with disc.DiscImage(image_file) as image:
            disc.extract(image, tmp_path / 'game', verify_supported=False)
    assert not (tmp_path / 'game').exists()


@pytest.mark.parametrize('fault', ['trailer', 'table', 'extent', 'node', 'cycle', 'directory-cycle',
                                  'duplicate', 'filename', 'table-limit', 'depth-limit', 'entry-limit', 'metadata-limit'])
def test_invalid_tables_are_bounded_and_rejected(image_file, fault, monkeypatch):
    data = bytearray(image_file.read_bytes())
    if fault == 'trailer': data[67564] ^= 1
    elif fault == 'table': struct.pack_into('<I', data, 65556, 0xffffffff)
    elif fault == 'extent': struct.pack_into('<I', data, 40 * 2048 + 32 + 8, 0xffffffff)
    elif fault == 'node': struct.pack_into('<H', data, 40 * 2048, 0xfffe)
    elif fault == 'cycle': struct.pack_into('<H', data, 40 * 2048 + 32, 8)
    elif fault == 'directory-cycle': struct.pack_into('<I', data, 40 * 2048 + 4, 40)
    elif fault == 'duplicate':
        data[40 * 2048 + 32 + 13] = 3
        data[40 * 2048 + 32 + 14:40 * 2048 + 32 + 17] = b'PAK'
    elif fault == 'filename':
        struct.pack_into('<H', data, 40 * 2048, 508)
        struct.pack_into('<HHIIBB', data, 40 * 2048 + 2032, 0, 0, 0, 0, 0, 20)
    elif fault == 'table-limit': monkeypatch.setattr(disc, 'MAX_TABLE', 1024)
    elif fault == 'depth-limit': monkeypatch.setattr(disc, 'MAX_DEPTH', 1)
    elif fault == 'entry-limit': monkeypatch.setattr(disc, 'MAX_ENTRIES', 2)
    elif fault == 'metadata-limit': monkeypatch.setattr(disc, 'MAX_METADATA', 4096)
    image_file.write_bytes(data)
    with pytest.raises(disc.DiscError):
        disc.DiscImage(image_file)


def test_wrong_game_rejected_before_output_creation(image_file, tmp_path):
    with disc.DiscImage(image_file) as image, pytest.raises(disc.DiscError, match='supported US'):
        disc.extract(image, tmp_path / 'game')
    assert not (tmp_path / 'game').exists()


def test_existing_conflict_is_detected_before_any_copy(image_file, tmp_path):
    output = tmp_path / 'game'
    output.mkdir()
    (output / 'z.empty').write_bytes(b'user data')
    with disc.DiscImage(image_file) as image, pytest.raises(disc.DiscError, match='Existing file differs'):
        disc.extract(image, output, verify_supported=False)
    assert list(output.iterdir()) == [output / 'z.empty']
    assert (output / 'z.empty').read_bytes() == b'user data'


def test_case_alias_is_rejected_on_all_platforms(image_file, tmp_path):
    output = tmp_path / 'game'
    (output / 'PAK').mkdir(parents=True)
    with disc.DiscImage(image_file) as image, pytest.raises(disc.DiscError, match='spelling'):
        disc.extract(image, output, verify_supported=False)
    assert not (output / 'default.xbe').exists()


def test_redirected_output_is_rejected(image_file, tmp_path):
    outside = tmp_path / 'outside'
    outside.mkdir()
    output = tmp_path / 'redirect'
    try:
        output.symlink_to(outside, target_is_directory=True)
    except OSError:
        pytest.skip('Host does not permit symlink creation')
    with disc.DiscImage(image_file) as image, pytest.raises(disc.DiscError, match='Redirected'):
        disc.extract(image, output, verify_supported=False)
    assert not list(outside.iterdir())


def test_disk_space_and_copy_failure_leave_existing_data(image_file, tmp_path, monkeypatch):
    output = tmp_path / 'game'
    with disc.DiscImage(image_file) as image:
        original = disc.shutil.disk_usage
        monkeypatch.setattr(disc.shutil, 'disk_usage', lambda _: shutil._ntuple_diskusage(1, 1, 0))
        with pytest.raises(disc.DiscError, match='free space'):
            disc.extract(image, output, verify_supported=False)
        assert not output.exists()
        monkeypatch.setattr(disc.shutil, 'disk_usage', original)
        read = image.read
        def fail(offset, size):
            if offset >= 43 * 2048:
                raise OSError('simulated read failure')
            return read(offset, size)
        monkeypatch.setattr(image, 'read', fail)
        with pytest.raises(OSError, match='simulated'):
            disc.extract(image, output, verify_supported=False)
        assert not (output / 'default.xbe').exists()
        assert not list(output.rglob('.tsfp-import-*'))
        monkeypatch.setattr(image, 'read', read)
        assert disc.extract(image, output, verify_supported=False)['copied'] == 4


def test_concurrent_destination_is_not_replaced(image_file, tmp_path, monkeypatch):
    output = tmp_path / 'game'
    link = disc.os.link
    def competing(source, target):
        target.write_bytes(b'concurrent user file')
        link(source, target)
    monkeypatch.setattr(disc.os, 'link', competing)
    with disc.DiscImage(image_file) as image, pytest.raises(FileExistsError):
        disc.extract(image, output, verify_supported=False)
    assert (output / 'default.xbe').read_bytes() == b'concurrent user file'
    assert not list(output.rglob('.tsfp-import-*'))


def test_read_only_cli_rejects_wrong_game_without_external_toolkit(image_file, tmp_path):
    script = tmp_path / 'tools'
    script.mkdir()
    for name in ('import-game.py', 'disc_image.py'):
        shutil.copy2(repo_file(ROOT, 'tools', name), script / name)
    result = subprocess.run([sys.executable, str(script / 'import-game.py'),
                             '--iso', str(image_file), '--verify-only'], cwd=tmp_path,
                            capture_output=True, text=True)
    assert result.returncode == 1 and 'supported US' in result.stderr
    assert not (tmp_path / 'external').exists()


def test_supported_private_xbe_can_be_imported_without_a_toolkit(tmp_path):
    source = ROOT / 'private/game/default.xbe'
    if not source.exists():
        pytest.skip('Supported private XBE is not present')
    data = source.read_bytes()
    assert len(data) == disc.SUPPORTED_XBE_SIZE
    assert hashlib.sha256(data).hexdigest() == disc.SUPPORTED_XBE_SHA256
    image_path = tmp_path / 'supported.iso'
    image_path.write_bytes(disc_bytes(data))
    with disc.DiscImage(image_path) as image:
        image.verify_supported()
        disc.extract(image, tmp_path / 'game')
    assert (tmp_path / 'game/default.xbe').read_bytes() == data


@pytest.mark.skipif(os.name != 'nt', reason='PowerShell driver is Windows only')
def test_powershell_extract_needs_no_external_checkout_or_venv(image_file, tmp_path):
    project = tmp_path / 'project'
    (project / 'tools/compiler').mkdir(parents=True)
    (project / 'tools/build').mkdir(parents=True)
    (project / 'config').mkdir()
    (project / 'config/pc-controls.default.json').write_text('{}')
    payload = b'unit-test executable'
    sha = hashlib.sha256(payload).hexdigest()
    driver = (ROOT / 'tsfp.ps1').read_text()
    assert disc.SUPPORTED_XBE_SHA256.upper() in driver
    (project / 'tsfp.ps1').write_text(driver.replace(disc.SUPPORTED_XBE_SHA256.upper(), sha.upper()))
    module = (ROOT / 'tools/compiler/disc_image.py').read_text().replace(disc.SUPPORTED_XBE_SHA256, sha)
    module = module.replace('SUPPORTED_XBE_SIZE = 6270976', f'SUPPORTED_XBE_SIZE = {len(payload)}')
    (project / 'tools/compiler/disc_image.py').write_text(module)
    shutil.copy2(ROOT / 'tools/build/import-game.py', project / 'tools/build/import-game.py')
    shutil.copy2(ROOT / 'tools/project_paths.py', project / 'tools/project_paths.py')
    env = dict(os.environ, PSModulePath=(str(Path(os.environ['SystemRoot']) / 'System32/WindowsPowerShell/v1.0/Modules')
        + ';' + str(Path(os.environ['ProgramFiles']) / 'WindowsPowerShell/Modules')))
    result = subprocess.run(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass',
        '-File', str(project / 'tsfp.ps1'), '-Action', 'Extract', '-Iso', str(image_file),
        '-Python', sys.executable], capture_output=True, text=True, env=env)
    assert result.returncode == 0, result.stdout + result.stderr
    assert not (project / 'external').exists()
    assert not (project / '.venv').exists()
    assert (project / 'private/game/default.xbe').read_bytes() == payload
    assert json.loads((project / 'private/disc.json').read_text(encoding='utf-8-sig'))['xbe_sha256'] == sha.upper()
