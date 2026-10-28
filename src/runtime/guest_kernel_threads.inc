/* Original thread publication/lifetime adapter. The TIB and parent handle
 * identify the same object before the worker can execute any guest code. */
typedef struct bridge_thread_launch {
    recomp_func_t entry;
    uint32_t first,second,stack,tib;
    uint64_t object;
    volatile LONG published;
} bridge_thread_launch;

static void bridge_run_thread_inline(recomp_func_t fn,uint32_t first,uint32_t second) {
    g_esp-=4;BRIDGE_MEM32(g_esp)=second;
    g_esp-=4;BRIDGE_MEM32(g_esp)=first;
    g_esp-=4;BRIDGE_MEM32(g_esp)=0;
    g_seh_ebp=g_esp;fn();g_esp+=12;
}
static DWORD WINAPI bridge_thread_main(LPVOID parameter) {
    bridge_thread_launch *launch=(bridge_thread_launch *)parameter;
    recomp_func_t entry;uint32_t first,second;
    while(!InterlockedCompareExchange(&launch->published,0,0)) Sleep(1);
    entry=launch->entry;first=launch->first;second=launch->second;
    bridge_thread_key=launch->object;g_fs_base=launch->tib;
    g_thread_stack_top=launch->stack;g_esp=launch->stack;g_is_spawned_thread=1;
    free(launch);
    xbox_GuestLockEnter();xbox_GuestLiftedEnter();
    xbox_NameCurrentThread(L"guest worker");
    bridge_run_thread_inline(entry,first,second);
    xbox_GuestLiftedExit();
    bridge_object_thread_exit();
    xbox_FreeThreadStack(g_thread_stack_top);g_thread_stack_top=0;
    return 0;
}
static uint32_t bridge_spawn_thread(recomp_func_t entry,uint32_t first,uint32_t second,
    uint32_t output_va,uint32_t thread_id_va,int suspended) {
    bridge_object_creation creation;bridge_thread_launch *launch;
    HANDLE host;uint32_t status;void *id_output=NULL;
    if(thread_id_va && !(id_output=bridge_object_output(NULL,thread_id_va,4))) return TSFP_OBJECT_BAD_ADDRESS;
    status=bridge_object_prepare(&creation,output_va,TSFP_KIND_THREAD,0);
    if(status) return status;
    launch=(bridge_thread_launch *)calloc(1,sizeof(*launch));
    if(!launch) { tsfp_guest_objects_cancel(&bridge_objects,creation.ticket);return TSFP_OBJECT_NO_MEMORY; }
    launch->entry=entry;launch->first=first;launch->second=second;
    launch->stack=xbox_AllocThreadStack();
    launch->tib=launch->stack?xbox_AllocThreadTib():0;
    if(!launch->tib) {
        if(launch->stack) xbox_FreeThreadStack(launch->stack);
        free(launch);tsfp_guest_objects_cancel(&bridge_objects,creation.ticket);return TSFP_OBJECT_NO_MEMORY;
    }
    host=CreateThread(NULL,0,bridge_thread_main,launch,suspended?CREATE_SUSPENDED:0,NULL);
    if(!host) {
        xbox_HeapFree(launch->tib);xbox_FreeThreadStack(launch->stack);free(launch);
        tsfp_guest_objects_cancel(&bridge_objects,creation.ticket);return TSFP_OBJECT_NO_MEMORY;
    }
    /* Capacity and storage were reserved before CreateThread. No allocation
     * remains on this publication path; a failure here means corrupt metadata. */
    if(!tsfp_guest_objects_commit(&bridge_objects,creation.ticket,(uintptr_t)host) ||
       !tsfp_guest_objects_reference(&bridge_objects,creation.ticket,&launch->object) ||
       !bridge_object_publish_thread(launch->object,launch->tib)) abort();
    bridge_object_word(creation.output,creation.ticket);
    if(id_output) bridge_object_word(id_output,BRIDGE_MEM32(BRIDGE_MEM32(launch->tib+0x28)+0x12c));
    InterlockedExchange(&launch->published,1);
    return TSFP_OBJECT_SUCCESS;
}
static uint32_t bridge_create_thread(uint32_t output,uint32_t id_output,uint32_t routine,
    uint32_t first,uint32_t second,int suspended) {
    recomp_func_t entry;uint32_t status;int first_inline;const char *inline_workers;
    if(!routine) return TSFP_OBJECT_BAD_PARAMETER;
    entry=recomp_lookup(routine);if(!entry) entry=recomp_lookup_manual(routine);
    if(!entry) return TSFP_OBJECT_BAD_PARAMETER;
    if(!bridge_object_output(NULL,output,4) || (id_output && !bridge_object_output(NULL,id_output,4)))
        return TSFP_OBJECT_BAD_ADDRESS;
    first_inline=g_thread_mode==XBOX_THREAD_MODE_INLINE &&
        InterlockedIncrement((volatile LONG *)&g_thread_call_count)==1;
    inline_workers=getenv("RECOMP_WORKERS");
    if(!suspended && (first_inline || (inline_workers && !strcmp(inline_workers,"inline")))) {
        status=tsfp_guest_object_duplicate(&bridge_object_services,UINT32_MAX-1,2,output);
        if(status) return status;
        if(id_output) bridge_object_word(bridge_object_output(NULL,id_output,4),BRIDGE_MEM32(xbox_CurrentThreadObject()+0x12c));
        bridge_run_thread_inline(entry,first,second);return TSFP_OBJECT_SUCCESS;
    }
    return bridge_spawn_thread(entry,first,second,output,id_output,suspended);
}
