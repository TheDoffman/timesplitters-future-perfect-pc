"""Project instruction model and x86-32 operand analysis using permitted Capstone.

Table and immediate references are discovery hints, not evidence of a function
boundary. Function recovery and translation are separate stages. See the
Capstone Python API documentation and docs/independent-runtime.md.
"""
from dataclasses import dataclass

from capstone import Cs, CS_ARCH_X86, CS_MODE_32, CS_OP_IMM, CS_OP_MEM


@dataclass
class Instruction:
    address: int
    size: int
    mnemonic: str
    op_str: str
    bytes_hex: str
    is_call: bool = False
    is_ret: bool = False
    is_jump: bool = False
    is_cond_jump: bool = False
    is_nop: bool = False
    call_target: int | None = None
    jump_target: int | None = None
    memory_ref: int | None = None
    jump_table: int | None = None
    call_table: int | None = None
    imm_ref: int | None = None

    @property
    def end_address(self):
        return self.address + self.size

    @property
    def is_branch(self):
        return self.is_jump or self.is_cond_jump

    @property
    def is_terminator(self):
        return self.is_jump or self.is_ret

    def to_dict(self):
        result = dict(address=f'0x{self.address:08X}', size=self.size,
                      mnemonic=self.mnemonic, op_str=self.op_str, bytes=self.bytes_hex)
        for name in ('call_target', 'jump_target', 'memory_ref', 'imm_ref'):
            value = getattr(self, name)
            if value is not None:
                result[name] = f'0x{value:08X}'
        return result


def make_decoder():
    decoder = Cs(CS_ARCH_X86, CS_MODE_32)
    decoder.detail = True
    return decoder


def classify_instruction(decoded, base_address, image_size):
    """Convert a detailed Capstone record without mutating discovery state."""
    name = decoded.mnemonic
    result = Instruction(decoded.address, decoded.size, name, decoded.op_str, decoded.bytes.hex())
    result.is_call = name == 'call'
    result.is_ret = name in ('ret', 'retf', 'retn')
    result.is_jump = name == 'jmp'
    result.is_cond_jump = name in (
        'ja', 'jae', 'jb', 'jbe', 'jc', 'jcxz', 'je', 'jecxz', 'jg', 'jge',
        'jl', 'jle', 'jna', 'jnae', 'jnb', 'jnbe', 'jnc', 'jne', 'jng', 'jnge',
        'jnl', 'jnle', 'jno', 'jnp', 'jns', 'jnz', 'jo', 'jp', 'jpe', 'jpo',
        'js', 'jz', 'loop', 'loope', 'loopne', 'loopnz', 'loopz')
    result.is_nop = name == 'nop'
    in_image = lambda value: base_address <= value < base_address + image_size
    # Capstone's skip-data pseudo records have no operand detail.
    operands = decoded.operands if decoded.id else ()
    transfer = result.is_call or result.is_branch
    if transfer and operands:
        first = operands[0]
        if first.type == CS_OP_IMM:
            target = first.imm & 0xFFFFFFFF
            if result.is_call:
                result.call_target = target
            else:
                result.jump_target = target
        elif first.type == CS_OP_MEM:
            memory = first.mem
            displacement = memory.disp & 0xFFFFFFFF
            if not memory.base and not memory.index:
                result.memory_ref = displacement
            elif in_image(displacement) and memory.index:
                # Indexed jumps nominate a possible table; later recovery must
                # prove its entries. Calls require the flat four-byte ABI form.
                if result.is_branch:
                    result.jump_table = displacement
                elif not memory.base and memory.scale == 4:
                    result.call_table = displacement
    elif not transfer:
        for operand in operands:
            if operand.type == CS_OP_MEM and not operand.mem.base and not operand.mem.index:
                address = operand.mem.disp & 0xFFFFFFFF
                if result.memory_ref is None and in_image(address):
                    result.memory_ref = address
            elif operand.type == CS_OP_IMM:
                address = operand.imm & 0xFFFFFFFF
                if result.imm_ref is None and in_image(address):
                    result.imm_ref = address
    return result
