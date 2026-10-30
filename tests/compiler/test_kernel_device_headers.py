from pathlib import Path
import os,sys,subprocess
HERE=Path(__file__).resolve().parent
ROOT=next(p for p in HERE.parents if (p/'tools/project_paths.py').exists())
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(ROOT/'tests/compiler'))
from native_compiler import native_compiler

def test_device_host_binding_with_actual_runtime_headers(tmp_path):
    compiler,env=native_compiler(tmp_path)
    for name in ('guest_object_names.h', 'guest_name_service.h', 'guest_device_factory.h', 'guest_device_body.h', 'guest_device_callback.h', 'guest_callback_call.h', 'guest_device_dispatch.h', 'guest_device_event.h', 'guest_device_event_scope.h', 'guest_device_event_batch.h', 'guest_objects.h', 'guest_object_services.h', 'guest_object_scope.h'):
        (tmp_path/name).write_bytes(source_file(name).read_bytes())
    for name in ('guest_kernel_device_host.inc','guest_kernel_devices.inc','guest_device_host_header_probe.c'):
        (tmp_path/name).write_bytes(source_file(name).read_bytes())
    for name in ('guest_name_service.h','guest_object_names.h','guest_kernel_names.inc'):
        (tmp_path/name).write_bytes(source_file(name).read_bytes())
    runtime=ROOT/'external/ts3FPrecomp';source=tmp_path/'guest_device_host_header_probe.c';obj=tmp_path/('probe.obj' if os.name=='nt' else 'probe.o')
    if os.name=='nt':
        args=[compiler,'/nologo','/std:c11','/W4','/WX','/D_CRT_SECURE_NO_WARNINGS','/c',str(source),'/I'+str(ROOT/'src/runtime'),'/I'+str(runtime/'src'),'/I'+str(runtime/'src/kernel'),'/Fo:'+str(obj)]
    else:
        args=[compiler,'-std=c11','-Wall','-Wextra','-Werror','-c',str(source),'-I',str(ROOT/'src/runtime'),'-I',str(runtime/'src'),'-isystem',str(runtime/'src/kernel'),'-o',str(obj)]
    run=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert run.returncode==0,run.stdout+run.stderr
    assert obj.is_file()


def source_file(name):
    fixture=HERE/name
    return fixture if fixture.is_file() else ROOT/"src/runtime"/name
