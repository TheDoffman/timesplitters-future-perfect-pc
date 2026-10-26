#!/usr/bin/env python3
"""
Drive xemu without a person at the pad: snapshot once, probe from it forever.

scripts/xemu_probe.py answers "what does the real game hold here", but every
run used to need someone to boot xemu, play to the right screen and press a
button at the right moment. For TimeSplitters: Future Perfect's cutscene crash
that was five round trips of a minute each, and one of them was lost to a
probe bug nobody could see from the pad.

This takes the person out after the first time. QEMU (which xemu is) can save
the whole machine to its qcow2 disk image and start from it again, and its
monitor and GDB stub are both scriptable. So:

    # once, with someone playing: freeze the game the moment it first reaches
    # 0x000A6880 and save the machine there as "fp_mission"
    py -3 scripts/xemu_session.py snapshot --tag fp_mission --addr 0x000A6880

    # then, unattended, as often as needed: start paused at the snapshot and
    # run xemu_probe.py against it with whatever arguments you like
    py -3 scripts/xemu_session.py probe --tag fp_mission -- \\
        --addr 0x000A68B0 --hits 12 --deref esi --hw

Because the snapshot is taken with the guest halted at a breakpoint, loading
it puts the game back at exactly that instruction, and everything after it --
a level load, a cutscene's set-up -- replays without any input.

Nothing of the user's is modified. The run uses a copy of their xemu.toml
with two changes, the HDD image and the disc, and the HDD image is a copy too
(--hdd, default games/_pipeline/xemu/xbox_hdd_auto.qcow2), because a snapshot
is written into the image itself. Their controller binding is kept, so the
snapshot run is played exactly as usual.

Ports: the GDB stub on 1234, the QEMU monitor on 4444, both localhost.
"""

import argparse
import os
import re
import socket
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
import xemu_probe  # noqa: E402

DEFAULT_XEMU = Path(r"X:\gitrepos\XboxRecomps\xboxrecomp\xemu-0.8.136\xemu.exe")
DEFAULT_CONFIG = Path(os.environ.get("APPDATA", "")) / "xemu" / "xemu" / "xemu.toml"
WORK = ROOT / "games" / "_pipeline" / "xemu"
MONITOR_PORT = 4444
GDB_PORT = 1234


# -- the QEMU monitor ---------------------------------------------------------

class Monitor:
    """The human monitor over TCP: send a line, collect until the prompt."""

    def __init__(self, port=MONITOR_PORT, timeout=10.0):
        self.sock = socket.create_connection(("127.0.0.1", port), timeout=timeout)
        self._until_prompt(timeout)

    def _until_prompt(self, timeout):
        self.sock.settimeout(timeout)
        buf = b""
        while not buf.rstrip().endswith(b"(qemu)"):
            chunk = self.sock.recv(65536)
            if not chunk:
                break
            buf += chunk
        # The monitor echoes with terminal redraw sequences; strip them.
        text = re.sub(r"\x1b\[[0-9;]*[A-Za-z]", "", buf.decode(errors="replace"))
        return text.replace("(qemu)", "").strip()

    def command(self, line, timeout=120.0):
        self.sock.sendall((line + "\n").encode())
        out = self._until_prompt(timeout)
        # Drop the echoed command line, keep the answer.
        lines = [l for l in out.splitlines() if l.strip() and line not in l]
        return "\n".join(lines)

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass


# -- launching ----------------------------------------------------------------

def write_config(src, hdd, iso, out):
    """The user's xemu.toml with only the HDD image and the disc changed."""
    text = Path(src).read_text(encoding="utf-8")
    for key, value in (("hdd_path", hdd), ("dvd_path", iso)):
        line = f"{key} = '{value}'"
        if re.search(rf"^{key}\s*=", text, re.M):
            text = re.sub(rf"^{key}\s*=.*$", line.replace("\\", "\\\\"), text,
                          flags=re.M)
        else:
            text = text.replace("[sys.files]", f"[sys.files]\n{line}")
    Path(out).write_text(text, encoding="utf-8")


def port_open(port):
    try:
        socket.create_connection(("127.0.0.1", port), timeout=0.5).close()
        return True
    except OSError:
        return False


def launch(args, paused, loadvm=None):
    if port_open(GDB_PORT) or port_open(MONITOR_PORT):
        sys.exit("xemu (or something) is already listening on 1234/4444; "
                 "close it first -- two instances cannot share the HDD image")
    WORK.mkdir(parents=True, exist_ok=True)
    config = WORK / "xemu_auto.toml"
    write_config(args.config, args.hdd, args.iso, config)
    argv = [str(args.xemu), "-config_path", str(config), "-s",
            "-monitor", f"tcp:127.0.0.1:{MONITOR_PORT},server,nowait"]
    if paused:
        argv.append("-S")
    if loadvm:
        argv += ["-loadvm", loadvm]
    log = open(WORK / "xemu_last_run.log", "w", encoding="utf-8")
    proc = subprocess.Popen(argv, stdout=log, stderr=subprocess.STDOUT)
    for _ in range(120):
        if proc.poll() is not None:
            sys.exit(f"xemu exited at start-up (code {proc.returncode}); "
                     f"see {WORK / 'xemu_last_run.log'}")
        if port_open(GDB_PORT) and port_open(MONITOR_PORT):
            return proc
        time.sleep(0.5)
    proc.kill()
    sys.exit("xemu did not open its ports within a minute")


def shut_down(proc):
    try:
        mon = Monitor()
        mon.sock.sendall(b"quit\n")
        mon.close()
    except OSError:
        pass
    try:
        proc.wait(timeout=20)
    except subprocess.TimeoutExpired:
        proc.kill()


