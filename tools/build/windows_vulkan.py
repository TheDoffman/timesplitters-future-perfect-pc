"""Install/verify the pinned Windows DXC; the Vulkan driver remains system-owned."""
import argparse
import hashlib
import json
from pathlib import Path
import tempfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[2]
FILES = ('inc/dxcapi.h', 'bin/x64/dxcompiler.dll', 'bin/x64/dxil.dll',
         'LICENSE-LLVM.txt', 'LICENSE-MIT.txt', 'LICENSE-MS.txt')


def sha256(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def verify(root=ROOT):
    pin = json.loads((root/'config/build/dxc-windows.json').read_text())
    directory = root/'external/ts3FPrecomp/third_party/dxc'
    try:
        receipt = json.loads((directory/'receipt.json').read_text())
    except FileNotFoundError as error:
        raise ValueError('Windows DXC is not installed; run tsfp.ps1 -Action Setup or windows_vulkan.py --setup') from error
    if receipt.get('archive_sha256') != pin['sha256'] or set(receipt.get('files', {})) != set(FILES):
        raise ValueError('Windows DXC receipt differs from the pin; run windows_vulkan.py --setup')
    for name in FILES:
        if sha256(directory/name) != receipt['files'][name]:
            raise ValueError(f'Windows DXC file changed: {name}')
    return directory


def setup(root=ROOT):
    directory = root/'external/ts3FPrecomp/third_party/dxc'
    if directory.exists():
        return verify(root)
    pin = json.loads((root/'config/build/dxc-windows.json').read_text())
    download = root/'private/downloads'
    download.mkdir(parents=True, exist_ok=True)
    archive = download/('dxc-'+pin['version']+'.zip')
    if not archive.exists():
        temporary = archive.with_suffix('.download')
        urllib.request.urlretrieve(pin['url'], temporary)
        temporary.replace(archive)
    if archive.stat().st_size != pin['bytes'] or sha256(archive) != pin['sha256']:
        raise ValueError('Windows DXC archive differs from the pinned publisher digest')
    with tempfile.TemporaryDirectory(prefix='dxc-', dir=directory.parent) as temporary:
        stage = Path(temporary)/'dxc'
        with zipfile.ZipFile(archive) as zipped:
            # Extract only named build/runtime/license files; no archive paths are trusted.
            for name in FILES:
                target = stage/name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(zipped.read(name))
        receipt = dict(archive_sha256=pin['sha256'], version=pin['version'],
                       files={name: sha256(stage/name) for name in FILES})
        (stage/'receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')
        stage.rename(directory)
    return verify(root)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--setup', action='store_true')
    args = parser.parse_args()
    print(setup() if args.setup else verify())
