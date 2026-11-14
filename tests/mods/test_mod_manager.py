"""Simulation owner with real contained workers, transitions and reap barriers."""
import subprocess

import pytest
from test_mod_match import image_pins
from test_script_process import process_tools


@pytest.mark.parametrize('scenario',['policy','pin','leave','roster','authority','network','tick','new-epoch','in-flight','fault','finished','close'])
def test_match_owner_never_resurrects_stale_workers_and_drains_before_successor(process_tools,tmp_path,scenario):
    worker=process_tools['worker'];driver=worker.with_name('tsfp-mod-manager-driver'+worker.suffix)
    p=subprocess.run([str(driver),str(worker),str(tmp_path),scenario,*image_pins(worker)],capture_output=True,timeout=12)
    assert p.returncode==0 and p.stderr==b'' and b'PASS' in p.stdout,(p.returncode,p.stdout,p.stderr)
    assert not list(tmp_path.iterdir()),'All owned workers/images must be reaped before teardown'
