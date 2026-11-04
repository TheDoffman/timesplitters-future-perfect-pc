# Linux experimental release candidate — 2026-11-04

The local candidate is ready for external Linux testing. Public upload is still
pending review and native desktop acceptance. This session created no tag,
GitHub release, PR, push, network-forwarding change or Steam Deck modification.

Candidate: `v0.4.2-linux-experimental.1` (Linux x64 only), Ubuntu24.04 /
glibc2.39 or newer with a desktop and working system Vulkan driver. Launcher1.4.2
is bundled. Game data, personal settings and saves are excluded.

- ZIP: `TimeSplitters-Future-Perfect-PC-v0.4.2-linux-experimental.1-linux-x64.zip`
- SHA256: `a859e409e83ec023065103bb3d1a31760faf5e2e66f0784012711d59a9cb4638`
- Size:801,892,380 bytes; corresponding source archives are included.
- Game build source:`b7ed598c89b91c15092def3d82be3d14fb02e63d`.
- Packaging/source-snapshot commit:`c19f400f75110749da64a9ae7d11e5f1f50700cc`.
- Native output:`build/linux-experimental-release-12/`; verified Windows copy:
  `build/linux-experimental-release/` in the release editing worktree.
- Sidecars:ZIP SHA256, external BUILD.json and draft notes with the named channel,
  packaging commit, Linux platform and actual minimum launcher. These prepare
  reviewable assets; they do not publish anything.

The task branch is `feature/linux-experimental-release`, based on the unmerged
Linux completion branch and merged with main63896020. Other chats' dirty Windows
checkout and the original game-data copy were preserved. Changes are committed
locally; remote publication/synchronization is not claimed.

## Repairs made

The current complete shared patch stack now applies after explicitly deduplicating
one identical declaration guard. The actual native game link failure from the new
shared settings navigation was fixed by exporting the SDL key/pad contracts.
Windows and SDL use one menu-alias helper; Mac uses the same SDL adapter.

The release decoder is a pinned FFmpeg6.1.5 WMV2-only LGPL build, replacing the
preview's host FFmpeg/external-codec closure. Source, archive, extracted files,
configuration, license and binaries are checked. Native `--release-media` builds
record the exact dependency receipt; ordinary developer builds cannot silently
be repackaged as release builds. System FFmpeg remains intact.

ELF relocation now includes every launcher/Python root, prefers staged providers,
keeps distro and wheel provenance distinct, and preserves the .NET single-file
launcher byte-for-byte. The real-ELF regression executes a relocated alternate
provider and checks preservation of an appended single-file overlay.

Clean-userspace tests found missing dynamic Avalonia session libraries, GTK SVG
modules, font configuration and MIME data. Those are now included where needed;
the entry point builds a relocatable per-user GTK module cache. MIME data is
regenerated from the single owned source XML, never the host's merged cache.
Every shipped ELF and MIME source input has classified provenance. Exact Ubuntu
source archives and notices are checked against authenticated APT SHA256/size.
The shared launcher-compatible flat license layout is retained.

Native installer checks now execute the installed GTK module-query helper and
use fresh/upgrade paths containing spaces and Unicode. The package requires a
clean tracked tree, complete desktop/media/source payload and verified launcher
sidecars. Linux UDP and actual Tk companion lifecycle checks now run in CI.

## Measured acceptance on this computer

