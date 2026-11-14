"""Copy verified native mod helpers without changing their compiled-in digests."""
import hashlib
import json
from pathlib import Path
import re
import shutil

import mod_runtime_images as images


def inspect(root, build):
    root=Path(root);build=images.plain(build)
    target,receipt,files=images.validate_build(root/'build/mod-worker')
    header=images.plain(build/'generated/mod-runtime-images.h')
    if header.stat().st_size>4096:raise ValueError('Helper pin header exceeds budget')
    text=header.read_text(encoding='utf-8')
    if not text.startswith(images.HEADER_MARKER.decode()):raise ValueError('Unrecognized game helper pin header')
    def pin(name):
        matches=re.findall(r'^#define TSFP_MOD_RUNTIME_'+name+r' "([^"\r\n]*)"$',text,re.M)
        if len(matches)!=1:raise ValueError('Missing or duplicate compiled helper pin')
        return matches[0]
    subdir=pin('SUBDIR')
    if not re.fullmatch(r'ModRuntime/[a-f0-9]{64}',subdir):raise ValueError('Invalid compiled helper bundle path')
    parent=build/'Release' if target=='windows-x64' else build
    bundle=images.plain(parent/subdir);descriptor=images.strict_json(bundle/'runtime.json')
    identity=descriptor.get('identity');unsigned={k:v for k,v in descriptor.items() if k!='identity'}
    if (identity!=subdir.split('/')[1] or identity!=hashlib.sha256(json.dumps(unsigned,sort_keys=True,separators=(',',':')).encode()).hexdigest()
            or descriptor.get('schema')!=1 or descriptor.get('platform')!=target
            or descriptor.get('worker_build')!=receipt or descriptor.get('producer_sha256')!=images.sha(Path(images.__file__))):
        raise ValueError('Game helper bundle provenance differs from the production build')
    expected={n:dict(sha256=images.sha(p),bytes=p.stat().st_size) for n,p in files.items()}
    if descriptor.get('files')!=expected:raise ValueError('Game helper bundle does not match production images')
    worker='tsfp-mod-worker.exe' if target=='windows-x64' else 'tsfp-mod-worker'
    if (pin('WORKER')!=worker or pin('WORKER_SHA256')!=expected[worker]['sha256']
            or pin('GUARDIAN_SHA256')!=expected.get('tsfp-mod-guardian',{}).get('sha256','')):
        raise ValueError('Game helper header differs from its immutable bundle')
    images.verify_bundle(bundle,descriptor)
    return bundle,descriptor


def copy(root, build, stage):
    """All game packages put the executable in Bin; helpers remain adjacent."""
    bundle,descriptor=inspect(root,build)
    destination=images.plain(Path(stage)/'Bin/ModRuntime'/descriptor['identity'],False)
    # Never merge with or overwrite an existing package tree.
    shutil.copytree(bundle,destination)
    images.verify_bundle(destination,descriptor)
    return descriptor
