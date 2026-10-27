"""Project x86 operand model and bounded block decoding for C translation.

Only supplied bytes and explicit entries are decoded. Restart anchors partition
linear streams; reachable decoding follows direct branches and fall-through,
never guesses indirect destinations or follows calls outside the function.
"""
from dataclasses import dataclass, field

from capstone import CS_OP_IMM, CS_OP_MEM, CS_OP_REG
from x86_decode import make_decoder

LOOP_JUMPS = frozenset(('loop', 'loope', 'loopne', 'loopz', 'loopnz'))
COND_JUMPS = frozenset(('ja', 'jae', 'jb', 'jbe', 'jc', 'jcxz', 'je', 'jecxz',
    'jg', 'jge', 'jl', 'jle', 'jna', 'jnae', 'jnb', 'jnbe', 'jnc', 'jne',
    'jng', 'jnge', 'jnl', 'jnle', 'jno', 'jnp', 'jns', 'jnz', 'jo', 'jp',
    'jpe', 'jpo', 'js', 'jz')) | LOOP_JUMPS


@dataclass
class Operand:
    type: str
    reg: str | None = None
    imm: int | None = None
    mem_base: str | None = None
    mem_index: str | None = None
    mem_scale: int = 1
    mem_disp: int = 0
    mem_size: int = 0
    mem_seg: str | None = None


@dataclass
class Instruction:
    address: int
    size: int
    mnemonic: str
    op_str: str
    bytes_hex: str
    operands: list[Operand] = field(default_factory=list)
    call_target: int | None = None
    jump_target: int | None = None
    memory_refs: list[int] = field(default_factory=list)
    imm_values: list[int] = field(default_factory=list)

    @property
    def is_call(self):
        return self.mnemonic == 'call'

    @property
    def is_ret(self):
        return self.mnemonic in ('ret', 'retf', 'retn')

    @property
    def is_jump(self):
        return self.mnemonic == 'jmp'

    @property
    def is_cond_jump(self):
        return self.mnemonic in COND_JUMPS

    @property
    def is_branch(self):
        return self.is_jump or self.is_cond_jump

    @property
    def is_terminator(self):
        return self.is_ret or self.is_jump

    @property
    def end_address(self):
        return self.address + self.size


@dataclass
class BasicBlock:
    start: int
    instructions: list[Instruction] = field(default_factory=list)
    successors: list[int] = field(default_factory=list)

    @property
    def end(self):
        return self.instructions[-1].end_address if self.instructions else self.start

    @property
    def last_insn(self):
        return self.instructions[-1] if self.instructions else None


class Disassembler:
    def __init__(self):
        self._cs = make_decoder()

    def _decode_instruction(self, decoded):
        result = Instruction(decoded.address, decoded.size, decoded.mnemonic,
                             decoded.op_str, decoded.bytes.hex())
        for operand in decoded.operands:
            if operand.type == CS_OP_REG:
                result.operands.append(Operand('reg', reg=decoded.reg_name(operand.reg)))
            elif operand.type == CS_OP_IMM:
                value = operand.imm & 0xFFFFFFFF
                result.operands.append(Operand('imm', imm=value))
                result.imm_values.append(value)
            elif operand.type == CS_OP_MEM:
                memory = operand.mem
                names = {key: decoded.reg_name(getattr(memory, key)) or None
                         for key in ('base', 'index', 'segment')}
                displacement = memory.disp if memory.disp < 0 else memory.disp & 0xFFFFFFFF
                result.operands.append(Operand('mem', mem_base=names['base'], mem_index=names['index'],
                    mem_scale=memory.scale, mem_disp=displacement, mem_size=operand.size,
                    mem_seg=names['segment']))
                if not memory.base and not memory.index and memory.disp:
                    result.memory_refs.append(memory.disp & 0xFFFFFFFF)
            else:
                raise ValueError(f'Unsupported decoded operand type: {operand.type}')
        if result.operands and result.operands[0].type == 'imm':
            if result.is_call:
                result.call_target = result.operands[0].imm
            elif result.is_branch:
                result.jump_target = result.operands[0].imm
        return result

    @staticmethod
    def _valid_window(raw, start, end):
        return (isinstance(start, int) and isinstance(end, int) and not isinstance(start, bool)
                and not isinstance(end, bool) and 0 <= start < end <= 2**32 and end - start <= len(raw))

    def disassemble_function(self, raw_bytes, start_va, end_va, resync=None):
        if not self._valid_window(raw_bytes, start_va, end_va):
            return []
        anchors = sorted({start_va, end_va, *(point for point in resync or () if start_va < point < end_va)})
        result = []
        for begin, end in zip(anchors, anchors[1:]):
            raw = raw_bytes[begin - start_va:end - start_va]
            result.extend(self._decode_instruction(decoded) for decoded in self._cs.disasm(raw, begin))
        return result

    def disassemble_cfg(self, raw_bytes, start_va, end_va, entry_points, stop_addresses=None):
        if not self._valid_window(raw_bytes, start_va, end_va):
            return []
        known, pending = {}, list(entry_points)
        stops = set(stop_addresses or ())
        while pending:
            root = pending.pop()
            cursor = root
            while start_va <= cursor < end_va and cursor not in known:
                if cursor != root and cursor in stops:
                    break
                raw = raw_bytes[cursor - start_va:min(end_va - start_va, cursor - start_va + 15)]
                decoded = next(self._cs.disasm(raw, cursor, count=1), None)
                if decoded is None:
                    break
                instruction = self._decode_instruction(decoded)
                known[cursor] = instruction
                if instruction.is_branch and instruction.jump_target is not None:
                    pending.append(instruction.jump_target)
                if instruction.is_terminator:
                    break
                cursor = instruction.end_address
        return [known[key] for key in sorted(known)]

    def build_basic_blocks(self, instructions, func_start, func_end, extra_leaders=None):
        known = {item.address: item for item in instructions
                 if func_start <= item.address < item.end_address <= func_end}
        if not known:
            return []
        leaders = {func_start, min(known), *(extra_leaders or ())}
        predecessors = {location: set() for location in known}
        for location, instruction in sorted(known.items()):
            if instruction.is_branch:
                leaders.add(instruction.jump_target)
                if instruction.jump_target in known:
                    predecessors[instruction.jump_target].add(location)
            if not instruction.is_terminator and instruction.end_address in known:
                predecessors[instruction.end_address].add(location)
            if instruction.is_branch or instruction.is_call:
                leaders.add(instruction.end_address)
        # Independently decoded entries may overlap. Split their convergence
        # point instead of duplicating the shared suffix in both blocks.
        leaders.update(location for location, incoming in predecessors.items() if len(incoming) > 1)
        # Bytes following a return or an undecodable hole are not evidence of
        # another entry. They may be padding/data, even in an executable range.
        # Explicit leaders or real branch/call boundaries can still nominate them.
        leaders.intersection_update(known)
        blocks = []
        for leader in sorted(leaders):
            block, cursor = BasicBlock(leader), leader
            while cursor in known:
                instruction = known[cursor]
                block.instructions.append(instruction)
                cursor = instruction.end_address
                if instruction.is_terminator or instruction.is_cond_jump or cursor in leaders:
                    break
            last = block.last_insn
            if last.is_branch and last.jump_target in leaders:
                block.successors.append(last.jump_target)
            if not last.is_terminator and last.end_address in leaders:
                block.successors.insert(0, last.end_address)
            block.successors = list(dict.fromkeys(block.successors))
            blocks.append(block)
        return blocks
