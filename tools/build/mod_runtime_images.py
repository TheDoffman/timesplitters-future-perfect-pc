"""Build/stage production-only helper images and generate game compilation pins."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import shutil
import struct
import tempfile

ROOT=Path(__file__).resolve().parents[2]
HEADER_MARKER=b'/* Generated native mod helper image pins. */\n'
LIMIT=32*1024*1024


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def worker_builder():
    spec=importlib.util.spec_from_file_location('mod_runtime_worker_builder',ROOT/'tools/build/build-mod-worker.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module


def host_platform():
    host=platform.system();machine=platform.machine().lower()
    if host=='Windows' and machine in ('amd64','x86_64'):return 'windows-x64'
    if host=='Darwin' and machine=='arm64':return 'macos-arm64'
    if host=='Linux' and machine in ('amd64','x86_64'):return 'linux-x64'
    raise ValueError('Unsupported production mod helper platform')


def plain(path,exists=True):
    path=Path(path).absolute()
    if any(p.is_symlink() or getattr(p,'is_junction',lambda:False)() for p in (path,*path.parents)):
        raise ValueError('Refusing linked helper build/staging paths')
    if exists and not path.exists():raise ValueError('Missing helper build/staging input')
    return path


def native_image(path,target):
    path=plain(path)
    if not path.is_file() or not 0<path.stat().st_size<=LIMIT:raise ValueError('Invalid helper image size/type')
    data=path.read_bytes()
    if target=='windows-x64':
        offset=struct.unpack_from('<I',data,0x3c)[0] if len(data)>=64 else len(data)
        valid=(data[:2]==b'MZ' and offset<=len(data)-26 and data[offset:offset+4]==b'PE\0\0' and struct.unpack_from('<H',data,offset+4)[0]==0x8664)
    elif target=='macos-arm64':valid=len(data)>=32 and data[:4]==b'\xcf\xfa\xed\xfe' and struct.unpack_from('<I',data,4)[0]==0x100000c
    else:valid=len(data)>=64 and data[:6]==b'\x7fELF\x02\x01' and struct.unpack_from('<H',data,18)[0]==62
    if not valid:raise ValueError('Helper image does not match the native target')
    return data


def strict_json(path):
    def pairs(items):
        result={}
        for name,value in items:
            if name in result:raise ValueError('Duplicate helper receipt field')
            result[name]=value
        return result
    if plain(path).stat().st_size>1024*1024:raise ValueError('Helper receipt exceeds budget')
    return json.loads(path.read_text(encoding='utf-8'),object_pairs_hook=pairs)


def validate_build(output):
    output=plain(output);receipt=strict_json(output/'BUILD-worker.json');target=host_platform()
    if (receipt.get('schema')!=2 or any(receipt.get(k) is not False for k in ('testing','sanitize','host_sanitize'))):
        raise ValueError('Only production non-instrumented helper builds can be packaged')
    if receipt.get('sources')!=worker_builder().source_hashes():raise ValueError('Helper build sources differ from current source')
    cache=(output/'cmake/CMakeCache.txt').read_text(encoding='utf-8')
    values={line.split('=',1)[0].split(':',1)[0]:line.split('=',1)[1] for line in cache.splitlines() if '=' in line and not line.startswith(('#','//'))}
    for key in ('TSFP_MOD_TESTING','TSFP_MOD_SANITIZE','TSFP_MOD_HOST_SANITIZE'):
        if values.get(key)!='OFF':raise ValueError('Helper build configuration is not production')
    binary=output/'cmake'/('Release' if target=='windows-x64' else '')/('tsfp-mod-worker.exe' if target=='windows-x64' else 'tsfp-mod-worker')
    files={binary.name:binary, 'Lua.txt':output/'Licenses/Lua.txt'}
    native_image(binary,target)
    if sha(binary)!=receipt.get('binary_sha256'):raise ValueError('Worker image differs from its build receipt')
    if target=='macos-arm64':
        guardian=binary.with_name('tsfp-mod-guardian');native_image(guardian,target)
        if sha(guardian)!=receipt.get('guardian_sha256'):raise ValueError('Guardian differs from its build receipt')
        files[guardian.name]=guardian
    elif receipt.get('guardian_sha256') is not None:raise ValueError('Unexpected guardian on this platform')
    if sha(plain(files['Lua.txt']))!=sha(ROOT/'LICENSES/Lua.txt'):raise ValueError('Helper license differs from source')
    return target,receipt,files


def verify_bundle(bundle,descriptor):
    bundle=plain(bundle)
    if not bundle.is_dir() or set(p.name for p in bundle.iterdir())!=set(descriptor['files'])|{'runtime.json'}:
        raise ValueError('Helper bundle contains missing or unrecognized files')
    if strict_json(bundle/'runtime.json')!=descriptor:raise ValueError('Immutable helper bundle metadata changed')
    for name,record in descriptor['files'].items():
        path=plain(bundle/name)
        if not path.is_file() or path.stat().st_size!=record['bytes'] or sha(path)!=record['sha256']:
            raise ValueError('Immutable helper bundle image changed')


def stage(output,runtime_root,header):
    output=plain(output);runtime_root=plain(runtime_root,False);header=plain(header,False)
    if runtime_root.name!='ModRuntime':raise ValueError('Game helper destination must be named ModRuntime')
    target,receipt,files=validate_build(output)
    descriptor=dict(schema=1,platform=target,worker_build=receipt,
                    producer_sha256=sha(Path(__file__)),
                    files={n:dict(sha256=sha(p),bytes=p.stat().st_size) for n,p in sorted(files.items())})
    identity=hashlib.sha256(json.dumps(descriptor,sort_keys=True,separators=(',',':')).encode()).hexdigest()
    descriptor['identity']=identity;bundle=runtime_root/identity
    runtime_root.mkdir(parents=True,exist_ok=True)
    if bundle.exists():verify_bundle(bundle,descriptor)
    else:
        with tempfile.TemporaryDirectory(prefix='.images-',dir=runtime_root) as temporary:
            temporary=Path(temporary)
            if not temporary.resolve().is_relative_to(runtime_root.resolve()):raise ValueError('Escaped helper staging root')
            for name,source in files.items():
                destination=temporary/name;shutil.copyfile(plain(source),destination)
                if sha(destination)!=descriptor['files'][name]['sha256']:raise ValueError('Helper changed while packaging')
                destination.chmod(0o444 if name=='Lua.txt' else 0o555)
            (temporary/'runtime.json').write_text(json.dumps(descriptor,sort_keys=True,indent=2)+'\n',encoding='utf-8')
            temporary.chmod(0o755)
            temporary.rename(bundle)
    verify_bundle(bundle,descriptor)
    worker='tsfp-mod-worker.exe' if target=='windows-x64' else 'tsfp-mod-worker'
    worker_hash=descriptor['files'][worker]['sha256'];guardian_hash=descriptor['files'].get('tsfp-mod-guardian',{}).get('sha256','')
    text=HEADER_MARKER+('''#ifndef TSFP_MOD_RUNTIME_IMAGES_H
#define TSFP_MOD_RUNTIME_IMAGES_H
#define TSFP_MOD_RUNTIME_SUBDIR "ModRuntime/'''+identity+'''"
#define TSFP_MOD_RUNTIME_WORKER "'''+worker+'''"
#define TSFP_MOD_RUNTIME_WORKER_SHA256 "'''+worker_hash+'''"
#define TSFP_MOD_RUNTIME_GUARDIAN_SHA256 "'''+guardian_hash+'''"
#endif
''').encode()
    if header.exists():
        previous=header.read_bytes()
        if not previous.startswith(HEADER_MARKER):raise ValueError('Preserving unrecognized helper header')
        if previous==text:return descriptor
    header.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.NamedTemporaryFile(prefix='.images-header-',dir=header.parent,delete=False) as stream:
        name=Path(stream.name);stream.write(text)
    try:os.replace(name,header)
    finally:name.unlink(missing_ok=True)
    return descriptor


def prepare(output,runtime_root,header,cmake='cmake'):
    worker_builder().build(output,cmake)
    return stage(output,runtime_root,header)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'build/mod-worker')
    parser.add_argument('--runtime-root',type=Path,required=True)
    parser.add_argument('--header',type=Path,required=True)
    parser.add_argument('--cmake',default='cmake')
    args=parser.parse_args();result=prepare(args.output,args.runtime_root,args.header,args.cmake)
    print('Production helper bundle: '+result['identity'])
