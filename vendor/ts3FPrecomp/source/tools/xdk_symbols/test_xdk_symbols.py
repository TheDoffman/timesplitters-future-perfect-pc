"""
Self-check for tools.xdk_symbols' parsing of XbSymbolDatabaseCLI output.

Synthetic lines in the tool's own formats; no game files or CLI needed.
Run: py -3 -m unittest tools.xdk_symbols.test_xdk_symbols
"""

import os
import tempfile
import unittest
from unittest import mock

import tools.xdk_symbols.__main__ as xdk
from tools.xdk_symbols.__main__ import parse_line, parse_output


class ParseLine(unittest.TestCase):
    def test_stdcall_function_with_stack_parameters(self):
        s = parse_line("D3D8__FUN__stdcall__D3DDevice_Clear(psh Count, psh pRects, "
                       "psh Flags, psh Color, psh Z, psh Stencil) = 0x00123450")
        self.assertEqual(s["library"], "D3D8")
        self.assertEqual(s["kind"], "function")
        self.assertEqual(s["name"], "D3DDevice_Clear")
        self.assertEqual(s["call"], "stdcall")
        self.assertEqual(s["address"], 0x00123450)
        self.assertEqual(len(s["params"]), 6)
        self.assertEqual(s["stack_bytes"], 24)

    def test_register_parameters_take_no_stack(self):
        # A thiscall's `this` arrives in ecx; a 64-bit value is two pushes.
        s = parse_line("DSOUND__FUN__thiscall__CMcpxBuffer_Stop_Ex"
                       "(ecx this, psh2 rtTimeStamp, psh dwFlags) = 0x00234560")
        self.assertEqual(s["call"], "thiscall")
        self.assertEqual(s["params"][0], {"type": "ecx", "name": "this"})
        self.assertEqual(s["stack_bytes"], 12)

    def test_function_with_no_parameters(self):
        s = parse_line("DSOUND__FUN__stdcall__DirectSoundEnterCriticalSection() = 0x00220000")
        self.assertEqual(s["params"], [])
        self.assertEqual(s["stack_bytes"], 0)

    def test_variable_whose_name_contains_double_underscores(self):
        s = parse_line("D3D8__VAR__D3DDevice__m_SwapCallback_OFFSET = 0x00001984")
        self.assertEqual(s["kind"], "variable")
        self.assertEqual(s["name"], "D3DDevice__m_SwapCallback_OFFSET")
        self.assertEqual(s["address"], 0x1984)

    def test_unknown_and_plain_forms(self):
        self.assertEqual(parse_line("XAPILIB__UNK__Something = 0x00010000")["kind"], "unknown")
        plain = parse_line("D3D8__D3DDevice_Swap = 0x00200000")
        self.assertEqual(plain["name"], "D3DDevice_Swap")
        self.assertEqual(plain["kind"], "unknown")

    def test_lines_that_are_not_symbols_are_skipped(self):
        text = "\n".join([
            "",
            "ERROR: param_size(9) + buffer_count(9) is too long",
            "D3D8__FUN__stdcall__D3DDevice_Swap(psh Flags) = 0x00200000",
        ])
        symbols = parse_output(text)
        self.assertEqual([s["name"] for s in symbols], ["D3DDevice_Swap"])


class FindCli(unittest.TestCase):
    """A real Visual Studio build tree has several files that start with the
    CLI's name. The finder must return the executable, and prefer Release."""

    def test_the_executable_is_found_among_build_files(self):
        exe = "XbSymbolDatabaseCLI" + (".exe" if os.name == "nt" else "")
        with tempfile.TemporaryDirectory() as repo:
            cli = os.path.join(repo, "third_party", "XbSymbolDatabase",
                               "build", "projects", "cli")
            os.makedirs(os.path.join(cli, "Debug"))
            os.makedirs(os.path.join(cli, "Release"))
            os.makedirs(os.path.join(cli, "XbSymbolDatabaseCLI.dir"))
            for name in ("XbSymbolDatabaseCLI.slnx",
                         "XbSymbolDatabaseCLI.vcxproj",
                         "XbSymbolDatabaseCLI.vcxproj.filters",
                         os.path.join("Debug", exe),
                         os.path.join("Release", exe)):
                open(os.path.join(cli, name), "w").close()
            with mock.patch.object(xdk, "REPO", repo), \
                    mock.patch.dict(os.environ, {}, clear=False):
                os.environ.pop("XBSDB_CLI", None)
                self.assertEqual(xdk.find_cli(),
                                 os.path.join(cli, "Release", exe))

    def test_an_explicit_path_wins(self):
        self.assertEqual(xdk.find_cli("C:/tools/cli.exe"), "C:/tools/cli.exe")


if __name__ == "__main__":
    unittest.main()
