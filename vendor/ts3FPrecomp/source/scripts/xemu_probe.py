#!/usr/bin/env python3
"""
Break at a guest address in xemu and report the registers.

The reference-oracle step: when the recompiled build reaches a function with a
value that looks wrong, this reads what the real game had there.

    python3 scripts/xemu_probe.py --addr 0x000B9030

Requires xemu started with its GDB stub open:

    xemu.exe -s          # stub on localhost:1234
    xemu.exe -s -S       # ... and wait for a connection before booting

Safety, per docs/technical/xemu-debugging.md: the stub will not accept a
breakpoint while the guest is running, so this halts first; and a breakpoint
left behind is an int3 in the guest's code, so this removes every one it set
even when interrupted.
"""

import argparse
import socket
import struct
import sys
import time

# Order of the 16 general registers in an i386 "g" reply.
I386_REGS = ["eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
             "eip", "eflags", "cs", "ss", "ds", "es", "fs", "gs"]


class GdbError(RuntimeError):
    pass


class Gdb:
    """Just enough GDB Remote Serial Protocol to halt, break and read."""

    def __init__(self, host, port, timeout, verbose=False):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.sock.settimeout(timeout)
        self.breakpoints = set()
        self.verbose = verbose
        # Bytes received but not yet consumed. Kept across reads: a chunk can
        # carry more than one packet, and throwing the tail away put every
        # later read one packet out of step -- which is how a stale stop
        # reply came to be taken for a breakpoint hit, and the probe then
        # asked a running guest for its registers and waited forever.
        self.rx = b""

    # -- wire format -----------------------------------------------------

    def _send(self, payload: str) -> None:
        checksum = sum(payload.encode()) & 0xFF
        if self.verbose:
            print(f"    -> ${payload}")
        self.sock.sendall(f"${payload}#{checksum:02x}".encode())

    def _read_packet(self, timeout=None) -> str:
        """The next whole packet, acknowledged. '+'/'-' are skipped."""
        if timeout is not None:
            self.sock.settimeout(timeout)
        while True:
            start = self.rx.find(b"$")
            if start >= 0:
                end = self.rx.find(b"#", start)
                if end >= 0 and len(self.rx) >= end + 3:
                    payload = self.rx[start + 1:end].decode(errors="replace")
                    self.rx = self.rx[end + 3:]
                    # Every packet from the stub is acknowledged, stop
                    # replies included; an unacknowledged one may be sent
                    # again and read later as something new.
                    self.sock.sendall(b"+")
                    if self.verbose:
                        print(f"    <- ${payload[:120]}")
                    return payload
            try:
                chunk = self.sock.recv(4096)
            except socket.timeout:
                raise GdbError("timed out waiting for the stub")
            if not chunk:
                raise GdbError("stub closed the connection")
            self.rx += chunk

    def drain(self) -> None:
        """Discard anything already received and not asked for."""
        self.sock.settimeout(0.2)
        try:
            while True:
                chunk = self.sock.recv(4096)
                if not chunk:
                    break
                self.rx += chunk
        except (socket.timeout, BlockingIOError):
            pass
        while b"$" in self.rx and b"#" in self.rx[self.rx.find(b"$"):]:
            stale = self._read_packet(timeout=0.2)
            if self.verbose:
                print(f"    (discarded stale packet {stale[:40]!r})")
        self.rx = b""

    def command(self, payload: str, timeout=None) -> str:
        self._send(payload)
        return self._read_packet(timeout)

    # -- operations ------------------------------------------------------

    def halt(self) -> str:
        """Interrupt the guest. Required before touching breakpoints."""
        self.sock.sendall(b"\x03")
        try:
            reply = self._read_packet(timeout=3.0)
        except GdbError:
            reply = ""       # already stopped
        self.drain()
        return reply

    def continue_until_stop(self, timeout: float) -> str:
        """Resume, and return the stop reply -- only a stop reply.

        Console output ('O...') and anything else the stub volunteers is
        read past; 'W'/'X' mean the guest is gone."""
        self.drain()
        self._send("c")
        deadline = time.time() + timeout
        while True:
            left = deadline - time.time()
            if left <= 0:
                raise GdbError("timed out waiting for the stub")
            reply = self._read_packet(timeout=left)
            if reply[:1] in ("T", "S"):
                return reply
            if reply[:1] in ("W", "X"):
                raise GdbError(f"the guest exited ({reply})")

    def set_breakpoint(self, addr: int, hardware: bool = False) -> None:
        kind = "Z1" if hardware else "Z0"
        reply = self.command(f"{kind},{addr:x},1")
        if reply != "OK":
            raise GdbError(f"stub refused a breakpoint at 0x{addr:08X}: "
                           f"{reply!r} (is the address mapped yet?)")
        self.breakpoints.add((addr, hardware))

    def clear_breakpoint(self, addr: int, hardware: bool = False) -> None:
        self.command(f"{'z1' if hardware else 'z0'},{addr:x},1")
        self.breakpoints.discard((addr, hardware))

    def clear_all(self) -> None:
        for addr, hardware in list(self.breakpoints):
            try:
                self.clear_breakpoint(addr, hardware)
            except Exception:                       # noqa: BLE001
                print(f"  WARNING: could not remove the breakpoint at "
                      f"0x{addr:08X}; it may still be set in the guest",
                      file=sys.stderr)

    def registers(self) -> dict:
        raw = self.command("g")
        if raw.startswith("E") and len(raw) <= 3:
            raise GdbError(f"register read failed: {raw}")
        values = {}
        for i, name in enumerate(I386_REGS):
            field = raw[i * 8:(i + 1) * 8]
            if len(field) < 8:
                break
            values[name] = struct.unpack("<I", bytes.fromhex(field))[0]
        return values

    def read_memory(self, addr: int, length: int):
        raw = self.command(f"m{addr:x},{length:x}")
        if raw.startswith("E") and len(raw) <= 3:
            return None                             # unmapped
        try:
            return bytes.fromhex(raw)
        except ValueError:
            return None

    def close(self) -> None:
        try:
            self.sock.close()
        except Exception:                           # noqa: BLE001
            pass


