"""Owned public-server scratch leases, containment canaries and bounded recovery."""
import json
import os
from pathlib import Path
import socket
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/mods'))
import mod_public_sandbox as public
from mod_package import ModError

pytestmark = pytest.mark.skipif(os.name != 'posix', reason='Mac/POSIX public server resources')


@pytest.fixture
def state(tmp_path):
    tmp_path.chmod(0o700)
    return tmp_path


def test_live_lease_prevents_recovery_and_complete_release_reclaims_only_own_work(state):
    scratch = public.acquire(state)
    with pytest.raises(BlockingIOError):
        public.acquire(state)
    (scratch.path / 'generated').write_text('server output')
    scratch.close()
    next_scratch = public.acquire(state)
    assert not list(next_scratch.path.iterdir())
    next_scratch.close()
    assert not list((state / 'caddy-runtime/units').iterdir())


def test_recovery_after_released_controller_lease_preserves_outside_symlink_target(state):
    scratch = public.acquire(state)
    canary = state / 'personal-canary'
    canary.write_text('preserve')
    (scratch.path / 'alias').symlink_to(canary)
    with pytest.raises(ModError, match='Special'):
        scratch.check()
    os.close(scratch.lease)
    scratch.lease = None
    recovered = public.acquire(state)
    assert canary.read_text() == 'preserve' and recovered.unit != scratch.unit
    recovered.close()


def test_storage_overrun_withdraws_but_bounded_owned_output_can_be_cleaned(state):
    scratch = public.acquire(state)
    with (scratch.path / 'oversized').open('wb') as stream:
        stream.truncate(public.MAX_BYTES + 1)
    with pytest.raises(ModError, match='storage budget'):
        scratch.check()
    scratch.close()
    assert not scratch.unit.exists()


def test_unknown_root_and_incomplete_unit_are_preserved(state):
    root = state / 'caddy-runtime'
    root.mkdir(mode=0o700)
    canary = root / 'personal.txt'
    canary.write_text('preserve')
    with pytest.raises(OSError):
        public.acquire(state)
    assert canary.read_text() == 'preserve'


@pytest.fixture
def guardian():
    if sys.platform != 'darwin':
        pytest.skip('Actual Darwin sandbox and guardian')
    subprocess.run([sys.executable, str(ROOT / 'tools/build/build-scan-guardian.py')],
                   capture_output=True, check=True, timeout=90)
    from mod_scan_sandbox import verified_guardian
    return verified_guardian()


