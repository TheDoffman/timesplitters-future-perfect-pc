"""Install and verify the pinned, minimal SPIRV-Cross source dependency."""
import argparse
import hashlib
import json
from pathlib import Path
import tarfile
import urllib.request

ROOT=Path(__file__).resolve().parents[2]
SOURCES=('spirv_cross.cpp','spirv_parser.cpp','spirv_cross_parsed_ir.cpp','spirv_cfg.cpp','spirv_glsl.cpp')

def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def verify(root=ROOT):
    root=Path(root)
    pin=json.loads((root/'config/build/spirv-cross.json').read_text())
    target=root/'external/spirv-cross'
    receipt=json.loads((target/'receipt.json').read_text())
    if receipt.get('archive_sha256')!=pin['sha256'] or receipt.get('commit')!=pin['commit']:
        raise ValueError('SPIRV-Cross receipt does not match the source pin')
    files=receipt.get('files',{})
    if not set((*SOURCES,'LICENSE','spirv_glsl.hpp','spirv_cross.hpp')).issubset(files):
        raise ValueError('SPIRV-Cross receipt omits required source or license')
    for name,digest in files.items():
        if Path(name).name!=name or sha256(target/name)!=digest:
            raise ValueError('SPIRV-Cross source changed: '+name)
    return target

def setup(root=ROOT):
    root=Path(root);pin=json.loads((root/'config/build/spirv-cross.json').read_text())
    target=root/'external/spirv-cross'
    if target.exists():return verify(root)
    download=root/'private/downloads'/('spirv-cross-'+pin['commit'][:8]+'.tar.gz')
    download.parent.mkdir(parents=True,exist_ok=True)
    if not download.exists():
        with urllib.request.urlopen(pin['url'],timeout=60) as response,download.open('wb') as output:
            count=0
            while chunk:=response.read(65536):
                count+=len(chunk)
                if count>pin['bytes']:raise ValueError('SPIRV-Cross archive exceeds pinned size')
                output.write(chunk)
    if download.stat().st_size!=pin['bytes'] or sha256(download)!=pin['sha256']:
        raise ValueError('SPIRV-Cross archive does not match pinned bytes/hash')
    content={}
    with tarfile.open(download,'r:gz') as archive:
        for member in archive.getmembers():
            parts=Path(member.name).parts
            if len(parts)!=2:continue
            name=parts[1]
            if name not in (*SOURCES,'LICENSE') and not name.endswith(('.hpp','.h')):continue
            if not member.isfile() or member.size>4*1024*1024:
                raise ValueError('Invalid SPIRV-Cross source member')
            content[name]=archive.extractfile(member).read()
    if not set((*SOURCES,'LICENSE','spirv_glsl.hpp','spirv_cross.hpp')).issubset(content):
        raise ValueError('SPIRV-Cross source archive is incomplete')
    if sum(map(len,content.values()))>16*1024*1024:
        raise ValueError('SPIRV-Cross minimal source exceeds budget')
    target.mkdir(parents=True)
    for name,data in content.items():(target/name).write_bytes(data)
    (target/'receipt.json').write_text(json.dumps(dict(commit=pin['commit'],archive_sha256=pin['sha256'],
        files={name:hashlib.sha256(data).hexdigest() for name,data in content.items()}),indent=2)+'\n')
    return verify(root)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--setup',action='store_true')
    args=parser.parse_args()
    print(setup() if args.setup else verify())
