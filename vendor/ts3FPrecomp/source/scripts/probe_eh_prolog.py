#!/usr/bin/env python3
"""Re-apply the __EH_prolog frame-pointer probe to a title's generated C.

This is a **probe, not the fix**. The fix belongs in tools/recomp, which
emits the code this script patches; see docs/technical/eh-prolog-frames.md.
Generated code is regenerated and gitignored, so a re-lift wipes the patch
and this script puts it back.

What it patches
---------------
MSVC's `__EH_prolog` builds its *caller's* frame pointer and hands it back in
ebp. The lifter gives every frameless function a local C `ebp`, so the lifted
`__EH_prolog` assigns the new frame to a local that is discarded on return,
and the caller carries on with an inherited frame pointer that points at
somebody else's frame. Every `[ebp-N]` access in that caller then lands in an
outer function's locals.

Two edits make it behave:

1. In `__EH_prolog` itself, publish the frame it just built to `g_ebp` and
   `g_seh_ebp`.
2. At every call site, reload `ebp` from `g_seh_ebp` afterwards, because the
   caller captured its `ebp` before the call and would otherwise never see it.

Usage
-----
    py -3 scripts/probe_eh_prolog.py titles/outrun2 0x00185AD0

The address is the title's `__EH_prolog`. Find it by looking for a lifted
function of about 31 bytes that pushes -1, pushes eax, reads and writes
`XBOX_FS_BASE` and ends `ebp = esp + 0xC`.
"""

import pathlib
import re
import sys


def find_prolog_body(gen_dir, name):
    for path in sorted(gen_dir.glob("*.c")):
        text = path.read_text(encoding="utf-8", errors="ignore")
        if "void %s(void)" % name in text:
            return path, text
    return None, None


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 2

    project = pathlib.Path(argv[1])
    addr = int(argv[2], 0)
    name = "sub_%08X" % addr
    gen = project / "src" / "recomp" / "gen"
    if not gen.is_dir():
        print("no generated code at %s -- lift the title first" % gen)
        return 1

    # 1. Publish the frame that __EH_prolog built.
    path, text = find_prolog_body(gen, name)
    if path is None:
        print("%s is not in the generated code" % name)
        return 1

    anchor = "    ebp = esp + 0xC;\n    PUSH32(esp, eax);"
    if "PROBE: publish" in text:
        print("%s: already patched" % path.name)
    elif text.count(anchor) != 1:
        print("%s: expected one __EH_prolog tail in %s, found %d -- check that "
              "0x%08X really is __EH_prolog" % (path.name, name,
                                                text.count(anchor), addr))
        return 1
    else:
        text = text.replace(
            anchor,
            "    ebp = esp + 0xC;\n"
            "    g_ebp = ebp; g_seh_ebp = ebp;"
            " /* PROBE: publish the frame __EH_prolog built */\n"
            "    PUSH32(esp, eax);", 1)
        path.write_text(text, encoding="utf-8")
        print("%s: published the frame pointer in %s" % (path.name, name))

    # 2. Reload ebp at every call site.
    call = re.compile(
        r"(RECOMP_ABI_CALL_POP\(0x%08Xu, %s, 0u\); /\* call 0x%08X \*/\n\n"
        r"loc_[0-9A-F]+: ;\n)" % (addr, name, addr))
    total = 0
    for path in sorted(gen.glob("*.c")):
        text = path.read_text(encoding="utf-8", errors="ignore")
        if "PROBE: __EH_prolog rebuilt" in text:
            continue
        patched, n = call.subn(
            lambda m: m.group(1)
            + "    ebp = g_seh_ebp;"
            " /* PROBE: __EH_prolog rebuilt the frame */\n", text)
        if n:
            path.write_text(patched, encoding="utf-8")
            total += n
    print("reloaded ebp at %d call sites" % total)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
