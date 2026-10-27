"""Original function context and bounded control-flow entry discovery.

Metadata and table patterns are evidence, not a general proof of x86 function
boundaries. Recovered arms must have closed decoded paths and pass the existing
entry-flag screen. The C function emitter remains a separate consumer.
"""
from bisect import bisect_left, bisect_right
from collections import defaultdict
import struct

import translation_layout as layout
from translation_decode import Disassembler
from translation_helpers import _read
from translation_operands import _PARTS


# ISA categories used by the conservative entry-prefix screen. This is not
# whole-graph flag liveness or complete EFLAGS emulation.
_READS_FLAGS = frozenset('adc sbb rcl rcr pushf pushfd lahf into salc'.split())
_PRODUCES_FLAGS = frozenset('add sub cmp test and or xor inc dec neg shl sal shr sar mul imul bsf bsr bt bts btr btc cmpxchg xadd popf popfd sahf comiss ucomiss comisd ucomisd fcomi fcomip fucomi fucomip'.split())
_WRITES_DEST = frozenset('lea pop movzx movsx bswap add adc sub sbb and or xor inc dec neg not shl sal shr sar shld shrd rol ror rcl rcr bsf bsr imul xadd cmpxchg'.split())


def _indexed_table(instruction):
    if not instruction.is_jump or instruction.jump_target is not None or not instruction.operands:
        return None
    operand = instruction.operands[0]
    if operand.type == 'mem' and operand.mem_index and not operand.mem_base:
        return operand.mem_disp
    return None


def _record(address, end, instructions, callers, name, section, method, prologue=False):
    return dict(_addr=address, start=f'0x{address:08X}', end=end, size=end-address,
                name=name or f'sub_{address:08X}', section=section, confidence=0.9,
                detection_method=method, num_instructions=len(instructions),
                has_prologue=prologue, calls_to=[], called_by=sorted(callers))


