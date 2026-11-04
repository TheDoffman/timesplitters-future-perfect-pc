"""Shared hash-pinned native runtime notice downloads."""
import hashlib,json,urllib.request
from pathlib import Path
def prepare(root,directory,names=None):
    pins=json.loads((Path(root)/'config/build/native-mac-runtime-licenses.json').read_text())
    directory=Path(directory);directory.mkdir(parents=True,exist_ok=True)
    for name,pin in pins.items():
        if names is not None and name not in names:continue
        path=directory/name
        if not path.exists():
            with urllib.request.urlopen(pin['url'],timeout=30) as response:data=response.read()
            if hashlib.sha256(data).hexdigest()!=pin['sha256']:raise ValueError('Downloaded license differs from its pin: '+name)
            with path.open('xb') as output:output.write(data)
        if path.is_symlink() or hashlib.sha256(path.read_bytes()).hexdigest()!=pin['sha256']:raise ValueError('Runtime license differs from its pin: '+name)
