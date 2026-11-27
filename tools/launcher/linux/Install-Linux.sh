#!/bin/sh
# One Linux entry point. Installation and recovery remain in the native launcher.
if ! command -v python3 >/dev/null 2>&1; then
    printf '%s\n' 'Install Python 3, then run this installer again.' >&2
    exit 1
fi
exec python3 - "$@" <<'TSFP_INSTALLER_PYTHON'
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import stat
import subprocess
import sys
import tempfile
import urllib.parse
import urllib.request
import zipfile

REPOSITORY = 'TheDoffman/timesplitters-future-perfect-pc'
API = 'https://api.github.com/repos/' + REPOSITORY + '/releases?per_page=20&page='
MAX_ARCHIVE = 256 * 1024**2
EXECUTABLE = 'TimeSplitters.Launcher'
MANIFEST = 'Support/Launcher/manifest.json'
APPLICATION = 'Support/Launcher/application.json'


def architecture(machine=None):
    value = (machine or platform.machine()).lower()
    if value in ('x86_64', 'amd64', 'x64'):
        return 'linux-x64'
    if value in ('aarch64', 'arm64'):
        return 'linux-arm64'
    raise ValueError('This installer supports Linux x64 and ARM64; detected ' + value + '.')


def request(url):
    return urllib.request.urlopen(urllib.request.Request(url, headers={
        'User-Agent': 'TimeSplitters-Linux-Installer', 'Accept': 'application/vnd.github+json'}), timeout=45)


def read_small(url, limit):
    with request(url) as response:
        data = response.read(limit + 1)
    if len(data) > limit:
        raise ValueError('Release metadata exceeds its size limit.')
    return data


def asset_url(asset, tag):
    name = asset.get('name', '')
    expected = '/' + REPOSITORY + '/releases/download/' + tag + '/' + name
    url = asset.get('browser_download_url', '')
    parsed = urllib.parse.urlsplit(url)
    if (parsed.scheme != 'https' or parsed.netloc != 'github.com' or parsed.query or parsed.fragment
            or '/' in name or '\\' in name or urllib.parse.unquote(parsed.path) != expected):
        raise ValueError('Release download does not belong to the project.')
    return url


def select_release(releases, suffix):
    candidates = []
    for release in releases:
        tag = release.get('tag_name', '')
        version = re.fullmatch(r'launcher-v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)', tag)
        if not version or release.get('draft') or release.get('prerelease'):
            continue
        text = '.'.join(version.groups())
        expected = 'TimeSplitters-Launcher-v' + text + '-' + suffix + '.zip'
        assets = release.get('assets', [])
        matches = [asset for asset in assets if asset.get('name') == expected]
        if len(matches) != 1:
            continue
        asset = matches[0]
        if not 0 < asset.get('size', 0) <= MAX_ARCHIVE:
            continue
        asset_url(asset, tag)
        digest = asset.get('digest', '') or ''
        sides = [item for item in assets if item.get('name') == expected + '.sha256']
        if not re.fullmatch(r'sha256:[0-9a-fA-F]{64}', digest) and len(sides) != 1:
            continue
        if sides:
            asset_url(sides[0], tag)
        candidates.append((tuple(map(int, version.groups())), text, tag, asset, sides))
    if not candidates:
        raise ValueError('No stable ' + suffix + ' launcher package is published yet. No x64/ARM64 substitution was made.')
    _, version, tag, asset, sides = max(candidates, key=lambda value: value[0])
    return version, tag, asset, sides


def checksum(asset, tag, sides):
    value = asset.get('digest', '') or ''
    if re.fullmatch(r'sha256:[0-9a-fA-F]{64}', value):
        return value[7:].lower()
    fields = read_small(asset_url(sides[0], tag), 4096).decode('ascii').split()
    if len(fields) != 2 or fields[1] != asset['name'] or not re.fullmatch('[0-9a-fA-F]{64}', fields[0]):
        raise ValueError('Invalid launcher checksum file.')
    return fields[0].lower()


def download(asset, tag, expected, destination):
    size = asset['size']
    if not 0 < size <= MAX_ARCHIVE:
        raise ValueError('Launcher download size is invalid.')
    digest = hashlib.sha256()
    total = 0
    with request(asset_url(asset, tag)) as source, destination.open('xb') as output:
        while True:
            chunk = source.read(65536)
            if not chunk:
                break
            total += len(chunk)
            if total > size:
                raise ValueError('Launcher download exceeds its declared size.')
            digest.update(chunk)
            output.write(chunk)
    if total != size or digest.hexdigest() != expected:
        raise ValueError('Launcher download is incomplete or its checksum does not match.')


