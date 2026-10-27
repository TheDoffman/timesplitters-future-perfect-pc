"""Evidence-based x86 code recovery for the project's static analysis pipeline.

Tables are bounded runs of section-local code pointers named by indexed jumps.
Probes report structural evidence, not proof that an address is a function.
Only table recovery changes the recorded stream; all probes are read-only.
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

from dataclasses import dataclass

from capstone import CS_OP_IMM, CS_OP_MEM, CS_OP_REG

from recomp_image import read_code_window
from x86_flow import InstructionTraversal


RETURNS = frozenset(('ret', 'retf', 'retn'))
SAVED_REGISTERS = frozenset(('ebx', 'ebp', 'edi', 'esi'))


@dataclass(frozen=True)
class TableSpan:
    start: int
    targets: tuple[int, ...]

    @property
    def end(self):
        return self.start + 4 * len(self.targets)


@dataclass(frozen=True)
class BlockBoundary:
    end: int
    returns: bool
    target: int | None


def immediate_target(instruction):
    operands = instruction.operands
    return (operands[0].imm & 0xFFFFFFFF
            if operands and operands[0].type == CS_OP_IMM else None)


def register_name(instruction, operand):
    return instruction.reg_name(operand.reg) if operand.type == CS_OP_REG else None


class CodeRecovery(InstructionTraversal):
    """Traversal plus measured table spans and bounded code-shape evidence."""

    def _table_span(self, displacement, owner_end, minimum, maximum):
        section = self.image.get_section_at_va(displacement)
        if section is None or not section.executable:
            return None
        low = section.virtual_addr
        high = low + section.virtual_size

        def pointer(address):
            # Both the slot and its destination must belong to this section.
            if not low <= address <= high - 4:
                return None
            value = self.image.read_u32_at_va(address)
            return value if value is not None and low <= value < high else None

        def forward(address):
            values = []
            for slot in range(address, address + 4 * maximum, 4):
                value = pointer(slot)
                if value is None:
                    break
                values.append(value)
            return values

        start = displacement
        right = forward(start)
        if not right:
            # Index zero can be unused; accept a later run only if it already
            # carries enough independent pointer evidence on its own.
            for omitted in range(1, 4):
                candidate = displacement + 4 * omitted
                values = forward(candidate)
                if len(values) >= minimum:
                    start, right = candidate, values
                    break

        # The backward extension is deliberately stricter: epilogue bytes can
        # look like addresses elsewhere in a large executable section. Require
        # the same local destination cluster, and never consume the dispatch.
        anchor = owner_end if start == displacement and owner_end is not None else start
        cluster = [anchor, *right]
        near_low, near_high = min(cluster) - 4096, max(cluster) + 4096
        floor = max(low, owner_end or low)
        left = []
        for slot in range(start - 4, max(floor - 1, start - 4 * maximum - 1), -4):
            value = pointer(slot)
            if value is None or not near_low <= value <= near_high:
                break
            left.append(value)
        if len(left) + len(right) < minimum:
            return None
        return TableSpan(start - 4 * len(left), tuple(reversed(left)) + tuple(right))

    def resync_jump_tables(self, min_entries=3, max_entries=512):
        if min_entries <= 0 or max_entries <= 0:
            return 0
        owners = {}
        for instruction in self.instructions.values():
            if instruction.jump_table is not None:
                candidate = instruction.jump_table
                owners[candidate] = max(owners.get(candidate, 0), instruction.end_address)
        recovered = 0
        # New candidates discovered while decoding arms belong to the next pass.
        for candidate in sorted(self._jt_candidates - self._jt_done):
            span = self._table_span(candidate, owners.get(candidate), min_entries, max_entries)
            if span is None:
                continue
            collisions = self.get_instructions_in_range(span.start - 15, span.end)
            for instruction in collisions:
                if instruction.end_address > span.start:
                    del self.instructions[instruction.address]
            self._sorted_addrs = None
            self.jump_tables[span.start] = span.end
            self._jt_by_disp[candidate] = span.start
            self._jt_done.add(candidate)
            for address in (span.end, *span.targets):
                self.decode_at(address)
            recovered += 1
        return recovered

    def jump_table_entries(self, tbl):
        start = self._jt_by_disp.get(tbl, tbl)
        end = self.jump_tables.get(start, start)
        return [self.image.read_u32_at_va(slot) or 0 for slot in range(start, end, 4)]

    def _first_arm_after(self, table, site):
        return min((target for target in self.jump_table_entries(table) if target > site), default=None)

    def _probe_stream(self, address, count, byte_limit=None):
        # Eight bytes per requested instruction is the established evidence
        # window, not the architectural maximum instruction size. Count and
        # byte bounds are independent: short encodings cannot exceed count.
        data = read_code_window(self.image, address, max(0, count * 8 if byte_limit is None else byte_limit))
        stream = self._cs.disasm(data, address, count=max(1, count)) if count > 0 else iter(())
        return address + len(data), stream

    def _block_boundary(self, address, count):
        limit, stream = self._probe_stream(address, count)
        for instruction in stream:
            is_return = instruction.mnemonic in RETURNS
            if is_return:
                return BlockBoundary(instruction.address + instruction.size, True, None)
            if instruction.mnemonic == 'jmp':
                target = immediate_target(instruction)
                if target is not None and instruction.address < target < limit:
                    continue  # Local forward transfers are part of this lexical run.
                return BlockBoundary(instruction.address + instruction.size, False, target)
        return None

    def block_extent_end(self, addr, max_insns=256):
        boundary = self._block_boundary(addr, max_insns)
        return boundary.end if boundary is not None else None

    def block_tail_jump(self, addr, max_insns=256):
        boundary = self._block_boundary(addr, max_insns)
        return boundary.target if boundary is not None and not boundary.returns else None

    def entry_pops_unsaved(self, addr, max_insns=256):
        _, stream = self._probe_stream(addr, max_insns)
        saved, violations = set(), []
        for instruction in stream:
            if instruction.mnemonic in RETURNS:
                break
            if instruction.mnemonic not in ('push', 'pop') or len(instruction.operands) != 1:
                continue
            name = register_name(instruction, instruction.operands[0])
            if name not in SAVED_REGISTERS:
                continue
            if instruction.mnemonic == 'push':
                saved.add(name)
            elif name not in saved and name not in violations:
                violations.append(name)
        return violations

    def probes_as_returning_body(self, addr, max_insns=64):
        remaining = max_insns
        cursor = addr
        while remaining > 0:
            limit, stream = self._probe_stream(cursor, remaining)
            for instruction in stream:
                remaining -= 1
                if instruction.mnemonic in RETURNS:
                    return True
                if instruction.mnemonic != 'jmp':
                    continue
                operand = instruction.operands[0]
                if operand.type == CS_OP_MEM and operand.mem.index:
                    cursor = self._first_arm_after(operand.mem.disp & 0xFFFFFFFF, instruction.address)
                    if cursor is None:
                        return False
                    break
                target = immediate_target(instruction)
                if target is None or not instruction.address < target < limit:
                    return False
            else:
                return False
        return False

    def probes_as_function_body(self, addr, max_insns=8192):
        section = self.image.get_section_at_va(addr)
        if section is None or not section.executable:
            return False
        limit = section.virtual_addr + section.virtual_size
        cursor = addr
        for _ in range(max(0, max_insns)):
            if not section.virtual_addr <= cursor < limit:
                break
            known = self.instructions.get(cursor)
            if known is not None:
                if known.is_terminator:
                    return True
                cursor = known.end_address
                continue
            _, stream = self._probe_stream(cursor, 1, byte_limit=15)
            instruction = next(stream, None)
            if instruction is None:
                break
            if instruction.mnemonic in RETURNS or instruction.mnemonic == 'jmp':
                return True
            cursor += instruction.size
        return False

    def follows_padding(self, addr):
        section = self.image.get_section_at_va(addr)
        if section is None or not section.executable or addr <= section.virtual_addr:
            return False
        pair = read_code_window(self.image, addr - 1, 2)
        return len(pair) == 2 and pair[0] in (0x90, 0xCC)

    def _shape(self, address, count, byte_limit):
        return list(self._probe_stream(address, count, byte_limit)[1])

    def probes_as_constant_stub(self, addr):
        shape = self._shape(addr, 2, 12)
        if len(shape) != 2 or shape[0].mnemonic != 'mov' or shape[1].mnemonic not in RETURNS:
            return False
        return [operand.type for operand in shape[0].operands] == [CS_OP_REG, CS_OP_IMM]

    def probes_as_vcall_thunk(self, addr):
        shape = self._shape(addr, 2, 16)
        if len(shape) != 2 or [instruction.mnemonic for instruction in shape] != ['mov', 'jmp']:
            return False
        load, jump = (instruction.operands for instruction in shape)
        if [operand.type for operand in load] != [CS_OP_REG, CS_OP_MEM]:
            return False
        if [operand.type for operand in jump] != [CS_OP_MEM]:
            return False
        source, destination = load[1].mem, jump[0].mem
        return bool(source.base and not source.index and source.disp == 0
                    and destination.base == load[0].reg and not destination.index)

    def probes_as_prologue(self, addr):
        shape = self._shape(addr, 3, 24)
        if not shape:
            return False
        first = shape[0]
        operands = first.operands
        registers = [register_name(first, operand) for operand in operands]
        if first.mnemonic == 'sub' and registers and registers[0] == 'esp':
            return True
        if first.mnemonic == 'mov' and registers == ['edi', 'edi']:
            return True
        if first.mnemonic != 'push' or len(operands) != 1:
            return False
        if registers[0] in ('ebx', 'edi', 'esi'):
            return True
        if registers[0] == 'ebp' and len(shape) >= 2:
            second = shape[1]
            pair = [register_name(second, operand) for operand in second.operands]
            return ((second.mnemonic == 'mov' and pair == ['ebp', 'esp'])
                    or (second.mnemonic == 'lea' and pair and pair[0] == 'ebp'))
        # A plausible SEH frame size and scope pointer followed by its helper.
        # Small/negative constants are weak evidence and do not qualify here.
        return (len(shape) == 3 and operands[0].type == CS_OP_IMM and operands[0].imm >= 10
                and shape[1].mnemonic == 'push' and len(shape[1].operands) == 1
                and shape[1].operands[0].type == CS_OP_IMM and shape[1].operands[0].imm >= 10
                and shape[2].mnemonic == 'call')
