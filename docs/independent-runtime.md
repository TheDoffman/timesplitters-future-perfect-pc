# Independent runtime replacement

The objective is to replace the code taken from `ts3FPrecomp` with this
project's implementations. Standard dependencies such as SDL, FFmpeg,
Capstone, the compiler and host graphics SDKs remain allowed. Copying upstream
files into this repository does not meet that objective. The earlier vendoring
proposal in PR #369 remains a separate, unmerged availability solution.

**The replacement is not complete.** The current game still executes using the
pinned runtime and its patches. Disc import and executable analysis now have
independent implementations. Do not present the port as an original implementation of
the complete Xbox runtime or recompiler.

## Replacement sequence and acceptance

| Area | Current replacement status | Evidence required before removing the old implementation |
| --- | --- | --- |
| Disc catalog and extraction | Implemented in `tools/disc_image.py` and `tools/import-game.py`; Windows Extract uses it, same Python CLI available on Mac | Format fixtures, bounds/cycles/names, output preservation, private supported-XBE fixture; full original ISO extraction on both hosts remains unverified |
| Executable parser and analysis model | Implemented in `tools/xbe_image.py`; production generation uses the compatibility bridge | Synthetic valid/malformed images, verified-US metadata equality, regeneration and bounded gameplay; native Mac validation pending |
| Function discovery and x86-to-C translation | Still upstream | Independent decoder integration and control-flow recovery; flags, x87/SSE, rounding, indirect calls, exceptions, stack/ABI and memory tests; deterministic generation from the supported XBE |
| Guest memory, executable loader and kernel | Still upstream | Guest address-space ownership, protection, imports, threads/TLS, synchronization, filesystem and saves; boot and isolated save/load on Windows and Mac |
| Graphics and GPU command handling | Still upstream, with project adapters/patches | Own resource/state/command model, texture formats, shaders, draw/clear/present paths; asset-free GPU tests and representative captured/live scenes on both hosts |
| Audio and movies | Still upstream, with project integration | Own Xbox-facing buffer/voice/clock services and movie adapter around permitted libraries; decode, mixing, resampling, looping, synchronization and pause/resume |
| Input, devices and networking | Mixed project and upstream code | Own Xbox-facing controller/device/network services using shared PC input and permitted SDL; hotplug, multiplayer and timing contracts |
| Title startup, hooks and builds | Still upstream title target plus project patches | Own entry/dispatch integration and title configuration, migrated patches, native builds and bounded gameplay; generated game code stays private |
| Final removal and source audit | Not started | Build/generate/test/package with the old checkout and snapshot unavailable; remove obsolete patch context and copied fixtures; audit runtime source, tests, headers and packages, retaining notices for any code still shipped |

This order keeps the current port usable while replacements gain evidence. A
component is finished only when a production entry point uses the replacement,
tests verify behavior, and that component can operate with the old source absent.
Whole-project independence requires all applicable rows, not merely successful
compilation, renamed symbols or removal of attribution links.

## Disc importer: implementation and provenance

The new Python reader was authored for this project from published binary-format
descriptions. It imports only Python standard-library modules. It does not import,
copy or patch `tools.xiso`; it runs from a directory containing just the two new
Python files. The constants describe the file format, not an upstream code API.

References:

- [XboxDevWiki volume descriptor](https://xboxdevwiki.net/XDVDFS).
- [Xbox-Linux filesystem layout](https://multimedia.cx/xdvdfs.html).
- [XISO Studio format reference](https://purelogiccode.github.io/XISOStudio/XDVDFS-Technical-Documentation.html).

Validation is in `tests/test_disc_image.py`. The Windows driver test constructs
a disposable project with no external checkout or virtual environment. Public
tests use original synthetic bytes. The optional private-XBE fixture packages
only the user's existing supported executable and synthetic payloads into a
temporary image; this is not evidence of a full retail-disc extraction.

The original ISO recorded in this Windows checkout is no longer present. Keep
the existing `private/game`; do not create another full extraction merely to
fill that evidence gap. The importer has a read-only verification command for
when an original ISO is available.

## Current commands

Executable analysis uses the same project-owned reader on Windows and Mac:

```sh
python tools/xbe_image.py private/game/default.xbe --json private/game/default_analysis.json
python -m pytest tests/test_xbe_image.py tests/test_recomp_generation.py -q
```

The reader was authored from [Caustik's XBE format description](https://xbdev.net/fileformats/xbe/NonOfficialXBEFileFormat.pdf).
It validates file/virtual section extents, retail/debug entry and thunk decoding,
certificate metadata, library versions and TLS initialization ranges. Analysis
omits certificate secrets. It preserves the attributes and JSON consumed by the
current disassembler/recompiler; those consumers remain upstream implementations.
The supported title is the pinned US executable; synthetic debug images are not
evidence that other titles or beta executables work.

`tools/xbe_kernel_exports.def` is the unmodified CC0 export declaration table
from [XboxDev/nxdk at 03acd1415f7ccc3127bcfb629224aacdd95d783c](https://github.com/XboxDev/nxdk/blob/03acd1415f7ccc3127bcfb629224aacdd95d783c/lib/xboxkrnl/xboxkrnl.exe.def),
credited to Stefan Schmidt in its SPDX header. This standard ABI data is retained
as an allowed third-party dependency; it is not ts3FPrecomp parser code.

`patches/independent-xbe-reader.patch` removes the installed upstream parser body
and routes its old module entry point to this reader. The two ordinal audit
consumers use the new table reader too. Build drivers set `TSFP_PROJECT_ROOT`
to the source checkout, including Mac builds sharing another checkout's ignored
dependencies. Direct toolkit commands can discover the containing project, or
must set that variable when dependencies are elsewhere. There is no fallback to
the old parser. The patch's deletion records remain historical upstream text.
The independent source, export data and bridge enter the generation fingerprint.
Windows automatically reruns all four stages when that fingerprint changes;
Mac continues to require `--generate` for stale generation.

Python 3.12 or newer is sufficient; Git, the runtime checkout and third-party
Python packages are unnecessary for disc import:

```sh
python tools/import-game.py --iso /path/to/game.iso --verify-only
python tools/import-game.py --iso /path/to/game.iso --output private/game
```

Windows also supports extraction before Setup:

```powershell
.\tsfp.ps1 Extract -Python 'C:\path\to\python.exe' -Iso 'C:\path\to\game.iso'
```

The executable must match the existing US SHA-256 pin, and `pak`/`xmv` must be
directories. Image cataloging and existing-file checks happen before writes.
Identical existing files are reused; conflicting files and path aliases stop
the operation. Unrelated files are retained. Completed files remain after a
handled interruption and can be reused on retry; incomplete temporary files
from handled failures are removed. Forced process termination can leave a
temporary file, which is never mistaken for a completed game file.

The destination must support hard links (ordinary NTFS/APFS project locations
do). Publication is exclusive, so a concurrent destination file is not replaced.
Unsupported filesystems fail without replacing existing files. Source images
are read-only, payload copies are bounded and verified, and imports check free
space. Personal settings and saves are outside the output path.

Run the focused checks with `python -m pytest tests/test_disc_image.py -q`.
CI runs them on Windows and macOS. Build/generation still need the existing
toolkit until subsequent replacements are completed.
