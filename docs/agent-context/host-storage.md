The native mod bootstrap now registers the fence before kernel init, after verifying a source capsule and holding an isolated namespace lease. It checks the actual committed root before exposing source. A permanent pc_storage_lock_locations latch also closes the host picker before folder/preference writes. Ordinary startup remains unfenced; actual scripted matches are still pending.

# Original host storage and retired legacy path module

The optional startup fence is a one-way process-lifetime policy in
guest_storage.c/.h. tsfp_storage_require must run before storage initialization;
it fixes one caller-validated namespace, clears any pre-init profile override
and rejects later nonempty profile overrides. Invalid/late registration closes
storage; no reset API can recover a personal default. tsfp_storage_required
returns0 for ordinary sessions,1 for the immutable root and-1 for a closed fence.
Snapshot reads and commits independently enforce the fence under the state lock.

xbox_require_save_root_utf8 validates an existing absolute directory, then arms
that policy even on invalid input. xbox_path_init consults it before default
selection, mkdir or image preparation. It therefore covers explicit/env/default
and lazy xbox_translate_path initialization, plus later root reinitialization.
An ordinary unfenced process retains its existing root/profile behavior. The
future mod bootstrap must supply a held SaveNamespace before any kernel init;
the fence itself does not authenticate packages or validate namespace ownership.
No launcher or startup route enables this mod policy yet.

Stage0234 selects guest_storage.c/.h and guest_kernel_storage.c through0234 after
0233 before2090. KERNEL_SOURCES excludes kernel_path.c entirely. The original host
adapter includes guest_kernel_paths.inc and supplies all path init/translation,
profile selection, save-root metadata and Windows latest-path interfaces. The
historical file/snapshot/patches remain for unwind provenance; no legacy path
implementation is compiled. kernel.h/types, other file I/O and broader runtime
components remain transitional, so this is not full toolkit independence.

Shared storage selection prioritizes explicit save, nonempty RECOMP_SAVE_DIR,
then a platform default. Explicit empty/over4095-byte roots reject without output
changes. Host adapters validate encoding/create directories before atomic root
commit. Zero-initialized shared state copies game/save/profile snapshots under
one short metadata lock; no OS call/guest callback runs under it. Root commits
preserve profile overrides. A profile may be selected before first init; snapshots
remain unavailable until roots commit. Failed init retains the previous roots.
Init serialization uses a separate native mutex; snapshot readers see complete
root pairs. Translation snapshots selected roots before conversion/joining.

Platform defaults preserve prior behavior: Windows game fallback is CWD, saves
LocalAppData/xboxrecomp (CWD/SaveData fallback). POSIX game fallback is CWD/game;
saves XDG_DATA_HOME/xboxrecomp or HOME/.local/share/xboxrecomp. Caller/env roots
avoid default filesystem setup during tests. Profile overrides require an existing
absolute directory; empty clears. Windows uses strict UTF8 conversion, POSIX native
byte paths. Existing directories/files are never removed to repair failed setup.

