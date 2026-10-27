"""
Function boundary detection for the disassembler.

Implements multi-pass function detection with confidence scoring:
1. Known addresses (entry point)
2. Standard prologues (push ebp; mov ebp, esp)
3. CC padding boundaries (CC run after ret)
4. Call targets (destinations of call instructions)
5. Cross-validation and overlap resolution
"""

import bisect
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Set, Tuple

from . import config
from .engine import DisasmEngine, Instruction
from .loader import BinaryImage, SectionInfo
from .xrefs import XRefTracker
from .labels import LabelManager, Label, LabelType


@dataclass
class Function:
    """A detected function with boundaries and metadata."""
    start: int
    end: int           # Address after last instruction
    name: str
    section: str = ""
    confidence: float = 0.0
    detection_method: str = ""

    # Call graph data
    calls_to: List[int] = field(default_factory=list)     # Functions this calls
    called_by: List[int] = field(default_factory=list)     # Functions that call this

    # Instruction stats
    num_instructions: int = 0
    has_prologue: bool = False

    @property
    def size(self) -> int:
        return self.end - self.start

    def to_dict(self) -> dict:
        return {
            "start": f"0x{self.start:08X}",
            "end": f"0x{self.end:08X}",
            "size": self.size,
            "name": self.name,
            "section": self.section,
            "confidence": self.confidence,
            "detection_method": self.detection_method,
            "num_instructions": self.num_instructions,
            "has_prologue": self.has_prologue,
            "calls_to": [f"0x{a:08X}" for a in self.calls_to],
            "called_by": [f"0x{a:08X}" for a in self.called_by],
        }


