"""Production texture-camera placement; asset-free Windows/macOS coverage."""
from pathlib import Path
import re
import pytest
from test_cutscene_letterbox import compile_and_run

ROOT = Path(__file__).resolve().parents[1]

@pytest.mark.parametrize("mutation", ["none", "no-scope", "no-restore", "no-flush"])
def test_camera_feed_scope(tmp_path, mutation):
    source = (ROOT / "src/pc_display_ui.inc").read_text(encoding="utf-8")
    functions = []
    for signature in ("static void ui_place", "void pc_display_camera_feed"):
        match = re.search(re.escape(signature) + r"\([^\n]*\)\n\{.*?\n\}", source, re.S)
        assert match
        functions.append(match.group())
    body = "\n".join(functions)
    if mutation == "no-scope":
        body = body.replace("if(feed)ui_place(1,0x000C8120u);", "")
    elif mutation == "no-restore":
        body = body.replace("if(feed)ui_place(old,old_tag);", "")
    elif mutation == "no-flush":
        body = body.replace("guest_call(sub_00022FF0,0);", "")
    fixture = (ROOT / "tests/camera_feed.c").read_text(encoding="utf-8")
    compile_and_run(tmp_path, fixture.replace("/* PRODUCTION */", body), fails=mutation != "none")


def test_camera_feed_hook():
    manual = (ROOT / "external/ts3FPrecomp/titles/futureperfect/src/recomp_manual.c").read_text(encoding="utf-8")
    assert "pc_display_camera_feed(sub_000C8120_gen);" in manual
    # The generator recognizes only the explicit extern declaration as a wrapper.
    import importlib.util
    path = ROOT / "external/ts3FPrecomp/tools/recomp/manual_scan.py"
    spec = importlib.util.spec_from_file_location("feed_manual_scan", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    _, wrapped, _ = module.scan(str(ROOT / "external/ts3FPrecomp/titles/futureperfect/src/recomp_manual.c"))
    assert 0x000C8120 in wrapped
