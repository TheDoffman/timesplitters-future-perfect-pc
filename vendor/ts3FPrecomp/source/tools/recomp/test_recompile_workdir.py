"""Two titles side by side must not share stage outputs.

Every stage of scripts/recompile.py defaults to one tools/*/output directory,
and tools.recomp's guard against lifting one binary from another's
disassembly compares file names only. Every retail disc is default.xbe, so
the second title of a two-title session lifted the first title's code and
said nothing. --work-dir routes each stage's output to a per-title directory,
and --project points the lift at a title project made from the template.
"""
import argparse
import importlib.util
import pathlib
import tempfile
import unittest

REPO = pathlib.Path(__file__).resolve().parent.parent.parent


def _load_driver():
    spec = importlib.util.spec_from_file_location(
        "recompile_driver", REPO / "scripts" / "recompile.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _args(**over):
    base = dict(text_only=False, seeds=[], verbose=False, all=False,
                split=1000, gen_dir=None, trace_all_entries=False,
                only=None, start="parse", work_dir=None, project=None,
                game_name=None)
    base.update(over)
    return argparse.Namespace(**base)


def _stage(commands, name):
    return dict(commands)[name]


class WorkDirRoutesEveryStage(unittest.TestCase):

    def setUp(self):
        self.driver = _load_driver()
        self.xbe = pathlib.Path("/t/default.xbe")
        self.json = pathlib.Path("/t/default_analysis.json")

    def test_without_it_no_stage_names_an_output_directory(self):
        cmds = self.driver.build_commands(_args(), self.xbe, self.json)
        for name in ("disasm", "identify", "lift"):
            argv = _stage(cmds, name)
            self.assertNotIn("-o", argv, name)
            self.assertNotIn("--disasm-dir", argv, name)

    def test_disasm_identify_and_lift_all_point_at_it(self):
        with tempfile.TemporaryDirectory() as d:
            work = pathlib.Path(d) / "work"
            cmds = self.driver.build_commands(
                _args(work_dir=str(work)), self.xbe, self.json)
            disasm = _stage(cmds, "disasm")
            self.assertEqual(disasm[disasm.index("-o") + 1],
                             str(work.resolve() / "disasm"))
            identify = _stage(cmds, "identify")
            self.assertEqual(
                identify[identify.index("--functions") + 1],
                str(work.resolve() / "disasm" / "functions.json"))
            self.assertEqual(identify[identify.index("-o") + 1],
                             str(work.resolve() / "func_id"))
            lift = _stage(cmds, "lift")
            self.assertEqual(lift[lift.index("--disasm-dir") + 1],
                             str(work.resolve() / "disasm"))
            self.assertEqual(lift[lift.index("--func-id-dir") + 1],
                             str(work.resolve() / "func_id"))
            self.assertEqual(lift[lift.index("-o") + 1],
                             str(work.resolve() / "recomp"))


class ProjectPointsTheLiftAtATitle(unittest.TestCase):

    def setUp(self):
        self.driver = _load_driver()
        self.xbe = pathlib.Path("/t/default.xbe")
        self.json = pathlib.Path("/t/default_analysis.json")

    def test_generated_sources_land_where_the_template_globs(self):
        with tempfile.TemporaryDirectory() as d:
            proj = pathlib.Path(d)
            cmds = self.driver.build_commands(
                _args(project=str(proj)), self.xbe, self.json)
            lift = _stage(cmds, "lift")
            self.assertEqual(lift[lift.index("--gen-dir") + 1],
                             str(proj.resolve() / "src" / "recomp" / "gen"))
            # No recomp_manual.c yet: nothing to exclude, and no path to a
            # file that does not exist.
            self.assertNotIn("--exclude-manual", lift)

    def test_hand_written_overrides_are_excluded_when_present(self):
        with tempfile.TemporaryDirectory() as d:
            proj = pathlib.Path(d)
            (proj / "src").mkdir()
            manual = proj / "src" / "recomp_manual.c"
            manual.write_text("/* overrides */\n", encoding="utf-8")
            cmds = self.driver.build_commands(
                _args(project=str(proj)), self.xbe, self.json)
            lift = _stage(cmds, "lift")
            self.assertEqual(lift[lift.index("--exclude-manual") + 1],
                             str(manual.resolve()))

    def test_an_explicit_gen_dir_wins_over_the_project(self):
        with tempfile.TemporaryDirectory() as d:
            cmds = self.driver.build_commands(
                _args(project=d, gen_dir="elsewhere"), self.xbe, self.json)
            lift = _stage(cmds, "lift")
            self.assertEqual(lift[lift.index("--gen-dir") + 1], "elsewhere")


if __name__ == "__main__":
    unittest.main()
