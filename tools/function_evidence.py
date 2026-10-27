"""Bounded instruction and pointer evidence for project function classification.

This layer reports observations; category policy belongs to the catalog that
consumes them. Embedded byte patterns are never treated as decoded operands.
"""
from bisect import bisect_right
from collections import defaultdict
from dataclasses import dataclass
import struct

from capstone import CS_OP_IMM, CS_OP_MEM, CS_OP_REG, CS_AC_READ, CS_AC_WRITE, CS_GRP_JUMP
from recomp_image import DATA_SECTION_NAMES, read_code_window
from x86_decode import make_decoder
from x86_recovery import CodeRecovery


def address(value, limit=0xFFFFFFFF):
    if isinstance(value, bool) or not isinstance(value, (int, str)):
        raise ValueError('Function address must be an integer or hexadecimal string')
    result = int(value, 16) if isinstance(value, str) else value
    if not 0 <= result <= limit:
        raise ValueError('Function address is outside the guest address space')
    return result


class FunctionRanges:
    """Find a containing body, including overlapping callable aliases."""
    def __init__(self, records):
        self.records = {}
        for record in records:
            start, end = address(record['start']), address(record['end'], limit=2**32)
            if end <= start or start in self.records:
                raise ValueError('Function bodies need distinct starts and positive extents')
            self.records[start] = dict(record, start=start, end=end)
        self.starts = sorted(self.records)
        self.prefix_end = []
        greatest = 0
        for start in self.starts:
            greatest = max(greatest, self.records[start]['end'])
            self.prefix_end.append(greatest)

    def containing(self, location):
        index = bisect_right(self.starts, location) - 1
        while index >= 0 and self.prefix_end[index] > location:
            record = self.records[self.starts[index]]
            if location < record['end']:
                return record['start']
            index -= 1
        return None


@dataclass
class OperandEvidence:
    data_references: dict[int, list[int]]
    immediate_stores: dict[int, list[int]]


def collect_operands(image, functions):
    """Decode each bounded body and associate data pointers with its owner."""
    ranges = FunctionRanges(functions)
    references, stores = defaultdict(set), defaultdict(set)
    decoder = make_decoder()
    seen = set()
    for start in ranges.starts:
        end = ranges.records[start]['end']
        for instruction in decoder.disasm(read_code_window(image, start, end - start), start):
            if instruction.address in seen:
                continue
            seen.add(instruction.address)
            owner = ranges.containing(instruction.address)
            if owner is None:
                continue
            for operand in instruction.operands:
                target = None
                if operand.type == CS_OP_IMM:
                    target = operand.imm & 0xFFFFFFFF
                elif operand.type == CS_OP_MEM and not operand.mem.base:
                    target = operand.mem.disp & 0xFFFFFFFF
                if target is not None:
                    section = image.get_section_at_va(target)
                    if section is not None and (not section.executable or section.name in DATA_SECTION_NAMES):
                        references[target].add(owner)
            operands = instruction.operands
            if (instruction.mnemonic == 'mov' and len(operands) == 2
                    and operands[0].type == CS_OP_MEM and operands[1].type == CS_OP_IMM
                    and operands[0].size == 4 and operands[1].size == 4):
                stores[operands[1].imm & 0xFFFFFFFF].add(owner)
    return OperandEvidence({key: sorted(value) for key, value in sorted(references.items())},
                           {key: sorted(value) for key, value in sorted(stores.items())})


def find_pointer_tables(image, functions, operands=None):
    """Find arrays corroborated by callable entries, rather than VA range alone.

Three slots and at least two distinct targets establish an array. At least one
    target must already be a known function; remaining targets need a body probe
    bounded to 128 instructions. This is evidence, not proof of a C++ vtable.
"""
    ranges = FunctionRanges(functions)
    known = set(ranges.starts)
    recovery = CodeRecovery(image)
    eligibility = {}
    def callable_target(target):
        if target not in eligibility:
            section = image.get_section_at_va(target)
            eligibility[target] = bool(section and section.executable
                and image.va_to_file_offset(target) is not None
                and (target in known or recovery.probes_as_function_body(target, max_insns=128)))
        return eligibility[target]
    results = []
    for section in image.sections:
        if not section.raw_size:
            continue
        data = image.get_section_data(section)
        size = min(len(data), section.virtual_size)
        offset = (-section.virtual_addr) % 4
        while offset + 4 <= size:
            first = offset
            entries = []
            while offset + 4 <= size:
                target = struct.unpack_from('<I', data, offset)[0]
                if not callable_target(target):
                    break
                entries.append(target)
                offset += 4
            if len(entries) >= 3 and len(set(entries)) >= 2 and any(target in known for target in entries):
                location = section.virtual_addr + first
                results.append(dict(address=location, entries=entries, section=section.name,
                                    constructors=list(operands.immediate_stores.get(location, ())) if operands else []))
            # A failing slot terminates this run. Consume it once, then resume
            # at the next aligned slot; never skip a final four-byte entry.
            offset += 4
    return results