class FunctionDetector:
    """
    Multi-pass function boundary detector.

    Identifies function start addresses through multiple heuristics,
    then determines function boundaries by following instruction flow
    until the next function or a terminal instruction.
    """

    def __init__(self, engine: DisasmEngine, image: BinaryImage,
                 xrefs: XRefTracker, labels: LabelManager):
        self.engine = engine
        self.image = image
        self.xrefs = xrefs
        self.labels = labels

        # Candidate function starts: address -> (confidence, method)
        self._candidates: Dict[int, Tuple[float, str]] = {}

        # Final function list
        self.functions: Dict[int, Function] = {}

        # Tail-jump targets landing inside another function: addr -> that
        # function's end. Kept out of self._candidates so they cannot truncate
        # the function they land in.
        self._alias_entries: Dict[int, int] = {}

    def detect_all(self, sections: Optional[List[SectionInfo]] = None) -> int:
        """
        Run all detection passes and build the function database.

        Args:
            sections: Sections to analyze. If None, uses all executable sections.

        Returns:
            Number of functions detected.
        """
        if sections is None:
            sections = self.image.get_code_sections()

        # Pass 1: Known addresses
        self._pass_known_addresses()

        # Pass 2: Prologue patterns
        for sec in sections:
            self._pass_prologues(sec)

        # Pass 3: CC padding boundaries
        for sec in sections:
            self._pass_cc_boundaries(sec)

        # Pass 4: Call targets
        self._pass_call_targets(sections)

        # Pass 4b: Function addresses installed into indirect-call slots
        self._pass_indirect_call_slots()

        # Pass 5: Build functions from candidates
        self._build_functions(sections)

        # Pass 6: Tail-jump targets. A function reached only by "jmp" and never
        # by "call" is invisible to every pass above, so it is emitted as a stub
        # that returns without unwinding the frame its jumping caller built --
        # silently corrupting the simulated stack for everything upstream. Halo
        # had 127 of these; one of them (the CRT two-arg error handler) leaked
        # 0x24 bytes per call and turned a 6-iteration init loop into 21,938
        # allocations that exhausted the heap.
        #
        # Needs the bodies from pass 5 to tell a tail jump from an ordinary
        # intra-function branch, so it runs after and rebuilds. Iterate: a newly
        # found function can itself tail-jump somewhere new.
        for _round in range(8):
            before = len(self._candidates)
            if not self._pass_tail_jump_targets(sections):
                break
            print(f"  tail-jump pass {_round}: "
                  f"+{len(self._candidates) - before} standalone, "
                  f"{len(self._alias_entries)} aliases")
            self.functions.clear()
            self._build_functions(sections)

        # Function addresses taken as an immediate. Runs once, after the
        # bodies exist: the test is whether the target lands in a gap, which
        # needs the gaps to be known. One rebuild picks up what it finds.
        if self._pass_imm_ref_targets(sections):
            self.functions.clear()
            self._build_functions(sections)

        # A function that begins immediately after a ret, with no padding.
        if self._pass_gap_prologues(sections):
            self.functions.clear()
            self._build_functions(sections)

        # Then the same for addresses that only ever exist as table entries.
        # After the immediate pass, so its results narrow the gaps first. These
        # become aliases rather than function starts, so no rebuild: aliases are
        # materialised by _build_alias_entries once boundaries are final.
        self._pass_data_ptr_targets(sections)

        # Seeds that landed inside a function rather than on its start.
        self._pass_seed_aliases()

        # Tail jumps out of the alias entries. Must come after every pass that
        # adds aliases, and before they are built.
        self._pass_alias_tail_jumps(sections)

        self._build_alias_entries()

        # Populate call graph
        self._build_call_graph()

        return len(self.functions)

    def _pass_gap_prologues(self, sections: List[SectionInfo]) -> bool:
        """A function that starts right after a ret, with no padding between.

        _pass_cc_boundaries only recognises a boundary when the compiler left
        int3 padding to the next alignment. It usually does -- but not when the
        next function happens to start on the boundary already, and then a
        clean prologue sits immediately after the previous function's ret with
        nothing to mark it.

        Nothing else covers that case either. Such a function is not a call
        target if it is only ever reached through a vtable, and the prologue
        pass looks for "push ebp; mov ebp, esp", which an FPO function like
        "sub esp, 0x18" does not have.

        Restricted to addresses in an unclaimed gap, which makes it safe by
        construction rather than by judgement: MSVC parks out-of-line tails
        after a ret too, and those look identical from here. The difference is
        that a tail belongs to the function above it, so _find_function_end has
        already walked over it and it is not in a gap. Half-Life 2 has 20 of
        these; 12 are in gaps and 8 are tails, and the gap test separates them.

        The one that mattered was 0x00476EB0, a material-system method reached
        only through a shader's vtable. Unresolved, the call was skipped rather
        than made, so eax kept a stale value that the caller then used as a
        string pointer.
        """
        bounds = sorted((f.start, f.end) for f in self.functions.values())
        starts = [b[0] for b in bounds]

        def in_a_gap(addr: int) -> bool:
            i = bisect.bisect_right(starts, addr) - 1
            return not (i >= 0 and addr < bounds[i][1])

        # A switch's tables sit after the function's ret, outside its range,
        # so they are "in a gap" -- and MSVC aligns the dword table with the
        # two-byte nop 8B FF, which is also the hot-patch prologue. So the
        # padding before every inline jump table read as a function start. The
        # "function" then ran on through the dword table and the byte index
        # table into the real function after them, which became an entry in
        # the middle of a bogus one. TimeSplitters: Future Perfect's per-level
        # callbacks at 0x001C4DC0 and 0x001C51E0 were each hidden that way,
        # were never lifted, and the call to each was skipped at level load.
        tables = sorted(self.engine.jump_tables.items())
        table_starts = [t[0] for t in tables]

        def table_or_its_padding(addr: int) -> bool:
            """Inside a jump table, or up to 3 bytes of alignment before one."""
            i = bisect.bisect_right(table_starts, addr + 3) - 1
            return i >= 0 and addr < tables[i][1]

        added = False
        for insn in list(self.engine.instructions.values()):
            if not insn.is_ret:
                continue
            nxt = insn.end_address
            if nxt in self._candidates or nxt in self.functions:
                continue
            section = self.image.get_section_at_va(nxt)
            if section is None or not section.executable:
                continue
            if not in_a_gap(nxt):
                continue                    # an out-of-line tail, not a start
            if table_or_its_padding(nxt):
                continue                    # a switch table's 8B FF alignment
            # A prologue, or a whole small function.
            #
            # MSVC packs runs of constant-returning accessors -- "mov eax,
            # <address>; ret", six bytes each -- back to back with no padding,
            # and they are reached only through vtables. There is no prologue
            # to recognise because there is no frame; the entire function is
            # two instructions. Half-Life 2 has 3,147 of that exact shape, of
            # which 19 land in a gap and are found by nothing else.
            #
            # The gap restriction is what makes this safe. The same two
            # instructions appear 795 times *inside* larger functions as a
            # return path, and splitting one of those would cut a function in
            # half -- but those are covered, so they are not in a gap.
            if not (self.engine.probes_as_prologue(nxt)
                    or self.engine.probes_as_constant_stub(nxt)):
                continue
            self._add_candidate(nxt, config.CONFIDENCE_CC_BOUNDARY,
                                "gap_prologue")
            added = True

        # And the function that follows a switch's tables. Nothing marks it:
        # it is not after a ret, it need not have a prologue (Future Perfect's
        # level callbacks begin "mov eax, [esp+4]"), and if it is reached only
        # through a pointer built at run time, no pass sees it. What does mark
        # it is the shape of the data in front of it. MSVC emits a two-level
        # switch as the dword table followed immediately by a byte table of
        # indices into it, and every index is below the dword table's entry
        # count; the first byte of real code (8B, 55, 83, 56, ...) is not. So
        # skip the index bytes, then int3/nop padding, and what is left is the
        # next function -- or another switch's 8B FF, which is not.
        after = 0
        for tbl, end in tables:
            entries = (end - tbl) // 4
            section = self.image.get_section_at_va(end)
            if section is None or not section.executable or entries < 2:
                continue
            addr = end
            for _ in range(256):
                b = self.image.read_bytes_at_va(addr, 1)
                if not b or b[0] >= entries:
                    break
                addr += 1
            while True:
                b = self.image.read_bytes_at_va(addr, 1)
                if not b or b[0] not in (0xCC, 0x90):
                    break
                addr += 1
            if (addr in self._candidates or addr in self.functions
                    or table_or_its_padding(addr) or not in_a_gap(addr)):
                continue
            if not self.engine.probes_as_function_body(addr):
                continue
            # The sweep resumed at the dword table's end and decoded the index
            # bytes as code, so it can be out of phase here: Future Perfect's
            # 0x002257D0 follows "... 0d 0d 0c" directly, 0D is a five-byte
            # "or eax, imm32", and no instruction started at the function.
            # A candidate with no instruction under it is dropped when bodies
            # are built, so put the stream back in step first.
            self.engine.decode_at(addr)
            self._add_candidate(addr, config.CONFIDENCE_CC_BOUNDARY,
                                "after_switch_table")
            after += 1
            added = True

        if added:
            print("  functions recovered that follow a ret with no padding"
                  + (f" ({after} after a switch table)" if after else ""))
        return added

    def _pass_seed_aliases(self) -> None:
        """
        Turn seeded addresses that fell inside a function into alias entries.

        A seed is usually a function start the detector could not reach, but an
        indirect-branch target need not be one: Wreckless's D3DX calls
        0x0010E9C3 and 0x0010EA2D, each in the middle of a function, as
        alternate entry points that share the tail.

        Splitting the enclosing function is not an option -- that truncates the
        code the call wanted to reach. Neither is dropping it, which leaves the
        ICALL unresolved and its stub returning without the epilogue, walking
        esp off by the callee's argument bytes. An alias body, exactly as the
        tail-jump pass builds for the same shape, is callable and keeps the
        original intact.

        Runs after the bodies exist, since "inside a function" is not knowable
        before that.
        """
        seeded = sorted(addr for addr, (_conf, method) in self._candidates.items()
                        if method == "seed_vtable_thunk")
        if not seeded:
            return
        bodies = sorted((f.start, f.end) for f in self.functions.values())
        starts = [b[0] for b in bodies]
        for addr in seeded:
            if addr in self.functions:
                continue
            i = bisect.bisect_right(starts, addr) - 1
            if i < 0:
                continue
            body_start, body_end = bodies[i]
            if not (body_start < addr < body_end):
                continue
            if addr not in self._alias_entries:
                self._alias_entries[addr] = body_end

    def _add_candidate(self, addr: int, confidence: float, method: str) -> None:
        """Add a function start candidate, keeping highest confidence."""
        existing = self._candidates.get(addr)
        if existing is None or confidence > existing[0]:
            self._candidates[addr] = (confidence, method)

    def _pass_known_addresses(self) -> None:
        """Pass 1: Add known function addresses."""
        # Entry point
        self._add_candidate(
            self.image.entry_point,
            config.CONFIDENCE_KNOWN,
            "entry_point"
        )

    def _pass_prologues(self, section: SectionInfo) -> None:
        """
        Pass 2: Scan for standard function prologues.

        Looks for: push ebp (0x55); mov ebp, esp (0x8BEC or 0x89E5)
        """
        data = self.image.get_section_data(section)
        if not data:
            return

        va_start = section.virtual_addr
        i = 0
        while i < len(data) - 2:
            # Check for push ebp; mov ebp, esp
            if data[i] == 0x55:
                if (i + 2 < len(data) and
                        data[i + 1] == 0x8B and data[i + 2] == 0xEC):
                    addr = va_start + i
                    # Verify this address has a decoded instruction
                    if addr in self.engine.instructions:
                        self._add_candidate(
                            addr,
                            config.CONFIDENCE_PROLOGUE,
                            "prologue"
                        )
                    i += 3
                    continue
                elif (i + 2 < len(data) and
                      data[i + 1] == 0x89 and data[i + 2] == 0xE5):
                    addr = va_start + i
                    if addr in self.engine.instructions:
                        self._add_candidate(
                            addr,
                            config.CONFIDENCE_PROLOGUE,
                            "prologue_alt"
                        )
                    i += 3
                    continue
            i += 1

    def _pass_cc_boundaries(self, section: SectionInfo) -> None:
        """
        Pass 3: Find function boundaries at CC padding.

        Pattern: ret instruction, followed by one or more 0xCC bytes,
        followed by the start of the next function.
        """
        data = self.image.get_section_data(section)
        if not data:
            return

        va_start = section.virtual_addr
        i = 0

        while i < len(data):
            # Look for CC padding runs
            if data[i] == config.CC_PADDING:
                cc_start = i
                while i < len(data) and data[i] == config.CC_PADDING:
                    i += 1

                cc_run_length = i - cc_start

                if cc_run_length >= config.MIN_CC_RUN and i < len(data):
                    # Check what the CC run interrupts.
                    #
                    # A `ret` is the obvious terminator, but a tail call ends a
                    # function just as completely: MSVC turns "return f(x)" into
                    # a bare `jmp f` and pads to the next boundary exactly as it
                    # would after a `ret`. Only accepting `ret` left the
                    # function before such a jmp running on through the padding
                    # and swallowing the next function whole -- and a vtable
                    # slot pointing into the middle of that merged range then
                    # has no function to resolve to, so the indirect call is
                    # skipped at runtime rather than made.
                    #
                    # The back-scan has to reach 5 bytes for `jmp rel32`; at 3
                    # it could not have seen one even if it had looked. Only
                    # instructions the sweep actually decoded at that address
                    # are considered, so this cannot invent a misaligned one.
                    before_addr = va_start + cc_start
                    found_ret = False
                    for check_offset in range(1, 8):
                        check_addr = before_addr - check_offset
                        insn = self.engine.get_instruction(check_addr)
                        if (insn and insn.end_address == before_addr
                                and (insn.is_ret or insn.is_jump)):
                            found_ret = True
                            break

                    if found_ret:
                        next_addr = va_start + i
                        if next_addr in self.engine.instructions:
                            self._add_candidate(
                                next_addr,
                                config.CONFIDENCE_CC_BOUNDARY,
                                "cc_boundary"
                            )
            else:
                i += 1

    def _pass_call_targets(self, sections: List[SectionInfo]) -> None:
        """
        Pass 4: Add destinations of direct call instructions as function starts.
        """
        # Build set of valid code ranges
        code_ranges = set()
        for sec in sections:
            for addr in range(sec.virtual_addr,
                              sec.virtual_addr + sec.virtual_size):
                code_ranges.add(addr)

        call_targets = self.engine.get_call_targets()
        realigned = unaligned = 0
        for target in call_targets:
            section = self.image.get_section_at_va(target)
            if not (section and section.executable):
                continue
            # A direct call to an executable address is the strongest evidence
            # of a function start there is -- stronger than a prologue match,
            # which is a guess about bytes. It used to be discarded whenever the
            # linear sweep had stepped over that exact address, which happens
            # wherever the sweep passes through data and comes out of phase.
            # Decode there instead of dropping the candidate; see
            # Engine.decode_at. On Halo 2276 this is 46 functions that were
            # becoming `g_esp += 4` no-op stubs.
            if target not in self.engine.instructions:
                # Manufacturing an instruction here is creating evidence, not
                # reading it, so require corroboration: the bytes there have to
                # decode as a function body, reaching a ret or a tail jump
                # without hitting something undecodable. A call operand read
                # out of data usually does not.
                #
                # Alignment used to be the corroboration. It is a weaker test
                # in both directions, and it silently dropped Wreckless's
                # _mtinitlocks at 0x000F211A -- a CRT function packed right
                # after a jump table at no alignment at all. Stubbed out, it
                # left the lock table all zeroes, and _lock(_LOCKTAB_LOCK)
                # recursed into _mtinitlocknum until the stack overflowed.
                # Targets that already decoded are untouched -- pre-existing
                # behaviour.
                if not self.engine.probes_as_function_body(target):
                    unaligned += 1
                    continue
                if self.engine.decode_at(target):
                    realigned += 1
                else:
                    continue  # genuinely undecodable: not code
            self._add_candidate(
                target,
                config.CONFIDENCE_CALL_TARGET,
                "call_target"
            )
        if realigned or unaligned:
            print(f"  Realigned {realigned} call targets the sweep stepped over"
                  f" ({unaligned} rejected: bytes do not decode as a function)")

    def _pass_indirect_call_slots(self) -> None:
        """
        Pass 4b: An immediate written into a memory slot that is elsewhere the
        operand of an indirect call is a function address.

        A function whose address is only ever taken -- never the operand of a
        direct call, never the target of a jump -- is invisible to every other
        pass. It gets no dispatch entry, and every indirect call to it lands on
        a target the recompiler cannot resolve.

        Only immediates stored into a slot something actually calls through
        count. That is the whole of the evidence: the value is provably used as
        a call target. Promoting every in-image immediate would drag in string
        and table addresses, and a spurious start inside a real function
        truncates it, which is worse than missing one.

        Wreckless: `mov dword ptr [0x1D1938], 0xF643B` installs the
        InitializeCriticalSectionAndSpinCount fallback thunk, which
        `call dword ptr [0x1D1938]` six instructions later invokes. Unresolved,
        the call returned 0, _mtinitlocks read that as failure and bailed after
        lock 0, and _lock(_LOCKTAB_LOCK) then recursed through _mtinitlocknum
        until the stack overflowed.
        """
        slots = set(self.engine.get_indirect_call_refs())
        if not slots:
            return
        found = 0
        for insn in list(self.engine.instructions.values()):
            if insn.memory_ref not in slots or insn.imm_ref is None:
                continue
            target = insn.imm_ref
            section = self.image.get_section_at_va(target)
            if not (section and section.executable):
                continue
            if target not in self.engine.instructions:
                if not self.engine.probes_as_function_body(target):
                    continue
                if not self.engine.decode_at(target):
                    continue
            self._add_candidate(
                target,
                config.CONFIDENCE_CALL_TARGET,
                "indirect_call_slot"
            )
            found += 1
        if found:
            print(f"  {found} function address(es) installed into"
                  f" indirect-call slots")

    def _pass_imm_ref_targets(self, sections: List[SectionInfo]) -> bool:
        """
        An immediate that points into unclaimed executable bytes and decodes as
        a whole function body is a function whose address was taken.

        _pass_indirect_call_slots already promotes an immediate stored into a
        slot that something calls through by name. That is the strong evidence,
        and it misses the ordinary C++ case entirely: a function pointer written
        into an object field and later invoked as `call dword ptr [reg]` has no
        fixed slot to match against.

        Half-Life 2's CUtlRBTree constructor does exactly that --
        `mov dword ptr [esi], 1D0F8h` installs the comparator, and the tree
        calls it through `this`. Both of its comparators sat in a 42-byte hole
        between two detected functions, so neither existed. Every tree search
        called an address the recompiler could not resolve, got 0 back, and
        descended into a node that was never there.

        The evidence here is weaker than a named slot, so the filter carries the
        weight, and it is the *gap* that does most of the work: the target has
        to land in executable bytes that no function already covers, and decode
        from there to a ret or a tail jump without hitting anything invalid. A
        string or table address fails the decode; an offset into a real function
        fails the gap. A spurious start inside a function would truncate it,
        which is worse than missing one -- hence testing coverage rather than
        just membership.

        Runs after _build_functions for that reason, and returns whether it
        added anything so the caller can rebuild.
        """
        bounds = sorted((f.start, f.end) for f in self.functions.values())
        starts = [b[0] for b in bounds]

        def inside_a_function(addr: int) -> bool:
            i = bisect.bisect_right(starts, addr) - 1
            return i >= 0 and addr < bounds[i][1]

        # Collect distinct targets first. The instruction dict holds millions of
        # entries and the same address is taken over and over, so probing per
        # instruction rather than per address is the difference between seconds
        # and not finishing.
        # Only the sections actually being analysed as code. An XBE marks
        # .rdata and .data executable, so `section.executable` alone lets a
        # string or a vtable through -- and .rdata disassembles happily into
        # bound/popal/arpl, which then fails to compile. The caller's section
        # list is the real answer to "is this code".
        code_ranges = [(sec.virtual_addr, sec.virtual_addr + sec.virtual_size)
                       for sec in sections]

        def in_code_section(addr: int) -> bool:
            return any(lo <= addr < hi for lo, hi in code_ranges)

        targets = set()
        for insn in self.engine.instructions.values():
            target = insn.imm_ref
            if target is None or target in self.functions:
                continue
            if inside_a_function(target) or not in_code_section(target):
                continue
            targets.add(target)

        found = 0
        for target in sorted(targets):
            # A ret, not merely a terminator: an immediate is weak evidence,
            # so the probe has to reject data that happens to disassemble. The
            # cap also keeps a wrong guess from walking the rest of the section.
            # ...or a virtual-call thunk, which never reaches a ret: it
            # dispatches through the vtable and is gone. Those are taken by
            # address and passed around as values, so an immediate is exactly
            # how they show up.
            # ...or an adjustor thunk: a few instructions that end in a direct
            # jmp onto a *known function start*. A tail jump alone is weak
            # evidence, but landing exactly on a start is not something data
            # does by accident. Burnout 2's stream object installs three
            # callbacks side by side (sub_001104F0: +0x0C, +0x10, +0x14); the
            # middle one is `mov eax,[esp+4]; mov ecx,[eax+0x4c];
            # mov [esp+4],ecx; jmp sub_00110320`. The other two were found and
            # it was not, so every call through it was an unresolved ICALL that
            # popped the wrong amount, and the attract movie's path string
            # ended up written over saved registers.
            thunk_to = self.engine.block_tail_jump(target, max_insns=16)
            is_thunk = (thunk_to in self.functions
                        and not self.engine.entry_pops_unsaved(target))
            if not (self.engine.probes_as_returning_body(target)
                    or self.engine.probes_as_vcall_thunk(target)
                    or is_thunk
                    or self._is_padded_entry(target)):
                continue

            # The same two guards _pass_data_ptr_targets carries, and for the
            # same reason: an immediate is weak evidence, so the filter has to
            # do the work.
            #
            # Requiring the address to already be a start the linear sweep
            # produced rejects one that sits *inside* an instruction. Calling
            # decode_at here instead manufactured a second, out-of-phase stream
            # over real code and registered a start in the middle of it.
            #
            # Rejecting a first instruction of int3 or nop rejects alignment
            # padding. An entry point never begins with padding, and a run of
            # nops before a function is exactly where a stray immediate lands.
            #
            # Burnout 2 at 0x00160000 shows both. The real leaf is 19 bytes --
            #     fld [0x2943E4]; fsub [0x2B0BF4]; fstp [0x4D29E4]; ret
            # -- preceded by five nops. Without these guards the pass created
            # starts at 0x0015FFFD, 0x0015FFFE and 0x0015FFFF (all nops) and at
            # 0x00160002 (inside the fld), and the real entry inherited a
            # 6-byte body from them. sub_00160000 therefore executed the fld
            # and returned: the store never happened and the x87 stack leaked a
            # slot on every call, which is the "wrong numbers, no crash" class
            # of failure rather than anything that announces itself.
            if target not in self.engine.instructions:
                continue
            first = self.engine.instructions[target]
            if first.mnemonic.lower() in ("int3", "nop"):
                continue

            self._add_candidate(target, config.CONFIDENCE_IMM_REF,
                                "imm_ref_target")
            found += 1

        if found:
            print(f"  {found} function address(es) taken as an immediate")
        return found > 0

    def _pass_tail_jump_targets(self, sections: List[SectionInfo]) -> bool:
        """
        Pass 6: Add the target of every unconditional jmp that leaves the body
        of the function containing it.

        Deliberately NOT conditional branches. A jcc is not a terminator, so a
        jcc target past the end of its own function almost always means the
        body was measured short, not that a function starts there -- and
        registering it splits the function instead of extending it. Tried on
        Halo 2276: +15 functions, but two of them landed inside an existing
        body and a third function lost 0xA0 bytes off its end.

                Returns True if any new candidate was added.
        """
        bodies = sorted((f.start, f.end) for f in self.functions.values())
        starts = [b[0] for b in bodies]
        added = False

        for insn in self.engine.instructions.values():
            if not insn.is_jump or insn.is_cond_jump:
                continue
            target = insn.jump_target
            if target is None or target in self._candidates:
                continue
            if target not in self.engine.instructions:
                continue

            # The jump is a tail jump only if it leaves its own function.
            i = bisect.bisect_right(starts, insn.address) - 1
            if i < 0:
                continue
            body_start, body_end = bodies[i]
            if insn.address >= body_end:
                continue            # not inside any known function
            if body_start <= target < body_end:
                continue            # ordinary intra-function branch

            section = self.image.get_section_at_va(target)
            if section is None or not section.executable:
                continue

            # Does the target land inside some *other* function? The CRT does
            # this constantly -- _startOneArgErrorHandling jumps into the middle
            # of _startTwoArgErrorHandling to share its tail. Registering that
            # address as an ordinary candidate would truncate the function it
            # lands in, breaking the very code it wanted to reach. Record it as
            # an alias entry instead: same end address, translated separately.
            j = bisect.bisect_right(starts, target) - 1
            if j >= 0 and bodies[j][0] < target < bodies[j][1]:
                if target not in self._alias_entries:
                    self._alias_entries[target] = bodies[j][1]
                    added = True
                continue

            # A target that restores callee-saved registers it never saved
            # is the *continuation* of the function that jumped to it, not a
            # function of its own -- the pushes it unwinds were made before the
            # jump. Registering it as a candidate is actively harmful: the
            # candidate becomes the upper bound when the jumping function's end
            # is measured, so that function is truncated at its own tail jump,
            # and if the candidate is later rejected the continuation is left
            # undiscovered. The tail jump then lifts as a call to a stub, which
            # can infer the argument cleanup from the ret but knows nothing
            # about the frame, so every call leaks it.
            #
            # Burnout 2's sub_00218CD0 is exactly this: 79 bytes ending in
            # "jmp 0x218d23", with the pops and ret living at the target. Its
            # frame -- sub esp,8 plus four pushes -- leaked on every call, and
            # esp walked out of the guest stack into .data.
            #
            # Skipping it lets the jumping function extend over the target
            # naturally, since nothing clamps the bound any more.
            if self.engine.entry_pops_unsaved(target):
                continue

            self._add_candidate(target, config.CONFIDENCE_TAIL_JUMP,
                                "tail_jump_target")
            added = True

        added = self._pass_cond_branch_orphans(bodies, starts) or added
        return added

    def _pass_data_ptr_targets(self, sections: List[SectionInfo]) -> bool:
        """
        A code pointer stored in a data section is a function whose address is
        only ever taken at run time.

        _pass_imm_ref_targets catches an address that appears as an immediate in
        code. It cannot catch one that only ever exists as a *value in a table*
        -- a vtable the compiler emitted into .rdata, a callback array, a
        dispatch table. RTTI recovery covers vtables belonging to polymorphic
        classes; nothing covers the rest.

        Half-Life 2 shows why that matters. Its boot reached module loading and
        then stopped on one unresolved indirect call after another --
        0x00427F80, 0x005AC780, 0x00581CC0 -- each a clean function reached
        through a table, each found only by running the title and reading the
        indirect-call feedback. There are 4,892 of them in this binary.

        Same filter as the immediate pass, and the gap does the same work: the
        target must land in executable bytes no function already covers, and
        must decode to a ret. A data word that happens to fall in a code
        section's range fails the decode; an offset into a real function fails
        the gap.
        """
        code_ranges = [(sec.virtual_addr, sec.virtual_addr + sec.virtual_size)
                       for sec in sections]
        code_names = {sec.name for sec in sections}

        def in_code_section(addr: int) -> bool:
            return any(lo <= addr < hi for lo, hi in code_ranges)

        bounds = sorted((f.start, f.end) for f in self.functions.values())
        starts = [b[0] for b in bounds]

        def inside_a_function(addr: int) -> bool:
            i = bisect.bisect_right(starts, addr) - 1
            return i >= 0 and addr < bounds[i][1]

        # Deliberately NOT filtering out targets inside a function. That guard
        # belongs to the immediate pass, which creates function *starts* and
        # would split whatever it landed in. This pass creates aliases, and
        # inside-a-function is precisely what an alias is for: an alternate
        # entry point sharing the enclosing body.
        #
        # It matters. MSVC merges runs of tiny C++ constructor thunks into one
        # body -- sub_005C10D6 covers 2,246 bytes of them -- so 45 of Half-Life
        # 2's static initialisers are addresses inside another function and
        # nowhere else. Excluding them left those constructors with no body at
        # all, and _initterm silently skipped every one.
        targets = set()
        # target -> the values stored beside it in the same table. Used below
        # as the extra evidence a long body needs.
        table_mates: Dict[int, Set[int]] = {}
        # Values seen only as a lone word, never in a table run. Burnout 2's
        # callback 0x0010A9F0 is one: a registration record {name, 1, fn, ...}
        # with no neighbouring code pointer.
        lone_only: Set[int] = set()
        for sec in self.image.sections:
            if sec.name in code_names:
                continue                    # scan data, not code
            # .XTLID is the XDK's library-ID table: (id, address) pairs. The
            # addresses are library entry points that calls already find; the
            # ids are small numbers that fall in .text's range -- 0x0002013D,
            # 0x00020170 -- and, once the function they land in is found, the
            # inside-a-function branch below took them as entry points.
            if sec.name == ".XTLID":
                continue
            data = self.image.get_section_data(sec)
            if not data:
                continue
            # Require a neighbour. A real function table is a run of code
            # addresses; a lone one surrounded by unrelated bytes is a
            # coincidence, and 14% of the candidates on Burnout 2 were exactly
            # that -- 5,595 isolated words against a single genuine run of
            # 28,826. One of them, 0x000B9087, sat in the middle of DSOUND's
            # audio data and happened to name a branch target inside
            # sub_000B9030, 87 bytes past the prologue that loads esi from ecx.
            #
            # Accepting it is worse than missing it. The entry goes in the
            # dispatch table, so an indirect call to that address stops being
            # an unresolved ICALL -- which is reported, and which
            # tools.seed_from_log can recover from a run log -- and becomes a
            # silent jump into the middle of a function with whatever the
            # previous one left in the registers. Burnout 2 faulted reading
            # esi+0x6B948 with a stale esi, four frames and no diagnostic away
            # from the cause.
            hits = []
            for off in range(0, len(data) - 3, 4):
                value = int.from_bytes(data[off:off + 4], "little")
                if in_code_section(value):
                    hits.append(off)
            # Group the hits into runs of adjacent words; a run of two or more
            # is a table. Every value in it is a target, and the rest of the
            # run is its evidence below -- the whole run, not just the words
            # beside it: a vtable's slot 0 can sit next to entries that are
            # only found by this same pass.
            runs, run = [], []
            for off in hits:
                if run and off != run[-1] + 4:
                    runs.append(run)
                    run = []
                run.append(off)
            if run:
                runs.append(run)
            for run in runs:
                if len(run) < 2:
                    # A lone word. Coincidences live here, so it only counts
                    # if it lands exactly where a compiler puts a function --
                    # see the lone_only check below.
                    value = int.from_bytes(data[run[0]:run[0] + 4], "little")
                    lone_only.add(value)
                    continue
                values = {int.from_bytes(data[o:o + 4], "little") for o in run}
                for value in values:
                    targets.add(value)
                    table_mates.setdefault(value, set()).update(values - {value})

        # Tables inside a code section, which the loop above skips. XDK
        # library sections (D3D, DSOUND, XPP) are executable and carry their
        # own data, so a handler table can sit between two functions of the
        # same section. D3D8's SetRenderState dispatches its complex states
        # through one -- `call [state*4 + table]` -- and on Nightfire,
        # Gauntlet and Bloody Roar the 16 handlers nothing calls directly were
        # the 16 unresolved ICALLs every one of them stopped on, right after
        # CreateDevice.
        #
        # Scanning whole code sections for pointer runs would drag in anything
        # that happens to decode, so only the neighbourhood of a table an
        # indexed call actually names is read, and only the words in it that
        # no switch table and no well-evidenced function body covers (see
        # covered_by_code). The dispatch's
        # displacement is where index 0 would be, not where the entries
        # start (SetRenderState's first entry is 0x88 slots in, and slot 0
        # lands inside another function), hence a window rather than a walk
        # from the displacement. What the window finds goes through the same
        # filters as a data-section table below.
        jump_tables = sorted(self.engine.jump_tables.items())
        jt_starts = [t[0] for t in jump_tables]

        def in_jump_table(addr: int) -> bool:
            i = bisect.bisect_right(jt_starts, addr) - 1
            return i >= 0 and addr < jump_tables[i][1]

        def covered_by_code(addr: int) -> bool:
            # A body found only by the padding rule does not vouch for its
            # bytes. Gauntlet's SetRenderState table starts with 11 words
            # inside sub_000CC4C8, a "function" the cc_boundary pass found
            # because the data before it decodes to a ret and an int3; it
            # has no callers and no prologue. Skipping those words lost 10
            # of the 16 handlers. A run of code pointers is the stronger
            # evidence there.
            i = bisect.bisect_right(starts, addr) - 1
            if i < 0 or addr >= bounds[i][1]:
                return False
            fn = self.functions.get(bounds[i][0])
            return getattr(fn, "detection_method", "") != "cc_boundary"

        for disp in sorted(getattr(self.engine, "call_tables", ())):
            sec = self.image.get_section_at_va(disp)
            if sec is None or sec.name not in code_names:
                continue                    # a data section: scanned above
            data = self.image.get_section_data(sec)
            if not data:
                continue
            base = disp - sec.virtual_addr
            window = data[base:base + config.CALL_TABLE_WINDOW]
            run: List[int] = []
            for off in range(0, len(window) - 3, 4):
                addr = disp + off
                value = int.from_bytes(window[off:off + 4], "little")
                if (in_code_section(value) and not covered_by_code(addr)
                        and not in_jump_table(addr)):
                    run.append(value)
                    continue
                if len(run) >= 2:
                    values = set(run)
                    for v in values:
                        targets.add(v)
                        table_mates.setdefault(v, set()).update(values - {v})
                run = []
            if len(run) >= 2:
                values = set(run)
                for v in values:
                    targets.add(v)
                    table_mates.setdefault(v, set()).update(values - {v})

        # Alias entries, not candidates.
        #
        # Registering these as function starts measurably hurt: Half-Life 2
        # went from 5,305 constructors to 5,260 and from 1 clobbered
        # callee-saved register to 13, and the generated code shrank by 70%.
        # A start inside a gap does not truncate an already-measured body, but
        # it does stop its neighbour *extending* into that gap on the rebuild,
        # and plenty of bodies legitimately reach past their first measurement
        # via an out-of-line tail.
        #
        # An alias is the whole point: a callable entry that shares the
        # enclosing extent, built after every boundary is fixed, so it cannot
        # clamp anyone. Exactly what the tail-jump pass does for the same shape.
        section_end = {}
        for sec in sections:
            section_end[sec.name] = sec.virtual_addr + sec.virtual_size

        found = 0
        lone_only -= targets
        for target in sorted(targets | lone_only):
            if target in self.functions or target in self._alias_entries:
                continue
            lone = target in lone_only
            j = bisect.bisect_right(starts, target) - 1
            if j >= 0 and bounds[j][0] < target < bounds[j][1]:
                if lone:
                    continue            # a lone word inside a function: no
                # Inside a function, so the bytes are known to be code and the
                # Inside a function, so the bytes are known to be code and the
                # only real question is whether the address is an instruction
                # boundary rather than the middle of one. Requiring a ret here
                # would be wrong: MSVC's constructor thunks are
                # `mov ecx, <this>; jmp <ctor>` and end in a tail jump, which
                # is exactly what the strict probe rejects. Share the enclosing
                # end, as the tail-jump pass does.
                if target not in self.engine.instructions:
                    continue
                end = bounds[j][1]
            else:
                # In a gap there is no enclosing body vouching for the bytes,
                # so require two things instead of one: the address is an
                # instruction boundary the sweep already found, and the stream
                # from it reaches a ret *or* a tail jump.
                #
                # Insisting on a ret was too strict. MSVC emits C++ constructor
                # thunks as `mov ecx, <this>; jmp <ctor>`, and a whole run of
                # them can sit between two detected functions -- 45 of Half-Life
                # 2's static initialisers are exactly that, in the gap after
                # sub_005C0FB0. Their `push .. call .. ret` siblings passed the
                # strict probe and they did not, which is a distinction with no
                # meaning: both are entry points named by the same table.
                #
                # Padding is what the boundary check buys: a run of int3 is not
                # an instruction the sweep records a start for, and a table does
                # not point into it anyway.
                if target not in self.engine.instructions:
                    continue
                first = self.engine.instructions[target]
                if first.mnemonic.lower() in ("int3", "nop"):
                    continue
                # A short probe first. The cap is doing precision work here,
                # not just bounding cost: with the probe's full 8192, runs of
                # data that merely decode (WMADEC's `add [eax],eax` tables,
                # XON_RD's strings) eventually hit some ret and got through.
                #
                # But 64 alone rejects real vtable methods whose first ret is
                # further in -- Burnout 2's 0x000419C0 and 0x0004B7B0 (71 and
                # 178 instructions), each the first of four entries whose
                # other three were already known. Both became unresolved
                # ICALLs, their objects were never initialised, and the title
                # rebooted itself during the attract movie.
                #
                # So a longer body needs better evidence than length, and two
                # pieces of it:
                #
                #   - another entry in the same table run is already a known
                #     function start. Both of Burnout 2's vtables have
                #     0x000D7400 two slots along; the words right beside slot 0
                #     are themselves only found by this pass.
                #   - the target starts where an entry can: after padding, or
                #     after a ret or jmp. .XTLID is a table of (library id,
                #     address) pairs whose ids -- 0x0002013D, 0x00020170 --
                #     fall in .text's range, sit beside real addresses, and land
                #     mid-function, straight after a mov that runs into them.
                #
                # Or it is laid out exactly like a function start
                # (_is_padded_entry: 16-aligned, straight after padding).
                # 0x0007C7B0's table mates are only found by this same pass,
                # so the first test cannot see them yet; it sits after a nop
                # on a 16-byte boundary. A lone word gets only this route.
                if lone:
                    if not self._is_padded_entry(target):
                        continue
                elif not self.engine.probes_as_function_body(target,
                                                             max_insns=64):
                    mates = table_mates.get(target, ())
                    if not ((any(m in self.functions for m in mates)
                             and self._starts_after_boundary(target)
                             and self.engine.probes_as_function_body(target))
                            or self._is_padded_entry(target)):
                        continue
                i = bisect.bisect_right(starts, target)
                sec = self.image.get_section_at_va(target)
                end = starts[i] if i < len(starts) else section_end.get(
                    sec.name if sec else "", target + 4)
                # In a gap, "the next known start" is a poor bound on its
                # own: the starts list predates this pass, so a table of 30,000
                # entries lands in a region with almost no known starts and
                # every alias runs to the next real function or the section
                # end. On Burnout 2 that gave 30,378 alias bodies with a median
                # size of 115 KB, in a 2 MB .text whose genuine functions
                # median 84 bytes -- 22,483 of them over 50 KB.
                #
                # Measure the body properly instead. _find_function_end walks
                # forward tracking every internal branch target, so it stops
                # only once it has decoded past all of them -- which is what a
                # function with several return paths needs.
                #
                # Stopping at the first terminator instead, as this did, cuts
                # any such function at its first ret. Burnout 2's global
                # constructor sub_00120307 was cut that way: two later branches
                # target the byte after that ret, so they lifted as tail calls
                # to a stub and the function returned without popping either the
                # saved esi or its own return address. _initterm calls that
                # constructor with esi holding its table cursor, and got it back
                # as 0x14.
                measured = self._find_function_end(
                    target, end,
                    section_end.get(sec.name if sec else "", None))
                if measured and target < measured < end:
                    end = measured
            if end <= target:
                continue
            self._alias_entries[target] = end
            found += 1

        if found:
            print(f"  {found} function address(es) found in data tables")
        return found > 0

    def _is_padded_entry(self, addr: int) -> bool:
        """Is `addr` laid out exactly like a function start: 16-byte aligned,
        right after int3/nop padding, decoding as a body with the probe's full
        reach, and not unwinding registers it never saved?

        For an immediate, the strict returning-body probe is the right first
        test -- an immediate is weak evidence -- but it rejects real callbacks
        whose first ret is further in than its window, or that pass through a
        jmp on the way. Burnout 2 stores 0x000BFAD0 into a field at four call
        sites (first ret after 77 instructions) and pushes 0x00115140 as a
        callback; both sit after padding on a 16-byte boundary, neither was
        found, and the unresolved calls to them walked esp up through main
        until it returned and the title booted to the dashboard.

        The padding is what makes this safe: a compiler pads to an alignment
        boundary only in front of a function, and an immediate that is really
        a number rarely lands both 16-aligned and straight after padding.
        """
        if addr % 16 != 0:
            return False
        if self.image.read_bytes_at_va(addr - 1, 1) not in (b"\xcc", b"\x90"):
            return False
        if self.engine.entry_pops_unsaved(addr):
            return False
        return self.engine.probes_as_function_body(addr)

    def _starts_after_boundary(self, addr: int) -> bool:
        """Is `addr` where a function entry can begin: right after padding
        (int3 or nop), or right after a ret or unconditional jmp?

        Code that falls through into `addr` -- a mov that ends exactly there
        -- makes it the middle of something, whatever table points at it.
        Read-only.
        """
        before = self.image.read_bytes_at_va(addr - 1, 1)
        if before in (b"\xcc", b"\x90"):
            return True
        for a in range(addr - 15, addr):
            insn = self.engine.instructions.get(a)
            if insn is not None and insn.address + insn.size == addr:
                return bool(insn.is_ret or (insn.is_jump
                                            and not insn.is_cond_jump))
        return False

    def _pass_cond_branch_orphans(self, bodies, starts) -> bool:
        """
        A conditional branch out of its function into unclaimed bytes.

        The pass above excludes jcc for a good reason -- a jcc target past the
        end of its own function usually means the body was measured short, and
        registering a function start there splits it instead of extending it.
        But the code still has to exist somewhere, and when it does not, the
        branch is lifted as a call to a stub that pops a return address and
        returns.

        That is not a small loss. MSVC parks an out-of-line block of realloc
        (sub_005B2C88) at 0x005B2D38 and reaches it with `jne`. Half-Life 2's
        realloc was truncated at 0x005B2D2A because a helper at 0x005B2D2F is
        separately called, so the block became orphaned and the branch went to
        a stub -- meaning the *common* path of realloc never ran its SEH
        epilogue. esp came back unrestored and ebx/esi/edi were never popped,
        so every CUtlMemory::Grow got a garbage buffer and every growable
        container in the game was quietly corrupt.

        Registering an alias rather than a candidate is what makes this safe:
        aliases are built after the bodies are measured, so they cannot clamp
        anyone's end, which is precisely the failure the jcc exclusion was
        protecting against. The target must also land in a gap -- inside
        another function is the alias case the pass above already handles --
        and must decode to a ret, so a mis-measured body's interior does not
        qualify on the strength of one branch.
        """
        added = False
        for insn in self.engine.instructions.values():
            if not insn.is_cond_jump:
                continue
            target = insn.jump_target
            if target is None or target in self._alias_entries:
                continue
            if target in self._candidates or target in self.functions:
                continue

            i = bisect.bisect_right(starts, insn.address) - 1
            if i < 0:
                continue
            body_start, body_end = bodies[i]
            if insn.address >= body_end:
                continue                    # not inside any known function
            if body_start <= target < body_end:
                continue                    # ordinary intra-function branch

            j = bisect.bisect_right(starts, target) - 1
            if j >= 0 and bodies[j][0] <= target < bodies[j][1]:
                continue                    # inside a function: handled above

            section = self.image.get_section_at_va(target)
            if section is None or not section.executable:
                continue
            # A block that ends in an unconditional jmp instead of a ret is
            # the other half of this shape, and rejecting it left the branch
            # pointing at a stub just the same. Half-Life 2's _lock helper
            # (sub_005BE146) exits its scan loop into two such blocks at
            # 0x005BE244 and 0x005BE250; both were emitted as stubs that
            # returned without running the function's __finally, so _unlock
            # never ran. The CRT lock was taken 15 times and released none,
            # and the next thread to want it waited forever -- a level load
            # that deadlocked two thirds of the way in, with nothing in the
            # log to connect it to a missed function boundary.
            tail = self.engine.block_tail_jump(target)
            if not (self.engine.probes_as_returning_body(target, max_insns=256)
                    or self.engine.probes_as_vcall_thunk(target)
                    or tail is not None):
                continue

            # Run to the next known function start, or the section end --
            # then clamp to where the block this pass just probed actually
            # ends. The next-start bound is only a proxy for the body's
            # extent, and where starts are sparse it is a wild one: on
            # Burnout 2 it produced 30,378 alias bodies with a median size of
            # 115 KB, in a .text of 2 MB whose real functions median 84 bytes.
            # Translating those is the difference between a working build and
            # one that never finishes generating.
            k = bisect.bisect_right(starts, target)
            end = starts[k] if k < len(starts) else (section.virtual_addr
                                                    + section.virtual_size)
            # Measured the same way as _pass_data_ptr_targets, and for the
            # same reason: block_extent_end stops at the first terminator, so
            # it cuts a function with several return paths at its first ret
            # (0x00120307: 0x00120357 rather than 0x001203B0) and a switch
            # dispatcher at its indirect jmp (0x000EC640: 30 bytes of 320,
            # stranding every case body and all four epilogues).
            # _find_function_end tracks internal branch targets instead, so it
            # stops only once it has decoded past all of them.
            measured = self._find_function_end(
                target, end,
                section.virtual_addr + section.virtual_size)
            if measured and target < measured < end:
                end = measured
            if end <= target:
                continue
            self._alias_entries[target] = end
            added = True

            # Where that jump lands has to be addressable too, or the block we
            # just recovered ends in a call to a stub and nothing is gained.
            # Landing inside a function is the ordinary case -- it is the rest
            # of the loop -- and the alias mechanism already exists for it.
            if tail is not None and tail not in self._alias_entries                     and tail not in self._candidates and tail not in self.functions:
                m = bisect.bisect_right(starts, tail) - 1
                if m >= 0 and bodies[m][0] < tail < bodies[m][1]:
                    self._alias_entries[tail] = bodies[m][1]

        if added:
            print("  conditional-branch orphans recovered as alias entries")
        return added

    def _pass_alias_tail_jumps(self, sections: List[SectionInfo]) -> int:
        """
        Follow the tail jumps out of alias entries.

        _pass_tail_jump_targets only sees a jmp that sits inside a function
        body, and alias entries are not bodies yet when it runs -- most of them
        come from _pass_data_ptr_targets, after its last round. So a function
        reached only by a tail jump *from an alias* was never discovered, and
        the jump lifted as a call to a stub that pops the return address and
        does nothing else.

        That is the usual shape of a C++ static initialiser: an entry in the
        CRT's initialiser table, `mov ecx, <object>; jmp <constructor>`.
        Burnout 2 had 40 such targets. One was the sound manager's constructor
        (sub_000CA4E0, via sub_00139360), which never ran; the object kept a
        null mixer pointer, and the title screen crashed a minute later in the
        sound Pause, with nothing in the log pointing back here.

        New entries are aliases, for the reason _pass_data_ptr_targets gives:
        built after every boundary is fixed, they cannot clamp a neighbour.
        Each new alias is scanned in turn, since it can tail-jump onward.

        Returns the number of entries added.
        """
        bounds = sorted((f.start, f.end) for f in self.functions.values())
        starts = [b[0] for b in bounds]
        section_end = {sec.name: sec.virtual_addr + sec.virtual_size
                       for sec in sections}

        found = 0
        # Real function bodies as well as aliases. The tail-jump rounds run
        # before the immediate, gap-prologue and data-table passes, so a
        # function one of those finds never has its jumps followed either.
        # Burnout 2's sub_0003B150 and sub_0003B884 are found by immediate and
        # both jump to 0x0003B90D, a label inside sub_0003B8D3; the only entry
        # there had been an .XTLID id that happened to equal it, and once those
        # stopped counting the jumps went to an empty stub. Targets the tail
        # rounds already registered are skipped below, so rescanning the
        # functions they did see costs time and changes nothing.
        pending = sorted(self._alias_entries.items())
        pending += sorted((f.start, f.end) for f in self.functions.values())
        scanned = set()
        while pending:
            a_start, a_end = pending.pop()
            if a_start in scanned:
                continue
            scanned.add(a_start)
            # A jump back to a label before a_start -- a loop head in the host
            # function -- is followed like any other. It looks internal, and
            # for the host it is: the host lifts it as a goto. But the alias is
            # lifted as its own copy of the host's tail, and in that copy the
            # label is outside the body, so the jump lifts as a tail call. With
            # no entry there it is a call to an empty stub. Burnout 2 has 28
            # alias copies that jump back like this, 42 of them into
            # sub_000FFF20 alone; the alias mechanism is what makes those
            # labels callable.
            # Conditional jumps too, when they land inside a function. A
            # conditional branch out of a body lifts exactly like a tail jmp:
            # `if (cc) { sub_TARGET(); return; }`. Burnout 2's sub_000C08F0 is
            # split at 0x000C0A07 by a DSOUND data word that happens to equal
            # it, and the split-off tail closes its loop with `jne 0x000C0960`
            # back into the host. With no entry there that was a call to an
            # empty stub: the loop body was skipped, the frame leaked, and the
            # front end's element list walk called a null callback forever on
            # the first race's loading screen. Into a gap a conditional jump is
            # weaker evidence of a function start than a jmp, so those are
            # still left alone below.
            for insn in self.engine.get_instructions_in_range(a_start, a_end):
                if not insn.is_branch:      # jmp or Jcc; is_jump is jmp only
                    continue
                target = insn.jump_target
                if target is None or a_start <= target < a_end:
                    continue            # an ordinary branch within the alias
                if (target in self.functions or target in self._alias_entries
                        or target in self._candidates):
                    continue
                sec = self.image.get_section_at_va(target)
                if sec is None or not sec.executable \
                        or sec.name not in section_end:
                    continue

                j = bisect.bisect_right(starts, target) - 1
                if j >= 0 and bounds[j][0] < target < bounds[j][1]:
                    # Inside a function: share its end, as the other alias
                    # passes do. An instruction boundary is all that is needed.
                    if target not in self.engine.instructions:
                        continue
                    end = bounds[j][1]
                elif insn.is_cond_jump:
                    continue
                else:
                    # In a gap: the same evidence _pass_call_targets asks of a
                    # target it has to realign, plus the checks that keep
                    # padding and function interiors out. All of them are
                    # read-only and run first, so a target that is rejected
                    # leaves no decoded stream behind; decode_at only once it
                    # has passed. A direct jmp from code is good evidence, so
                    # the probe gets its full default reach.
                    if self.image.read_bytes_at_va(target, 1) in (b"\xcc",
                                                                  b"\x90"):
                        continue        # padding: int3 or nop
                    if self.engine.entry_pops_unsaved(target):
                        continue        # a continuation, not an entry
                    if not self.engine.probes_as_function_body(target):
                        continue
                    if target not in self.engine.instructions \
                            and not self.engine.decode_at(target):
                        continue
                    k = bisect.bisect_right(starts, target)
                    end = starts[k] if k < len(starts) else section_end[sec.name]
                    measured = self._find_function_end(
                        target, end, section_end[sec.name])
                    if measured and target < measured < end:
                        end = measured
                if end <= target:
                    continue
                self._alias_entries[target] = end
                pending.append((target, end))
                found += 1

        if found:
            print(f"  {found} tail-jump target(s) reached from alias entries")
        return found

    def _build_alias_entries(self) -> None:
        """
        Emit a Function for each tail-jump target that lands inside another
        function, running from the target to that function's end.

        The overlap is deliberate: the translator produces a second body for the
        shared tail, which costs a little code size and makes the entry point
        callable. The alternative -- a stub that returns immediately -- silently
        skips the epilogue and leaks the caller's frame.
        """
        for addr, end in sorted(self._alias_entries.items()):
            if addr in self.functions:
                continue
            insns = self.engine.get_instructions_in_range(addr, end)
            if not insns:
                continue
            section = self.image.get_section_at_va(addr)
            sec_name = section.name if section else ""
            label = self.labels.get(addr)
            name = label.name if label else f"sub_{addr:08X}"
            if not label:
                self.labels.auto_name_function(
                    addr, sec_name, config.CONFIDENCE_TAIL_JUMP)
            self.functions[addr] = Function(
                start=addr,
                end=end,
                name=name,
                section=sec_name,
                confidence=config.CONFIDENCE_TAIL_JUMP,
                detection_method="tail_jump_alias",
                num_instructions=len(insns),
                has_prologue=False,
            )

    def _build_functions(self, sections: List[SectionInfo]) -> None:
        """
        Pass 5: Build Function objects from candidates.

        Determines function boundaries by finding the extent of each
        function (up to the next function start or unreachable point).
        """
        # Switch tables the sweep only exposed after it ran. The first
        # resync_jump_tables() pass sees the dispatches the linear sweep
        # decoded in phase; every decode_at() since -- call targets, seeds,
        # tail-jump targets -- can put a `jmp [reg*4 + tbl]` on the map that
        # was junk before, and its table has never been measured. Without
        # this the function ends on that dispatch and its cases are lost
        # (TimeSplitters 2, sub_000DDD70). Idempotent: tables already done
        # are skipped, so calling it on every rebuild is cheap.
        n_tables = self.engine.resync_jump_tables()
        if n_tables:
            print(f"  Resynced past {n_tables:,d} jump table(s) exposed by realignment")

        # Sort candidates by address.
        #
        # A candidate with no instruction decoded at its own address is not a
        # function, and must not be allowed to bound the one before it. The
        # loop below takes sorted_starts[idx + 1] as the previous function's
        # upper bound *before* discovering the candidate is empty, then drops
        # it with `continue` -- so the clamp stands and nothing is emitted to
        # fill what it cut off.
        #
        # Burnout 2's memcpy (sub_0011EFB0) is the case that matters. Its
        # tail-copy jump table sits inline at 0x0011F230, and a candidate
        # landed at 0x0011F248, four bytes inside that table. resync_jump_tables
        # had already deleted the instructions the sweep hallucinated over the
        # table, so the candidate decoded to nothing -- but it had already
        # clamped memcpy to 0x0011F248, cutting off every unrolled copy block
        # and the epilogue behind them. The result is a 165-byte hole covered
        # by no function, memcpy returning without restoring esi/edi/esp, and
        # the table's own entries lifted as unresolvable indirect calls.
        #
        # Filtering here rather than skipping later is what makes the bound
        # honest: the list this loop walks is the list of real starts.
        sorted_starts = [a for a in sorted(self._candidates.keys())
                         if self.engine.get_instruction(a) is not None]
        if not sorted_starts:
            return

        # Build section boundary lookup
        sec_ranges = {}
        for sec in sections:
            sec_ranges[sec.name] = (sec.virtual_addr,
                                    sec.virtual_addr + sec.virtual_size)

        # Create functions
        for idx, start_addr in enumerate(sorted_starts):
            confidence, method = self._candidates[start_addr]

            # Determine section
            section = self.image.get_section_at_va(start_addr)
            sec_name = section.name if section else ""

            # Determine end address:
            # Walk instructions until we hit the next function start,
            # leave the section, or reach an unconditional terminator
            # with no fall-through.
            if idx + 1 < len(sorted_starts):
                next_func = sorted_starts[idx + 1]
            else:
                next_func = None

            # Section end boundary
            sec_end = None
            if section:
                sec_end = section.virtual_addr + section.virtual_size

            end_addr = self._find_function_end(start_addr, next_func, sec_end)

            # Count instructions
            insns = self.engine.get_instructions_in_range(start_addr, end_addr)
            num_insns = len(insns)

            if num_insns == 0:
                continue

            # Check for prologue
            first_insn = self.engine.get_instruction(start_addr)
            has_prologue = (first_insn is not None and
                            first_insn.mnemonic == "push" and
                            first_insn.op_str == "ebp")

            # Get or create name
            label = self.labels.get(start_addr)
            if label:
                name = label.name
            else:
                name = f"sub_{start_addr:08X}"
                self.labels.auto_name_function(
                    start_addr, sec_name, confidence)

            func = Function(
                start=start_addr,
                end=end_addr,
                name=name,
                section=sec_name,
                confidence=confidence,
                detection_method=method,
                num_instructions=num_insns,
                has_prologue=has_prologue,
            )
            self.functions[start_addr] = func

    def _find_function_end(self, start: int, next_func: Optional[int],
                           sec_end: Optional[int]) -> int:
        """
        Determine where a function ends.

        Walks forward from start, tracking the furthest reachable point
        through fall-through and internal jumps.
        """
        max_addr = start   # exclusive end of the code decoded so far
        max_target = start  # highest branch target that must be *inside* it
        addr = start

        # Upper bound
        upper = sec_end if sec_end else start + 0x100000
        if next_func and next_func < upper:
            upper = next_func

        while addr < upper:
            # An embedded switch table is data sitting on the fall-through
            # path. engine.resync_jump_tables() removed the instructions the
            # sweep hallucinated over it, so there is nothing to decode here --
            # step over the table and carry on with the real code after it.
            # Without this the function ends at its own switch and loses the
            # epilogue, which is how callers of memcpy lost esi/edi.
            tbl_end = self.engine.jump_tables.get(addr)
            if tbl_end is not None and tbl_end <= upper:
                if tbl_end > max_addr:
                    max_addr = tbl_end
                addr = tbl_end
                continue

            insn = self.engine.get_instruction(addr)
            if insn is None:
                break

            end = insn.end_address
            if end > max_addr:
                max_addr = end

            # Track internal forward jumps to extend function bounds.
            #
            # Unconditional jumps count too, not just conditional ones. A body
            # ending in "jmp <forward>" - the tail of an if/else, or a jump
            # over an interleaved block - otherwise hit the break below with
            # max_addr still short of the target, truncating the function
            # mid-body. Everything past the cut then looked like separate code,
            # and the function's own jump targets became calls to empty stubs.
            #
            # `upper` is already clamped to the next known function start, so a
            # target inside these bounds is internal rather than a tail call.
            # is_jump and is_cond_jump are mutually exclusive; is_branch is both.
            if insn.is_branch and insn.jump_target is not None:
                target = insn.jump_target
                if start <= target < upper and target > max_target:
                    # This jump goes forward within bounds, extend
                    max_target = target

            # A switch dispatch's case bodies are jump targets too, they are
            # just held in a table rather than encoded in the instruction.
            # Without them the terminator check below ends the function on the
            # dispatch itself, dropping every case and the shared epilogue --
            # MSVC's memcpy loses its "pop edi / pop esi", so every caller is
            # left with those registers clobbered.
            if insn.jump_table is not None:
                for target in self.engine.jump_table_entries(insn.jump_table):
                    if start <= target < upper and target > max_target:
                        max_target = target

            # A switch dispatch is an unconditional jump, so the terminator
            # check below would end the function on it -- but the table it
            # reads is parked immediately after, and the case bodies and the
            # epilogue follow that. Step over the table instead of stopping.
            if insn.is_jump and not insn.is_cond_jump:
                skipped = self._table_after(insn.end_address, upper)
                if skipped is not None:
                    if skipped > max_addr:
                        max_addr = skipped
                    addr = skipped
                    continue

            if insn.is_ret or (insn.is_jump and not insn.is_cond_jump):
                # Stop only once we have decoded *past* every internal branch
                # target. A target is an address that must be inside the
                # function, so landing exactly on it is not coverage -- the
                # instruction there still has to be decoded. Using the target
                # as an exclusive end cut functions off at their own
                # out-of-line tail: MSVC routinely emits "jmp <backward>" and
                # then parks a conditional branch's target after it. Halo's
                # get_edge_vertex ended at the branch target, so the tail was
                # lifted as a separate function and the jump to it became a
                # tail call that returned without running the epilogue --
                # leaking the whole 28-byte frame on every call.
                if insn.end_address > max_target:
                    break
                # There might be more code after (jumped over)
                addr = insn.end_address
                continue

            addr = insn.end_address

        return max_addr

    def _table_after(self, addr: int, upper: int) -> Optional[int]:
        """End of an embedded jump table starting at or just after `addr`.

        MSVC usually parks the table directly after the dispatching jump, but
        it may align first, so allow a few padding bytes.
        """
        for start in range(addr, addr + 16):
            end = self.engine.jump_tables.get(start)
            if end is not None and end <= upper:
                return end
        return None

    def _build_call_graph(self) -> None:
        """Populate calls_to and called_by for all functions."""
        func_starts = set(self.functions.keys())

        for func in self.functions.values():
            insns = self.engine.get_instructions_in_range(func.start, func.end)
            callees = set()
            for insn in insns:
                if insn.call_target is not None:
                    callees.add(insn.call_target)

            func.calls_to = sorted(callees)

            for callee_addr in callees:
                callee = self.functions.get(callee_addr)
                if callee is not None:
                    callee.called_by.append(func.start)

        # Sort called_by lists
        for func in self.functions.values():
            func.called_by = sorted(set(func.called_by))

    def get_function_at(self, addr: int) -> Optional[Function]:
        """Get the function containing an address."""
        # First check direct match
        if addr in self.functions:
            return self.functions[addr]
        # Search for containing function
        for func in self.functions.values():
            if func.start <= addr < func.end:
                return func
        return None

    def get_functions_in_section(self, section_name: str) -> List[Function]:
        """Get all functions in a section, sorted by address."""
        return sorted(
            [f for f in self.functions.values() if f.section == section_name],
            key=lambda f: f.start
        )

    def summary(self) -> dict:
        """Return summary statistics."""
        by_method: Dict[str, int] = {}
        by_section: Dict[str, int] = {}
        total_insns = 0
        with_prologue = 0

        for func in self.functions.values():
            by_method[func.detection_method] = by_method.get(
                func.detection_method, 0) + 1
            by_section[func.section] = by_section.get(func.section, 0) + 1
            total_insns += func.num_instructions
            if func.has_prologue:
                with_prologue += 1

        return {
            "total_functions": len(self.functions),
            "total_instructions_in_functions": total_insns,
            "with_prologue": with_prologue,
            "by_detection_method": by_method,
            "by_section": by_section,
        }
