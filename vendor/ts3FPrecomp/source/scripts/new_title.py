"""Scaffold a title project from templates/new-game, ready to lift.

Bringing up a title needs four things that are the same every time -- a
CMakeLists with the project name and the path back to the engine, a main.c
carrying the entry point and the game directory, the shared recomp_manual.c,
and the per-title .gitignore -- and one that is different, which is the entry
point. Doing that by hand is how titles/jsrf ended up without a .gitignore and
why its generated sources showed as untracked on every status for days.

    py -3 scripts/new_title.py gauntlet "Gauntlet Dark Legacy"

The game folder is looked up under games/ by the name given; the entry point
is read from the XBE, so it cannot be transcribed wrongly. Nothing is
overwritten: an existing project is left alone and reported.

Then:

    py -3 scripts/recompile.py "games/<folder>/default.xbe" \\
        --work-dir games/_pipeline/<name>/out --project titles/<name>
"""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TEMPLATE = ROOT / "templates" / "new-game"


def entry_point(xbe):
    """The XBE entry point VA, from the header, XOR'd with the right key.

    Offset 0x0128, obfuscated. Retail and debug images use different keys and
    the right one is whichever yields an address inside the image, which is a
    far better test than guessing at which kind of disc this is.
    """
    data = Path(xbe).read_bytes()
    if len(data) < 0x0130 or data[:4] != b"XBEH":
        raise ValueError("not an XBE: %s" % xbe)
    base = int.from_bytes(data[0x0104:0x0108], "little")
    size = int.from_bytes(data[0x010C:0x0110], "little")
    raw = int.from_bytes(data[0x0128:0x012C], "little")
    for key in (0xA8FC57AB, 0x94859D4B):        # retail, debug
        va = raw ^ key
        if base <= va < base + max(size, 1 << 24):
            return va
    raise ValueError("no XOR key gives an entry point inside the image")


def main(argv=None):
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("name", help="project name, e.g. gauntlet")
    ap.add_argument("game_folder", help="folder under games/, e.g. "
                                        '"Gauntlet Dark Legacy"')
    ap.add_argument("--xbe", default="default.xbe",
                    help="which XBE in that folder (default: default.xbe)")
    args = ap.parse_args(argv)

    game_dir = ROOT / "games" / args.game_folder
    if not game_dir.is_dir():
        print("error: no such game folder: %s" % game_dir, file=sys.stderr)
        return 2
    xbe = next((p for p in game_dir.iterdir()
                if p.name.lower() == args.xbe.lower()), None)
    if xbe is None:
        print("error: no %s in %s" % (args.xbe, game_dir), file=sys.stderr)
        return 2

    proj = ROOT / "titles" / args.name
    if proj.exists():
        print("%s already exists; leaving it alone" % proj.relative_to(ROOT))
        return 0

    try:
        entry = entry_point(xbe)
    except ValueError as exc:
        print("error: %s" % exc, file=sys.stderr)
        return 2

    (proj / "src").mkdir(parents=True)
    shutil.copy(TEMPLATE / "src" / "recomp_manual.c", proj / "src")
    shutil.copy(TEMPLATE / ".gitignore", proj / ".gitignore")

    # CMakeLists: the project name, and the path back to the engine. The
    # template assumes a sibling checkout; titles/<name>/ is two levels down.
    text = (TEMPLATE / "CMakeLists.txt").read_text(encoding="utf-8")
    text = text.replace("project(your_game_recomp C)",
                        "project(%s_recomp C)" % args.name)
    text = text.replace(
        'set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../xboxrecomp" CACHE PATH',
        'set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH')
    text = text.replace("# YOUR_GAME_NAME - Static Recompilation",
                        "# %s - Static Recompilation" % args.game_folder)
    (proj / "CMakeLists.txt").write_text(text, encoding="utf-8")

    rc = subprocess.call([sys.executable, str(ROOT / "scripts" / "regen_title_main.py"),
                          args.name, args.game_folder, "0x%08X" % entry,
                          args.game_folder])
    if rc != 0:
        print("error: regen_title_main failed", file=sys.stderr)
        return rc

    print("%s ready; entry point 0x%08X" % (proj.relative_to(ROOT), entry))
    print("next: py -3 scripts/recompile.py \"games/%s/%s\" "
          "--work-dir games/_pipeline/%s/out --project titles/%s"
          % (args.game_folder, xbe.name, args.name, args.name))
    return 0


if __name__ == "__main__":
    sys.exit(main())