| Check | Result and scope |
| --- | --- |
| Native Linux game, full current generator/patch stack | Build and protected MMIO/CRTC/AC97 checks pass; optimized host, assertions and title numeric policy retained |
| Full patch tracking/upgrade suite |59 passed,2 explicit skips |
| Linux renderer and production replay |Software Vulkan normal/forced1.2 and RTX4070SUPER through experimental WSL Dozen pass with SDK validation; injected present-failure negative control is expected |
| Linux launcher |38 shared UI groups; actual repair through copied helper and restart/render checks pass |
| Final candidate installation |1559 files verified in fresh/upgrade Unicode paths; corruption rejected; game data/saves/settings preserved; installed Python/Tk/TLS and GTK loader execute |
| Clean Ubuntu userspace |Final package mounted read-only at a spaces/Unicode path, assets read-only, running as nobody with temporary HOME and private Xvfb/Openbox; settings/debug/input/pixels/fullscreen/restore, Python/Tk/TLS and38 launcher UI groups pass; no development checkout/SDK or system Python packages inside |
| Online native self-test in clean userspace |Explicit skip77 without an automated peer command; not counted as a network pass |
| Real-file movie fixture with release decoder |EA/FRD image, timestamp and PCM hashes match at Original/60/120/144/180/240;2 pass,2 Windows-timer skips |
| Final packaged startup movies |EA88 and FRD81 pictures reach EOF through the actual game, using native case-sensitive asset directory entries |
| Story, captured |120-second packaged run reaches observed active Story; reviewed first-person image;6300 guest presents, no failed presents |
| Story, longer resource run |300.13seconds; scripted movement/fire/turn, held-input focus loss, weapon/reload and two observed pause/resume cycles;17100 guest presents, none interpolated/skipped/failed |
| Longer Story performance window |State-verified final161.55seconds after warmup:59.96 host presents/s, p95 18.610ms, p99 19.314ms; shader cache on, validation off, no captures |
| Longer Story resource window |After140seconds RSS351.1–351.6MB; ending16threads/74descriptors. A five-minute sample does not establish leak-free campaign play |
| Arcade, captured |100.17seconds; shared rocket-impact menu route, actual rocket-launcher gameplay image reviewed;5700 guest presents, no failed presents |
| Final case-sensitive Story |Final candidate150.13seconds; state-verified Story, reviewed first-person capture;8100 guest presents with no failures. Measured active window59.56 presents/s with captures; case-sensitive asset lookup passes |
| Local Online |37 shared companion/directory/transport checks pass,3 host-specific skips; actual packaged adapter/interpreter host/join/cancel7tests pass; separate production Linux Xbox UDP socket fixture1pass,1 Windows-only skip |
| Broader Linux regression |196 pass,7 explicit skips; final changed package/runner/media/storage subset78pass |
| Windows repository/CI/backlog/Mac-package contracts |160 pass,1 POSIX-only skip; later CI checks94pass |

Packaged functional runs did not load an optional SDK validation layer; their
receipts correctly report validation_enabled=false. The separate RHI/replay
checks above did use validation. Host-present FPS does not measure physical
scanout or guarantee native Linux performance. The private experimental Dozen
GPU driver is a test dependency, not a shipped ICD.

Raw local evidence is under ignored `logs/linux-experimental-*`, including
`story-long.json`, `arcade-capture.json`, `packaged-movies.json`,
`cleanroot-final.log`, `packaged-network.log`, `native-udp.log`, renderer logs
and final regression logs. Case-sensitive testing uses415 native file symlinks
plus native directory entries pointing to the original extract: zero asset
bytes copied. Tests use disposable profiles and retain diagnostic logs.

## Remaining gates and published limitations

Native AMD/Intel/NVIDIA desktop smoke tests remain required, particularly
Wayland/X11 driver loading, mouse capture, resize/fullscreen, audible output,
real controller hotplug, display/audio-device switching and suspend/resume.
A prepared minimal Ubuntu userspace on WSL is not a fresh physical desktop.
Steam Deck desktop/Gaming Mode, Steam Input, scaling and resume remain untested.
No SteamOS system changes or transfer occurred.

Full mission completion, checkpoint/progress save/reload, repeated level
transitions and longer campaign soak remain pending. The real existing Test Lab
profile loads in Story/Arcade and storage/save-model regressions pass, but that
is not full campaign save/progress acceptance. Native GTK capture-to-bind remains
pending; launcher/shared controls and GTK dropdown editing are available.

Linux Online exposes Arcade only; Story co-op native transport remains unavailable
as on Mac. Loopback tests do not establish a real two-player match or an external
WAN connection. Windows/WSL NAT/firewall/forwarding was deliberately left unchanged.

Before public upload, review this matrix and the generated notes, choose the
publication commit, push/review the branch and obtain native hardware evidence.
A compatible independent Linux launcher release needs its own Linux ZIP and
`launcher-update-linux-x64.json`; existing Windows/Mac assets are not substitutes.
The current Windows game publisher is not a Linux publication command. Do not
feed this ZIP to it or create a tag/upload automatically. Publication should use
the reviewed native assets, exact hashes and preview metadata after authorization.