def test_guardian_retains_scratch_lease_after_controller_closes_its_copy(state, guardian):
    scratch = public.acquire(state)
    read, write = os.pipe()
    try:
        child = subprocess.Popen([str(guardian), str(read), '--deadline-seconds', '5', '/bin/sleep', '1'],
                                 pass_fds=(read, scratch.lease), close_fds=True,
                                 stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        os.close(scratch.lease)
        scratch.lease = None
        with pytest.raises(BlockingIOError):
            public.acquire(state)
        assert child.wait(timeout=4) == 0
        recovered = public.acquire(state)
        assert recovered.unit != scratch.unit
        recovered.close()
    finally:
        os.close(read)
        os.close(write)


PROBE = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/sysctl.h>
int main(int argc,char **argv){
 if(argc<3)return 2;int action=0,result=-1,error=0,seen=0;
 if(strcmp(argv[1],"env-target")==0){puts(getenv("TSFP_FAKE_ENV")?"ready":"missing");fflush(stdout);sleep(30);return 0;}
 if(strcmp(argv[1],"fds")==0){result=0;for(int fd=3;fd<256;fd++)if(fcntl(fd,F_GETFD)>=0)result++;}
 else if(strcmp(argv[1],"procargs")==0){
  int mib[3]={CTL_KERN,KERN_PROCARGS2,atoi(argv[2])};size_t size=1024*1024;char *bytes=malloc(size);if(!bytes)return 3;
  errno=0;result=sysctl(mib,3,bytes,&size,NULL,0);error=errno;const char needle[]="TSFP_FAKE_ENV=test-only";
  if(result==0)for(size_t i=0;i+sizeof(needle)-1<=size;i++)if(memcmp(bytes+i,needle,sizeof(needle)-1)==0)seen=1;free(bytes);
 }
 else if(strcmp(argv[1],"bound-net")==0 || strcmp(argv[1],"connectx")==0 || strcmp(argv[1],"udp-send")==0){
  int udp=strcmp(argv[1],"udp-send")==0;int s=socket(AF_INET,udp?SOCK_DGRAM:SOCK_STREAM,0),one=1;
  setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one));setsockopt(s,SOL_SOCKET,SO_REUSEPORT,&one,sizeof(one));
  struct sockaddr_in a={0};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons(8766);
  errno=0;action=bind(s,(void*)&a,sizeof(a));error=errno;a.sin_port=htons((unsigned short)atoi(argv[2]));
  if(action==0){errno=0;if(udp)result=sendto(s,"x",1,0,(void*)&a,sizeof(a));
   else if(strcmp(argv[1],"connectx")==0){sa_endpoints_t endpoints={0};endpoints.sae_dstaddr=(void*)&a;endpoints.sae_dstaddrlen=sizeof(a);result=connectx(s,&endpoints,SAE_ASSOCID_ANY,0,NULL,0,NULL,NULL);}
   else result=connect(s,(void*)&a,sizeof(a));error=errno;}
  close(s);
 }
 else if(strcmp(argv[1],"net")==0 || strcmp(argv[1],"bind")==0){
  int s=socket(AF_INET,SOCK_STREAM,0);struct sockaddr_in a={0};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons((unsigned short)atoi(argv[2]));
  errno=0;result=s<0?-1:(strcmp(argv[1],"net")==0?connect(s,(void*)&a,sizeof(a)):bind(s,(void*)&a,sizeof(a)));error=errno;if(s>=0)close(s);
 }else if(strcmp(argv[1],"fork")==0){errno=0;pid_t p=fork();if(p==0)_exit(0);result=p<0?-1:0;error=errno;if(p>0)waitpid(p,NULL,0);
 }else{
  const char *path=argv[2];
  if(strcmp(argv[1],"link")==0 || strcmp(argv[1],"symlink")==0){if(argc<4)return 2;errno=0;action=strcmp(argv[1],"link")==0?link(path,argv[3]):symlink(path,argv[3]);error=errno;path=argv[3];}
  errno=0;FILE *f=fopen(path,strcmp(argv[1],"write")==0?"w":"r");result=f?0:-1;if(!f)error=errno;
  if(f){if(strcmp(argv[1],"write")==0)fputs("changed",f);else{char b[32]={0};fread(b,1,sizeof(b)-1,f);seen=strcmp(b,"TSFP_FAKE_KEY")==0;}fclose(f);}
 }
 printf("{\"action\":%d,\"result\":%d,\"errno\":%d,\"seen\":%s}\n",action,result,error,seen?"true":"false");return 0;
}
'''


def test_actual_public_policy_blocks_private_aliases_network_and_children(state, guardian):
    from mod_scan_sandbox import GuardedChild
    source, binary = state / 'probe.c', state / 'probe'
    source.write_text(PROBE)
    subprocess.run(['/usr/bin/clang', '-Wall', '-Wextra', '-Werror', str(source), '-o', str(binary)], check=True, timeout=30)
    subprocess.run(['/usr/bin/codesign', '--force', '--sign', '-', str(binary)], capture_output=True, check=True, timeout=15)
    approved = state / 'public'
    approved.mkdir(mode=0o700)
    readable = approved / 'catalog'
    readable.write_text('TSFP_FAKE_KEY')
    private = state / 'private-canary'
    private.write_text('TSFP_FAKE_KEY')
    scratch = public.acquire(state)
    policy = public.profile(binary, approved, source, scratch.path)
    def run(args, confined=True):
        if not confined:
            result = subprocess.run([str(binary), *args], capture_output=True, check=True, timeout=5)
            return json.loads(result.stdout)
        read, write = os.pipe()
        child = None
        try:
            process = subprocess.Popen([str(guardian), str(read), '--deadline-seconds', '5',
                                        '/usr/bin/sandbox-exec', '-p', policy, str(binary), *args],
                                       pass_fds=(read, scratch.lease), close_fds=True, stdin=subprocess.DEVNULL,
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=scratch.path,
                                       env=dict(PATH='/usr/bin:/bin', HOME=str(scratch.path), TMPDIR=str(scratch.path)))
            child = GuardedChild(process, write)
            write = None
            out, err = process.communicate(timeout=8)
            assert process.returncode == 0, err
            return json.loads(out)
        finally:
            if child is not None:
                child.kill()
            os.close(read)
            if write is not None:
                os.close(write)
    try:
        assert run(['read', str(private)], False)['seen']
        assert run(['read', str(readable)])['seen']
        assert run(['read', str(private)])['result'] == -1
        assert run(['write', str(private)])['result'] == -1 and private.read_text() == 'TSFP_FAKE_KEY'
        assert run(['write', str(scratch.path / 'output')])['result'] == 0
        for mode in ('link', 'symlink'):
            assert run([mode, str(private), str(state / ('control-' + mode))], False)['seen']
            assert run([mode, str(private), str(scratch.path / mode)])['seen'] is False
        assert run(['fork', '-'], False)['result'] == 0 and run(['fork', '-'])['result'] == -1
        assert run(['bind', '0'], False)['result'] == 0 and run(['bind', '0'])['result'] == -1
        assert run(['fds', '-'])['result'] == 0  # Neither parent-liveness nor scratch-lease FD reaches the server.
        target = subprocess.Popen([str(binary), 'env-target', '-'],
                                  env=dict(PATH='/usr/bin:/bin', TSFP_FAKE_ENV='test-only'),
                                  stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        try:
            assert target.stdout.readline() == b'ready\n'
            assert run(['procargs', str(target.pid)], False)['seen']
            refused = run(['procargs', str(target.pid)])
            assert refused['result'] == -1 and refused['errno'] in (1, 13) and refused['seen'] is False
        finally:
            target.kill()
            target.communicate(timeout=5)
        with socket.socket() as listener:
            listener.bind(('127.0.0.1', 0))
            listener.listen(4)
            args = ['net', str(listener.getsockname()[1])]
            assert run(args, False)['result'] == 0 and run(args)['result'] == -1
        for mode in ('bound-net', 'connectx', 'udp-send'):
            kind = socket.SOCK_DGRAM if mode == 'udp-send' else socket.SOCK_STREAM
            # Separate destination ports avoid TCP TIME_WAIT masquerading as
            # sandbox denial when the positive control reuses source8766.
            with socket.socket(type=kind) as control, socket.socket(type=kind) as denied:
                for listener in (control, denied):
                    listener.bind(('127.0.0.1', 0))
                    if kind == socket.SOCK_STREAM:
                        listener.listen(4)
                allowed = run([mode, str(control.getsockname()[1])], False)
                assert allowed['action'] == 0 and allowed['result'] >= 0
                refused = run([mode, str(denied.getsockname()[1])])
                assert refused['action'] == 0 and refused['result'] == -1 and refused['errno'] in (1, 13)
    finally:
        scratch.close()
