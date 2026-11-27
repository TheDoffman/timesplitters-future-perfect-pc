# Linux launcher architecture and installer

`LauncherPlatform.LinuxSuffixFor` maps OSArchitecture X64/Arm64 to linux-x64 /
linux-arm64 and rejects other CPUs. The selected suffix controls game releases,
self-update manifests, checkout build paths/scripts and ELF64 machine checks.
`NativeHost.CheckBuild` also rejects an explicitly different manifest platform;
legacy manifests without that field retain the executable check. Crash reports
accept both Linux platform values. Windows and Darwin adapters keep their policy.

`tools/launcher/linux/Install-Linux.sh` is the single Linux download bootstrap.
It requires Python 3, resolves the newest stable matching launcher release,
walks the complete release catalog (100 pages maximum; no partial selection),
checks project URLs, bounded size/SHA256, sealed application metadata, ELF CPU
and installer_protocol=1 before executing the temporary self-contained launcher.
Old public packages without that command are rejected before execution. The
bootstrap cannot substitute x64 for ARM64. `--check` reads release metadata only;
it does not prove that the selected package implements the installation protocol.

`LinuxInstaller` handles --install-launcher ROOT ARCHIVE VERSION [--no-launch]
using SelfUpdate.Unpack, the existing update lock, recovery and Core.ApplyLauncher
transaction. Matching complete installs are left untouched; damaged files are
repaired. Downgrades/developer roots are rejected. Game data, saves and config
remain outside the launcher payload. Default root is XDG_DATA_HOME/TimeSplitters
(normally ~/.local/share/TimeSplitters). Successful installation opens the same
launcher UI unless --no-launch was supplied.

`build-linux-launcher.py` pins host SDKs for both CPUs and publishes either RID;
--test requires a matching host. Both packages seal platform/version/protocol in
application.json. CI runs the full native tests on ubuntu-24.04 and ubuntu-24.04-arm.
The release verifier checks ELF machines, sealed metadata and the exact installer
source/checksum. Four packages and their aliases plus the one installer/checksum
produce 22 release assets, all verified before publication. Linux ARM game and
launcher packages remain separate artifacts behind one installer entry point.

The shared texture fixture uses NativeBuildScript when creating a fake Linux
checkout, so ARM64 exercises private-pack routing rather than an installed root.

Tests: tests/launcher/test_linux_installer.py covers bootstrap architecture,
release selection, corruption and CLI handoff without network/executing downloads;
LinuxTests covers both mappings, wrong CPU/metadata rejection, first install,
idempotence, lock contention, repair and personal-file preservation with the real
package. test_launcher_release.py covers the four-platform publication contract.
The shared UI breadcrumb group checks Mac and both Linux native crash summaries.
No version bump/public release is implied by preparing this feature. Keep README
downloads on existing live assets until a new unused version is actually published.
