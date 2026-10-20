import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('texture_release', ROOT/'tools/texture-release.py')
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class TextureReleaseTests(unittest.TestCase):
    def fixture(self, pack):
        words = [0]*32
        for index, value in {0: 0x20534444, 1: 124, 3: 1, 4: 1, 7: 1,
                             19: 32, 20: 4, 21: 0x31545844}.items():
            words[index] = value
        path = pack/'0123456789abcdef.dds'
        path.write_bytes(struct.pack('<32I', *words)+b'\0'*8)
        path.with_suffix('.json').write_text(json.dumps({
            'key': path.stem, 'sha256': release.digest(path),
            'source': 'C:/Users/private-name/art.png', 'prompt': 'private',
            'format': 'DXT1', 'source_size': [1, 1], 'output_size': [1, 1],
            'resampled': False}), encoding='utf-8')
        return path

    def test_deterministic_packaging_strips_private_metadata(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            pack = root/'pack'
            pack.mkdir()
            self.fixture(pack)
            rows, size = release.audit(pack)
            a, b = root/'a.zip', root/'b.zip'
            release.archive(rows, a)
            release.archive(rows, b)
            self.assertEqual(a.read_bytes(), b.read_bytes())
            with zipfile.ZipFile(a) as archive:
                self.assertEqual(len(archive.namelist()), 2)
                self.assertEqual(size, sum(i.file_size for i in archive.infolist()))
                metadata = json.loads(archive.read('0123456789abcdef.json'))
                self.assertNotIn('source', metadata)
                self.assertNotIn('prompt', metadata)
                self.assertEqual(metadata['format'], 'DXT1')

    def test_source_changes_and_checksum_mismatches_stop_publication(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            pack = root/'pack'
            pack.mkdir()
            path = self.fixture(pack)
            rows, _ = release.audit(pack)
            path.write_bytes(path.read_bytes()[:-1]+b'x')
            with self.assertRaisesRegex(ValueError, 'changed'):
                release.archive(rows, root/'part.zip')
            with self.assertRaisesRegex(ValueError, 'does not match'):
                release.audit(pack)

    def test_invalid_dds_and_unexpected_files_stop_publication(self):
        with tempfile.TemporaryDirectory() as name:
            pack = Path(name)
            path = self.fixture(pack)
            (pack/'default.xbe').write_bytes(b'game')
            with self.assertRaises(ValueError):
                release.audit(pack)
            (pack/'default.xbe').unlink()
            path.write_bytes(b'not a texture')
            with self.assertRaises(ValueError):
                release.audit(pack)

    def manifest(self):
        return dict(schema=1, version='0.1.0-alpha.1', texture_count=1,
                    installed_bytes=1024, parts=[dict(name='textures-001.zip',
                    size=500, sha256='a'*64)])

    def test_limits_and_duplicate_parts(self):
        for field, value in [('installed_bytes', release.BUDGET+1),
                             ('version', '../other'), ('texture_count', 0)]:
            manifest = self.manifest()
            manifest[field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                release.validate_manifest(manifest)
        manifest = self.manifest()
        manifest['parts'] *= 2
        with self.assertRaises(ValueError):
            release.validate_manifest(manifest)
        manifest = self.manifest()
        manifest['parts'][0]['size'] = release.PART_LIMIT+1
        with self.assertRaises(ValueError):
            release.validate_manifest(manifest)

    def test_remote_manifest_requires_published_matching_assets(self):
        manifest = self.manifest()
        asset = dict(name='textures-001.zip', size=500, digest='sha256:'+'a'*64,
                     state='uploaded')
        class Api:
            def call(self, method, path):
                return self.result
        api = Api()
        metadata = (json.dumps(manifest, indent=2)+'\n').encode()
        manifest_asset = dict(name='texture-pack.json', size=len(metadata),
                             digest='sha256:'+release.hashlib.sha256(metadata).hexdigest(),
                             state='uploaded')
        api.result = dict(draft=False, assets=[asset, manifest_asset], html_url='release')
        for size, digest in release.manifest_encodings(manifest):
            manifest_asset.update(size=size, digest=digest)
            self.assertEqual(release.verify_remote(manifest, api), 'release')
        api.result['draft'] = True
        with self.assertRaises(ValueError):
            release.verify_remote(manifest, api)
        api.result['draft'] = False
        asset['digest'] = 'sha256:'+'b'*64
        with self.assertRaises(ValueError):
            release.verify_remote(manifest, api)

    def test_publication_uploads_before_visibility_and_reuses_identical_assets(self):
        class Api:
            result = None
            uploads = 0
            def call(self, method, path, data=None, upload=None):
                if method == 'GET':
                    return self.result
                if method == 'PATCH':
                    self.assert_ready()
                    self.result.update(data)
                    return self.result
                if upload:
                    self.uploads += 1
                    value = dict(name=Path(upload).name, size=Path(upload).stat().st_size,
                                 digest='sha256:'+release.digest(Path(upload)), state='uploaded')
                    self.result['assets'].append(value)
                    return value
                self.result = {**data, 'id': 1, 'assets': [], 'html_url': 'release'}
                return self.result
            def assert_ready(self):
                assert self.result['draft']
                assert {a['name'] for a in self.result['assets']} == {'textures-001.zip', 'texture-pack.json'}
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            pack = root/'pack'
            pack.mkdir()
            self.fixture(pack)
            api = Api()
            output = root/'config.json'
            with patch.object(release.subprocess, 'check_output', return_value='a'*40):
                release.publish(pack, '0.1.0-alpha.1', output, root/'work', api)
                self.assertEqual(api.result['make_latest'], 'false')
                self.assertFalse(api.result['draft'])
                self.assertEqual(api.uploads, 2)
                release.publish(pack, '0.1.0-alpha.1', output, root/'work', api)
                self.assertEqual(api.uploads, 2)
                self.assertEqual(list((root/'work').iterdir()), [])

    def test_excessive_parts_are_rejected_before_any_api_call(self):
        class Api:
            def call(self, *args, **kwargs):
                raise AssertionError('Network called before part-count validation')
        with (patch.object(release, 'audit', return_value=([], 1)),
              patch.object(release, 'groups', return_value=[[]]*(release.MAX_PARTS+1))):
            with self.assertRaisesRegex(ValueError, 'too many download parts'):
                release.publish(Path('unused'), '0.1.0', Path('unused.json'), Path('unused-work'), Api())

    def test_resumed_draft_with_stale_asset_is_never_published(self):
        class Api:
            def call(self, method, path, **kwargs):
                if method != 'GET':
                    raise AssertionError('Stale draft was modified')
                return dict(id=1, draft=True, assets=[dict(name='textures-099.zip')])
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            pack = root/'pack'
            pack.mkdir()
            self.fixture(pack)
            with self.assertRaisesRegex(ValueError, 'unaudited or stale'):
                release.publish(pack, '0.1.0', root/'config.json', root/'work', Api())

    def test_remote_verification_rejects_extra_assets(self):
        manifest = self.manifest()
        with self.assertRaisesRegex(ValueError, 'unaudited assets'):
            release.verify_assets(manifest, dict(assets=[dict(name='extra.zip')]))


if __name__ == '__main__':
    unittest.main()