def extract_bootstrap(archive, directory, suffix, version):
    # Extract only the fixed, hashed native entry point. The launcher then
    # validates/unpacks the full archive through its normal shared policy.
    with zipfile.ZipFile(archive) as package:
        names = package.namelist()
        if names.count(MANIFEST) != 1 or names.count(EXECUTABLE) != 1 or names.count(APPLICATION) != 1:
            raise ValueError('Launcher bootstrap or manifest is missing/duplicated.')
        info = package.getinfo(MANIFEST)
        if info.file_size > 1024**2:
            raise ValueError('Launcher manifest is too large.')
        manifest = json.loads(package.read(info))
        expected = manifest.get('files', {}).get(EXECUTABLE, '')
        if manifest.get('schema') != 1 or manifest.get('version') != version or not re.fullmatch('[0-9a-fA-F]{64}', expected):
            raise ValueError('Launcher manifest version or checksum is invalid.')
        app_info = package.getinfo(APPLICATION)
        if app_info.file_size > 16384:
            raise ValueError('Launcher application metadata is too large.')
        app_data = package.read(app_info)
        if hashlib.sha256(app_data).hexdigest() != manifest.get('files', {}).get(APPLICATION):
            raise ValueError('Launcher application metadata checksum does not match.')
        application = json.loads(app_data)
        if application.get('platform') != suffix or application.get('version') != version:
            raise ValueError('Launcher package does not match this architecture/version.')
        if type(application.get('installer_protocol')) is not int or application['installer_protocol'] != 1:
            raise ValueError('This launcher predates automatic installation. Use its ZIP installation instructions until a newer launcher is published.')
        info = package.getinfo(EXECUTABLE)
        if not 0 < info.file_size <= MAX_ARCHIVE or stat.S_ISLNK(info.external_attr >> 16):
            raise ValueError('Launcher executable is invalid or linked.')
        executable = directory / EXECUTABLE
        digest = hashlib.sha256()
        with package.open(info) as source, executable.open('xb') as output:
            while True:
                chunk = source.read(65536)
                if not chunk:
                    break
                digest.update(chunk)
                output.write(chunk)
        if digest.hexdigest() != expected.lower():
            raise ValueError('Launcher executable checksum does not match.')
    with executable.open('rb') as source:
        header = source.read(20)
    machine = {'linux-x64': 62, 'linux-arm64': 183}[suffix]
    if len(header) != 20 or header[:6] != b'\x7fELF\x02\x01' or int.from_bytes(header[18:20], 'little') != machine:
        raise ValueError('Launcher executable belongs to a different Linux architecture.')
    executable.chmod(0o700)
    return executable


def main(argv=None):
    parser = argparse.ArgumentParser(description='Install the native TimeSplitters launcher for this Linux computer.')
    parser.add_argument('--install-dir', type=Path, default=Path(os.environ.get('XDG_DATA_HOME', str(Path.home() / '.local/share'))) / 'TimeSplitters')
    parser.add_argument('--check', action='store_true', help='Report the matching published launcher without downloading or installing')
    parser.add_argument('--no-launch', action='store_true', help='Install without opening the launcher')
    args = parser.parse_args(argv)
    if platform.system() != 'Linux':
        parser.error('Run this installer on Linux.')
    suffix = architecture()
    releases = []
    for page in range(1, 6):
        batch = json.loads(read_small(API + str(page), 4 * 1024**2))
        if not isinstance(batch, list):
            raise ValueError('Invalid release response.')
        releases.extend(batch)
        if len(batch) < 20:
            break
    version, tag, asset, sides = select_release(releases, suffix)
    if args.check:
        print(json.dumps({'platform': suffix, 'version': version, 'asset': asset['name']}))
        return 0
    root = args.install_dir.expanduser().resolve()
    print('Detected ' + suffix + '; installing launcher ' + version + ' into ' + str(root), flush=True)
    with tempfile.TemporaryDirectory(prefix='timesplitters-installer-') as temporary:
        directory = Path(temporary)
        archive = directory / 'launcher.zip'
        download(asset, tag, checksum(asset, tag, sides), archive)
        executable = extract_bootstrap(archive, directory, suffix, version)
        command = [str(executable), '--install-launcher', str(root), str(archive), version]
        if args.no_launch:
            command.append('--no-launch')
        return subprocess.run(command, check=False).returncode


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        print('TimeSplitters installer: ' + str(error), file=sys.stderr)
        raise SystemExit(1)
TSFP_INSTALLER_PYTHON
