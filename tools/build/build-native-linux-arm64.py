#!/usr/bin/env python3
"""Cross-build the native Linux arm64 game on an x86_64 host.

Needs clang, clang++, make, cmake and ninja on PATH. --setup also downloads the
Steam Runtime 4 arm64 sysroot and DXC, cross-builds the pinned FFmpeg into the
sysroot, prepares the runtime and regenerates the guest C.
"""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from project_paths import configure  # noqa: E402

configure()
from vulkan_sdk import sha256  # noqa: E402
import spirv_cross  # noqa: E402

RUNTIME_VERSION = "4.0.20261021.265997"
SYSROOT_ARCHIVE = "com.valvesoftware.SteamRuntime.Sdk-arm64-steamrt4-sysroot.tar.gz"
SYSROOT_SHA256 = "58e5a73d3f0d39c47bb6f0b194240ee67a004fd89c2f0e996b33f27deb333067"
SYSROOT_URL = f"https://repo.steampowered.com/steamrt4/images/{RUNTIME_VERSION}/{SYSROOT_ARCHIVE}"

# DXC is pinned separately because the LunarG SDK is x86_64 only, and because
# DXC 1.8.2403 rejects the HLSL this title generates.
DXC_PIN = json.loads((ROOT / "config/build/dxc-arm64.json").read_text())

SYSROOT = ROOT / "external/steamrt4-arm64/sysroot"
DXC = ROOT / "external/dxc-arm64"
BUILD = ROOT / "build/linux-arm64/game"


def run(*args, cwd=ROOT, env=None):
    subprocess.run([str(a) for a in args], cwd=cwd,
                   env=dict(os.environ if env is None else env, TSFP_PROJECT_ROOT=str(ROOT)), check=True)


def download(url, path):
    if path.is_file():
        return path
    path.parent.mkdir(parents=True, exist_ok=True)
    partial = path.with_suffix(path.suffix + ".partial")
    with urllib.request.urlopen(url, timeout=60) as response, partial.open("wb") as output:
        shutil.copyfileobj(response, output)
    partial.replace(path)
    return path


def setup(jobs):
    if shutil.disk_usage(ROOT).free < 8 * 1024**3:
        raise SystemExit("arm64 dependencies need at least 8 GiB free")
    archive = download(SYSROOT_URL, ROOT / "external/steamrt4-arm64" / SYSROOT_ARCHIVE)
    if sha256(archive) != SYSROOT_SHA256:
        raise SystemExit("arm64 sysroot archive differs from the pinned checksum")
    if not (SYSROOT / "usr/include/stdio.h").is_file():
        if shutil.disk_usage(ROOT).free < 8 * 1024**3:
            raise SystemExit("sysroot extraction needs 8 GiB free")
        SYSROOT.mkdir(parents=True, exist_ok=True)
        subprocess.run(["tar", "-xzf", str(archive), "-C", str(SYSROOT)], check=True)
    install_dxc()
    install_gtk()
    media(jobs)


def install_dxc():
    """Unpacks the pinned DXC library and headers."""
    archive = download(DXC_PIN["url"], DXC / DXC_PIN["package"])
    if sha256(archive) != DXC_PIN["sha256"]:
        raise SystemExit("arm64 DXC package differs from the pinned checksum")
    # Do not adopt an older install merely because its headers exist. Replaying
    # the verified small package also restores its matching library and headers.
    with tarfile.open(archive) as bundle:
        bundle.extractall(DXC, filter="data")


def install_gtk():
    """Add pinned target headers/libraries missing from Valve's SDK sysroot.

    Extract data only: never install these packages into the host or execute
    package maintainer scripts. The target OS supplies runtime dependencies.
    """
    spec = json.loads((ROOT / "config/build/linux-arm64-gtk.json").read_text())
    for package in spec["packages"]:
        archive = download(package["url"], ROOT / "external/linux-arm64-debs" / package["url"].rsplit("/", 1)[-1])
        if sha256(archive) != package["sha256"]:
            raise SystemExit("arm64 GTK package differs from the pinned checksum: " + package["name"])
        run("dpkg-deb", "--extract", archive, SYSROOT)


