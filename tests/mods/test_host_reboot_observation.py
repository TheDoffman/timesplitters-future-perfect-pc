"""Real protected files, fake launchd/process samples; no host mutations."""
import copy
import os
from pathlib import Path
import plistlib
import sys
from types import SimpleNamespace

import pytest
from test_host_cutover import protected_jobs

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools/mods'))
import mod_host_cutover as cutover
import mod_host_reboot_observation as reboot
from mod_package import ModError


@pytest.fixture
def restarted(protected_jobs,monkeypatch):
    old=protected_jobs;new=copy.deepcopy(old);current=['f'*64];calls=[];loaded={};origin=['boot'];damage=[None]
    for index,(path,spec) in enumerate(new):
        if not spec['Label'].endswith('.backup'):spec['ProgramArguments'][0]='/candidate/python'
        selected=spec if index%2 else old[index][1]
        target=cutover.LAUNCHD/(spec['Label']+'.plist');target.write_bytes(plistlib.dumps(selected));target.chmod(0o644)
        loaded[spec['Label']]=dict(spec=selected,path=path,pid=500+index)
    monkeypatch.setattr(reboot,'boot_identity',lambda:current[0])
    def command(argv):
        calls.append(argv)
        if argv[0]=='/bin/ps':
            assert argv==['/bin/ps','-axo','pid=,ppid=,pgid=,uid=']
            rows='1 0 1 0\n123 1 123 501\n'  # Unrelated process reuses a pre-boot PID.
            for label,item in loaded.items():
                if item is not None and item['pid'] is not None:
                    pid=item['pid'];rows+=f'{pid} 1 {pid} {0 if label.endswith(".monitor") else 600}\n'
            if any(item is not None and item['pid']==500 for item in loaded.values()):rows+='800 500 500 600\n'
            if damage[0]=='escaped':rows+='999 1 999 600\n'
            if damage[0]=='owner':rows=rows.replace('500 1 500 600','500 1 500 501')
            if damage[0]=='boot':current[0]='e'*64
            if damage[0]=='pid':next(item for item in loaded.values() if item is not None)['pid']=777
            if damage[0]=='replace':
                path=cutover.LAUNCHD/(old[0][1]['Label']+'.plist');wire=path.read_bytes()
                path.rename(path.with_suffix('.preserved'));path.write_bytes(wire);path.chmod(0o644)
            return (1,'') if damage[0]=='ps-error' else (0,rows)
        assert argv[:2]==['/bin/launchctl','print']
        label=argv[2].removeprefix('system/');item=loaded[label]
        if item is None:return 113,'Bad request.\nCould not find service "'+label+'" in domain for system\n'
        path=cutover.LAUNCHD/(label+'.plist') if origin[0]=='boot' else item['path']
        arguments=item['spec']['ProgramArguments']
        return 0,('system/'+label+' = {\n\tpath = '+str(path)+'\n\ttype = LaunchDaemon\n'
            '\tprogram = '+arguments[0]+'\n\targuments = {\n'+''.join('\t\t'+v+'\n' for v in arguments)+'\t}\n'
            +('' if item['pid'] is None else '\tpid = '+str(item['pid'])+'\n')+'}\n')
    return SimpleNamespace(old=old,new=new,current=current,calls=calls,loaded=loaded,origin=origin,damage=damage,command=command)


@pytest.mark.parametrize('absent',[False,True,'all','no-pid'])
def test_snapshot_admits_known_mixed_boots_and_tracks_only_current_roots(restarted,absent):
    case=restarted
    if absent is True:case.loaded[next(iter(case.loaded))]=None
    elif absent=='all':
        for label in case.loaded:case.loaded[label]=None
    elif absent=='no-pid':
        for item in case.loaded.values():item['pid']=None
    result=reboot.observe(case.old,case.new,600,'a'*64,case.command)
    assert result['phase']=='reboot-jobs-observed' and result['services_changed'] is False
    assert result['boot_id']=='f'*64 and len(result['files'])==11 and len(case.calls)==21
    assert 123 not in result['tracked']['pids'] and 123 not in result['tracked']['groups']
    if absent in ('all','no-pid'):assert result['tracked']==dict(pids=[],groups=[])
    elif absent is False:assert result['tracked']['pids']==list(range(500,510))+[800]


@pytest.mark.parametrize('fault',['same-boot','uid','root','backup-policy','boot-bytes','foreign-argv',
    'escaped','owner','boot','pid','replace','ps-error','origin-mismatch'])
def test_changed_or_unknown_reboot_evidence_is_preserved_without_actions(restarted,monkeypatch,fault):
    case=restarted;uid=600;prior='a'*64
    if fault=='same-boot':prior=case.current[0]
    elif fault=='uid':uid=True
    elif fault=='root':monkeypatch.setattr(cutover,'ROOT_UID',os.geteuid()+1)
    elif fault=='backup-policy':
        next(spec for _,spec in case.new if spec['Label'].endswith('.backup'))['ProgramArguments'][0]='/foreign/python'
    elif fault=='boot-bytes':
        path=cutover.LAUNCHD/(case.old[0][1]['Label']+'.plist');path.write_bytes(b'unknown');path.chmod(0o644)
    elif fault=='foreign-argv':
        item=next(iter(case.loaded.values()));item['spec']=dict(item['spec'],ProgramArguments=['/foreign/python'])
    elif fault=='origin-mismatch':case.origin[0]='generated'
    else:case.damage[0]=fault
    with pytest.raises((ModError,OSError)):reboot.observe(case.old,case.new,uid,prior,case.command)
    assert all(argv[0]=='/bin/ps' or argv[:2]==['/bin/launchctl','print'] for argv in case.calls)
