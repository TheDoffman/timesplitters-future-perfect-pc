"""Run the windowed game with diagnostics written directly to local files."""

import os
import subprocess
import sys
from datetime import datetime
from pathlib import Path


def main():
    executable = Path(sys.argv[1]).resolve()
    if os.environ.get('TSFP_NETWORK') == '1':
        # Online/LAN identities describe the data used at build time. Refuse
        # stale content instead of letting a changed map use that old identity.
        root = Path(__file__).resolve().parents[1]
        subprocess.run([sys.executable, str(root / 'tools/network-identity.py'),
                        '--verify', '--executable', str(executable),
                        '--game', os.environ['RECOMP_GAME_DIR']], check=True)
    log_directory = Path(sys.argv[2]).resolve()
    log_directory.mkdir(parents=True, exist_ok=True)
    tag = datetime.now().strftime("play-%Y%m%d-%H%M%S")
    environment = dict(os.environ, RECOMP_LOG_FILE=str(log_directory / f"{tag}.err"))
    with (log_directory / f"{tag}.out").open("wb") as output, (
        log_directory / f"{tag}.err"
    ).open("wb") as errors:
        # Native file handles avoid console buffering and PowerShell's text
        # redirection. This is also how the bounded gameplay runner launches.
        result = subprocess.run(
            [str(executable)], cwd=executable.parent, stdout=output, stderr=errors,
            creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == 'win32' else 0,
            env=environment,
        )
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
