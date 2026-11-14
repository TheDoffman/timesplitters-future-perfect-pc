"""Blocked native preflight stays off simulation; approvals bind only fresh epochs."""
import subprocess

import pytest
from test_mod_match import image_pins
from test_script_process import process_tools


@pytest.mark.parametrize('scenario',['fresh','close-pending','stale-preflight','old-epoch','probe-fail','probe-throw','callback-fail','roster','network','authority'])
def test_background_preflight_is_bounded_and_stale_approvals_never_activate(process_tools,tmp_path,scenario):
    worker=process_tools['worker'];driver=worker.with_name('tsfp-mod-runtime-driver'+worker.suffix)
    p=subprocess.run([str(driver),str(worker),str(tmp_path),scenario,*image_pins(worker)],capture_output=True,timeout=12)
    assert p.returncode==0 and not p.stderr and b'PASS' in p.stdout,(p.returncode,p.stdout,p.stderr)
    assert not list(tmp_path.iterdir()),'Host keeps resources until probe and contained worker stop'
