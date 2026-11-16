"""Create and package local mod projects without loading workshop server code."""
from pathlib import Path
import sys

if sys.version_info < (3, 12):
    raise SystemExit('Mod creator needs Python3.12 or newer.')
sys.path.insert(0, str(Path(__file__).resolve().parent))
from mod_creator_cli import main

if __name__ == '__main__':
    raise SystemExit(main())
