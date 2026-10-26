#!/usr/bin/env python3
"""
Survey a library of Xbox titles and rank them as recompilation targets.

LTCG status and XDK build cannot be looked up on a wiki. LTCG is a per-build
linker setting recorded nowhere public, and the XDK build is a property of the
disc, not of the title. Both are in the XBE header, so read them.

Reports per title: XDK version, D3D8 vs D3D8LTCG, the library list, RenderWare
detection and version, .text size, kernel import count, and demand-loaded
sections.

Usage:
    python3 scripts/survey_xbe_library.py /path/to/isos --csv survey.csv
    python3 scripts/survey_xbe_library.py /path/to/isos --xdk 5849
    python3 scripts/survey_xbe_library.py /path/to/one/default.xbe --verbose

Reads default.xbe straight out of .iso/.xiso images, so a library does not have
to be unpacked just to answer "which XDK built this?".

The ranking is a starting point, not a verdict. It cannot see hand-rolled push
buffers, threading complexity, or whether you actually want to play the game.
"""

import argparse
import csv
import os
import re
import sys
import tempfile
from pathlib import Path

# Import the XBE parser from the toolkit rather than re-deriving header offsets.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from tools.xbe_parser.xbe_parser import (  # noqa: E402
    XBEParser,
    SECTION_PRELOAD,
)
from tools.xiso.xdvdfs import Xiso, XisoError  # noqa: E402

DISC_SUFFIXES = (".iso", ".xiso")

# Sections whose presence signals work the toolkit does not yet cover.
COSTLY_SECTIONS = {
    "WMADEC": "WMA audio decoding is not implemented",
    "XONLINE": "Xbox Live code path",
}

COSTLY_LIBRARIES = {
    "XONLINE": "Xbox Live",
    "XONLINES": "Xbox Live",
    "XNET": "networking",
    "XVOICE": "voice chat",
}

RENDERWARE_RE = re.compile(rb"RenderWare(?:\x00|\s)*(?:Version|V)?\s*"
                           rb"([0-9]+\.[0-9]+(?:\.[0-9]+)*)", re.I)

# RenderWare builds leave Perforce $Id: strings from the SDK sources, e.g.
#   @@(#)$Id: //RenderWare/RW34Active/rwsdk/src/bacamera.c#3 $
# "RW34" is the SDK branch, i.e. RenderWare 3.4. Retail builds rarely carry a
# printed version string, so in practice this is the pattern that identifies
# the engine version.
RENDERWARE_SDK_RE = re.compile(rb"//RenderWare/RW(\d)(\d)")


