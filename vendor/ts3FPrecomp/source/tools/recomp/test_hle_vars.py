"""
Self-check for XDK variables imported by name (HLE_IMPORT_VAR).

Run: py -3 tools/recomp/test_hle_vars.py

A replacement can need the address of an XDK *variable*: XAPI's input
functions are told which device type a call is about by a pointer into the
title's own data (g_DeviceType_Gamepad, g_DeviceType_MU). tools.recomp scans
src/hle for HLE_IMPORT_VAR(Name) and writes hle_var_<Name> into recomp_hle.c
from the title's XDK symbols -- 0 when it cannot be named, so the build links
either way.
"""

import os
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

from tools.recomp.hle import (imported_variables, render_thunks,  # noqa: E402
                              resolve_variables)


def test_markers_are_found():
    with tempfile.TemporaryDirectory() as d:
        with open(os.path.join(d, "hle_input.c"), "w") as fh:
            fh.write("HLE_IMPORT_VAR(g_DeviceType_Gamepad);\n"
                     "  HLE_IMPORT_VAR( g_DeviceType_MU );\n"
                     "/* HLE_IMPORT_VAR(not_at_line_start) */\n")
        assert imported_variables([d]) == {"g_DeviceType_Gamepad",
                                           "g_DeviceType_MU"}


def test_resolution():
    variables = [
        {"name": "g_DeviceType_Gamepad", "address": 0x28C054, "kind": "variable"},
        {"name": "g_DeviceType_Gamepad", "address": 0x28C054, "kind": "variable"},
        {"name": "g_Twice", "address": 0x1000, "kind": "variable"},
        {"name": "g_Twice", "address": 0x2000, "kind": "variable"},
    ]
    values, notes = resolve_variables(
        {"g_DeviceType_Gamepad", "g_Twice", "g_Missing"}, variables)
    assert values == {"g_DeviceType_Gamepad": 0x28C054, "g_Twice": 0,
                      "g_Missing": 0}, values
    assert len(notes) == 2, notes


def test_definitions_are_rendered():
    out = render_thunks({}, {"g_DeviceType_Gamepad": 0x28C054, "g_Missing": 0})
    assert "const uint32_t hle_var_g_DeviceType_Gamepad = 0x0028C054u;" in out, out
    assert "const uint32_t hle_var_g_Missing = 0x00000000u;" in out, out


def test_no_variables_renders_as_before():
    assert "hle_var_" not in render_thunks({})


if __name__ == "__main__":
    for name, fn in list(globals().items()):
        if name.startswith("test_"):
            fn()
            print("ok", name)
