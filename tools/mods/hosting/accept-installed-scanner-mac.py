"""Locked installed-engine admission using disposable owned files, never uploads."""
import importlib.util
import json
import os
from pathlib import Path
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from mod_backup import private_directory
from mod_package import ModError, require
from mod_repository import ClamScanner


def initialize():
    source=Path(__file__).with_name('initialize-private-scan-mac.py')
    spec=importlib.util.spec_from_file_location('scanner_admission_initializer',source)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    release=module.installed_release()
    require(Path(__file__).absolute()==release/'source/tools/mods/hosting/accept-installed-scanner-mac.py',
            'Use the immutable installed scanner admission program')
    module.check(release)
    return release,module.ROOT


def accept(release, root):
    parent=private_directory(root/'data')
    database=private_directory(parent/'scan/database')
    executable=release/'runtime/bin/clamscan'
    with tempfile.TemporaryDirectory(prefix='scanner-admission-',dir=parent) as temporary:
        temporary=Path(temporary)
        clean=temporary/'clean';blocked=temporary/'blocked';empty=temporary/'empty-database'
        for folder in (clean,blocked,empty):folder.mkdir(mode=0o700)
        (clean/'main.lua').write_bytes(b'return { name = "scanner admission fixture" }\n')
        # Standard harmless antivirus test string, generated only on the Mac.
        test=b'X5O!P%@AP[4\\PZX54(P^)7CC)7}$' + b'EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*'
        (blocked/'antivirus-test.txt').write_bytes(test)
        scanner=ClamScanner(str(executable),database)
        results=dict(clean=scanner.scan(clean),blocked=scanner.scan(blocked),
                     empty_database=ClamScanner(str(executable),empty).scan(clean),
                     missing_engine=ClamScanner(str(temporary/'missing-engine'),database).scan(clean))
        expected=dict(clean='clean',blocked='blocked',empty_database='error',missing_engine='error')
        require(all(value.get('status')==expected[name] for name,value in results.items()),
                'Installed scanner admission failed; no services may start')
        require(all(results[name].get('isolation')=='macos-seatbelt-v3'
                    and results[name].get('engine')=='ClamAV' and results[name].get('version','').startswith('ClamAV ')
                    for name in ('clean','blocked')), 'Installed scanner isolation or engine identity differs')
        version=results['clean']['version']
    require(not temporary.exists(),'Scanner admission files remain')
    return dict(schema=1,phase='scanner-admitted',uid=os.geteuid(),release=release.name,version=version,
                results=expected,temporary_state_removed=True,services_started=False,network_started=False)


def main():
    try:
        release,root=initialize()
        print(json.dumps(accept(release,root),indent=2));return 0
    except (OSError,ValueError,ModError) as error:
        print('Installed scanner admission: '+str(error)[:2000],file=sys.stderr);return 2


if __name__=='__main__':raise SystemExit(main())