def media(jobs):
    """Cross-builds the FFmpeg pinned in ffmpeg-linux.json into the sysroot."""
    spec = json.loads((ROOT / "config/build/ffmpeg-linux.json").read_text())
    if (SYSROOT / "usr/lib/libavcodec.so.60").is_file():
        return
    base = ROOT / "external/ffmpeg-linux-arm64"
    archive = download(spec["url"], base / spec["archive"])
    if sha256(archive) != spec["sha256"]:
        raise SystemExit("FFmpeg archive differs from the pinned checksum")
    source = base / spec["archive_prefix"]
    if not source.is_dir():
        subprocess.run(["tar", "-xzf", str(archive), "-C", str(base)], check=True)
    work = base / "build"
    work.mkdir(exist_ok=True)
    flags = (f"--target=aarch64-linux-gnu --sysroot={SYSROOT} "
             f"--gcc-toolchain={SYSROOT}/usr -fuse-ld=lld -O2 "
             f"-B{SYSROOT}/usr/lib/gcc/aarch64-linux-gnu/14")
    subprocess.run([str(source / "configure"), "--prefix=/usr", "--cc=clang", "--cxx=clang++",
                    "--target-os=linux", "--arch=aarch64", "--enable-cross-compile",
                    f"--sysroot={SYSROOT}", f"--extra-cflags={flags}", f"--extra-cxxflags={flags}",
                    f"--extra-ldflags={flags}", *spec["configuration"]], cwd=work, check=True)
    subprocess.run(["make", f"-j{jobs}"], cwd=work, check=True)
    # STRIP=false: the host strip cannot read aarch64 objects.
    subprocess.run(["make", "install", "DESTDIR=" + str(SYSROOT), "STRIP=true"], cwd=work, check=True)


