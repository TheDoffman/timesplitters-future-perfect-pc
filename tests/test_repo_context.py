"""Navigation reflects the loader, stays bounded and requires maintained guides."""
import importlib.util
import json
from pathlib import Path
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('repo_context', ROOT / 'tools/repo-context.py')
context = importlib.util.module_from_spec(spec)
spec.loader.exec_module(context)


def test_repository_context_links_index_and_summary_budgets():
    assert 'current' in context.check(ROOT)
    for query in ('fix Issue 105 FPS flip gate', 'launcher install updates', 'controls texture network',
                  'build compiler test', 'investigation slow context', '\u754c' * 10000):
        assert len(context.packet(ROOT, query).encode('utf-8')) <= 6500


def test_index_excludes_untracked_experiments_and_uses_real_loader_order():
    index = context.patch_index(ROOT)
    tracked = context.tracked(ROOT)
    names = [p['name'] for p in index['patches']]
    assert {f'patches/{n}' for n in names} == {n for n in tracked if n.startswith('patches/')
                                              and n.count('/') == 1 and n.endswith('.patch')}
    def position(suffix):
        return next(i for i, name in enumerate(names) if name.endswith(suffix))
    assert position('-native-frame-timing.patch') < position('-native-interp-retirement.patch')
    assert position('-native-interp-retirement.patch') < position('-fixed-60-fps.patch')
    assert position('-late-frame-pacing.patch') == position('-fixed-60-fps.patch') + 1
    assert position('-late-frame-pacing.patch') < position('-native-numeric-policy.patch')
    assert position('-native-surface-live.patch') < position('-native-flip-handoff.patch')
    assert position('-native-flip-handoff.patch') < position('-native-surface-capture.patch')
    assert position('-native-surface-capture.patch') < position('-native-surface-render.patch')
    assert position('-native-surface-render.patch') < position('-native-demo-timing.patch')
    assert position('-native-demo-timing.patch') < position('-texture-bind-scan.patch')
    assert position('-texture-bind-scan.patch') < position('-model-transform-entry.patch')
    assert position('-model-transform-entry.patch') < position('-zero-base-indices.patch')
    assert position('-zero-base-indices.patch') < position('-radial-stick-deadzone.patch')
    assert position('-radial-stick-deadzone.patch') < position('independent-translation-moves.patch')
    assert position('independent-translation-moves.patch') < position('independent-translation-integer.patch')
    assert position('independent-translation-integer.patch') < position('independent-translation-bits.patch')
    assert position('independent-translation-bits.patch') < position('independent-translation-product.patch')
    assert names[-1] == 'independent-translation-product.patch'

    assert position('-native-surface-live.patch') < position('-native-surface-float32.patch')
    assert all(p['targets'] and len(p['sha256']) == 64 for p in index['patches'])


def test_changed_systems_require_each_owning_guide():
    data = context.manifest(ROOT)
    changed = ['patches/new.patch', 'src/pc_input.c', 'tools/launcher/GameSessions.cs']
    required = context.missing_updates(data, changed)
    assert set(required) == {context.CONTEXT + n + '.md' for n in ('runtime', 'systems', 'launcher', 'build-test')}
    assert not context.missing_updates(data, changed + required)
    assert not context.missing_updates(data, ['docs/launcher.md'])


def test_finder_is_literal_bounded_and_points_to_effective_runtime_target():
    text = context.find(ROOT, 'Nv2aFlipGate', limit=3)
    assert 'src/kernel/xbox_memory_layout.c' in text
    assert 'active order' in text or 'untracked experiment' in text
    assert 'showing 3.' in text
    assert 'showing 0.' in context.find(ROOT, '.*[not-a-regex')


def test_stale_index_is_rejected_without_modifying_sources(tmp_path, monkeypatch):
    monkeypatch.setattr(context, 'manifest', lambda root: {'overview': 'map.md', 'topics': [], 'owners': []})
    monkeypatch.setattr(context, 'excerpt', lambda *args: 'reference')
    monkeypatch.setattr(context, 'index_text', lambda root: 'new stack')
    path = tmp_path / context.INDEX
    path.parent.mkdir(parents=True)
    path.write_text('old stack')
    with pytest.raises(ValueError, match='stale'):
        context.check(tmp_path)
    assert path.read_text() == 'old stack'


@pytest.mark.parametrize('name', ['../auth.json', 'C:/auth.json', 'docs/../auth.json', 'docs\\auth.json'])
def test_context_reader_rejects_outside_or_ambiguous_paths(tmp_path, name):
    with pytest.raises(ValueError):
        context.safe_file(tmp_path, name)


