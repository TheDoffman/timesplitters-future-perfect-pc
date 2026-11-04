"""Pinned, minimal Linux movie decoder; preserve the system FFmpeg installation."""
import ctypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import urllib.request

ROOT=Path(__file__).resolve().parents[2]
def digest(path):
    with Path(path).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def pin(root=ROOT):return json.loads((root/'config/build/ffmpeg-linux.json').read_text())
def location(root=ROOT):return root/'external/ffmpeg-linux'/pin(root)['version']

def verify_install(root=ROOT):
    spec=pin(root);base=location(root);receipt=json.loads((base/'BUILD.json').read_text())
    if receipt.get('pin')!=spec:raise ValueError('Linux FFmpeg pin/receipt mismatch; rebuild media')
    if digest(base/spec['archive'])!=spec['sha256']:raise ValueError('Linux FFmpeg source archive changed')
    if not set(spec['libraries'])<=receipt.get('libraries',{}).keys():raise ValueError('Incomplete Linux FFmpeg receipt')
    for name,sha in receipt['libraries'].items():
        if Path(name).name!=name or digest(base/'install/lib'/name)!=sha:raise ValueError('Linux FFmpeg library changed: '+name)
    if receipt.get('license')!='LGPL version 2.1 or later' or '--enable-gpl' in receipt.get('configuration','') or '--enable-nonfree' in receipt.get('configuration',''):
        raise ValueError('Linux FFmpeg license/configuration differs from the decoder-only build')
    return base,receipt

def build(root=ROOT,jobs=2):
    spec=pin(root);base=location(root)
    if base.is_symlink() or not base.resolve().is_relative_to((root/'external/ffmpeg-linux').resolve()):raise ValueError('Redirected media directory')
    if shutil.disk_usage(root).free<1024**3:raise ValueError('Media build needs 1 GiB free')
    base.mkdir(parents=True,exist_ok=True);archive=base/spec['archive']
    if not archive.exists():
        # An earlier verified download can be adopted without a second copy.
        cached=base.parent/spec['archive']
        if cached.is_file() and digest(cached)==spec['sha256']:shutil.move(cached,archive)
        else:
            partial=archive.with_suffix('.partial')
            with urllib.request.urlopen(spec['url'],timeout=45) as response,partial.open('wb') as output:shutil.copyfileobj(response,output)
            partial.replace(archive)
    if digest(archive)!=spec['sha256']:raise ValueError('Linux FFmpeg archive differs from the signed-release source pin')
    source=base/spec['archive_prefix']
    if not source.exists():
        with tarfile.open(archive,'r:gz') as bundle:
            if any(m.name!=spec['archive_prefix'] and not m.name.startswith(spec['archive_prefix']+'/') for m in bundle.getmembers()):raise ValueError('Unexpected FFmpeg source prefix')
            bundle.extractall(base,filter='data')
    with tarfile.open(archive,'r:gz') as bundle:
        for member in bundle:
            if not member.isfile():continue
            path=base/member.name
            if not path.resolve().is_relative_to(source.resolve()) or digest(path)!=hashlib.file_digest(bundle.extractfile(member),'sha256').hexdigest():
                raise ValueError('Extracted FFmpeg source changed: '+member.name)
    work=base/'build';work.mkdir(exist_ok=True);install=base/'install'
    command=[str(source/'configure'),'--prefix='+str(install),'--cc='+os.environ.get('CC','clang'),*spec['configuration']]
    subprocess.run(command,cwd=work,check=True)
    subprocess.run(['make','-j'+str(jobs)],cwd=work,check=True)
    subprocess.run(['make','install'],cwd=work,check=True)
    ctypes.CDLL(str(install/'lib/libavutil.so.58'),mode=ctypes.RTLD_GLOBAL)
    library=ctypes.CDLL(str(install/'lib/libavcodec.so.60'))
    library.avcodec_license.restype=ctypes.c_char_p;library.avcodec_configuration.restype=ctypes.c_char_p
    library.avcodec_find_decoder_by_name.restype=ctypes.c_void_p
    license=library.avcodec_license().decode();configuration=library.avcodec_configuration().decode()
    if license!='LGPL version 2.1 or later' or not library.avcodec_find_decoder_by_name(b'wmv2') or library.avcodec_find_decoder_by_name(b'h264'):
        raise ValueError('Unexpected media license/decoder set')
    receipt=dict(pin=spec,configuration=configuration,license=license,
                 libraries={name:digest(install/'lib'/name) for name in spec['libraries']})
    (base/'BUILD.json').write_text(json.dumps(receipt,indent=2)+'\n')
    return verify_install(root)
