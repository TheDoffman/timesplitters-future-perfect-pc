# Independent runtime replacement

The objective is to replace the code taken from `ts3FPrecomp` with this
project's implementations. Standard dependencies such as SDL, FFmpeg,
Capstone, the compiler and host graphics SDKs remain allowed. Copying upstream
files into this repository does not meet that objective. The earlier vendoring
proposal in PR #369 remains a separate, unmerged availability solution.

**The replacement is not complete.** The current game still executes using the
pinned runtime and its patches. Disc import and executable analysis now have
independent implementations, as do the disassembler's image view and instruction
classification, traversal and direct-edge reachability. Do not present the port as an original implementation of
the complete Xbox runtime or recompiler.

## Active goal and completion gates

The user requested completion of the remaining replacement as an active goal
on 2026-10-26. Work proceeds through the milestones below without requiring a
new request for each component. Keep each accepted stage reviewable in Git;
the goal does not authorize merging into main. Host validation remains separate
from implementation status.

1. **Own the tooling.** Finish instruction traversal, table/prologue recovery,
   function discovery, labels/cross-references, analysis caches/output, symbol
   integration and x86-to-C translation. Retain permitted Capstone; verify
   integer/flags, floating point, indirect calls and ABI behavior.
2. **Own the guest execution foundation.** Replace memory, executable loading,
   dispatch, kernel objects/imports, threads/TLS, timing, filesystems and saves.
3. **Own the device services.** Replace graphics/GPU commands, audio/movies,
   input/devices and networking around permitted host libraries/adapters.
4. **Own the title and delivery integration.** Replace title entry/hooks/build
   targets, migrate patches into original source, and update packaging/tests.
5. **Prove independence.** Audit executable and shipped source dependencies,
   fixtures, headers and historical copied text. Generate/build/test/package
   with the old toolkit and vendored snapshot unavailable. Retain notices for
   any third-party material still shipped. Verify isolated game behavior and
   record unavailable host checks accurately before assessing completion.

The goal stays active while required implementation or acceptance work remains.
Finishing a component, passing a build or exhausting a work session is not
completion of the whole goal. The table below and STATUS.md record progress.

## Replacement sequence and acceptance

| Area | Current replacement status | Evidence required before removing the old implementation |
| --- | --- | --- |
| Disc catalog and extraction | Implemented in `tools/disc_image.py` and `tools/import-game.py`; Windows Extract uses it, same Python CLI available on Mac | Format fixtures, bounds/cycles/names, output preservation, private supported-XBE fixture; full original ISO extraction on both hosts remains unverified |
| Executable parser and analysis model | Implemented in `tools/xbe_image.py`; production generation uses the compatibility bridge | Synthetic valid/malformed images, verified-US metadata equality, regeneration and bounded gameplay; native Mac validation pending |
| Disassembler input and instruction model | Project-owned `recomp_image.py` and `x86_decode.py`; production consumer uses them with permitted Capstone | Mapping/operand fixtures, cache invalidation, full regeneration and bounded gameplay; native Mac pending |
| Instruction traversal and reachability | Project-owned `x86_flow.py`; production engine inherits scan, realignment, reachability and queries | Synthetic edges/cycles/overlaps/limits, existing consumer tests, full regeneration and bounded gameplay; native Mac pending |
| Function discovery and x86-to-C translation | Still upstream, using the independent input layer | Independent control-flow recovery; flags, x87/SSE, rounding, indirect calls, exceptions, stack/ABI and memory tests; deterministic generation from the supported XBE |
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

### Disassembler input and instruction model

`tools/x86_flow.py` now owns the next layer: linear scanning across valid runs,
bounded realignment at an explicit address, finite reachability over cached
direct edges, overlap/range queries and direct/indirect call-reference queries.
Calls retain fall-through; unconditional jumps and returns stop it. Missing
decodes and unknown indirect jump targets end a path, without inventing code.
Realignment preserves overlapping streams and stops at a known successor or
terminator. A zero/negative instruction limit performs no decode.

The production engine inherits those methods through
`independent-disasm-traversal.patch`, after the input patch. Table recovery,
prologue/body probes and function boundaries still come from the existing
engine/detector; their presence is not concealed by the new base class.
The traversal source is included in both cache fingerprints. Its independent
tests are in `tests/test_x86_flow.py`; the consumer integration test asserts
that production traversal methods originate in the project module.

`tools/recomp_image.py` builds a read-only image from the independent XBE parser,
without requiring cached JSON or the external toolkit. Explicit or exactly named
sibling analysis JSON must match that executable's layout and imports; unrelated
JSON is never selected by a broad search. Headers use the actual header size.
Exact reads stay within a single file-backed region, rejecting virtual zero fill
and cross-region reads. Code probes accept a shorter window at a section boundary
instead of reading unrelated bytes that happen to follow it in the file.

`tools/x86_decode.py` supplies the instruction model, Capstone decoder setup and
operand classification used by production disassembly. It follows the
[Capstone Python detail/operand API](https://www.capstone-engine.org/lang_python.html)
with the existing pinned standard dependency. Direct transfers, absolute memory
references, address-like immediates and indexed table candidates retain the
consumer interface. A candidate is not proof of a function or table; recovery,
table/prologue recovery, function discovery, labels, cross-references and
translation remain upstream.

`patches/independent-disasm-input.patch` removes the old loader/model/classifier
bodies and connects the new implementations. It adapts code-probe reads and adds
the independent dependencies to the disassembly cache key. The build generation
fingerprint includes both the new source and the remaining disassembler source.
This prevents a changed reader/classifier from reusing stale analysis or C.
Deletion text in patches still records historical upstream code.

Tests: `python -m pytest tests/test_recomp_input.py -q`. Synthetic fixtures check
data reads, probe windows, stale JSON and x86 targets, including dynamic operands
that must not be treated as static addresses. A temporary directory containing
only the project modules and export data proves the input layer runs without the
toolkit. When the patched checkout is available, the test also runs its existing
97 disassembler tests against the new input layer. This stage does not replace
the full disassembler or recompiler.

### Executable analysis and disc import

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
