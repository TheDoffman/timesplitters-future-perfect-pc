/* Windows affinity adapter. Standard SDK records are walked by their Size,
 * bounded by the actual returned buffer, never by hand-maintained offsets.
 * Group-zero-only matches the existing process-mask setting's scope. */
typedef BOOL (WINAPI *execution_cpu_query)(PSYSTEM_CPU_SET_INFORMATION,ULONG,PULONG,HANDLE,ULONG);
static DWORD_PTR execution_cpu_configure(void) {
    DWORD_PTR allowed=0,system=0;tsfp_cpu_choice choice={0,0};
    const char *request=getenv("RECOMP_GUEST_ONE_CPU");
    execution_cpu_query query;ULONG capacity=0,received;unsigned char *data;
    uint64_t mask;size_t offset;
    if(!request || !*request || !GetProcessAffinityMask(GetCurrentProcess(),&allowed,&system))return 0;
    query=(execution_cpu_query)(void *)GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"GetSystemCpuSetInformation");
    if(query) {
        query(NULL,0,&capacity,GetCurrentProcess(),0);
        /* The OS query may race a topology change. A failed/oversized retry uses
         * the permitted-mask fallback; it never reads an unfilled allocation. */
        if(capacity && capacity<=16u*1024u*1024u && (data=malloc(capacity))!=NULL) {
            received=capacity;
            if(query((PSYSTEM_CPU_SET_INFORMATION)data,capacity,&received,GetCurrentProcess(),0) && received<=capacity) {
                offset=0;
                while(received-offset>=sizeof(DWORD)*2) {
                    SYSTEM_CPU_SET_INFORMATION item;DWORD size;
                    memcpy(&size,data+offset,sizeof size);
                    if(size<sizeof(DWORD)*2 || size>received-offset)break;
                    if(size>=sizeof item) {
                        memcpy(&item,data+offset,sizeof item);
                        if(item.Type==CpuSetInformation)tsfp_cpu_consider(&choice,(uint64_t)allowed,
                            item.CpuSet.Group,item.CpuSet.LogicalProcessorIndex,item.CpuSet.EfficiencyClass);
                    }
                    offset+=size;
                }
            }
            free(data);
        }
    }
    mask=tsfp_cpu_request(request,(uint64_t)allowed,choice.mask);
    if(mask)fprintf(stderr,"[THREAD] guest threads on one host core (mask 0x%llX, RECOMP_GUEST_ONE_CPU)\n",
        (unsigned long long)mask);
    return (DWORD_PTR)mask;
}