class Survey:
    """Everything the XBE header can tell us about one title."""

    def __init__(self, path, data=None):
        # `data` is the XBE image when it came from inside a disc image, where
        # `path` is only a label. A loose .xbe is read from disk as usual.
        self.path = Path(path)
        self.data = data
        self.error = None

        self.title = ""
        self.title_id = 0
        self.region = 0
        self.is_debug = False

        self.libraries = []        # list of (name, major, minor, build)
        self.xdk_build = 0
        self.d3d_library = ""      # "D3D8", "D3D8LTCG", or "" if absent
        self.ltcg = False

        self.renderware = False
        self.renderware_version = ""

        self.text_size = 0
        self.image_size = 0
        self.num_sections = 0
        self.demand_loaded = []    # section names without the preload flag
        self.kernel_imports = 0
        self.notes = []

        try:
            self._parse()
        except Exception as exc:                       # noqa: BLE001
            self.error = f"{type(exc).__name__}: {exc}"

    def _parse(self):
        # XBEParser only reads from a path, so an image pulled out of an ISO
        # goes via a temporary file rather than reaching into its internals.
        if self.data is None:
            raw = self.path.read_bytes()
            xbe = XBEParser(str(self.path)).parse()
        else:
            raw = self.data
            handle, tmp = tempfile.mkstemp(suffix=".xbe")
            try:
                with os.fdopen(handle, "wb") as f:
                    f.write(raw)
                xbe = XBEParser(tmp).parse()
            finally:
                os.unlink(tmp)

        self.title = xbe.certificate.title_name
        self.title_id = xbe.certificate.title_id
        self.region = xbe.certificate.game_region
        self.is_debug = xbe.header.is_debug
        self.image_size = xbe.header.image_size
        self.kernel_imports = len(xbe.kernel_imports)

        for lib in xbe.libraries:
            name = lib.name.strip()
            self.libraries.append((name, lib.major, lib.minor, lib.build))

            # The XDK build number is shared across a disc's libraries; take
            # the highest seen rather than trusting any single entry.
            self.xdk_build = max(self.xdk_build, lib.build)

            upper = name.upper()
            if upper.startswith("D3D8"):
                # "D3D8LTCG" means link-time code generation. It makes signature
                # matching harder but is not disqualifying: the existing proven
                # target is itself an LTCG build.
                self.d3d_library = name
                self.ltcg = "LTCG" in upper
            if upper in COSTLY_LIBRARIES:
                self.notes.append(f"{name} ({COSTLY_LIBRARIES[upper]})")

        self.num_sections = len(xbe.sections)
        for sec in xbe.sections:
            if sec.name == ".text":
                self.text_size = sec.virtual_size
            if not sec.flags & SECTION_PRELOAD:
                self.demand_loaded.append(sec.name)
            bare = sec.name.lstrip(".").upper()
            if bare in COSTLY_SECTIONS:
                self.notes.append(f"{sec.name} section ({COSTLY_SECTIONS[bare]})")

        match = RENDERWARE_RE.search(raw)
        sdk = RENDERWARE_SDK_RE.search(raw)
        if match:
            self.renderware = True
            self.renderware_version = match.group(1).decode("ascii", "replace")
        elif sdk:
            self.renderware = True
            self.renderware_version = (sdk.group(1).decode() + "."
                                       + sdk.group(2).decode())
        elif b"RenderWare" in raw or b"RwEngine" in raw:
            self.renderware = True

    # -- ranking ---------------------------------------------------------

    def score(self):
        """Lower is a better first target.

        Weighted so that the things that actually stop a bring-up dominate:
        unimplemented subsystems first, then code volume.
        """
        if self.error:
            return float("inf")

        score = 0.0

        # Code volume is the main driver of lifting and debugging time.
        score += self.text_size / (256 * 1024)

        # A documented engine means someone else has already mapped the
        # structures you will be staring at in the debugger.
        if self.renderware:
            score -= 3.0

        # Each unimplemented subsystem is a bring-up blocker, not a detail.
        score += 5.0 * len(self.notes)

        # Demand-loaded sections must be paged in on demand at runtime; each is
        # a chance to fault somewhere unrelated to the code you are debugging.
        score += 0.5 * len(self.demand_loaded)

        # Debug XBEs use debug-era XDK patterns the toolkit sees less often.
        if self.is_debug:
            score += 2.0

        return score

    def row(self):
        return {
            "title": self.title,
            "title_id": f"0x{self.title_id:08X}",
            "path": str(self.path),
            "xdk_build": self.xdk_build,
            "d3d": self.d3d_library,
            "ltcg": "yes" if self.ltcg else "no",
            "renderware": self.renderware_version or ("yes" if self.renderware else ""),
            "text_kb": round(self.text_size / 1024, 1),
            "image_kb": round(self.image_size / 1024, 1),
            "kernel_imports": self.kernel_imports,
            "sections": self.num_sections,
            "demand_loaded": len(self.demand_loaded),
            "debug": "yes" if self.is_debug else "no",
            "concerns": "; ".join(self.notes),
            "score": round(self.score(), 2),
            "error": self.error or "",
        }


def _dedup(paths):
    """rglob is case-sensitive on POSIX; collapse case-insensitive duplicates."""
    seen, unique = set(), []
    for p in paths:
        key = str(p).lower()
        if key not in seen:
            seen.add(key)
            unique.append(p)
    return unique


def find_targets(root: Path):
    """Yield (label, data) for every title under root.

    Accepts loose .xbe files and disc images alike. Reading default.xbe out of
    an ISO avoids unpacking a whole library just to answer "which XDK?", which
    is the question you have before you have decided what to extract.
    """
    if root.is_file():
        candidates = [root]
    else:
        candidates = _dedup(
            sorted(root.rglob("*.xbe")) + sorted(root.rglob("*.XBE"))
            + sorted(root.rglob("*.iso")) + sorted(root.rglob("*.ISO"))
            + sorted(root.rglob("*.xiso")) + sorted(root.rglob("*.XISO")))

    targets = []
    for path in candidates:
        if path.suffix.lower() not in DISC_SUFFIXES:
            targets.append((path, None))
            continue

        # A disc image: pull default.xbe out without unpacking the rest.
        try:
            image = Xiso(str(path))
        except (XisoError, OSError) as exc:
            targets.append((path, _Unreadable(f"not a readable disc image: {exc}")))
            continue
        try:
            entry = image.find("default.xbe")
            if entry is None:
                targets.append((path, _Unreadable("no default.xbe in image")))
            else:
                targets.append((path, image.read(entry)))
        except (XisoError, OSError) as exc:
            targets.append((path, _Unreadable(f"could not read default.xbe: {exc}")))
        finally:
            image.close()
    return targets


class _Unreadable:
    """Marks a target we could not get an XBE out of, with the reason."""

    def __init__(self, reason):
        self.reason = reason


