"""Admission policy blocks service startup on detection/identity/error regressions."""
import importlib.util
import os
from pathlib import Path

import pytest

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('installed_scanner_admission',ROOT/'tools/mods/hosting/accept-installed-scanner-mac.py')
admission=importlib.util.module_from_spec(spec);spec.loader.exec_module(admission)
from mod_package import ModError


@pytest.mark.skipif(os.name!='posix',reason='Private POSIX installed scanner fixture')
@pytest.mark.parametrize('fault',[None,'clean','blocked','empty_database','missing_engine','isolation','version'])
def test_admission_requires_all_positive_negative_and_failure_controls(tmp_path,monkeypatch,fault):
    tmp_path.chmod(0o700);(tmp_path/'data/scan/database').mkdir(parents=True,mode=0o700)
    (tmp_path/'data').chmod(0o700)
    calls=[]
    class Engine:
        def __init__(self,executable,database):self.executable,self.database=executable,database
        def scan(self,folder):
            name='missing_engine' if self.executable.endswith('/missing-engine') else 'empty_database' if self.database.name=='empty-database' else folder.name
            calls.append(name)
            if name=='blocked':assert (folder/'antivirus-test.txt').read_bytes().endswith(b'TEST-FILE!$H+H*')
            status={'clean':'clean','blocked':'blocked','empty_database':'error','missing_engine':'error'}[name]
            return dict(status='clean' if fault==name and status!='clean' else 'error' if fault==name else status,
                        isolation='disabled' if fault=='isolation' else 'macos-seatbelt-v3',engine='ClamAV',
                        version='other engine' if fault=='version' else 'ClamAV fixture')
    monkeypatch.setattr(admission,'ClamScanner',Engine)
    release=tmp_path/'releases'/('a'*64)
    if fault is None:
        result=admission.accept(release,tmp_path)
        assert result['phase']=='scanner-admitted' and result['temporary_state_removed'] is True
        assert result['services_started'] is result['network_started'] is False
    else:
        with pytest.raises(ModError):admission.accept(release,tmp_path)
    assert calls==['clean','blocked','empty_database','missing_engine']
    assert {p.name for p in (tmp_path/'data').iterdir()}=={'scan'}


def test_admission_refuses_uninstalled_context_before_scanner_or_private_files(monkeypatch):
    monkeypatch.setattr(admission,'initialize',lambda:(_ for _ in ()).throw(ModError('not installed')))
    monkeypatch.setattr(admission,'accept',lambda *_:pytest.fail('scanner ran without installed credential gate'))
    assert admission.main()==2
