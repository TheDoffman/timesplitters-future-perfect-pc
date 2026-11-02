#include "recomp_types.h"
#include "guest_context_execution.h"
#include <stdlib.h>
#include <stdio.h>

/* Native jmp_buf has a required host alignment. This private host object is
 * never serialized; retain its compiler-inserted padding rather than packing it. */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4324)
#endif
typedef struct execution_root {
    struct execution_root *previous;
    tsfp_context_execution_ops ops;
    tsfp_x86_context_state pending;
    jmp_buf checkpoint;
    uint64_t low,high,mark;
    uintptr_t native_tag;
    uint32_t entry_sp,pc,stop;
    int pending_apply,failed;
} execution_root;
#ifdef _MSC_VER
#pragma warning(pop)
#endif
enum {ROOT_LIMIT=64};
static RECOMP_TLS execution_root *current_root;
static RECOMP_TLS unsigned root_depth;

unsigned tsfp_context_execution_depth(void) {return root_depth;}
static int stack_contains(const execution_root *r,uint32_t sp) {
    return r->low<=sp && (uint64_t)sp+4<=r->high;
}
static int valid_ops(const tsfp_context_execution_ops *ops) {
    return ops && ops->allow_pc && ops->allow_state && ops->apply &&
        ops->step && ops->same_stack && ((ops->mark==NULL)==(ops->rewind==NULL));
}
int tsfp_context_execute(const tsfp_context_execution_ops *ops,
    const tsfp_x86_context_state *initial,uint64_t low,uint64_t high,uint32_t stop) {
    execution_root *root;volatile unsigned char native_anchor=0;int result;
    if(!valid_ops(ops) || !initial || !initial->eip ||
       low>=high || high>UINT64_C(0x100000000) ||
       low>initial->esp || (uint64_t)initial->esp+4>high ||
       root_depth==ROOT_LIMIT || !ops->allow_pc(ops->opaque,initial->eip) ||
       !ops->allow_state(ops->opaque,initial))return TSFP_CONTEXT_EXECUTE_REJECTED;
    root=(execution_root *)calloc(1,sizeof *root);
    if(!root)return TSFP_CONTEXT_EXECUTE_REJECTED;
    root->previous=current_root;root->ops=*ops;root->pending=*initial;
    root->low=low;root->high=high;root->entry_sp=initial->esp;
    root->pc=initial->eip;root->stop=stop;root->pending_apply=1;
    root->native_tag=(uintptr_t)&native_anchor;
    current_root=root;++root_depth;
    for(;;) {
        if(setjmp(root->checkpoint)) {
            if(root->failed){result=TSFP_CONTEXT_EXECUTE_CLEANUP_FAILED;break;}
        }
        if(root->pending_apply) {
            root->ops.apply(root->ops.opaque,&root->pending);
            root->pending_apply=0;
        }
        if(!root->pc || !root->ops.allow_pc(root->ops.opaque,root->pc)){
            result=TSFP_CONTEXT_EXECUTE_BAD_PC;break;
        }
        root->mark=root->ops.mark?root->ops.mark(root->ops.opaque):0;
        {
            uint32_t next=0;
            int outcome=root->ops.step(root->ops.opaque,root->pc,&next);
            if(outcome!=TSFP_CONTEXT_STEP_YIELD && outcome!=TSFP_CONTEXT_STEP_RETURN){
                result=TSFP_CONTEXT_EXECUTE_STEP_FAILED;break;
            }
            if(outcome==TSFP_CONTEXT_STEP_RETURN && next==root->stop){
                result=TSFP_CONTEXT_EXECUTE_COMPLETE;break;
            }
            root->pc=next;
        }
    }
    current_root=root->previous;--root_depth;free(root);return result;
}
int tsfp_context_transfer(const tsfp_x86_context_state *state) {
    execution_root *target,*discard;tsfp_x86_context_state saved;int same,clean;
    if(!state || !current_root)return 0;
    saved=*state;
    if(!saved.eip)return 0;
    target=current_root;
    while(target) {
        if(stack_contains(target,saved.esp) &&
           (!target->previous || saved.esp<=target->entry_sp))break;
        target=target->previous;
    }
    if(!target || !target->ops.allow_pc(target->ops.opaque,saved.eip) ||
       !target->ops.allow_state(target->ops.opaque,&saved))return 0;
    same=target->ops.same_stack(target->ops.opaque,target->native_tag);
    if(!same && !target->ops.foreign_jump)return 0;
    /* From here no abandoned native caller may be resumed. */
    clean=1;
    /* Roots can own different scope ledgers. Rewind every abandoned root and
     * the destination, deepest first, retaining failure across best-effort
     * cleanup of the remaining ledgers. */
    for(execution_root *cursor=current_root; ; cursor=cursor->previous) {
        if(cursor->ops.rewind &&
           !cursor->ops.rewind(cursor->ops.opaque,cursor->mark))clean=0;
        if(cursor==target)break;
    }
    target->pending=saved;target->pending_apply=clean;target->failed=!clean;
    target->pc=saved.eip;
    discard=current_root;
    while(discard!=target) {
        execution_root *previous=discard->previous;
        --root_depth;free(discard);discard=previous;
    }
    current_root=target;
    if(same)longjmp(target->checkpoint,1);
    target->ops.foreign_jump(target->ops.opaque,&target->checkpoint,1,target->native_tag);
    fputs("[tsfp-context] foreign-stack adapter returned without transferring control\n",stderr);
    fflush(stderr);abort();
}