def build(args):
    pins = (ROOT / "tsfp.ps1").read_text()
    revision = re.search(r"\$revision = '([a-f0-9]{40})'", pins)[1]
    expected = re.search(r"\$expectedHash = '([A-F0-9]{64})'", pins)[1].lower()
    game = args.game_dir.resolve()
    xbe = game / "default.xbe"
    if not xbe.is_file() or sha256(xbe) != expected:
        raise SystemExit("Missing/unsupported US Xbox executable")
    if not (game / "pak").is_dir() or not (game / "xmv").is_dir():
        raise SystemExit("Game folder must contain pak and xmv")
    for tool in ("clang", "clang++", "ld.lld", "cmake", "ninja", "make", "pkg-config", "dpkg-deb"):
        if shutil.which(tool) is None:
            raise SystemExit(f"{tool} is required on PATH")
    spirv_cross.setup(ROOT) if args.setup else spirv_cross.verify(ROOT)
    if args.setup:
        setup(args.jobs)
    if not (SYSROOT / "usr/include/stdio.h").is_file() or not (DXC / "usr/lib/libdxcompiler.so").is_file():
        raise SystemExit("Run --setup first")

    runtime = ROOT / "external/ts3FPrecomp"
    if args.setup:
        run(sys.executable, ROOT / "tools/build/prepare-runtime.py")
    actual = subprocess.check_output(["git", "-C", str(runtime), "rev-parse", "HEAD"], text=True).strip()
    if actual != revision:
        raise SystemExit(f"Runtime is {actual}, not the pinned {revision}")
    if not (runtime / "titles/futureperfect/src/recomp/gen").is_dir() and not args.generate:
        raise SystemExit("Generated guest C is missing; add --generate")
    # Reapply/verify the full ledger on incremental builds as well as setup.
    run(sys.executable, ROOT / "tools/build/apply-patches.py", runtime, ROOT / "patches")
    if args.setup:
        run("git", "-C", runtime, "submodule", "update", "--init", "--depth", "1",
            "third_party/SDL", "third_party/Vulkan-Headers", "third_party/volk",
            "third_party/VulkanMemoryAllocator", "third_party/XbSymbolDatabase")
        symbols = runtime / "third_party/XbSymbolDatabase"
        run("cmake", "-S", symbols, "-B", symbols / "build", "-G", "Ninja",
            "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_MAKE_PROGRAM=" + shutil.which("ninja"),
            "-DXBSDB_BUILD_UNITTEST=OFF")
        run("cmake", "--build", symbols / "build", "--target", "XbSymbolDatabaseCLI",
            "--parallel", str(args.jobs))
    if args.generate:
        run(sys.executable, "-m", "tools.xdk_symbols", xbe, cwd=runtime)
        run(sys.executable, "scripts/recompile.py", xbe, "--work-dir",
            ROOT / "private/pipeline", "--project", "titles/futureperfect",
            "--game-name", "TimeSplitters: Future Perfect", "--trace-all-entries", cwd=runtime)
        run(sys.executable, ROOT / "tools/build/recomp-generation.py", runtime, "--record")
    run(sys.executable, ROOT / "tools/build/recomp-generation.py", runtime)
    run(sys.executable, ROOT / "tools/build/network-identity.py", "--runtime", runtime,
        "--game", game, "--output", BUILD / "generated")
    # The Online companion runs the python3 the target provides.
    (BUILD / "generated/online-python.h").write_text('#define TSFP_ONLINE_PYTHON "python3"\n')

    env = dict(os.environ, TSFP_ARM64_SYSROOT=str(SYSROOT), PKG_CONFIG_SYSROOT_DIR=str(SYSROOT), PKG_CONFIG_PATH="",
               PKG_CONFIG_LIBDIR=":".join(str(SYSROOT / p) for p in (
                   "usr/lib/aarch64-linux-gnu/pkgconfig", "usr/lib/pkgconfig", "usr/share/pkgconfig")))
    configure_command = [
        "cmake", "-S", str(runtime / "titles/futureperfect"), "-B", str(BUILD), "-G", "Ninja",
        "-DCMAKE_TOOLCHAIN_FILE=" + str(ROOT / "tools/build/linux-arm64-toolchain.cmake"),
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DCMAKE_C_FLAGS_DEBUG=-O2 -g", "-DCMAKE_CXX_FLAGS_DEBUG=-O2 -g",
        "-DTSFP_NATIVE_DIR=" + str(ROOT / "src/native"), "-DTSFP_NATIVE_RHI=ON",
        "-DTSFP_VULKAN_SDK=" + str(DXC / "usr"), "-U*XMV_FFMPEG*",
    ]
    run(*configure_command, env=env)
    run("cmake", "--build", BUILD, "--target", "futureperfect_recomp", "tsfp_native_mmio_test",
        "--parallel", str(args.jobs), env=env)
    exe = BUILD / "futureperfect_recomp"
    run(sys.executable, ROOT / "tools/build/network-identity.py", "--output",
        BUILD / "generated", "--seal-executable", exe)

    sources = [p for area in ("src", "patches", "vendor/smaa") for p in (ROOT / area).rglob("*")
               if p.is_file() and "legacy" not in p.relative_to(ROOT).parts]
    manifest = dict(
        display_settings_version=5, platform="linux-arm64", renderer="vulkan",
        supported_renderers=["vulkan", "opengl"], build_type="Debug",
        host_optimization="optimized", host_debug_flags="-O2 -g", assertions_enabled=True,
        source_revision=subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        working_tree_changes=subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT, text=True),
        runtime_revision=revision, xbe_sha256=expected, game_directory=str(game),
        host=platform.platform(), compiler=subprocess.check_output(["clang", "--version"], text=True).splitlines()[0],
        sysroot_version=RUNTIME_VERSION, sysroot_sha256=SYSROOT_SHA256,
        dxc_version=DXC_PIN["version"], dxc_package_sha256=DXC_PIN["sha256"],
        executable_sha256=sha256(exe),
        compatibility_identity=json.loads(
            (BUILD / "generated/network-identity.json").read_text())["compatibility"],
        source_sha256={p.relative_to(ROOT).as_posix(): sha256(p) for p in sorted(sources)},
        configure_command=configure_command,
    )
    (BUILD / "BUILD.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print("Linux arm64 game built:", exe)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game-dir", type=Path, required=True)
    parser.add_argument("--jobs", type=int, default=4, help="Compiler workers (default 4 limits memory)")
    parser.add_argument("--setup", action="store_true",
                        help="Prepare the runtime, submodules, patches, sysroot and FFmpeg")
    parser.add_argument("--generate", action="store_true",
                        help="Regenerate the guest C from the verified XBE (needs --setup)")
    args = parser.parse_args()
    if platform.system() != "Linux":
        parser.error("Requires a Linux build host")
    if sys.version_info < (3, 12):
        parser.error("Use Python 3.12+ with the shared requirements")
    if not 1 <= args.jobs <= 8:
        parser.error("--jobs must be 1-8")
    if args.generate and not args.setup:
        parser.error("--generate needs --setup, which builds the symbol scanner")
    if args.generate:
        for module in ("capstone", "pefile"):
            if importlib.util.find_spec(module) is None:
                parser.error(f"--generate needs {module}; install requirements.txt")
    build(args)


if __name__ == "__main__":
    main()
