"""Original synthetic XBE fixtures and optional private US-image acceptance."""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from xbe_image import XBEParser, XBEError, analysis, KEYS, kernel_names


def fixture(kind='retail'):
    data = bytearray(0x1200)
    data[:4] = b'XBEH'
    def words(offset, *values):
        struct.pack_into('<' + 'I' * len(values), data, offset, *values)
    words(0x104, 0x10000, 0x1000, 0x2000, 0x178, 0, 0x10200, 1, 0x10400, 0,
          0x11000 ^ KEYS[kind][0], 0x10500)
    words(0x158, 0x11080 ^ KEYS[kind][1], 0, 1, 0x10540)
    words(0x200, 0x1D0, 0, 0x12345678)
    data[0x20C:0x218] = 'Sample'.encode('utf-16-le')
    words(0x400, 7, 0x11000, 0x300, 0x1000, 0x200, 0x10480)
    data[0x480:0x486] = b'.text\0'
    words(0x500, 0x11100, 0x11108, 0x11200, 0, 8, 0)
    struct.pack_into('<8s4H', data, 0x540, b'XAPILIB\0', 1, 0, 5849, 0)
    words(0x1080, 0x80000008, 0x80000010, 0)
    return data


def parse(tmp_path, data):
    path = tmp_path / 'sample.xbe'
    path.write_bytes(data)
    return XBEParser(path)


@pytest.mark.parametrize('kind', KEYS)
def test_layout_imports_tls_and_json(tmp_path, kind):
    parser = parse(tmp_path, fixture(kind))
    xbe = parser.parse()
    assert xbe.header.entry_point == 0x11000
    assert xbe.header.is_debug == (kind == 'debug')
    assert xbe.certificate.title_name == 'Sample'
    assert [(i.ordinal, i.name, i.thunk_addr) for i in xbe.kernel_imports] == [
        (8, 'DbgPrint', 0x11080), (16, 'ExEventObjectType', 0x11084)]
    assert xbe.tls.raw_data_end - xbe.tls.raw_data_start == 8
    assert parser.va_to_offset(0x11005) == 0x1005
    assert analysis(xbe)['tls'] == {'index_addr': '0x00011200', 'zero_fill_size': 8}
    assert not hasattr(xbe.certificate, 'signature_key')
    with pytest.raises(XBEError, match='Unmapped'):
        parser.at(0x11200, 4)  # Virtual zero-fill must not read the next file bytes.
    assert parser.parse().sections == xbe.sections  # Reader reuse is deterministic.


@pytest.mark.parametrize('offset,value', [
    (0, 0), (0x104, 0xFFFFF000), (0x108, 0x10000000), (0x110, 1),
    (0x118, 0x1FFFF), (0x11C, 0), (0x11C, 4097), (0x120, 0x10FF0),
    (0x128, 0), (0x158, 0), (0x160, 4097), (0x164, 0x11FF0),
    (0x200, 0x10), (0x400, 0), (0x404, 0x10000), (0x408, 0x2000),
    (0x40C, 0x100), (0x410, 0x100000), (0x414, 0x12000),
    (0x500, 0x11200), (0x504, 0x11000), (0x1080, 8), (0x1080, 0x80000000),
])
def test_rejects_malformed_extents_and_metadata(tmp_path, offset, value):
    data = fixture()
    struct.pack_into('<I', data, offset, value)
    with pytest.raises((XBEError, UnicodeError)):
        parse(tmp_path, data).parse()


@pytest.mark.parametrize('size', [0, 3, 0x104, 0x177, 0xFFF, 0x11FF])
def test_truncation(tmp_path, size):
    with pytest.raises(XBEError):
        parse(tmp_path, fixture()[:size]).parse()


def test_unterminated_imports_and_strings(tmp_path):
    data = fixture()
    data[0x1080:] = struct.pack('<I', 0x80000008) * ((len(data) - 0x1080) // 4)
    with pytest.raises(XBEError):
        parse(tmp_path, data).parse()
    data = fixture()
    struct.pack_into('<I', data, 0x414, 0x111FE)
    data[-2:] = b'xx'
    with pytest.raises(XBEError):
        parse(tmp_path, data).parse()


@pytest.mark.parametrize('raw,va', [(0x1000, 0x11400), (0x1100, 0x11100)])
def test_rejects_overlapping_sections(tmp_path, raw, va):
    data = fixture()
    struct.pack_into('<I', data, 0x11C, 2)
    struct.pack_into('<6I', data, 0x438, 1, va, 0x100, raw, 0x100, 0x10480)
    with pytest.raises(XBEError, match='Overlapping'):
        parse(tmp_path, data).parse()


def test_zero_optional_directories(tmp_path):
    data = fixture()
    for offset in (0x12C, 0x160, 0x164):
        struct.pack_into('<I', data, offset, 0)
    struct.pack_into('<I', data, 0x158, KEYS['retail'][1])
    xbe = parse(tmp_path, data).parse()
    assert not xbe.libraries and not xbe.kernel_imports and xbe.tls is None


def test_export_data_integrity():
    data = (ROOT / 'tools/xbe_kernel_exports.def').read_bytes()
    assert b'SPDX-License-Identifier: CC0-1.0' in data
    assert hashlib.sha256(data).hexdigest() == '8aa3e48417c8a60aea8d1953bcc0674cdee95ce9a11e5e861d3107c3339d2527'
    assert kernel_names()[255] == 'PsCreateSystemThreadEx'
    assert len(kernel_names()) >= 366


def test_cli_without_toolkit(tmp_path):
    source = parse(tmp_path, fixture()).path
    output = tmp_path / 'analysis.json'
    result = subprocess.run([sys.executable, '-I', str(ROOT / 'tools/xbe_image.py'),
                             str(source), '--json', str(output)], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    assert json.loads(output.read_text())['title_id'] == '0x12345678'


def test_patched_pipeline_imports_project_reader(tmp_path):
    # Apply the actual patch to pristine pinned files; no installed toolkit needed.
    runtime = ROOT / 'external/ts3FPrecomp'
    if not (runtime / '.git').exists():
        pytest.skip('Pinned runtime checkout unavailable')
    patch = ROOT / 'patches/0210-independent-xbe-reader.patch'
    targets = [line[6:] for line in patch.read_text().splitlines() if line.startswith('+++ b/')]
    for name in targets:
        content = subprocess.check_output(['git', '-C', str(runtime), 'show', 'HEAD:' + name])
        path = tmp_path / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
    subprocess.run(['git', 'init', '-q', str(tmp_path)], check=True)
    subprocess.run(['git', '-C', str(tmp_path), 'apply', str(patch)], check=True)
    env = dict(os.environ, TSFP_PROJECT_ROOT=str(ROOT))
    result = subprocess.run([sys.executable, '-c',
        "from tools.xbe_parser.xbe_parser import XBEParser; assert XBEParser.__module__ == 'xbe_image'"],
        cwd=tmp_path, env=env, capture_output=True, text=True)
    assert result.returncode == 0, result.stderr


def test_private_supported_us_metadata():
    path = ROOT / 'private/game/default.xbe'
    if not path.exists():
        pytest.skip('Private supported executable unavailable')
    assert hashlib.sha256(path.read_bytes()).hexdigest() == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    data = analysis(XBEParser(path).parse())
    assert data['title_id'] == '0x45410066'
    assert data['entry_point'] == '0x0038024D'
    assert data['kernel_thunk_addr'] == '0x00475780'
    assert len(data['sections']) == 23 and len(data['kernel_imports']) == 151
    saved = ROOT / 'private/game/default_analysis.json'
    if saved.exists():
        assert data == json.loads(saved.read_text())
