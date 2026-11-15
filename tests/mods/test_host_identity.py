"""Kernel credential inspection must not confuse directory memberships with groups."""
from pathlib import Path
import sys
from types import SimpleNamespace

import pytest

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools/mods'))
import mod_host_identity as identity
from mod_package import ModError


@pytest.mark.parametrize('values',[[],[600],[600,12,80]])
def test_bounded_native_group_array(values):
    calls=[]
    def native(count,out):
        calls.append(count)
        if out is None:return len(values)
        for index,value in enumerate(values):out[index]=value
        return len(values)
    assert identity._read_groups(native)==values
    assert calls==[0,len(values)]


@pytest.mark.parametrize('count,actual',[(-1,0),(257,0),(1,-1),(1,2)])
def test_native_failure_and_size_change_fail_closed(count,actual):
    def native(size,out):return count if out is None else actual
    with pytest.raises(ModError):identity._read_groups(native)


def credentials(monkeypatch,uid=600,real=600,gid=600,effective_gid=600,kernel=(),directory=(600,12,61,702,701,100)):
    monkeypatch.setattr(identity,'os',SimpleNamespace(geteuid=lambda:uid,getuid=lambda:real,getgid=lambda:gid,
                                                    getegid=lambda:effective_gid,getgroups=lambda:list(directory)))
    monkeypatch.setattr(identity,'kernel_groups',lambda:list(kernel))


def test_directory_memberships_are_separate_from_cleared_kernel_groups(monkeypatch):
    credentials(monkeypatch,kernel=[600])
    assert identity.require_locked_process()==600
    credentials(monkeypatch,kernel=[])
    assert identity.require_locked_process()==600


@pytest.mark.parametrize('changes',[dict(uid=0),dict(uid=501),dict(real=0),dict(gid=0),dict(effective_gid=0),
                                    dict(kernel=[0]),dict(kernel=[80]),dict(kernel=[600,12]),
                                    dict(directory=[600,80]),dict(directory=[600,0])])
def test_root_admin_stray_kernel_groups_and_partial_privilege_drops_refused(monkeypatch,changes):
    credentials(monkeypatch,**changes)
    with pytest.raises(ModError):identity.require_locked_process()
