"""Project-owned instruction traversal and bounded control-flow reachability.

This layer records decoded instructions and follows explicit edges. It does
not infer function boundaries, invent indirect targets or decode reachability
holes. The remaining table and function recovery passes supply that evidence.
"""

# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[1]
_layout_root = _layout_tools.parent
if (_layout_tools / 'project_paths.py').is_file():
    _layout_sys.path.insert(0, str(_layout_tools))
    from project_paths import configure as _configure_paths, source_file as repo_file
    _configure_paths()
else:
    # Standalone source fixtures retain their original flat-folder behavior.
    _layout_root = _LayoutPath(__file__).resolve().parents[1]
    def repo_file(root, area, name):
        return _LayoutPath(root) / area / name

from bisect import bisect_left, bisect_right

from recomp_image import read_code_window
from x86_decode import classify_instruction, make_decoder


class InstructionTraversal:
    """Reusable traversal with the state interface consumed by recovery passes."""

    def __init__(self, image):
        self.image = image
        self._cs = make_decoder()
        self.instructions = {}
        self._sorted_addrs = None
        # Recovery owns the contents of these evidence collections. Disassembly
        # only adds candidates; a candidate does not establish a table's extent.
        self.jump_tables = {}
        self.call_tables = set()
        self._jt_candidates = set()
        self._jt_by_disp = {}
        self._jt_done = set()

    def _classify_instruction(self, decoded):
        instruction = classify_instruction(decoded, self.image.base_address, self.image.image_size)
        for candidate, collection in ((instruction.jump_table, self._jt_candidates),
                                      (instruction.call_table, self.call_tables)):
            if candidate is not None:
                collection.add(candidate)
        return instruction

    def _record(self, decoded):
        instruction = self._classify_instruction(decoded)
        self.instructions[instruction.address] = instruction
        self._sorted_addrs = None
        return instruction

    def linear_sweep(self, section, progress_callback=None):
        """Decode each valid run, skipping one byte at an undecodable boundary."""
        data = self.image.get_section_data(section)
        if not data:
            return 0
        cursor = 0
        count = 0
        while cursor < len(data):
            for decoded in self._cs.disasm(data[cursor:], section.virtual_addr + cursor):
                self._record(decoded)
                count += 1
                cursor = decoded.address + decoded.size - section.virtual_addr
            if progress_callback is not None:
                progress_callback(cursor, len(data))
            # disasm stops at the first invalid/truncated encoding. Retrying
            # after that byte recovers the next valid run without looping there.
            cursor += 1
        if progress_callback is not None:
            progress_callback(len(data), len(data))
        return count

    def decode_at(self, addr, max_insns=4096):
        """Realign a bounded stream, retaining any overlapping older stream.

        Stop at a return/unconditional jump or convergence with a known start.
        The starting instruction may already exist; it can still lead into a
        missing suffix. The return value counts only newly recorded starts.
        """
        if max_insns <= 0:
            return 0
        raw = read_code_window(self.image, addr, (max_insns + 1) * 15)
        added = 0
        for decoded in self._cs.disasm(raw, addr):
            instruction = self.instructions.get(decoded.address)
            if instruction is not None and decoded.address != addr:
                break
            if instruction is None:
                instruction = self._record(decoded)
                added += 1
            if instruction.is_terminator or added == max_insns:
                break
        return added

    def recursive_descent(self, start_addresses, section_bounds):
        """Follow cached direct targets and fall-through within allowed ranges.

        Range membership applies to instruction starts. Calls also preserve the
        fall-through edge; unknown indirect jumps and missing decodes end a path.
        Cycles, overlapping roots and out-of-range targets cannot extend the
        finite set of recorded addresses.
        """
        intervals = []
        for low, high in sorted(section_bounds):
            if low >= high:
                continue
            if intervals and low <= intervals[-1][1]:
                intervals[-1] = (intervals[-1][0], max(high, intervals[-1][1]))
            else:
                intervals.append((low, high))
        starts = [low for low, _ in intervals]
        pending = list(start_addresses)
        visited = set()
        reachable = set()
        while pending:
            address = pending.pop()
            if address in visited:
                continue
            visited.add(address)
            index = bisect_right(starts, address) - 1
            if index < 0 or address >= intervals[index][1]:
                continue
            instruction = self.instructions.get(address)
            if instruction is None:
                continue
            reachable.add(address)
            if instruction.is_call and instruction.call_target is not None:
                pending.append(instruction.call_target)
            if instruction.is_branch and instruction.jump_target is not None:
                pending.append(instruction.jump_target)
            if not instruction.is_terminator:
                pending.append(instruction.end_address)
        return reachable

    def get_instruction(self, address):
        return self.instructions.get(address)

    def _ensure_sorted_addrs(self):
        if self._sorted_addrs is None:
            self._sorted_addrs = sorted(self.instructions)

    def instruction_covering(self, addr):
        """Find an overlapping earlier start, considering the x86 15-byte limit."""
        self._ensure_sorted_addrs()
        right = bisect_left(self._sorted_addrs, addr)
        left = bisect_left(self._sorted_addrs, addr - 15)
        for start in reversed(self._sorted_addrs[left:right]):
            instruction = self.instructions[start]
            if instruction.end_address > addr:
                return instruction
        return None

    def get_instructions_in_range(self, start, end):
        self._ensure_sorted_addrs()
        low = bisect_left(self._sorted_addrs, start)
        high = bisect_left(self._sorted_addrs, end)
        return [self.instructions[address] for address in self._sorted_addrs[low:high]]

    def instruction_count(self):
        return len(self.instructions)

    def get_call_targets(self):
        return {instruction.call_target for instruction in self.instructions.values()
                if instruction.call_target is not None}

    def get_indirect_call_refs(self):
        references = {}
        for instruction in self.instructions.values():
            if instruction.is_call and instruction.memory_ref is not None:
                references.setdefault(instruction.memory_ref, []).append(instruction.address)
        return references
