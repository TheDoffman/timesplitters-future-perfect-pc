"""Seeds belong to a title, not to the toolkit.

config/seed_functions.json was loaded for every XBE. A seed is an
unconditional claim that a function starts at an address, and tools.disasm
only refuses one that lands mid-instruction -- so Burnout 2's 214 addresses
were applied to any other title, and each that happened to land on an
instruction boundary became a fake function start, splitting the real
function around it. On a branch whose goal is "works for any game".
"""
import importlib.util
import json
import pathlib
import struct
import unittest

REPO = pathlib.Path(__file__).resolve().parent.parent.parent


def _load_driver():
    spec = importlib.util.spec_from_file_location(
        "recompile_driver", REPO / "scripts" / "recompile.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _fake_xbe(path, title_id):
    """The smallest XBE the title-ID reader accepts."""
    base = 0x00010000
    cert = base + 0x0180
    data = bytearray(0x0200)
    data[0:4] = b"XBEH"
    struct.pack_into("<I", data, 0x0104, base)
    struct.pack_into("<I", data, 0x0118, cert)
    struct.pack_into("<I", data, (cert - base) + 8, title_id)
    path.write_bytes(bytes(data))
    return path


class TitleIdFromCertificate(unittest.TestCase):

    def setUp(self):
        self.driver = _load_driver()

    def test_reads_the_id_the_certificate_holds(self):
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            xbe = _fake_xbe(pathlib.Path(d) / "default.xbe", 0x41430019)
            self.assertEqual(self.driver.xbe_title_id(xbe), "41430019")

    def test_a_file_that_is_not_an_xbe_has_no_title(self):
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d) / "not.xbe"
            p.write_bytes(b"not an xbe at all")
            self.assertIsNone(self.driver.xbe_title_id(p))

    def test_a_missing_file_has_no_title(self):
        self.assertIsNone(
            self.driver.xbe_title_id(pathlib.Path("/nonexistent/default.xbe")))


class SeedFilesAreKeyedToATitle(unittest.TestCase):
    """The shipped layout, checked as a layout rather than by running the CLI."""

    def test_no_unkeyed_seed_file_remains(self):
        self.assertFalse(
            (REPO / "config" / "seed_functions.json").exists(),
            "an unkeyed seed file is applied to every title")

    def test_every_seed_file_is_named_for_a_title_id(self):
        seeds = REPO / "config" / "seeds"
        self.assertTrue(seeds.is_dir(), "config/seeds must exist")
        found = 0
        for f in seeds.glob("*.json"):
            found += 1
            self.assertRegex(f.stem, r"^[0-9A-F]{8}$",
                             "%s is not named for a title ID" % f.name)
            entries = json.loads(f.read_text(encoding="utf-8"))
            self.assertIsInstance(entries, list)
            for e in entries:
                self.assertIn("start", e)
                self.assertIn("note", e, "a seed with no reason cannot be "
                                         "audited later")
        self.assertGreater(found, 0, "no seed files found")


if __name__ == "__main__":
    unittest.main()