def print_verbose(s: Survey):
    print(f"\n{'=' * 66}")
    print(f"  {s.title or '(no title)'}")
    print(f"  {s.path}")
    print("=" * 66)

    if s.error:
        print(f"  ERROR: {s.error}")
        return

    print(f"  Title ID        0x{s.title_id:08X}")
    print(f"  Build           {'debug' if s.is_debug else 'retail'}")
    print(f"  XDK build       {s.xdk_build or 'unknown'}")
    print(f"  Direct3D        {s.d3d_library or 'not linked'}"
          f"{'   (LTCG)' if s.ltcg else ''}")
    print(f"  RenderWare      {s.renderware_version or ('detected' if s.renderware else 'not detected')}")
    print(f"  .text           {s.text_size / 1024:.1f} KB")
    print(f"  Image           {s.image_size / 1024:.1f} KB")
    print(f"  Kernel imports  {s.kernel_imports}")
    print(f"  Sections        {s.num_sections} "
          f"({len(s.demand_loaded)} demand-loaded)")

    if s.libraries:
        print("\n  Libraries:")
        for name, major, minor, build in sorted(s.libraries):
            print(f"    {name:<16} {major}.{minor}.{build}")

    if s.demand_loaded:
        print("\n  Demand-loaded sections:")
        print("    " + ", ".join(s.demand_loaded))

    if s.notes:
        print("\n  Concerns:")
        for note in s.notes:
            print(f"    - {note}")

    print(f"\n  Score           {s.score():.2f}  (lower is a better first target)")


FIELDS = ["score", "title", "xdk_build", "d3d", "ltcg", "renderware", "text_kb",
          "kernel_imports", "demand_loaded", "debug", "concerns", "title_id",
          "image_kb", "sections", "path", "error"]


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("path",
                    help="An XBE, a disc image, or a directory containing either")
    ap.add_argument("--csv", metavar="FILE", help="Write full results as CSV")
    ap.add_argument("--verbose", "-v", action="store_true",
                    help="Print a full report per title")
    ap.add_argument("--limit", type=int, metavar="N",
                    help="Show only the N best-ranked titles in the table")
    ap.add_argument("--xdk", type=int, metavar="BUILD",
                    help="Show only titles built with this XDK build (e.g. 5849). "
                         "Bringing up a second title on a known-good XDK first "
                         "means anything that breaks is a generality bug rather "
                         "than an XDK difference.")
    args = ap.parse_args()

    root = Path(args.path)
    if not root.exists():
        print(f"error: not found: {root}", file=sys.stderr)
        return 1

    targets = find_targets(root)
    if not targets:
        print(f"error: no .xbe or disc images under {root}", file=sys.stderr)
        return 1

    print(f"Surveying {len(targets)} title(s)...", file=sys.stderr)

    surveys = []
    for path, data in targets:
        if isinstance(data, _Unreadable):
            survey = Survey(path, data=b"")
            survey.error = data.reason
        else:
            survey = Survey(path, data=data)
        surveys.append(survey)

    if args.xdk is not None:
        matched = [s for s in surveys if s.xdk_build == args.xdk]
        if not matched:
            builds = sorted({s.xdk_build for s in surveys if s.xdk_build})
            print(f"\nNo title on XDK {args.xdk}.", file=sys.stderr)
            print(f"Builds present: {', '.join(str(b) for b in builds) or 'none readable'}",
                  file=sys.stderr)
            return 1
        surveys = matched

    surveys.sort(key=Survey.score)

    if args.verbose:
        for s in surveys:
            print_verbose(s)
    else:
        shown = surveys[:args.limit] if args.limit else surveys
        print()
        print(f"{'SCORE':>6}  {'TITLE':<30} {'XDK':>6} {'D3D':<9} "
              f"{'RW':<8} {'.text':>8} {'IMP':>4}  CONCERNS")
        print("-" * 100)
        for s in shown:
            if s.error:
                print(f"{'':>6}  {s.path.name:<30} ERROR: {s.error}")
                continue
            print(f"{s.score():>6.1f}  {(s.title or s.path.name)[:30]:<30} "
                  f"{s.xdk_build:>6} {s.d3d_library[:9]:<9} "
                  f"{(s.renderware_version or ('yes' if s.renderware else '-'))[:8]:<8} "
                  f"{s.text_size / 1024:>7.0f}K {s.kernel_imports:>4}  "
                  f"{'; '.join(s.notes)}")

    # Which XDK builds are in this library, and how many titles on each. This
    # is usually the next question after "what should I port?", because a
    # second title on an XDK you have already brought up isolates generality
    # bugs from XDK differences.
    builds = {}
    for s in surveys:
        if not s.error and s.xdk_build:
            builds[s.xdk_build] = builds.get(s.xdk_build, 0) + 1
    if len(builds) > 1 or (builds and not args.xdk):
        print("\nXDK builds present:")
        for build in sorted(builds):
            titles = builds[build]
            print(f"  {build}   {titles} title{'s' if titles != 1 else ''}")
        print("  (--xdk BUILD narrows the table to one of these)")

    if args.csv:
        with open(args.csv, "w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=FIELDS)
            writer.writeheader()
            for s in surveys:
                writer.writerow(s.row())
        print(f"\nWrote {args.csv}", file=sys.stderr)

    print("\nThe ranking is a starting point. It reads the XBE header only, so it",
          file=sys.stderr)
    print("cannot see hand-rolled push buffers, threading complexity, or whether",
          file=sys.stderr)
    print("you want to play the game. Bring up a second title on the same XDK as",
          file=sys.stderr)
    print("your proven one first: anything that breaks is something the project",
          file=sys.stderr)
    print("wrongly treated as universal.", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