The shared compatibility geometry retains this runtime's image sizes: Partition0
is2560 bytes with a512-byte synthetic PARTINFO block at0x800; Partition1 has465400
hex sectors,2 hasFA000,3-5 each177000 (512-byte sectors). The explicit byte codec
writes aligned-independent little-endian entries and preserves output on short
buffers. This is a compatibility image, not a claim of a full physical retail disk.
Public [XboxDev drive mappings](https://xboxdevwiki.net/Hard_Drive#Partitions)
independently establish device roles/start offsets; no external algorithms are
incorporated. The synthesized table/E size remains the prior runtime contract.

Image preparation creates each path exclusively. EXISTS transfers no handle and
must not open/write/resize an existing image, even with custom bytes/sizes. Only
NEW owns a token, receives sparse sizing/table bytes and one final close. Partial
setup closes/discards only newly owned storage. Failed close/cleanup is reported
and retained by the backend without automatic retry; roots are not committed.
Windows discard uses the new file handle, POSIX checks device/inode before unlink
under caller-serialized initialization. Native close uncertainty retains its token
metadata/path independently of initialization-stack context; no blind retry.
Windows prefers sparse images. ERROR_INVALID_FUNCTION/ERROR_NOT_SUPPORTED from
sparse marking instead use regular files after a caller-available free-space
check before each allocation; other errors fail. exFAT therefore needs about
4.9 GiB for all images. Sizing errors still discard only the newly owned image.
Storage errors report operation, image path and Windows code to stderr; failed
initialization also reports directly, independent of kernel log filtering.
POSIX ftruncate is unchanged. Existing images are never repaired or truncated.
The backend's static quarantine remains process-lifetime diagnostic ownership.

Maintained test_guest_storage covers root priority/invalid-output preservation,
pre-init profile/failed commits, literal unaligned sector bytes/sizes, existing
bypass, resize/write/close failure accounting and1000 commits/reads on four native
threads. Eight bad variants compile then fail at runtime. Its real native adapter
case uses disposable explicit roots, six sparse image sizes/allocated bytes,
modified bytes/size/mtime preservation, profile switching, bad-directory rollback,
environment priority and output preservation. Only known Partition0-5 images in
those terminal child fixtures are recycled; no personal roots/defaults are used.
Windows and Mac CI select the same source/tests (native host APIs differ). The
previous path planner/Windows/POSIX fixture tests remain selected. Real build path
and bounded Story checks supplement these contracts; Windows checks do not prove
native Mac title gameplay or full storage/VFS/NT semantics.


Mac CI follow-up in0235: source855b0a0f runtime40987772559 failed only the native
storage fixture compilation (2066 passed/316 skipped/18 subtests). Strict POSIX
feature selection hid O_NOFOLLOW and Darwin st_mtimespec. The native adapter and
fixture now request _DARWIN_C_SOURCE before any headers while preserving POSIX
selection elsewhere, no-follow creation, nanosecond preservation and fatal warnings.
Apple's [namespace-selection header](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/sys/cdefs.h)
defines the Darwin extension mode independently of POSIX selection. Only interface
facts are used; no Apple implementation is incorporated. Old-head Windows native,
both upgrade hosts and both launchers passed. New-head native Mac verification is
pending; this is a source repair, not a passing rerun of the failed old head.

Codeb47b9d9e follow-up: runtime40989944205 and launcher40989944053 both passed
Windows/Mac host checks. Isolated Windows pool Story also passed; source/archive/
generated/game hashes and26 protected files agree. Native Mac title gameplay
remains unverified. See STATUS's stage0235 completed acceptance for exact scope.

Windows nonsparse regression coverage in test_guest_storage.py uses native file
ownership with injected sparse API responses, free-space/query/sizing failures,
exact requested geometry and existing-file byte preservation. Disposable sizing
is capped at 4 KiB; this is not a physical exFAT volume or full game test. The
normal native fixture still verifies real NTFS sparse sizes/allocation. These
cases run in the existing Windows CI storage suite; Windows adapters skip Mac.

Save relocation policy is maintained in root AGENTS.md. Regular images remain
valid when copied from exFAT to NTFS; no sparse conversion is required. Absolute
save-folder settings are not automatically rewritten by moving the installation;
users select a relocated folder explicitly. Future storage changes must preserve
this compatibility and existing bytes, including independently stored saves.

GCC compilation checks retain the early null guard before snapshot locking;
the following enter/ready check is a separate unconditional statement. The
formatting repair changes neither root synchronization nor storage behavior.

The UTF-8 save-root copy also retains separate early snapshot/capacity guards
before strlen/memcpy, so GCC strict builds check it without changing its bytes.

Launcher install/import capacity uses TsfpDiskSpace.Available in tools/release/ImportGame.cs. Walk to an existing destination directory and query DriveInfo there, never Path.GetPathRoot on POSIX: /home and SD storage can be different from /. ISO/folder import, update staging and diagnostic free-space reporting share the helper. LinuxTests exercises a distinct /dev/shm mount through a symlink and rejects insufficient capacity without allocating game data.
