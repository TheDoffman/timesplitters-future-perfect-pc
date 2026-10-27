#!/usr/bin/env python3
"""Rewrite titles/<name>/src/main.c from templates/new-game/src/main.c.

The title projects in titles/ differ from the template only in three defines
(entry point, XBE path, game directory) and the banner name. Keeping them as
copies means a template fix has to be applied by hand to each; this applies it.

    py -3 scripts/regen_title_main.py burnout2 "Burnout 2" 0x000E2555 "Burnout 2 - Point of Impact"

Paths are written relative to the executable's directory,
titles/<name>/build/<Config>/, which is where scripts/run_and_report.py
starts it from.
"""
import pathlib
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
BS = chr(92)


def main(argv):
    if len(argv) != 5:
        print(__doc__)
        return 2
    title_dir, name, entry, game_folder = argv[1:]
    rel = BS.join(["..", "..", "..", "..", "games", game_folder])
    c_rel = rel.replace(BS, BS + BS)            # as it appears inside a C string
    lines = (REPO / "templates" / "new-game" / "src" / "main.c").read_text(
        encoding="utf-8").split("\n")
    out = []
    replaced = set()
    for line in lines:
        if line.startswith("#define YOUR_GAME_ENTRY_POINT"):
            line = "#define YOUR_GAME_ENTRY_POINT   %s  /* XBE entry point VA */" % entry
            replaced.add("entry")
        elif line.startswith("#define YOUR_GAME_XBE_PATH"):
            out.append("/* Relative to the executable's own directory, "
                       "titles/<title>/build/<Config>/,")
            out.append(" * which is where scripts/run_and_report.py starts it. "
                       "Run it from there by")
            out.append(" * hand too, or the XBE is not found. */")
            line = '#define YOUR_GAME_XBE_PATH      "%s%s%sdefault.xbe"' % (c_rel, BS, BS)
            replaced.add("xbe")
        elif line.startswith("#define YOUR_GAME_DIR"):
            line = '#define YOUR_GAME_DIR            "%s"' % c_rel
            replaced.add("dir")
        out.append(line.replace("YOUR_GAME_NAME", name))
    if replaced != {"entry", "xbe", "dir"}:
        print("template layout changed; defines not found:", replaced)
        return 1
    dest = REPO / "titles" / title_dir / "src" / "main.c"
    dest.write_text("\n".join(out), encoding="utf-8")
    print("wrote", dest)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