class FunctionDiscovery:
    def __init__(self, xbe_data, func_db, label_db=None, classification_db=None,
                 abi_db=None, seh_prolog=None, seh_epilog=None, setjmp_fn=None,
                 longjmp_fn=None, trace_functions=None, trace_all_entries=False,
                 icall_sites=None, force_returns=None):
        from translation_context import Lifter
        self.xbe_data = xbe_data
        self.func_db = func_db
        self.label_db = label_db if label_db is not None else {}
        self.classification_db = classification_db if classification_db is not None else {}
        self.abi_db = abi_db if abi_db is not None else {}
        self.icall_sites = dict(icall_sites or {})
        self.force_returns = dict(force_returns or {})
        self.trace_functions = set(trace_functions or ())
        self.trace_all_entries = bool(trace_all_entries)
        self.disasm = Disassembler()
        self.lifter = Lifter(func_db=func_db, label_db=self.label_db, abi_db=self.abi_db,
            xbe_data=xbe_data, seh_prolog=seh_prolog, seh_epilog=seh_epilog,
            setjmp_fn=setjmp_fn, longjmp_fn=longjmp_fn)
        self.owned_function_starts = set()
        self.recovered_function_starts = set()
        self.jump_table_entry_starts = set()
        self._recovered_cfg = {}
        self._ownership_ready = False

    def _read_func_bytes(self, start_va, end_va):
        data = _read(self.xbe_data, start_va, end_va-start_va)
        return bytes(data) if data is not None else None

    def _word(self, address):
        data = _read(self.xbe_data, address, 4)
        return struct.unpack('<I', data)[0] if data is not None else None

    def _instructions(self, start, end):
        data = self._read_func_bytes(start, end)
        return self.disasm.disassemble_function(data, start, end) if data else []

    @staticmethod
    def _find_static_indirect_ranges(instructions, max_bytes=0x10000):
        known = {}
        intervals = set()
        indirect = False
        def invalidate(register):
            known.pop(_PARTS.get(register,(register,))[0],None)
        for insn in instructions:
            ops = insn.operands
            if insn.is_call and insn.call_target is None and ops and ops[0].type == 'reg':
                indirect = True
            if insn.is_call:
                for register in ('eax','ecx','edx'): invalidate(register)
            if insn.mnemonic == 'cmp' and len(ops) >= 2 and all(o.type == 'reg' for o in ops[:2]):
                a, b = (known.get(o.reg) for o in ops[:2])
                if a is not None and b is not None and 0 <= a < b <= 2**32 and not (a % 4 or b % 4) and b-a <= max_bytes:
                    intervals.add((a,b))
            if insn.mnemonic == 'mov' and len(ops) >= 2 and ops[0].type == 'reg':
                value = ops[1].imm if ops[1].type == 'imm' else known.get(ops[1].reg) if ops[1].type == 'reg' else None
                invalidate(ops[0].reg)
                if value is not None and ops[0].reg not in _PARTS: known[ops[0].reg] = value
            elif insn.mnemonic == 'xchg':
                for operand in ops:
                    if operand.type == 'reg': invalidate(operand.reg)
            elif (insn.mnemonic in _WRITES_DEST or insn.mnemonic.startswith(('set','cmov'))) and ops and ops[0].type == 'reg':
                invalidate(ops[0].reg)
        return sorted(intervals) if indirect else []

    def _read_static_callback_table(self, lower, upper, original_starts):
        if lower < 0 or upper <= lower or lower % 4 or upper % 4 or upper-lower > 0x10000:
            return None
        entries = []
        for address in range(lower, upper, 4):
            target = self._word(address)
            if target is None: return None
            if target in (0, 0xffffffff): continue
            index = bisect_right(original_starts, target)
            if not index or _read(self.xbe_data, target, 1) is None: return None
            before = self.func_db[original_starts[index-1]]
            if before.get('end', original_starts[index-1]) <= target:
                if index == len(original_starts): return None
                after = self.func_db[original_starts[index]]
                if before.get('section') != after.get('section') or after.get('section') in ('.rdata', '.data'):
                    return None
            entries.append(target)
        return entries or None

    def discover_static_indirect_targets(self):
        starts = sorted(self.func_db)
        candidates = defaultdict(set)
        for caller, info in list(self.func_db.items()):
            code = self._instructions(caller, info.get('end', caller))
            for low, high in self._find_static_indirect_ranges(code):
                for target in self._read_static_callback_table(low, high, starts) or ():
                    if target not in self.func_db: candidates[target].add(caller)
        for target in sorted(candidates):
            index = bisect_right(starts, target)
            if not 0 < index < len(starts): continue
            previous, following = self.func_db[starts[index-1]], self.func_db[starts[index]]
            if previous.get('end', starts[index-1]) > target or following.get('section', '') in ('.data', '.rdata'): continue
            code = self._instructions(target, starts[index])
            if not any(insn.is_ret for insn in code): continue
            self.func_db[target] = _record(target, starts[index], code, candidates[target], self.label_db.get(target),
                following.get('section', ''), 'static_indirect_table', self._func_has_prologue(code))
            self.recovered_function_starts.add(target)
        return self.recovered_function_starts

    @staticmethod
    def _is_strong_entry(info):
        return bool(info.get('has_prologue') or info.get('called_by'))

    def discover_cfg_ownership(self):
        if self._ownership_ready: return
        groups = defaultdict(lambda: ([], []))
        for address, info in self.func_db.items():
            groups[info.get('section', '')][0 if self._is_strong_entry(info) else 1].append(address)
        for strong, weak in groups.values():
            strong.sort(); weak.sort()
            for start, upper in zip(strong, strong[1:]):
                end = self.func_db[start].get('end', start)
                candidates = set(weak[bisect_left(weak, end):bisect_left(weak, upper)])
                if end >= upper or not candidates: continue
                prefix = self._instructions(start, end)
                bridges = {insn.jump_target for insn in prefix if insn.is_cond_jump and insn.jump_target is not None and end <= insn.jump_target < upper}
                indexed = any(insn.is_jump and insn.jump_target is None and insn.operands and
                              insn.operands[0].type == 'mem' and insn.operands[0].mem_index for insn in prefix)
                if not bridges or not indexed: continue
                recovered = self._recover_cfg(start, upper, bridges, {a for a in self.func_db if start < a < upper})
                if not recovered: continue
                code, tables, targets = recovered
                owned = candidates & targets
                if code and owned:
                    self.owned_function_starts.update(owned)
                    self._recovered_cfg[start] = dict(end=max(i.end_address for i in code), instructions=code, jump_tables=tables)
        self._ownership_ready = True

    def _read_local_jump_table(self, table_va, lower, upper, max_entries=256):
        def scan(first, direction):
            found = []
            for n in range(first, first+max_entries):
                target = self._word(table_va+direction*n*4)
                if target is None or not lower <= target < upper: break
                found.append(target)
            return found
        before, after = scan(1, -1), scan(0, 1)
        if not before and not after:
            after = next((values for skip in (1,2,3) if len(values := scan(skip,1)) >= 2), [])
        return before[::-1]+after if len(before)+len(after) >= 2 else []

    def _read_bounded_jump_table(self, table_va, lower, limit):
        targets = []
        for slot in range(table_va, min(limit, table_va+1024), 4):
            target = self._word(slot)
            if target is None: break
            if lower <= target < table_va: targets.append(target)
            elif slot != table_va: break
        return targets

    def _recover_cfg(self, start, upper, bridge_targets, stop_addresses):
        data = self._read_func_bytes(start, upper)
        if not data: return None
        entries = {start} | {a for a in bridge_targets if start <= a < upper}
        tables = {}
        while True:
            code = self.disasm.disassemble_cfg(data, start, upper, entries, stop_addresses=stop_addresses)
            additional = set()
            for insn in code:
                table = _indexed_table(insn)
                if table is None or not start <= table < upper: continue
                targets = self._read_local_jump_table(table, start, upper)
                if targets:
                    tables[table] = targets
                    additional.update(targets)
            if additional <= entries: break
            entries.update(additional)
        edges = {i.jump_target for i in code if i.jump_target is not None and start <= i.jump_target < upper}
        edges.update(target for targets in tables.values() for target in targets)
        return code, tables, edges

    def _arm_is_whole(self, target, instructions):
        decoded = {insn.address: insn for insn in instructions}
        if target not in decoded: return False
        def callable_entry(address):
            return address not in self.owned_function_starts and self.func_db.get(address, {}).get('end', address) > address
        for insn in instructions:
            destination = insn.call_target if insn.is_call else insn.jump_target
            if destination is not None and not (insn.is_branch and destination in decoded) and not callable_entry(destination): return False
            if not insn.is_terminator and insn.end_address not in decoded: return False
        cursor = target
        while cursor in decoded:
            insn = decoded[cursor]; name = insn.mnemonic
            if name in _READS_FLAGS or name.startswith(('set','cmov','fcmov')) or (insn.is_cond_jump and name not in ('loop','jecxz','jcxz')): return False
            if name in _PRODUCES_FLAGS or insn.is_call or insn.is_branch or insn.is_ret: break
            cursor = insn.end_address
        return True

    def discover_jump_table_entries(self):
        sites = []
        for start, info in self.func_db.items():
            if start in self.owned_function_starts: continue
            recovered = self._recovered_cfg.get(start)
            end = recovered['end'] if recovered else info.get('end',start)
            code = recovered['instructions'] if recovered else self._instructions(start,end)
            sites.extend((start,end,table) for insn in code if (table := _indexed_table(insn)) is not None)
        tables = sorted({table for _,_,table in sites})
        arms = {}
        for caller, end, table in sites:
            next_index = bisect_right(tables,table)
            limit = min(table+1024,tables[next_index]) if next_index<len(tables) else table+1024
            targets = self._read_bounded_jump_table(table,caller,limit)
            if all(caller <= target < end for target in targets): continue
            for target in targets:
                if target in self.func_db: continue
                callers, boundary = arms.setdefault(target,(set(),table))
                callers.add(caller)
                arms[target] = callers,min(boundary,table)
        for target,(callers,upper) in sorted(arms.items()):
            recovered = self._recover_cfg(target,upper,set(),set())
            if not recovered or not recovered[0] or not self._arm_is_whole(target,recovered[0]): continue
            code,tables,_ = recovered
            end = max(insn.end_address for insn in code)
            self.func_db[target] = _record(target,end,code,callers,self.label_db.get(target),
                self.func_db[max(callers)].get('section',''),'jump_table_arm')
            self._recovered_cfg[target] = dict(end=end,instructions=code,jump_tables=tables)
            self.jump_table_entry_starts.add(target)
        return self.jump_table_entry_starts

    def _stub_ret_bytes(self, addr, max_insns=48):
        section = layout._section_at(addr)
        offset = layout.va_to_file_offset(addr)
        if section is None or offset is None or max_insns <= 0: return 0
        end = min(addr+max_insns*8,section.va+min(section.va_size,section.raw_size),addr+len(self.xbe_data)-offset)
        data = self._read_func_bytes(addr,end)
        if not data: return 0
        for number, insn in enumerate(self.disasm._cs.disasm(data,addr)):
            if number >= max_insns or insn.mnemonic in ('call','jmp'): break
            if insn.mnemonic in ('ret','retn'):
                try: return int(insn.op_str,0) if insn.op_str else 0
                except ValueError: return 0
        return 0

    def _determine_calling_convention(self, info):
        return 'thiscall' if 'thiscall' in info.get('name','') or info.get('calling_convention')=='thiscall' else 'cdecl'

    def _func_has_prologue(self, instructions):
        return len(instructions)>=2 and [(i.mnemonic,i.op_str) for i in instructions[:2]]==[('push','ebp'),('mov','ebp, esp')]

    def _func_owns_a_frame(self, instructions):
        return self._func_has_prologue(instructions) or any(i.call_target in self.lifter.SEH_PROLOGS for i in instructions)