def scalar_initializers(image, functions):
    """Describe short, straight-line scalar SSE data initialization bodies.

    Require a complete decode, a plain return, data-only absolute memory accesses,
    and both a memory read and a memory write. Return observations, not names
    guessed from a byte prefix or an assumed CRT implementation.
    """
    ranges, decoder = FunctionRanges(functions), make_decoder()
    operations = {'mov', 'add', 'sub', 'mul', 'div', 'sqrt', 'min', 'max'}
    result = {}
    for start in ranges.starts:
        length = ranges.records[start]['end'] - start
        if length > 256:
            continue
        raw = read_code_window(image, start, length)
        instructions = list(decoder.disasm(raw, start))
        if (len(raw) != length or len(instructions) < 3
                or sum(i.size for i in instructions) != length
                or instructions[-1].mnemonic != 'ret' or instructions[-1].op_str):
            continue
        reads, writes, precisions = set(), set(), set()
        valid = True
        for instruction in instructions[:-1]:
            name = instruction.mnemonic
            suffix = name[-2:]
            if suffix not in ('ss', 'sd') or name[:-2] not in operations:
                valid = False
                break
            precisions.add(suffix)
            width = 4 if suffix == 'ss' else 8
            for operand in instruction.operands:
                if operand.type == CS_OP_REG:
                    if not instruction.reg_name(operand.reg).startswith('xmm'):
                        valid = False
                elif operand.type == CS_OP_MEM:
                    memory = operand.mem
                    target = memory.disp & 0xFFFFFFFF
                    section = image.get_section_at_va(target)
                    if (memory.base or memory.index or memory.segment or section is None
                            or section.executable and section.name not in DATA_SECTION_NAMES
                            or target + width > section.virtual_addr + section.virtual_size):
                        valid = False
                        break
                    if operand.access & CS_AC_READ:
                        reads.add(target)
                    if operand.access & CS_AC_WRITE:
                        writes.add(target)
                else:
                    valid = False
            if not valid:
                break
        if valid and reads and writes:
            result[start] = dict(precision='single' if precisions == {'ss'} else
                                'double' if precisions == {'sd'} else 'mixed',
                                operations=len(instructions) - 1, reads=sorted(reads), writes=sorted(writes))
    return result


def entry_blocks(image, tables, functions, maximum=128):
    """Measure the first block of unlisted table targets, without inventing bodies.

    A conditional branch ends this measurement too. The returned extent covers
    only decoded instructions up to that boundary, not an inferred function.
    Unknown indirect destinations and later blocks remain unmeasured.
    """
    known = set(FunctionRanges(functions).starts)
    decoder = make_decoder()
    result = []
    targets = sorted({target for table in tables for target in table['entries']} - known)
    for target in targets:
        section = image.get_section_at_va(target)
        if not section or not section.executable or maximum <= 0:
            continue
        for count, instruction in enumerate(decoder.disasm(
                read_code_window(image, target, maximum * 15), target, count=maximum), 1):
            if instruction.group(CS_GRP_JUMP) or instruction.mnemonic in ('ret', 'retf'):
                result.append(dict(start=target, end=instruction.address + instruction.size,
                    instructions=count, boundary=instruction.mnemonic, scope='entry_block'))
                break
    return result


def numeric_observations(image, functions):
    """Record decoded numeric operations, without claiming CRT symbol identity.

    Complete short bodies only. Operation presence is evidence of an operation,
    not proof of the whole function's arguments, return value or semantics.
    """
    ranges, decoder = FunctionRanges(functions), make_decoder()
    selected = {'fcos', 'fsin', 'fsincos', 'fprem', 'fprem1', 'fist', 'fistp',
                'fisttp', 'shld', 'shrd'}
    result = {}
    for start in ranges.starts:
        length = ranges.records[start]['end'] - start
        if length > 256:
            continue
        raw = read_code_window(image, start, length)
        instructions = list(decoder.disasm(raw, start))
        if len(raw) != length or sum(item.size for item in instructions) != length:
            continue
        operations = sorted({item.mnemonic for item in instructions} & selected)
        if operations:
            result[start] = dict(operations=operations, scope='instruction_presence')
    return result