def test_guide_sources_are_fixed_not_arbitrary_manifest_paths(tmp_path):
    path = tmp_path / context.CONTEXT
    path.mkdir(parents=True)
    (path / 'manifest.json').write_text(json.dumps({'version': 1, 'overview': context.CONTEXT + 'README.md',
        'topics': [{'id': 'runtime', 'file': 'private/game/default.xbe'}]}))
    with pytest.raises(ValueError, match='Only named'):
        context.manifest(tmp_path)


def test_changed_path_check_includes_deleted_files(tmp_path):
    subprocess.run(['git', 'init', '-q', str(tmp_path)], check=True)
    p = tmp_path / 'src/removed.c'
    p.parent.mkdir()
    p.write_text('old')
    subprocess.run(['git', '-C', str(tmp_path), 'add', '.'], check=True)
    subprocess.run(['git', '-C', str(tmp_path), '-c', 'user.name=Fixture', '-c', 'user.email=fixture@example.invalid',
                    'commit', '-qm', 'fixture'], check=True)
    p.unlink()
    assert 'src/removed.c' in context.changed_paths(tmp_path, 'HEAD')


@pytest.mark.parametrize('change, exempt', [
    ('version', True), ('initial', True), ('question', False),
    ('empty', False), ('marker', False), ('duplicate', False),
    ('injected', False), ('deleted', False), ('other-path', False),
])
def test_only_generated_version_data_avoids_guide_churn(tmp_path, monkeypatch, change, exempt):
    name = '.github/ISSUE_TEMPLATE/01-bug-report.yml'
    path = tmp_path / name
    path.parent.mkdir(parents=True)
    import re
    before = (ROOT / name).read_text(encoding='utf-8')
    for kind in ('GAME', 'LAUNCHER'):
        before = re.sub(rf'(?m)(^        <!-- BEGIN AUTO {kind} VERSIONS -->\n).*?(?=^        <!-- END AUTO {kind} VERSIONS -->$)',
                        lambda m: m[1] + '        - `v1.0.0`\n', before, flags=re.S)
    if change == 'initial':
        before = re.sub(r'(?m)^        - `v[^`]+`\n', '', before)
        before = before.replace('VERSIONS -->\n        <!-- END',
                                'VERSIONS -->\n        No published releases.\n        <!-- END')
    path.write_text(before, encoding='utf-8')
    subprocess.run(['git', 'init', '-q', str(tmp_path)], check=True)
    subprocess.run(['git', '-C', str(tmp_path), 'add', '.'], check=True)
    subprocess.run(['git', '-C', str(tmp_path), '-c', 'user.name=Fixture',
                    '-c', 'user.email=fixture@example.invalid', 'commit', '-qm', 'baseline'], check=True)
    marker = '        <!-- BEGIN AUTO GAME VERSIONS -->\n'
    if change == 'version':
        after = before.replace(marker, marker + '        - `v99.0.0`\n')
    elif change == 'initial':
        after = before.replace('        No published releases.\n', '        - `v99.0.0`\n')
    elif change == 'question':
        after = before + '\n  - type: input\n    id: new-question\n'
    elif change == 'empty':
        after = before.replace(marker + '        - `v1.0.0`\n',
                               marker + '        No published releases.\n')
    elif change == 'marker':
        after = before.replace('END AUTO GAME', 'END GAME')
    elif change == 'duplicate':
        after = before + marker + '        <!-- END AUTO GAME VERSIONS -->\n'
    elif change == 'injected':
        after = before.replace(marker, marker + '        Arbitrary text\n')
    else:
        after = before.replace(marker, marker + '        - `v99.0.0`\n')
    path.write_text(after, encoding='utf-8')
    if change == 'deleted':
        path.unlink()
    if change == 'other-path':
        name = '.github/ISSUE_TEMPLATE/other.yml'
        (tmp_path / name).write_text(after, encoding='utf-8')
    assert context.version_reference_only(tmp_path, 'HEAD', name) is exempt
    # The real ownership gate must still reject structural/form changes.
    if change != 'other-path':
        guide = context.CONTEXT + 'launcher.md'
        monkeypatch.setattr(context, 'manifest', lambda root: {
            'overview': 'overview.md', 'topics': [{'file': guide}],
            'owners': [{'patterns': ['.github/ISSUE_TEMPLATE/**'], 'guide': guide}]})
        monkeypatch.setattr(context, 'excerpt', lambda *args: '')
        monkeypatch.setattr(context, 'index_text', lambda root: 'fixture')
        index = tmp_path / context.INDEX
        index.parent.mkdir(parents=True)
        index.write_text('fixture')
        if exempt:
            assert 'current' in context.check(tmp_path, 'HEAD')
        else:
            with pytest.raises(ValueError, match='require guide updates'):
                context.check(tmp_path, 'HEAD')
