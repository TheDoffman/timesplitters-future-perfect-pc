"""Exact decoded-PC ownership for original continuation dispatch generation.

Recovered functions may share tails or overlapping instruction streams. Byte
agreement and decoded operand agreement are checked before adding a function;
the lowest function entry owns shared PCs. No range grants executable entries.
"""
import hashlib


def checked(start,instructions):
    rows=tuple(instructions)
    if type(start) is not int or not 0<start<2**32 or not rows:
        raise ValueError('Continuation dispatch requires a decoded entry')
    addresses=set()
    for row in rows:
        if (type(row.address) is not int or type(row.size) is not int or
                not 0<row.address<2**32 or not 0<row.size<=15 or row.address+row.size>2**32):
            raise ValueError('Invalid continuation instruction span')
        if row.address in addresses:raise ValueError('Duplicate instruction in continuation body')
        addresses.add(row.address)
        raw=bytes.fromhex(row.bytes_hex)
        if len(raw)!=row.size:raise ValueError('Continuation bytes do not match instruction size')
    if start not in addresses:raise ValueError('Continuation entry is not decoded')
    return rows


def _signature(row):
    # Compact storage keeps a title-wide map from retaining every decoder object.
    # Decoded text/operands/targets must agree even when raw bytes agree.
    value=(row.size,row.mnemonic,row.op_str,row.bytes_hex,
           tuple(tuple(vars(op).items()) for op in row.operands),row.call_target,
           row.jump_target,tuple(row.memory_refs),tuple(row.imm_values))
    return hashlib.sha256(repr(value).encode('utf-8')).digest()


class InstructionIndex:
    def __init__(self):
        self.functions=[]
        self.points={}
        self.pages={}
        self.shared=0

    def add(self,start,instructions):
        rows=checked(start,instructions)
        if self.functions and start<=self.functions[-1]:
            raise ValueError('Continuation functions must have unique increasing entries')
        if len(self.functions)>=2**32:raise ValueError('Too many continuation functions')
        owner=len(self.functions);points={};pages={};shared=0
        for row in rows:
            digest=_signature(row);old=self.points.get(row.address)
            if old is not None:
                if old[1]!=digest:raise ValueError('Conflicting decoded continuation instruction')
                shared+=1
            else:points[row.address]=(owner,digest)
            for offset,value in enumerate(bytes.fromhex(row.bytes_hex)):
                address=row.address+offset;page=address>>12;at=address&4095
                if page not in pages:
                    previous=self.pages.get(page)
                    pages[page]=(bytearray(previous[0]),bytearray(previous[1])) if previous else (bytearray(4096),bytearray(4096))
                data,valid=pages[page]
                if valid[at] and data[at]!=value:
                    raise ValueError('Conflicting continuation code bytes')
                data[at]=value;valid[at]=1
        # A failed add leaves the prior registry usable and unchanged.
        self.points.update(points);self.pages.update(pages)
        self.functions.append(start);self.shared+=shared

    def emit(self,*,run=False):
        if not self.functions:raise ValueError('Continuation dispatch needs decoded functions')
        tag='run' if run else 'step'
        lines=['#include "guest_context_execution.h"']
        lines += [f'int tsfp_context_{tag}_{start:08X}(uint32_t,uint32_t *);' for start in self.functions]
        lines += ['typedef int (*tsfp_context_code_fn)(uint32_t,uint32_t *);',
                  'static const tsfp_context_code_fn tsfp_context_functions[] = {']
        lines += [f'    tsfp_context_{tag}_{start:08X},' for start in self.functions]
        lines += ['};','typedef struct {uint32_t pc,owner;} tsfp_context_pc_entry;',
                  'static const tsfp_context_pc_entry tsfp_context_code[] = {']
        lines += [f'    {{0x{pc:08X}u,{self.points[pc][0]}u}},' for pc in sorted(self.points)]
        lines += ['};',
          'static const tsfp_context_pc_entry *tsfp_context_code_find(uint32_t pc) {',
          '    size_t low=0,high=sizeof tsfp_context_code/sizeof tsfp_context_code[0];',
          '    const size_t count=high;',
          '    while(low<high) {', '        size_t middle=low+(high-low)/2;',
          '        if(tsfp_context_code[middle].pc<pc)low=middle+1;',
          '        else high=middle;', '    }',
          '    if(low==count || tsfp_context_code[low].pc!=pc)return NULL;',
          '    if(tsfp_context_code[low].owner>=sizeof tsfp_context_functions/sizeof tsfp_context_functions[0])return NULL;',
          '    return &tsfp_context_code[low];','}',
          'int tsfp_context_generated_admit(uint32_t pc) {',
          '    return tsfp_context_code_find(pc)!=NULL;', '}',
          'int tsfp_context_generated_step(uint32_t pc,uint32_t *next) {',
          '    const tsfp_context_pc_entry *entry=tsfp_context_code_find(pc);',
          '    return entry && next?tsfp_context_functions[entry->owner](pc,next):TSFP_CONTEXT_STEP_ERROR;', '}', '']
        return '\n'.join(lines)