def shut_down_attached():
    try:
        mon = Monitor()
        mon.sock.sendall(b"quit\n")
        mon.close()
    except OSError:
        pass


# -- commands -----------------------------------------------------------------

def cmd_start(args):
    """Launch xemu running and leave it: the person plays to position."""
    launch(args, paused=False)
    # QEMU pauses the guest when a debugger first connects, and the '?' that
    # attaches does not resume it. Attach now and resume, so the later
    # snapshot --attach finds a stub that is already past that.
    gdb = xemu_probe.Gdb("127.0.0.1", GDB_PORT, timeout=10.0)
    gdb.command("?", timeout=5.0)
    gdb._send("c")
    gdb.close()
    print("xemu is running with the debug stub and monitor open. Play to the "
          "point just before the moment of interest, then run "
          "'snapshot --attach'.")
    return 0


def cmd_snapshot(args):
    proc = None if args.attach else launch(args, paused=False)
    mon = Monitor()
    gdb = xemu_probe.Gdb("127.0.0.1", GDB_PORT, timeout=10.0)
    try:
        gdb.command("?", timeout=5.0)          # attach; QEMU pauses on connect
        # QEMU can send a second stop reply for the pause on connect. Read it
        # now, or it is taken as the answer to the next command -- it was,
        # and the breakpoint was reported refused with 'T05thread:01;'.
        gdb.drain()
        gdb.set_breakpoint(args.addr, hardware=True)
        print(f"xemu is running. Play to the moment of interest; the game "
              f"will freeze when it reaches 0x{args.addr:08X}, and the machine "
              f"is saved as '{args.tag}' at that instant.", flush=True)
        # --when REG=VALUE: an address a title also reaches somewhere else.
        # Future Perfect builds characters through sub_000A6880 in its front
        # end too, and the first snapshot froze the menu, not the mission.
        # Every other stop is let go at once, so the person sees nothing.
        want = None
        if args.when:
            reg, _, val = args.when.partition("=")
            want = (reg.strip().lower(), int(val, 0))
        deadline = time.time() + args.wait
        passed = 0
        while True:
            stop = gdb.continue_until_stop(timeout=max(1.0, deadline - time.time()))
            regs = gdb.registers()
            if regs.get("eip") != args.addr:
                print(f"  stopped at eip=0x{regs.get('eip', 0):08X}, not the "
                      f"breakpoint; resuming", flush=True)
                continue
            if want and regs.get(want[0]) != want[1]:
                passed += 1
                continue
            break
        print(f"stopped: {stop}  eip=0x{regs.get('eip', 0):08X}"
              + (f"  ({want[0]}=0x{want[1]:08X}, after letting {passed} other "
                 f"stops go)" if want else ""), flush=True)
        # Remove the breakpoint first, so it is not part of the saved state.
        gdb.clear_breakpoint(args.addr, hardware=True)
        print(f"saving snapshot '{args.tag}' ...", flush=True)
        answer = mon.command(f"savevm {args.tag}", timeout=600.0)
        if answer:
            print("  monitor: " + answer)
        print(mon.command("info snapshots"))
    finally:
        gdb.close()
        mon.close()
        if proc is not None and not args.keep_running:
            shut_down(proc)
        elif args.attach and not args.keep_running:
            shut_down_attached()
    return 0


def cmd_probe(args):
    proc = launch(args, paused=True, loadvm=args.tag)
    try:
        probe_args = [a for a in args.probe_args if a != "--"]
        cmd = [sys.executable, "-u", str(ROOT / "scripts" / "xemu_probe.py"),
               "--port", str(GDB_PORT)] + probe_args
        return subprocess.call(cmd)
    finally:
        shut_down(proc)


def cmd_list(args):
    proc = launch(args, paused=True)
    try:
        print(Monitor().command("info snapshots"))
    finally:
        shut_down(proc)
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--xemu", type=Path, default=DEFAULT_XEMU)
    ap.add_argument("--config", type=Path, default=DEFAULT_CONFIG,
                    help="the xemu.toml to copy (default: the user's)")
    ap.add_argument("--hdd", type=Path, default=WORK / "xbox_hdd_auto.qcow2",
                    help="HDD image to use and to write snapshots into; "
                         "a copy, never the user's own")
    ap.add_argument("--iso", type=Path,
                    default=ROOT / "games" / "TimeSplittersFP.iso")
    sub = ap.add_subparsers(dest="cmd", required=True)

    st = sub.add_parser("start", help="launch xemu for a person to play")
    st.set_defaults(func=cmd_start)

    s = sub.add_parser("snapshot", help="play to an address once, save there")
    s.add_argument("--attach", action="store_true",
                   help="use the xemu 'start' launched instead of a new one")
    s.add_argument("--tag", required=True)
    s.add_argument("--addr", required=True, type=lambda v: int(v, 0))
    s.add_argument("--wait", type=float, default=1800.0)
    s.add_argument("--when", metavar="REG=VALUE",
                   help="only save on a stop where this register holds this "
                        "value, e.g. edi=0x90A; other stops resume at once")
    s.add_argument("--keep-running", action="store_true",
                   help="leave xemu running after saving")
    s.set_defaults(func=cmd_snapshot)

    p = sub.add_parser("probe", help="start at a snapshot and run xemu_probe.py")
    p.add_argument("--tag", required=True)
    p.add_argument("probe_args", nargs=argparse.REMAINDER,
                   help="arguments for xemu_probe.py, after --")
    p.set_defaults(func=cmd_probe)

    l = sub.add_parser("list", help="list the snapshots in the HDD image")
    l.set_defaults(func=cmd_list)

    args = ap.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
