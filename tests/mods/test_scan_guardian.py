"""Actual Darwin guardian: escaped sessions, controller death and bounded IO."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/mods'))
import mod_scan_sandbox as sandbox
from mod_package import ModError

pytestmark = pytest.mark.skipif(sys.platform != 'darwin', reason='Real Darwin spawn/lifetime confinement')

PROBE = r'''
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/sysctl.h>
int main(int argc,char **argv) {
    if(argc<3)return 2;
    if(strcmp(argv[1],"canary-target")==0){puts(getenv("TSFP_FAKE_CANARY")?"ready":"missing");fflush(stdout);sleep(30);return 0;}
    if(strcmp(argv[1],"procargs")==0){
        int mib[3]={CTL_KERN,KERN_PROCARGS2,atoi(argv[2])};size_t size=1024*1024;
        char *bytes=malloc(size);if(!bytes)return 3;errno=0;
        int result=sysctl(mib,3,bytes,&size,NULL,0),error=errno,found=0;
        const char needle[]="TSFP_FAKE_CANARY=test-only-environment";
        if(result==0)for(size_t i=0;i+sizeof(needle)-1<=size;i++)if(memcmp(bytes+i,needle,sizeof(needle)-1)==0)found=1;
        printf("{\"result\":%d,\"errno\":%d,\"seen\":%s}\n",result,error,found?"true":"false");free(bytes);return 0;
    }
    int joined=setpgid(0,getppid()),sid=(int)setsid();alarm(0);
    FILE *file=fopen(argv[2],"w");if(!file)return 3;
    fprintf(file,"{\"pid\":%d,\"guardian\":%d,\"joined\":%d,\"sid\":%d}\n",(int)getpid(),(int)getppid(),joined,sid);
    fclose(file);
    if(strcmp(argv[1],"signal")==0 || strcmp(argv[1],"group-signal")==0){errno=0;int result=kill(strcmp(argv[1],"signal")==0?getppid():-getppid(),SIGUSR1);printf("%d %d\n",result,errno);return 0;}
    if(strcmp(argv[1],"flood")==0){char bytes[65536];memset(bytes,'x',sizeof bytes);for(;;)if(write(1,bytes,sizeof bytes)<0)return 4;}
    sleep(30);return 0;
}
'''


@pytest.fixture(scope='module')
def guardian():
    subprocess.run([sys.executable, str(ROOT / 'tools/build/build-scan-guardian.py')],
                   check=True, capture_output=True, timeout=90)
    return sandbox.verified_guardian()


@pytest.fixture
def worker(guardian, tmp_path):
    source = tmp_path / 'probe.c'
    source.write_text(PROBE)
    binary = tmp_path / 'probe'
    subprocess.run(['/usr/bin/clang', '-std=c11', '-D_DARWIN_C_SOURCE', '-Wall', '-Wextra', '-Werror',
                    str(source), '-o', str(binary)], check=True, timeout=30)
    subprocess.run(['/usr/bin/codesign', '--force', '--sign', '-', str(binary)], check=True, capture_output=True, timeout=15)
    content, database = tmp_path / 'content', tmp_path / 'database'
    content.mkdir()
    database.mkdir()
    with sandbox.ScanSandbox(str(binary), content, database) as process:
        yield process


def gone(pid):
    result = subprocess.run(['/bin/ps', '-p', str(pid), '-o', 'stat='], capture_output=True, text=True, timeout=3)
    return result.returncode != 0 or not result.stdout.strip() or result.stdout.strip().startswith('Z')


def wait(test, seconds=10):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        if test():
            return
        time.sleep(.02)
    raise AssertionError('Owned scanner/guardian did not stop')


@pytest.mark.parametrize('mode', ['escape', 'flood'])
def test_timeout_or_output_limit_stops_session_escaped_scanner(worker, mode):
    identity = worker.scratch / 'identity.json'
    expected = subprocess.TimeoutExpired if mode == 'escape' else ModError
    with pytest.raises(expected):
        worker.scan([mode, str(identity)], timeout=1)
    record = json.loads(identity.read_text())
    assert record['joined'] == 0 and record['sid'] == record['pid']
    wait(lambda: gone(record['pid']) and gone(record['guardian']))


@pytest.mark.parametrize('mode', ['signal', 'group-signal'])
def test_escaped_scanner_cannot_signal_its_guardian(worker, mode):
    identity = worker.scratch / 'identity.json'
    result = worker.scan([mode, str(identity)], timeout=5)
    assert result.returncode == 0 and result.stdout.strip() == b'-1 1'


def test_sysctl_cannot_read_another_process_environment(worker):
    # Positive control uses only a fabricated environment in our own native
    # target. Platform /bin/sleep can hide its environment even unconfined.
    target = subprocess.Popen([str(worker.executable), 'canary-target', '-'],
                              env=dict(PATH='/usr/bin:/bin', TSFP_FAKE_CANARY='test-only-environment'),
                              stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    try:
        assert target.stdout.readline() == b'ready\n'
        args = ['procargs', str(target.pid)]
        control = subprocess.run([str(worker.executable), *args], capture_output=True, check=True, timeout=5)
        assert json.loads(control.stdout)['seen'] is True
        result = worker.run(args, capture_output=True, timeout=5)
        assert result.returncode == 0
        value = json.loads(result.stdout)
        assert value['result'] == -1 and value['errno'] in (1, 13) and value['seen'] is False
    finally:
        target.kill()
        target.communicate(timeout=5)


def test_controller_kill_closes_private_lifetime_even_after_group_escape(worker, tmp_path):
    script = tmp_path / 'controller.py'
    script.write_text('''import json,sys
from pathlib import Path
sys.path.insert(0,sys.argv[1])
from mod_scan_sandbox import ScanSandbox
with ScanSandbox(sys.argv[2],Path(sys.argv[3]),Path(sys.argv[4])) as worker:
 path=worker.scratch/'identity.json'
 print(json.dumps({'identity':str(path)}),flush=True)
 worker.scan(['escape',str(path)],timeout=60)
''')
    controller = subprocess.Popen([sys.executable, str(script), str(ROOT / 'tools/mods'), str(worker.executable),
                                   str(worker.content), str(worker.database)], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        header = json.loads(controller.stdout.readline())
        path = Path(header['identity'])
        wait(path.exists)
        record = json.loads(path.read_text())
        assert record['joined'] == 0 and record['sid'] == record['pid']
        controller.kill()
        controller.wait(timeout=5)
        wait(lambda: gone(record['pid']) and gone(record['guardian']))
    finally:
        if controller.poll() is None:
            controller.kill()
        controller.communicate(timeout=5)


def test_changed_guardian_receipt_or_image_is_refused(guardian, tmp_path, monkeypatch):
    folder = tmp_path / 'guardian'
    folder.mkdir(mode=0o700)
    shutil.copy2(guardian, folder / guardian.name)
    shutil.copy2(guardian.with_name('receipt.json'), folder / 'receipt.json')
    monkeypatch.setattr(sandbox, 'GUARDIAN_ROOT', folder)
    assert sandbox.verified_guardian() == folder / guardian.name
    with (folder / guardian.name).open('ab') as output:
        output.write(b'changed')
    with pytest.raises(ModError, match='receipt'):
        sandbox.verified_guardian()


def test_controller_that_ignores_child_status_is_refused(worker):
    code = '''import signal,sys
from pathlib import Path
sys.path.insert(0,sys.argv[1])
from mod_scan_sandbox import ScanSandbox
from mod_package import ModError
signal.signal(signal.SIGCHLD,signal.SIG_IGN)
try:
 with ScanSandbox(sys.argv[2],Path(sys.argv[3]),Path(sys.argv[4])):pass
except ModError:print('refused')
else:raise AssertionError('Lost child exit status could be treated as a clean scan')
'''
    result = subprocess.run([sys.executable, '-c', code, str(ROOT / 'tools/mods'), str(worker.executable),
                             str(worker.content), str(worker.database)], capture_output=True, timeout=10)
    assert result.returncode == 0 and result.stdout.strip() == b'refused'


def test_explicit_server_deadline_still_kills_escaped_owned_child(worker):
    identity = worker.scratch / 'lease.json'
    read, write = os.pipe()
    try:
        result = subprocess.run([str(worker.guardian), str(read), '--deadline-seconds', '1',
                                 *worker.command(['escape', str(identity)])],
                                pass_fds=(read,), env=worker.environment, cwd=worker.scratch,
                                stdin=subprocess.DEVNULL, capture_output=True, timeout=5)
        assert result.returncode == 137
        value = json.loads(identity.read_text())
        wait(lambda: gone(value['pid']))
    finally:
        os.close(read)
        os.close(write)
