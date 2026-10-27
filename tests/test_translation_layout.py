"""Original bounded-layout fixtures and production translation consumers."""
import importlib
import os
from pathlib import Path
import shutil
import subprocess
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import translation_layout as layout


@pytest.fixture(autouse=True)
def reset_layout():
    importlib.reload(layout)
    yield
    importlib.reload(layout)


def code():
    return layout.Section('.text', 0x2000, 0x20, 0x200, 0x20, True)


def test_unconfigured_queries_do_not_assume_another_games_layout():
    assert layout.configured_from() is None
    assert layout.SECTIONS == []
    for address in (0, 0x12000, 0x170000, 0xFFFFFFFF):
        assert not layout.is_code_address(address)
        assert not layout.is_data_address(address)
        assert layout.va_to_file_offset(address) is None


def test_file_backing_and_virtual_classification_have_distinct_bounds():
    data = layout.Section('.data', 0x3000, 0x30, 0x400, 0x10, False)
    layout._install([data, code()], 0x2000, 0x3004, 'synthetic')
    assert layout.SECTIONS[0][1] == 0x2000
    assert layout.va_to_file_offset(0x2000) == 0x200
    assert layout.va_to_file_offset(0x201F) == 0x21F
    assert layout.va_to_file_offset(0x2020) is None
    assert layout.va_to_file_offset(0x300F) == 0x40F
    assert layout.is_data_address(0x3010) and layout.va_to_file_offset(0x3010) is None
    assert not layout.is_data_address(0x3030)
    assert layout.RDATA_VA_START == layout.DATA_VA_START == 0x3000


def test_reconfiguration_clears_missing_ranges_and_zero_addresses():
    layout._install([code(), layout.Section('.data', 0x3000, 8, 0x400, 8, False)], 0x2000, 0x3000, 'first')
    layout._install([code()], 0, 0, 'second')
    assert layout.configured_from() == 'second'
    assert layout.ENTRY_POINT == layout.KERNEL_THUNK_ADDR == 0
    assert layout.DATA_VA_START == layout.DATA_VA_END == layout.RDATA_VA_START == layout.RDATA_VA_END == 0


@pytest.mark.parametrize('sections', [[],
    [layout.Section('bad', -1, 8, 0, 8, True)],
    [layout.Section('bad', 0xFFFFFFFC, 8, 0, 8, True)],
    [layout.Section('bad', 0x1000, 8, -1, 8, True)],
    [layout.Section('bad', 0x1000, 8, 0xFFFFFFFC, 8, True)],
    [layout.Section('bad', 0x2008, 8, 0, 8, True), layout.Section('other', 0x2000, 16, 16, 16, True)]])
def test_failed_install_preserves_the_previous_layout(sections):
    layout._install([code()], 0x2000, 0, 'valid')
    with pytest.raises(ValueError):
        layout._install(sections, 0, 0, 'invalid')
    assert layout.configured_from() == 'valid' and layout.ENTRY_POINT == 0x2000
    assert layout.va_to_file_offset(0x2001) == 0x201


def test_reader_and_explicit_title_precedence(tmp_path):
    from test_xbe_image import fixture
    path = tmp_path / 'title.xbe'
    path.write_bytes(fixture())
    layout.configure_from_xbe(path)
    assert layout.TEXT_VA_START == layout.ENTRY_POINT == 0x11000
    assert layout.KERNEL_THUNK_ADDR == 0x11080
    assert layout.va_to_file_offset(0x11010) == 0x1010
    assert layout.banner_name('Header title') == 'Header title'
    layout.set_game_name('Chosen title')
    assert layout.banner_name('Header title') == 'Chosen title'
    assert layout._classify('XIPS', 0)
    assert not layout._classify('EnglishXlate', 4)
    assert not layout._classify('.data', 4)


def test_failed_reader_keeps_prior_layout(tmp_path):
    layout._install([code()], 0x2000, 0, 'prior')
    path = tmp_path / 'invalid.xbe'
    path.write_bytes(b'not an executable')
    with pytest.raises(ValueError):
        layout.configure_from_xbe(path)
    assert layout.configured_from() == 'prior'
    assert layout.is_code_address(0x2000)


def test_data_only_layout_has_no_invented_text_range():
    layout._install([layout.Section('.data', 0x5000, 8, 0x200, 8, False)], 0, 0, 'data')
    assert layout.TEXT_VA_START == layout.TEXT_VA_END == 0
    assert layout.is_data_address(0x5000) and not layout.is_code_address(0x5000)
    for address in (True, -1, 2**32, '0x5000'):
        assert layout.va_to_file_offset(address) is None
        assert not layout.is_data_address(address)


def test_copied_layout_and_reader_run_without_the_toolkit(tmp_path):
    from test_xbe_image import fixture
    own = tmp_path / 'own'
    own.mkdir()
    root = Path(__file__).resolve().parents[1]
    for name in ('translation_layout.py', 'xbe_image.py', 'xbe_kernel_exports.def'):
        shutil.copyfile(root / 'tools' / name, own / name)
    path = tmp_path / 'sample.xbe'
    path.write_bytes(fixture())
    script = '''import sys
sys.path.insert(0,sys.argv[1])
import translation_layout as layout
layout.configure_from_xbe(sys.argv[2])
assert layout.va_to_file_offset(0x11010)==0x1010
assert layout.ENTRY_POINT==0x11000
assert 'tools.recomp.config' not in sys.modules
'''
    result = subprocess.run([sys.executable, '-I', '-c', script, str(own), str(path)],
                            cwd=tmp_path, capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr


def test_production_modules_share_mutable_configuration_and_run_consumers():
    root = Path(__file__).resolve().parents[1]
    runtime = root / 'external/ts3FPrecomp'
    if not (runtime / 'tools/recomp/config.py').is_file():
        pytest.skip('Pinned translator consumer unavailable')
    code = '''from tools.recomp import config, hle
import translation_layout, translation_hooks
assert config is translation_layout and hle is translation_hooks
saved=config._SECTIONS
config._SECTIONS=(config.Section('sample',0x5000,4,0x200,4,True),)
assert config.is_code_address(0x5000) and config.va_to_file_offset(0x5000)==0x200
config._SECTIONS=saved
assert not config.is_code_address(0x5000)
import pytest
raise SystemExit(pytest.main(['tools/recomp','-q','--tb=short']))
'''
    result = subprocess.run([sys.executable, '-c', code], cwd=runtime,
                            env=dict(os.environ, TSFP_PROJECT_ROOT=str(root)),
                            capture_output=True, text=True, timeout=300)
    assert result.returncode == 0, result.stdout + result.stderr
