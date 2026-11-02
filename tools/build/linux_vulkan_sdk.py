"""Verified checkout-local Linux SDK; use the host's installed Vulkan driver."""
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import tarfile
import urllib.request

from vulkan_sdk import sha256

ROOT = Path(__file__).resolve().parents[2]
CRITICAL = (
    'x86_64/include/dxc/dxcapi.h', 'x86_64/include/dxc/WinAdapter.h',
    'x86_64/lib/libdxcompiler.so', 'x86_64/lib/VulkanLoader/lib/libvulkan.so.1',
    'x86_64/lib/libVkLayer_khronos_validation.so',
    'x86_64/share/vulkan/explicit_layer.d/VkLayer_khronos_validation.json',
)


def pin(root=ROOT):
    spec = json.loads((root / 'config/build/vulkan-sdk-linux.json').read_text())
    mac = json.loads((root / 'config/build/vulkan-sdk.json').read_text())
    if spec['version'] != mac['version']:
        raise RuntimeError('Linux and macOS SDK versions must be updated together')
    return spec


def install_path(root=ROOT):
    return root / 'external/vulkan-sdk/linux' / pin(root)['version']


def verify_install(root=ROOT):
    installed = install_path(root)
    if installed.is_symlink() or installed.parent.is_symlink():
        raise RuntimeError('Refusing symlinked Linux SDK installation')
    receipt_path = installed / 'tsfp-receipt.json'
    if not receipt_path.is_file():
        raise RuntimeError('Linux Vulkan SDK missing; run test-rhi-linux.py --setup-sdk')
    receipt = json.loads(receipt_path.read_text())
    spec = pin(root)
    if receipt.get('version') != spec['version'] or receipt.get('archive_sha256') != spec['sha256']:
        raise RuntimeError('Linux SDK receipt differs from the shared pin')
    files = receipt.get('files', {})
    if not set(CRITICAL).issubset(files) or not files:
        raise RuntimeError('Incomplete Linux SDK receipt')
    for name, expected in files.items():
        path = installed / name
        if not path.resolve().is_relative_to(installed.resolve()) or sha256(path) != expected:
            raise RuntimeError('Linux SDK file changed: ' + name)
    if not any('license' in name.lower() for name in files):
        raise RuntimeError('Linux SDK licenses missing')
    return installed / 'x86_64'


def setup(root=ROOT):
    installed = install_path(root)
    if installed.exists():
        return verify_install(root)  # Preserve unknown/incomplete installations.
    if shutil.disk_usage(root).free < 5 * 1024**3:
        raise RuntimeError('Linux SDK setup needs at least 5 GiB free')
    spec = pin(root)
    download = root / 'external/vulkan-sdk/download'
    if download.is_symlink() or download.parent.is_symlink():
        raise RuntimeError('Refusing symlinked SDK download directory')
    download.mkdir(parents=True, exist_ok=True)
    archive = download / spec['archive']
    partial = archive.with_suffix('.partial')
    if archive.is_symlink() or partial.is_symlink():
        raise RuntimeError('Refusing symlinked SDK download')
    if not archive.exists():
        request = urllib.request.Request(spec['url'], headers={'User-Agent': 'TSFP-SDK-Setup/1.0'})
        with urllib.request.urlopen(request, timeout=60) as response, partial.open('wb') as output:
            shutil.copyfileobj(response, output)
        partial.replace(archive)
    if archive.stat().st_size != spec['bytes'] or sha256(archive) != spec['sha256']:
        raise RuntimeError('Linux SDK archive differs from the publisher size/SHA-256 pin')
    # Retain host binaries, headers, manifests and licenses, without installing
    # into /usr, changing the system driver, or unpacking SDK source duplicates.
    prefix = PurePosixPath(spec['version'])
    installed.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive, 'r:xz') as bundle:
        members = []
        for entry in bundle:
            path = PurePosixPath(entry.name)
            if path.is_absolute() or '..' in path.parts or not path.is_relative_to(prefix):
                raise RuntimeError('Unsafe Linux SDK archive path')
            relative = path.relative_to(prefix)
            if not relative.parts or relative.parts[0] == 'x86_64' or any(
                    'license' in part.lower() for part in relative.parts):
                members.append(entry)
        bundle.extractall(installed.parent, members=members, filter='data')
    files = {p.relative_to(installed).as_posix(): sha256(p)
             for p in installed.rglob('*') if p.is_file()}
    receipt = dict(version=spec['version'], archive_sha256=spec['sha256'], files=files)
    (installed / 'tsfp-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    sdk = verify_install(root)
    archive.unlink()
    return sdk


def probe_environment(sdk, root=ROOT):
    env = {k: v for k, v in os.environ.items()
           if not k.startswith(('RECOMP_', 'TSFP_', 'VK_', 'SDL_VULKAN', 'LD_'))}
    isolated = root / 'build/linux/rhi-tests/runtime'
    if isolated.is_symlink() or not isolated.resolve().is_relative_to((root / 'build/linux').resolve()):
        raise RuntimeError('Unexpected Linux probe directory')
    isolated.mkdir(parents=True, exist_ok=True)
    env.update(LD_LIBRARY_PATH=str(sdk / 'lib') + ':' + str(sdk / 'lib/VulkanLoader/lib'),
               SDL_VULKAN_LIBRARY=str(sdk / 'lib/VulkanLoader/lib/libvulkan.so.1'),
               VK_LAYER_PATH=str(sdk / 'share/vulkan/explicit_layer.d'),
               RECOMP_SHADER_CACHE='0', RECOMP_VK_VALIDATION='1',
               RECOMP_INPUT_CONFIG=str(isolated / 'controls.json'),
               RECOMP_DISPLAY_CONFIG=str(isolated / 'runtime.conf'),
               TSFP_DISPLAY_CONFIG=str(isolated / 'pc-display.ini'),
               RECOMP_SAVE_DIR=str(isolated / 'saves'),
               XDG_CONFIG_HOME=str(isolated / 'config'),
               XDG_DATA_HOME=str(isolated / 'data'), XDG_CACHE_HOME=str(isolated / 'cache'))
    # XDG_RUNTIME_DIR and display/session variables remain owned by WSLg/the
    # desktop. Driver selection is deliberately explicit in the test runner.
    return env
