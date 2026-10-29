"""Read fixture constants from original instructions, independent of C spelling."""
from capstone import CS_ARCH_X86, CS_MODE_32, Cs
from capstone.x86 import X86_OP_MEM


def absolute_data_words(image, ranges, offset_for, *, low=0x470000, high=0x490000):
    """Return original data words referenced by absolute memory operands.

    These fixtures model the supported US image's read-only constant region.
    Include every word of SIMD/x87 operands, not just their starting address.
    """
    decoder = Cs(CS_ARCH_X86, CS_MODE_32)
    decoder.detail = True
    addresses = set()
    for start, end in ranges:
        offset = offset_for(start)
        for instruction in decoder.disasm(image[offset:offset+end-start], start):
            for operand in instruction.operands:
                if (operand.type != X86_OP_MEM or operand.mem.base or operand.mem.index
                        or operand.mem.segment):
                    continue
                address = operand.mem.disp
                if low <= address and address+operand.size <= high:
                    addresses.update(range(address, address+operand.size, 4))
    return sorted(addresses)
