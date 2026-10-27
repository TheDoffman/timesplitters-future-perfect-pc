"""Original stage-selection, isolation and failure contracts for generation."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import struct
import sys
from types import SimpleNamespace

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import compile_pipeline as pipeline
from test_xbe_image import fixture


def sample(tmp_path):
    path = tmp_path / 'sample with spaces.xbe'
    path.write_bytes(fixture())
    return path


def capture(monkeypatch, fail=None):
    calls = []
    def run(name, argv, dry_run):
        calls.append((name, argv, dry_run))
        return name != fail
    monkeypatch.setattr(pipeline, 'run', run)
    return calls


def test_title_seeds_custom_metadata_and_output_routing(tmp_path, monkeypatch):
    xbe = sample(tmp_path)
    runtime = tmp_path / 'runtime'
    seeds = runtime / 'config/seeds/12345678.json'
    seeds.parent.mkdir(parents=True)
    seeds.write_text('[]')
    (seeds.parent / 'OTHER.json').write_text('[]')
    project = tmp_path / 'title'
    manual = project / 'src/recomp_manual.c'
    manual.parent.mkdir(parents=True)
    manual.write_text('void own(void) {}')
    symbols = xbe.with_name(xbe.stem + '_xdk_symbols.json')
    symbols.write_text('{}')
    calls = capture(monkeypatch)
    old_root = pipeline.REPO
    metadata, work = tmp_path / 'metadata.json', tmp_path / 'work'
    assert pipeline.main([str(xbe), '--runtime', str(runtime), '--work-dir', str(work),
        '--project', str(project), '--json', str(metadata), '--game-name', 'Example Title',
        '--split', '33', '--trace-all-entries', '-v']) == 0
    assert pipeline.REPO == old_root
    commands = {name: argv for name, argv, dry in calls}
    assert list(commands) == list(pipeline.STAGES)
    assert commands['parse'][1].endswith('xbe_image.py')
    disasm = commands['disasm']
    assert disasm[1].endswith('analysis_runner.py')
    assert disasm[disasm.index('--analysis-json') + 1] == str(metadata)
    assert disasm[disasm.index('--seed-functions') + 1] == str(seeds)
    assert disasm[disasm.index('-o') + 1] == str(work / 'disasm')
    identify, lift = commands['identify'], commands['lift']
    assert lift[1] == str(ROOT / 'tools/translation_cli.py')
    assert lift[lift.index('--runtime') + 1] == str(runtime)
    assert identify[identify.index('--functions') + 1] == str(work / 'disasm/functions.json')
    assert lift[lift.index('--gen-dir') + 1] == str(project / 'src/recomp/gen')
    assert lift[lift.index('--exclude-manual') + 1] == str(manual)
    assert lift[lift.index('--icall-sites') + 1] == str(work / 'recomp/icall_sites.json')
    assert lift[lift.index('--hle-symbols') + 1] == str(symbols)
    assert '--trace-all-entries' in lift and '--game-only' in lift
    assert lift[lift.index('--game-name') + 1] == 'Example Title'
    assert lift[lift.index('--split') + 1] == '33'


@pytest.mark.parametrize('flags,stages', [(['--only', 'lift'], ['lift']),
    (['--from', 'disasm'], ['disasm', 'identify', 'lift']), (['--only', 'parse'], ['parse'])])
def test_stage_selection_and_dry_run_missing_image(flags, stages, tmp_path, monkeypatch):
    calls = capture(monkeypatch)
    assert pipeline.main([str(tmp_path / 'absent.xbe'), '--dry-run', *flags]) == 0
    assert [name for name, argv, dry in calls] == stages
    assert all(dry for name, argv, dry in calls)
    assert not list(tmp_path.iterdir())


def test_multiple_explicit_seeds_override_and_no_seeds_wins(tmp_path, monkeypatch):
    xbe = sample(tmp_path)
    seeds = [tmp_path / 'one.json', tmp_path / 'two.json']
    for path in seeds:
        path.write_text('[]')
    calls = capture(monkeypatch)
    flags = [str(xbe), '--only', 'disasm', '--seeds', str(seeds[0]), '--seeds', str(seeds[1])]
    assert pipeline.main(flags) == 0
    assert calls[0][1].count('--seed-functions') == 2
    assert '--analysis-json' not in calls[0][1]  # No metadata is required on first standalone analysis.
    calls.clear()
    assert pipeline.main(flags + ['--no-seeds']) == 0
    assert '--seed-functions' not in calls[0][1]


def test_all_functions_explicit_generation_dir_and_zero_split(tmp_path, monkeypatch):
    xbe = sample(tmp_path)
    calls = capture(monkeypatch)
    output = tmp_path / 'generated'
    assert pipeline.main([str(xbe), '--only', 'lift', '--all', '--gen-dir', str(output), '--split', '0']) == 0
    args = calls[0][1]
    assert '--all' in args and '--game-only' not in args and '--split' not in args
    assert args[args.index('--gen-dir') + 1] == str(output)


def test_legacy_builder_options_and_cli_relative_output(tmp_path, monkeypatch):
    options = SimpleNamespace(only=None, start='parse', text_only=False, seeds=[],
        verbose=False, all=False, split=0, gen_dir='chosen', trace_all_entries=False,
        work_dir=None, project=None, game_name=None)
    commands = dict(pipeline.build_commands(options, tmp_path / 'sample.xbe', tmp_path / 'metadata.json'))
    assert commands['lift'][commands['lift'].index('--gen-dir') + 1] == 'chosen'
    monkeypatch.chdir(tmp_path)
    calls = capture(monkeypatch)
    assert pipeline.main([str(sample(tmp_path)), '--only', 'lift', '--gen-dir', 'chosen']) == 0
    assert calls[0][1][calls[0][1].index('--gen-dir') + 1] == str(tmp_path / 'chosen')


def test_stage_failure_stops_following_work(tmp_path, monkeypatch):
    calls = capture(monkeypatch, fail='disasm')
    assert pipeline.main([str(sample(tmp_path))]) == 1
    assert [name for name, argv, dry in calls] == ['parse', 'disasm']


def test_invalid_input_and_destructive_metadata_path_rejected(tmp_path, monkeypatch):
    xbe = sample(tmp_path)
    calls = capture(monkeypatch)
    original = xbe.read_bytes()
    for flags in (['--split', '-1'], ['--json', str(xbe)], ['--seeds', str(tmp_path / 'absent.json')]):
        assert pipeline.main([str(xbe), *flags]) == 1
    assert not calls and xbe.read_bytes() == original
    xbe.write_bytes(b'not an executable')
    assert pipeline.main([str(xbe)]) == 1 and not calls


def test_identifier_probe_is_bounded_but_not_full_image_validation(tmp_path, monkeypatch):
    raw = bytearray(0x200)
    raw[:4] = b'XBEH'
    struct.pack_into('<I', raw, 0x104, 0x10000)
    struct.pack_into('<I', raw, 0x118, 0x10180)
    struct.pack_into('<I', raw, 0x188, 0x11223344)
    path = tmp_path / 'identifier-only.xbe'
    path.write_bytes(raw)
    assert pipeline.xbe_title_id(path) == '11223344'
    calls = capture(monkeypatch)
    assert pipeline.main([str(path)]) == 1 and not calls
    for certificate in (0xFFFF, 0x101FC, 0xFFFFFFFF):
        struct.pack_into('<I', raw, 0x118, certificate)
        path.write_bytes(raw)
        assert pipeline.xbe_title_id(path) is None


def test_run_uses_arguments_without_shell_and_preserves_exit_status(tmp_path, monkeypatch):
    calls = []
    monkeypatch.setattr(pipeline, 'REPO', tmp_path)
    def execute(argv, **kwargs):
        calls.append((argv, kwargs))
        return SimpleNamespace(returncode=7)
    monkeypatch.setattr(pipeline.subprocess, 'run', execute)
    args = ['tool', 'argument with spaces', '$literal']
    assert pipeline.run('lift', args, True) and not calls
    assert not pipeline.run('lift', args)
    assert calls == [(args, {'cwd': tmp_path})]


def test_parse_and_disassemble_from_copied_project_modules_only(tmp_path):
    own = tmp_path / 'own'
    own.mkdir()
    names = ('compile_pipeline.py', 'analysis_cache.py', 'analysis_config.py', 'analysis_output.py',
             'analysis_runner.py', 'recomp_image.py', 'recomp_symbols.py', 'x86_decode.py',
             'x86_flow.py', 'x86_recovery.py', 'x86_functions.py', 'xbe_image.py', 'xbe_kernel_exports.def')
    for name in names:
        shutil.copyfile(ROOT / 'tools' / name, own / name)
    xbe, metadata, output = sample(tmp_path), tmp_path / 'custom.json', tmp_path / 'work'
    script = 'import runpy,sys; sys.path.insert(0,sys.argv.pop(1)); runpy.run_module("compile_pipeline",run_name="__main__")'
    command = [sys.executable, '-I', '-c', script, str(own), str(xbe), '--json', str(metadata), '--work-dir', str(output)]
    for stage in ('parse', 'disasm'):
        result = subprocess.run(command + ['--only', stage], cwd=tmp_path,
                                capture_output=True, text=True, timeout=30)
        assert result.returncode == 0, result.stderr
    assert json.loads(metadata.read_text())['title_id'] == '0x12345678'
    assert json.loads((output / 'disasm/summary.json').read_text())['total_functions'] > 0


def test_production_driver_routes_to_project_commands():
    runtime = ROOT / 'external/ts3FPrecomp'
    if not (runtime / '.git').exists():
        pytest.skip('Pinned runtime unavailable')
    result = subprocess.run([sys.executable, 'scripts/recompile.py', 'absent.xbe', '--dry-run', '--no-seeds'],
                            cwd=runtime, env=dict(os.environ, TSFP_PROJECT_ROOT=str(ROOT)),
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stderr
    assert str(ROOT / 'tools/xbe_image.py') in result.stdout
    assert str(ROOT / 'tools/analysis_runner.py') in result.stdout
    assert str(ROOT / 'tools/function_catalog.py') in result.stdout
    assert str(ROOT / 'tools/translation_cli.py') in result.stdout
    assert ' -m tools.recomp ' not in result.stdout
