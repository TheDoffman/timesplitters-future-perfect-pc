"""Project-owned lifter context and compatibility exports for function translation.

The context is instance-local; instruction handlers are the original project
modules. Function-level scheduling, joins and runtime storage remain separate.
"""
from translation_decode import Instruction, Operand, LOOP_JUMPS
import translation_operands as operands
import translation_moves as moves
import translation_integer as integer
import translation_bits as bits
import translation_bitops as bitops
import translation_product as product
import translation_exchange as exchange
import translation_compare as compare
import translation_strings as strings
import translation_conditions as conditions
import translation_sse as sse
import translation_mmx as mmx
import translation_x87 as x87
import translation_control as control
import translation_lifting as lifting
import translation_helpers as helpers
import translation_names as names


FLAG_SETTERS=frozenset('cmp test sub add and or xor inc dec neg shl shr sar imul mul adc sbb'.split()) | lifting._FLOAT_FLAGS
CF_TRACKED=frozenset('add sub adc sbb shl shr sar shld shrd'.split())
BT_MODIFY=frozenset(('bts','btr','btc'))
ZF_FROM_DEST=conditions._RESULT-{'sal','shld','shrd'}
_RESULT_SNAPSHOT_SETTERS=conditions._RESULT-{'inc','dec'}
_RESULT_SRC_SETTERS=frozenset(('add','sub'))
_EFLAGS_SETTERS=lifting._SETTERS-FLAG_SETTERS
_FLAGS_UNDEFINED=lifting._INVALIDATORS
_EFLAGS_PRESERVE=lifting._PRESERVE | lifting._SIMD_PRESERVE | {'prefetchnta','prefetcht0','prefetcht1','prefetcht2'}
_FUNC_RESERVED_IDENT=names.RESERVED
_WIN32_EXPORTS=names.HOST_EXPORTS
COND_MAP=conditions.COND_MAP
normalise_zero_test=compare.normalise_zero_test
lift_basic_block=lifting.block
try_match_cmp_jcc=lifting.adjacent_condition
_has_xmm_operand=lifting.has_xmm
_is_rep_compare=lifting.repeated_compare
_func_ident=names.identifier
_make_condition=conditions.make_condition
_make_setcc_value=conditions.setcc
_make_cmovcc_cond=conditions.cmovcc
_make_lahf_value=conditions.lahf
zf_expression=conditions.zf_expression
_emit_cond_goto=control.conditional
detect_seh_helpers=helpers.seh
detect_cxx_throw=helpers.cxx_throw
detect_setjmp_helpers=helpers.nonlocal_jumps
_is_eh_record_template=helpers.exception_record


def _addresses(override, detected):
    if override is None:return frozenset(detected)
    return frozenset([override] if isinstance(override,int) else override)


class Lifter:
    SEH_PROLOGS=SEH_EPILOGS=frozenset()
    CXX_THROW=SETJMP_FN=LONGJMP_FN=None
    _SNAP_MASK={width:f'0x{(1<<(8*width))-1:X}u' for width in (1,2,4)}
    _SNAP_SX={width:f'(int{8*width}_t)' for width in (1,2,4)}

    def __init__(self, func_db=None, label_db=None, abi_db=None, xbe_data=None,
                 seh_prolog=None, seh_epilog=None, setjmp_fn=None, longjmp_fn=None,
                 manual_functions=None):
        self.func_db={} if func_db is None else func_db
        self.label_db={} if label_db is None else label_db
        self.abi_db={} if abi_db is None else abi_db
        self.xbe_data=xbe_data
        self.manual_functions=set(manual_functions or ())
        self.imm_code_refs=set()
        for field in ('referenced_calls','unimplemented','_pop_cache','jump_table_targets','icall_site_targets'):
            setattr(self,field,{})
        self._fp_top=self.func_start=self.func_end=0
        self.needs_cf=self.publishes_ebp=False
        self.trace_exit_name=self.force_return_value=None
        detected=helpers.seh(self.func_db,xbe_data) if self.func_db and (seh_prolog is None or seh_epilog is None) else ((),())
        self.SEH_PROLOGS=_addresses(seh_prolog,detected[0])
        self.SEH_EPILOGS=_addresses(seh_epilog,detected[1])
        self.CXX_THROW=helpers.cxx_throw(self.func_db,xbe_data) if self.func_db else None
        self.SETJMP_FN=setjmp_fn
        self.LONGJMP_FN=longjmp_fn


# Compatibility spellings are explicit data, keeping a single implementation
# for direct consumers and for the old import path during staged migration.
_HANDLERS={
    moves: {'_lift_mov':'mov','_lift_movzx':'movzx','_lift_movsx':'movsx','_lift_lea':'lea','_lift_push':'push','_lift_pop':'pop'},
    integer: {'_lift_alu_binop':'alu_binop','_lift_inc_dec':'inc_dec','_lift_neg':'neg','_lift_not':'bit_not','_lift_sbb':'sbb','_lift_adc':'adc'},
    bits: {'_lift_double_shift':'double_shift','_lift_shld':'shld','_lift_shrd':'shrd','_lift_shift':'shift','_lift_sar':'sar','_lift_rotate_carry':'rotate_carry','_lift_rotate':'rotate'},
    bitops: {'_lift_bit_test':'bit_test','_lift_scan':'scan','_lift_swap':'swap'},
    product: {'_lift_imul':'imul','_lift_muldiv':'muldiv'},
    exchange: {'_lift_xchg':'exchange','_lift_exchange':'exchange'},
    compare: {'_snapshot_flags':'snapshot','_lift_cmp':'cmp','_lift_test':'test'},
    control: {'callee_pop':'callee_cleanup','_lift_call':'call','_lift_ret':'return_from','_forced_tail':'forced_tail','_is_external_target':'external','_read_jump_table':'read_table','_analyze_switch_table':'switch_targets','_lift_jmp':'jump','_lift_jcc':'branch','_lift_loop':'loop'},
    lifting: {'lift_instruction':'instruction','_result_snapshot':'result_snapshot','_unimplemented':'unsupported','_lift_setcc':'fallback_set','_lift_cmovcc':'fallback_move'},
    strings: {'_lift_rep_string':'emit','_lift_string_op':'emit'},
    sse: {'_lift_sse':'emit'},mmx: {'_lift_mmx':'emit'},x87: {'_lift_fpu':'emit'},
    names: {'_call_target_name':'call_target','_build_call_args':'call_arguments'},
}
for _module,_bindings in _HANDLERS.items():
    for _name,_handler in _bindings.items():setattr(Lifter,_name,getattr(_module,_handler))

# Operand helpers historically imported from the lifter remain identical to
# their project-owned implementations, including underscored public spellings.
for _name in ('_SYSTEM_REGS','_fmt_reg','_fmt_set_reg','_fmt_imm','_mem_accessor',
              '_smem_accessor','_fmt_fpu_operand','_fmt_mem','_fmt_mem_read',
              '_fmt_mem_write','_operand_width','_fmt_operand_read','_fmt_operand_write'):
    globals()[_name]=getattr(operands,_name)

__all__=[name for name in globals() if name in (
    'Lifter','Instruction','Operand','LOOP_JUMPS','FLAG_SETTERS','CF_TRACKED','BT_MODIFY',
    'ZF_FROM_DEST','COND_MAP','normalise_zero_test','lift_basic_block','try_match_cmp_jcc',
    'zf_expression','detect_seh_helpers','detect_cxx_throw','detect_setjmp_helpers')
    or (name.startswith('_') and not name.startswith('__') and name not in ('_module','_bindings','_name','_handler','_HANDLERS','_addresses'))]
