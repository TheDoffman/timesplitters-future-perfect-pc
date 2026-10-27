"""Project function discovery from decoded code and corroborating evidence.

Candidates split ordinary bodies. Alternate callable entries share an existing
extent instead, so registering a shared tail cannot truncate its owning body.
The detector remains heuristic; its bounded passes do not prove all code paths.
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

from bisect import bisect_right
from collections import Counter
from dataclasses import dataclass, field
import re
import struct

from recomp_symbols import LabelManager


@dataclass
class Function:
    start: int
    end: int
    name: str
    section: str = ''
    confidence: float = 0.0
    detection_method: str = ''
    calls_to: list[int] = field(default_factory=list)
    called_by: list[int] = field(default_factory=list)
    num_instructions: int = 0
    has_prologue: bool = False

    @property
    def size(self):
        return self.end - self.start

    def to_dict(self):
        result = {name: getattr(self, name) for name in
                  ('name', 'section', 'confidence', 'detection_method', 'num_instructions', 'has_prologue')}
        result.update(start=f'0x{self.start:08X}', end=f'0x{self.end:08X}', size=self.size)
        result.update({name: [f'0x{address:08X}' for address in getattr(self, name)]
                       for name in ('calls_to', 'called_by')})
        return result


class BodyIndex:
    """Immutable snapshot of ordered body bounds used during one evidence pass."""
    def __init__(self, pairs):
        self.bounds = sorted(pairs)
        self.starts = [start for start, _ in self.bounds]

    def containing(self, address, strict=False):
        index = bisect_right(self.starts, address) - 1
        if index >= 0:
            start, end = self.bounds[index]
            if start + int(strict) <= address < end:
                return start, end
        return None

    def following(self, address, default):
        index = bisect_right(self.starts, address)
        return self.starts[index] if index < len(self.starts) else default


class FunctionDetector:
    # These ranks order evidence, rather than estimating statistical certainty.
    SCORES = dict(entry_point=1.0, prologue=0.95, prologue_alt=0.95,
                  call_target=0.90, indirect_call_slot=0.90, tail_jump_target=0.88,
                  tail_jump_alias=0.88, cc_boundary=0.85, gap_prologue=0.85,
                  after_switch_table=0.85, imm_ref_target=0.86)

    def __init__(self, engine, image, xrefs=None, labels=None):
        self.engine, self.image, self.xrefs = engine, image, xrefs
        self.labels = labels if labels is not None else LabelManager()
        self._candidates = {}
        self._alias_entries = {}
        self.functions = {}

    def _index(self):
        return BodyIndex((function.start, function.end) for function in self.functions.values())

    def _code(self, address):
        section = self.image.get_section_at_va(address)
        return section if section is not None and section.executable else None

    def _nominate(self, address, method):
        self._add_candidate(address, self.SCORES[method], method)

    def _add_candidate(self, addr, confidence, method):
        prior = self._candidates.get(addr)
        if prior is None or confidence > prior[0]:
            self._candidates[addr] = confidence, method

    def _realign_entry(self, address):
        return (address in self.engine.instructions or
                (self.engine.probes_as_function_body(address) and bool(self.engine.decode_at(address))))

    def detect_all(self, sections=None):
        sections = self.image.get_code_sections() if sections is None else sections
        self._pass_known_addresses()
        for scan in (self._pass_prologues, self._pass_cc_boundaries):
            for section in sections:
                scan(section)
        self._pass_call_targets(sections)
        self._pass_indirect_call_slots()
        self._build_functions(sections)
        for _ in range(8):
            if not self._pass_tail_jump_targets(sections):
                break
            self.functions.clear()
            self._build_functions(sections)
        for discover in (self._pass_imm_ref_targets, self._pass_gap_prologues):
            if discover(sections):
                self.functions.clear()
                self._build_functions(sections)
        self._pass_data_ptr_targets(sections)
        self._pass_seed_aliases()
        self._pass_alias_tail_jumps(sections)
        self._build_alias_entries()
        self._build_call_graph()
        print(f'  Project discovery: {len(self.functions)} functions, {len(self._alias_entries)} alternate entries')
        return len(self.functions)

    def _pass_known_addresses(self):
        self._nominate(self.image.entry_point, 'entry_point')

    def _pass_prologues(self, section):
        data = self.image.get_section_data(section)
        for match in re.finditer(rb'\x55(?:\x8b\xec|\x89\xe5)', data):
            address = section.virtual_addr + match.start()
            if address in self.engine.instructions:
                self._nominate(address, 'prologue' if match.group()[1] == 0x8B else 'prologue_alt')

    def _pass_cc_boundaries(self, section):
        data = self.image.get_section_data(section)
        for match in re.finditer(rb'\xcc+', data):
            start, end = section.virtual_addr + match.start(), section.virtual_addr + match.end()
            if match.end() == len(data) or end not in self.engine.instructions:
                continue
            before = [self.engine.get_instruction(start - distance) for distance in range(1, 8)]
            if any(insn is not None and insn.end_address == start and (insn.is_ret or insn.is_jump)
                   for insn in before):
                self._nominate(end, 'cc_boundary')

    def _pass_call_targets(self, sections):
        for address in self.engine.get_call_targets():
            if self._code(address) is not None and self._realign_entry(address):
                self._nominate(address, 'call_target')

    def _pass_indirect_call_slots(self):
        slots = self.engine.get_indirect_call_refs()
        if not slots:
            return
        for instruction in list(self.engine.instructions.values()):
            target = instruction.imm_ref
            if target is not None and instruction.memory_ref in slots:
                if self._code(target) is not None and self._realign_entry(target):
                    self._nominate(target, 'indirect_call_slot')

    def _table_after(self, addr, upper):
        for offset in range(16):
            end = self.engine.jump_tables.get(addr + offset)
            if end is not None and end <= upper:
                return end
        return None

    def _find_function_end(self, start, next_func, sec_end):
        limit = min(value for value in (sec_end or start + 0x100000, next_func) if value is not None)
        cursor, extent, horizon = start, start, start
        while cursor < limit:
            table_end = self.engine.jump_tables.get(cursor)
            if table_end is not None and cursor < table_end <= limit:
                cursor = table_end
                extent = max(extent, cursor)
                continue
            instruction = self.engine.get_instruction(cursor)
            if instruction is None or instruction.end_address <= cursor:
                break
            extent = max(extent, instruction.end_address)
            targets = []
            if instruction.is_branch and instruction.jump_target is not None:
                targets.append(instruction.jump_target)
            if instruction.jump_table is not None:
                targets.extend(self.engine.jump_table_entries(instruction.jump_table))
            horizon = max([horizon, *(target for target in targets if start <= target < limit)])
            terminal = instruction.is_ret or (instruction.is_jump and not instruction.is_cond_jump)
            continuation = (self._table_after(instruction.end_address, limit)
                            if instruction.is_jump and not instruction.is_cond_jump else None)
            if continuation is not None:
                cursor = continuation
                extent = max(extent, cursor)
            elif terminal and instruction.end_address > horizon:
                break
            else:
                cursor = instruction.end_address
        return extent

    def _publish(self, start, end, confidence, method, alias=False):
        instructions = self.engine.get_instructions_in_range(start, end)
        if not instructions:
            return
        section = self.image.get_section_at_va(start)
        section_name = section.name if section is not None else ''
        label = self.labels.get(start)
        if label is None:
            label = self.labels.auto_name_function(start, section_name, confidence)
        first = self.engine.get_instruction(start)
        framed = not alias and first is not None and first.mnemonic == 'push' and first.op_str == 'ebp'
        self.functions[start] = Function(start, end, label.name, section_name, confidence, method,
                                         num_instructions=len(instructions), has_prologue=framed)

    def _build_functions(self, sections):
        self.engine.resync_jump_tables()
        starts = sorted(address for address in self._candidates if self.engine.get_instruction(address) is not None)
        for position, address in enumerate(starts):
            section = self.image.get_section_at_va(address)
            upper = section.virtual_addr + section.virtual_size if section is not None else None
            successor = starts[position + 1] if position + 1 < len(starts) else None
            score, method = self._candidates[address]
            self._publish(address, self._find_function_end(address, successor, upper), score, method)

    def _bounded_alias_end(self, address, index, section_end):
        upper = index.following(address, section_end)
        measured = self._find_function_end(address, upper, section_end)
        return measured if address < measured < upper else upper

    def _pass_seed_aliases(self):
        index = self._index()
        for address, (_, method) in sorted(self._candidates.items()):
            if method == 'seed_vtable_thunk' and address not in self.functions:
                body = index.containing(address, strict=True)
                if body is not None:
                    self._alias_entries.setdefault(address, body[1])

    def _build_alias_entries(self):
        for address, end in sorted(self._alias_entries.items()):
            if address not in self.functions:
                self._publish(address, end, self.SCORES['tail_jump_alias'], 'tail_jump_alias', alias=True)

    def _pass_tail_jump_targets(self, sections):
        index = self._index()
        changed = False
        for instruction in self.engine.instructions.values():
            target = instruction.jump_target
            if (not instruction.is_jump or instruction.is_cond_jump or target is None
                    or target in self._candidates or target not in self.engine.instructions):
                continue
            caller = index.containing(instruction.address)
            if caller is None or caller[0] <= target < caller[1] or self._code(target) is None:
                continue
            owner = index.containing(target, strict=True)
            if owner is not None:
                if target not in self._alias_entries:
                    self._alias_entries[target] = owner[1]
                    changed = True
            elif not self.engine.entry_pops_unsaved(target):
                self._nominate(target, 'tail_jump_target')
                changed = True
        return self._pass_cond_branch_orphans(index.bounds, index.starts) or changed

    def _pass_cond_branch_orphans(self, bodies, starts):
        index = BodyIndex(bodies)
        changed = False
        for instruction in self.engine.instructions.values():
            target = instruction.jump_target
            if (not instruction.is_cond_jump or target is None or target in self._alias_entries
                    or target in self._candidates or target in self.functions):
                continue
            if index.containing(instruction.address) is None or index.containing(target) is not None:
                continue
            section = self._code(target)
            if section is None:
                continue
            tail = self.engine.block_tail_jump(target)
            if not (tail is not None or self.engine.probes_as_returning_body(target, max_insns=256)
                    or self.engine.probes_as_vcall_thunk(target)):
                continue
            end = self._bounded_alias_end(target, index, section.virtual_addr + section.virtual_size)
            if end <= target:
                continue
            self._alias_entries[target] = end
            changed = True
            if tail is not None and tail not in self._candidates and tail not in self.functions:
                owner = index.containing(tail, strict=True)
                if owner is not None:
                    self._alias_entries.setdefault(tail, owner[1])
        return changed

    def _is_padded_entry(self, addr):
        return (addr % 16 == 0 and self.image.read_bytes_at_va(addr - 1, 1) in (b'\x90', b'\xcc')
                and not self.engine.entry_pops_unsaved(addr) and self.engine.probes_as_function_body(addr))

    def _starts_after_boundary(self, addr):
        if self.image.read_bytes_at_va(addr - 1, 1) in (b'\x90', b'\xcc'):
            return True
        for address in range(addr - 15, addr):
            instruction = self.engine.instructions.get(address)
            if instruction is not None and instruction.end_address == addr:
                return instruction.is_ret or (instruction.is_jump and not instruction.is_cond_jump)
        return False

    def _pass_imm_ref_targets(self, sections):
        index = self._index()
        code = BodyIndex((section.virtual_addr, section.virtual_addr + section.virtual_size) for section in sections)
        targets = {instruction.imm_ref for instruction in self.engine.instructions.values()
                   if instruction.imm_ref is not None}
        accepted = 0
        for target in sorted(targets):
            if target in self.functions or index.containing(target) is not None or code.containing(target) is None:
                continue
            first = self.engine.instructions.get(target)
            if first is None or first.mnemonic.lower() in ('int3', 'nop'):
                continue
            tail = self.engine.block_tail_jump(target, max_insns=16)
            thunk = tail in self.functions and not self.engine.entry_pops_unsaved(target)
            if (thunk or self.engine.probes_as_returning_body(target)
                    or self.engine.probes_as_vcall_thunk(target) or self._is_padded_entry(target)):
                self._nominate(target, 'imm_ref_target')
                accepted += 1
        return accepted > 0

    def _pass_gap_prologues(self, sections):
        index = self._index()
        tables = BodyIndex(self.engine.jump_tables.items())

        def available(address):
            near = bisect_right(tables.starts, address + 3) - 1
            return (address not in self._candidates and address not in self.functions
                    and index.containing(address) is None
                    and not (near >= 0 and address < tables.bounds[near][1]))

        changed = False
        for instruction in list(self.engine.instructions.values()):
            address = instruction.end_address
            if (instruction.is_ret and available(address) and self._code(address) is not None
                    and (self.engine.probes_as_prologue(address) or self.engine.probes_as_constant_stub(address))):
                self._nominate(address, 'gap_prologue')
                changed = True
        for start, end in tables.bounds:
            count = (end - start) // 4
            if count < 2 or self._code(end) is None:
                continue
            cursor = end
            for _ in range(256):
                value = self.image.read_bytes_at_va(cursor, 1)
                if not value or value[0] >= count:
                    break
                cursor += 1
            while self.image.read_bytes_at_va(cursor, 1) in (b'\x90', b'\xcc'):
                cursor += 1
            if available(cursor) and self.engine.probes_as_function_body(cursor):
                self.engine.decode_at(cursor)
                self._nominate(cursor, 'after_switch_table')
                changed = True
        return changed

    @staticmethod
    def _pointer_runs(data, base, accepts):
        run = []
        for offset, (value,) in enumerate(struct.iter_unpack('<I', data[:len(data) // 4 * 4])):
            if accepts(base + offset * 4, value):
                run.append(value)
            else:
                if run:
                    yield run
                run = []
        if run:
            yield run

    def _pass_data_ptr_targets(self, sections):
        index = self._index()
        code = BodyIndex((section.virtual_addr, section.virtual_addr + section.virtual_size) for section in sections)
        code_ends = {section.name: section.virtual_addr + section.virtual_size for section in sections}
        groups, singles = {}, set()

        def remember(run, allow_single):
            if len(run) == 1:
                if allow_single:
                    singles.add(run[0])
                return
            values = frozenset(run)
            for value in values:
                groups.setdefault(value, []).append(values)

        for section in self.image.sections:
            if section.name not in code_ends and section.name != '.XTLID':
                data = self.image.get_section_data(section)
                for run in self._pointer_runs(data, section.virtual_addr, lambda slot, value: code.containing(value) is not None):
                    remember(run, True)
        tables = BodyIndex(self.engine.jump_tables.items())

        def named_table_slot(slot, value):
            if code.containing(value) is None or tables.containing(slot) is not None:
                return False
            body = index.containing(slot)
            return body is None or getattr(self.functions.get(body[0]), 'detection_method', '') == 'cc_boundary'

        for displacement in sorted(getattr(self.engine, 'call_tables', ())):
            section = self.image.get_section_at_va(displacement)
            if section is None or section.name not in code_ends:
                continue
            offset = displacement - section.virtual_addr
            data = self.image.get_section_data(section)[offset:offset + 4096]
            for run in self._pointer_runs(data, displacement, named_table_slot):
                remember(run, False)
        accepted = 0
        for address in sorted(set(groups) | singles):
            if address in self.functions or address in self._alias_entries or address not in self.engine.instructions:
                continue
            isolated = address not in groups
            owner = index.containing(address, strict=True)
            if owner is not None:
                if isolated:
                    continue
                end = owner[1]
            else:
                first = self.engine.instructions[address]
                if first.mnemonic.lower() in ('int3', 'nop'):
                    continue
                if isolated:
                    valid = self._is_padded_entry(address)
                else:
                    related = any(any(mate in self.functions for mate in group) for group in groups[address])
                    valid = (self.engine.probes_as_function_body(address, max_insns=64)
                             or (related and self._starts_after_boundary(address)
                                 and self.engine.probes_as_function_body(address))
                             or self._is_padded_entry(address))
                if not valid:
                    continue
                section = self.image.get_section_at_va(address)
                section_end = code_ends.get(section.name if section else '', address + 4)
                end = self._bounded_alias_end(address, index, section_end)
            if end > address:
                self._alias_entries[address] = end
                accepted += 1
        return accepted > 0

    def _pass_alias_tail_jumps(self, sections):
        index = self._index()
        ends = {section.name: section.virtual_addr + section.virtual_size for section in sections}
        pending = sorted(self._alias_entries.items()) + index.bounds
        visited = set()
        accepted = 0
        while pending:
            start, end = pending.pop()
            if start in visited:
                continue
            visited.add(start)
            for instruction in self.engine.get_instructions_in_range(start, end):
                target = instruction.jump_target
                if not instruction.is_branch or target is None or start <= target < end:
                    continue
                if target in self.functions or target in self._alias_entries or target in self._candidates:
                    continue
                section = self._code(target)
                if section is None or section.name not in ends:
                    continue
                owner = index.containing(target, strict=True)
                if owner is not None:
                    if target not in self.engine.instructions:
                        continue
                    extent = owner[1]
                else:
                    if instruction.is_cond_jump or self.image.read_bytes_at_va(target, 1) in (b'\x90', b'\xcc'):
                        continue
                    if self.engine.entry_pops_unsaved(target) or not self.engine.probes_as_function_body(target):
                        continue
                    if target not in self.engine.instructions and not self.engine.decode_at(target):
                        continue
                    extent = self._bounded_alias_end(target, index, ends[section.name])
                if extent > target:
                    self._alias_entries[target] = extent
                    pending.append((target, extent))
                    accepted += 1
        return accepted

    def _build_call_graph(self):
        incoming = {address: set() for address in self.functions}
        for function in self.functions.values():
            instructions = self.engine.get_instructions_in_range(function.start, function.end)
            function.calls_to = sorted({instruction.call_target for instruction in instructions if instruction.call_target is not None})
            for target in function.calls_to:
                if target in incoming:
                    incoming[target].add(function.start)
        for address, function in self.functions.items():
            function.called_by = sorted(incoming[address])

    def get_function_at(self, addr):
        direct = self.functions.get(addr)
        return direct if direct is not None else next((function for function in self.functions.values()
                                                       if function.start <= addr < function.end), None)

    def get_functions_in_section(self, section_name):
        return sorted((function for function in self.functions.values() if function.section == section_name), key=lambda function: function.start)

    def summary(self):
        functions = list(self.functions.values())
        return dict(total_functions=len(functions), total_instructions_in_functions=sum(function.num_instructions for function in functions),
                    with_prologue=sum(function.has_prologue for function in functions),
                    by_detection_method=dict(Counter(function.detection_method for function in functions)),
                    by_section=dict(Counter(function.section for function in functions)))
