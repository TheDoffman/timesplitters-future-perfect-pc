"""The build must regenerate guest wrappers when runtime hooks change."""
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('generation', ROOT / 'tools/recomp-generation.py')
generation = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generation)


def test_runtime_declarations_invalidate_generated_wrappers(tmp_path):
    for name in ('scripts/recompile.py', 'templates/runtime/recomp_types.h',
                 'titles/futureperfect/src/recomp_manual.c',
                 'titles/futureperfect/src/recomp/gen/recomp_funcs.h',
                 'src/hle/stream.c'):
        p = tmp_path / name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text('/* original */')
    source = tmp_path / 'src/hle/stream.c'
    generation.record(tmp_path)
    for declaration in ('HLE_EXPORT(Release)', 'HLE_ORIGINAL(Release)', 'HLE_IMPORT_VAR(Clock)'):
        source.write_text(source.read_text() + '\n' + declaration + '\n')
        assert not generation.current(tmp_path)
        generation.record(tmp_path)
        assert generation.current(tmp_path)
    source.write_text(source.read_text() + '\n/* body-only edit */\n')
    assert generation.current(tmp_path)
    source.unlink()
    assert not generation.current(tmp_path)
    generation.record(tmp_path)
    symbols = tmp_path / 'config/extra_symbols/45410066.json'
    symbols.parent.mkdir(parents=True)
    symbols.write_text('{"symbols": []}')
    assert not generation.current(tmp_path)
    generation.record(tmp_path)
    symbols.write_text('{"symbols": [{"name": "XMVPlaybackStop"}]}')
    assert not generation.current(tmp_path)


def test_parser_sources_and_bridge_invalidate_generation(tmp_path, monkeypatch):
    project = tmp_path / 'project'
    runtime = tmp_path / 'runtime'
    for name in ('scripts/recompile.py', 'templates/runtime/recomp_types.h',
                 'titles/futureperfect/src/recomp_manual.c',
                 'titles/futureperfect/src/recomp/gen/recomp_funcs.h',
                 'tools/xbe_parser/xbe_parser.py'):
        path = runtime / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('original')
    for name in ('xbe_image.py', 'xbe_kernel_exports.def'):
        path = project / 'tools' / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('original')
    monkeypatch.setattr(generation, '__file__', str(project / 'tools/recomp-generation.py'))
    for path in (project / 'tools/xbe_image.py', project / 'tools/xbe_kernel_exports.def',
                 runtime / 'tools/xbe_parser/xbe_parser.py'):
        generation.record(runtime)
        assert generation.current(runtime)
        path.write_text('changed')
        assert not generation.current(runtime)
