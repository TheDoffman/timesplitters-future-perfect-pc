"""Storage helpers for native tests; only known disposable partition images."""
from pathlib import Path
import time


def recycle_test_disks(root, test_directory):
    root, test = Path(root).resolve(), Path(test_directory)
    allowed = (root / 'private/test-saves/native-mac', root / 'build/macos/path-tests')
    if test.is_symlink() or test.resolve() not in allowed:
        raise RuntimeError(f'Refusing unexpected test directory: {test}')
    if not test.exists():
        return {'files': 0, 'logical_bytes': 0, 'allocated_bytes': 0}
    images = []
    for p in test.rglob('Partition*.img'):
        if p.is_symlink() or not p.resolve().is_relative_to(test.resolve()):
            raise RuntimeError(f'Refusing unexpected partition path: {p}')
        if p.name not in {f'Partition{i}.img' for i in range(6)}:
            raise RuntimeError(f'Refusing unknown partition image: {p}')
        images.append(p.resolve())
    result = {'files': len(images), 'logical_bytes': 0, 'allocated_bytes': 0}
    if images:
        trash = Path.home() / '.Trash' / f'TSFP-native-test-disks-{time.time_ns()}'
        trash.mkdir(parents=True, exist_ok=False)
        for p in images:
            stat = p.stat()
            result['logical_bytes'] += stat.st_size
            result['allocated_bytes'] += stat.st_blocks * 512
            destination = trash / p.relative_to(test.resolve())
            destination.parent.mkdir(parents=True, exist_ok=True)
            p.rename(destination)
        result['recycled_to'] = str(trash)
    return result