def describe(addr: int) -> str:
    """Rough guess at what a guest address is, for orientation."""
    if addr == 0:
        return "null"
    if 0x00010000 <= addr < 0x00600000:
        return "image (code/data)"
    if 0x00780000 <= addr < 0x00F80000:
        return "guest stack"
    if 0x00F80000 <= addr < 0x04000000:
        return "heap"
    if 0x80000000 <= addr < 0x84000000:
        return "contiguous window"
    if 0x84000000 <= addr < 0xF0000000:
        return "ABOVE the contiguous window"
    if 0xFD000000 <= addr < 0xFE000000:
        return "NV2A registers"
    return "unmapped / unknown"


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--addr", required=True, type=lambda s: int(s, 0),
                    help="Guest VA to break on, e.g. 0x000B9030")
    ap.add_argument("--hits", type=int, default=1,
                    help="How many times to stop there (default 1)")
    ap.add_argument("--deref", metavar="REG", default="ecx",
                    help="Register to treat as a pointer and sample "
                         "(default ecx, the thiscall receiver)")
    ap.add_argument("--offset", type=lambda s: int(s, 0), action="append",
                    default=[], metavar="OFF",
                    help="Also read this offset from the --deref pointer "
                         "(repeatable)")
    ap.add_argument("--host", default="localhost")
    ap.add_argument("--port", type=int, default=1234)
    ap.add_argument("--wait", type=float, default=120.0,
                    help="Seconds to wait for the breakpoint (default 120)")
    ap.add_argument("--read", action="append", default=[], metavar="VA",
                    type=lambda s: int(s, 0),
                    help="Also dump the 4-byte word at this absolute guest "
                         "address when the breakpoint hits. Repeatable -- use "
                         "it for the function-pointer slots a call goes "
                         "through, which is what a runtime-populated table "
                         "looks like from the outside.")
    ap.add_argument("--dump", metavar="VA:WORDS", default=None,
                    help="Dump a run of 32-bit words, e.g. 0x5ADCF8:64. Use it "
                         "to recover a dispatch table the game builds at "
                         "runtime -- those live in BSS, so nothing in the "
                         "image file points at their contents and static "
                         "analysis cannot find the functions they name.")
    ap.add_argument("--peek", action="store_true",
                    help="Do not break: just halt, dump 16 bytes at --addr, "
                         "and resume. Use this first to confirm the game is "
                         "actually loaded there.")
    ap.add_argument("--hw", action="store_true",
                    help="Use a hardware breakpoint (Z1) instead of Z0")
    ap.add_argument("--verbose", "-v", action="store_true",
                    help="print every packet exchanged with the stub")
    args = ap.parse_args()

    print(f"connecting to {args.host}:{args.port} ...")
    try:
        gdb = Gdb(args.host, args.port, timeout=10.0, verbose=args.verbose)
    except OSError as exc:
        print(f"error: could not connect: {exc}\n"
              f"       is xemu running with -s ?", file=sys.stderr)
        return 1

    try:
        # '?' first, as every real client does. QEMU's stub (xemu) attaches
        # to the CPU when it answers it; without that, memory reads and Z
        # packets still work, but 'c' is answered "W00" -- the process has
        # exited -- and 'g' is not answered at all. Three probe runs of Future
        # Perfect's cutscene were lost to exactly that before it was found.
        # QEMU also pauses the guest when a debugger connects, so the reply
        # is normally a stop reply straight away.
        print("attaching ...")
        gdb.command("?", timeout=5.0)

        print("halting the guest ...")
        gdb.halt()

        code = gdb.read_memory(args.addr, 16)
        loaded = code is not None and set(code) not in ({0}, {0xFF})
        if loaded:
            print(f"  bytes at 0x{args.addr:08X}: {code.hex(chr(32))}")
        else:
            # With xemu -S the guest has not executed yet, so the image is
            # not in memory. That is expected and is exactly when a
            # hardware breakpoint is the right tool: it watches the address
            # rather than patching the code, so it survives the loader
            # writing the section in later.
            print(f"  0x{args.addr:08X} is not loaded yet")
            if not args.hw:
                print("  xemu is still in the BIOS or dashboard. Either let")
                print("  the game boot and retry, or start xemu with -S and")
                print("  pass --hw to break before the image is loaded.")
                return 1
            print("  --hw given: setting a hardware breakpoint anyway.")

        if args.read:
            print("  absolute reads:")
            for target in args.read:
                word = gdb.read_memory(target, 4)
                if word is None:
                    print(f"    [0x{target:08X}] NOT READABLE")
                else:
                    value = struct.unpack("<I", word)[0]
                    print(f"    [0x{target:08X}] = 0x{value:08X}   {describe(value)}")

        if args.dump:
            where, _, count = args.dump.partition(":")
            base = int(where, 0)
            words = int(count or "16", 0)
            blob = gdb.read_memory(base, words * 4)
            if blob is None:
                print(f"  [0x{base:08X}] NOT READABLE")
            else:
                print(f"  dump of {words} words at 0x{base:08X}:")
                for n in range(len(blob) // 4):
                    value = struct.unpack_from("<I", blob, n * 4)[0]
                    print(f"    [0x{base + n * 4:08X}] = 0x{value:08X}"
                          f"   {describe(value)}")

        if args.peek:
            print("peek only, not breaking.")
            return 0

        print(f"setting a {'hardware' if args.hw else 'software'} breakpoint "
              f"at 0x{args.addr:08X} ...")
        gdb.set_breakpoint(args.addr, hardware=args.hw)

        for hit in range(1, args.hits + 1):
            print(f"\nrunning (waiting up to {args.wait:.0f}s for hit "
                  f"{hit}/{args.hits}) ...")
            try:
                gdb.continue_until_stop(timeout=args.wait)
            except GdbError:
                print("  never reached it. Either the game does not run this "
                      "path yet, or it needs more play time.")
                break

            regs = gdb.registers()
            if regs.get("eip") != args.addr:
                # A stop that is not this breakpoint: an exception in the
                # guest, or a stray trap. Say so rather than report its
                # registers as the answer.
                print(f"  stopped at eip=0x{regs.get('eip', 0):08X}, not at "
                      f"0x{args.addr:08X}; not counted, resuming")
                continue
            print(f"--- hit {hit} at 0x{args.addr:08X} "
                  f"(eip=0x{regs.get('eip', 0):08X}) ---")
            for name in ("eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"):
                value = regs.get(name)
                if value is None:
                    continue
                print(f"  {name} = 0x{value:08X}   {describe(value)}")

            for target in args.read:
                word = gdb.read_memory(target, 4)
                if word is None:
                    print(f"  [0x{target:08X}] NOT READABLE")
                else:
                    value = struct.unpack("<I", word)[0]
                    print(f"  [0x{target:08X}] = 0x{value:08X}   {describe(value)}")

            pointer = regs.get(args.deref)
            if pointer:
                head = gdb.read_memory(pointer, 32)
                if head is None:
                    print(f"  [{args.deref}] is not readable - "
                          f"the guest does not have that mapped either")
                else:
                    words = struct.unpack("<8I", head)
                    print(f"  [{args.deref}] first 8 words: "
                          + " ".join(f"{w:08X}" for w in words))
                for off in args.offset:
                    field = gdb.read_memory(pointer + off, 4)
                    target = pointer + off
                    if field is None:
                        print(f"  [{args.deref}+0x{off:X}] "
                              f"(0x{target:08X}) is NOT readable")
                    else:
                        value = struct.unpack('<I', field)[0]
                        print(f"  [{args.deref}+0x{off:X}] "
                              f"(0x{target:08X}) = 0x{value:08X}   "
                              f"{describe(value)}")
    except GdbError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("\ninterrupted")
    finally:
        print("\nremoving breakpoints and resuming ...")
        # The stub refuses breakpoint changes while the guest runs, so stop
        # it first -- otherwise a failed run leaves the breakpoint behind.
        if gdb.breakpoints:
            try:
                gdb.halt()
            except Exception:                       # noqa: BLE001
                pass
        gdb.clear_all()
        try:
            gdb._send("c")
        except Exception:                           # noqa: BLE001
            pass
        gdb.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
