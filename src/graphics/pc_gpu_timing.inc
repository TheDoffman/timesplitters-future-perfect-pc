/* Opt-in D3D11 presentation investigation. GPU timestamps bracket only an
 * interpolated redraw + resolve, not the 60 Hz simulation or idle wait.
 * Elapsed GPU span may include GPU idle bubbles; it is not GPU busy time.
 * Pending queries are polled without flushing/waiting. No query/file is
 * allocated unless TSFP_GPU_TRACE is explicitly set. */
#define PC_GPU_SLOTS 16
#define PC_GPU_LIMIT 20000
typedef struct PcGpuSample {
    ID3D11Query *disjoint,*start,*end;
    int pending,stats_valid;
    unsigned id;
    LONGLONG begin,enter,leave;
    HRESULT present_result,stats_result;
    DXGI_FRAME_STATISTICS stats;
} PcGpuSample;
static PcGpuSample pc_gpu_samples[PC_GPU_SLOTS];
static PcGpuSample *pc_gpu_active;
static FILE *pc_gpu_file;
static int pc_gpu_initialized,pc_gpu_enabled;
static unsigned pc_gpu_next,pc_gpu_issued,pc_gpu_rows,pc_gpu_skipped;
static LARGE_INTEGER pc_gpu_frequency;
static void pc_gpu_close(void)
{
    for(unsigned i=0;i<PC_GPU_SLOTS;i++) {
        PcGpuSample *s=&pc_gpu_samples[i];
        if(s->start)ID3D11Query_Release(s->start);
        if(s->end)ID3D11Query_Release(s->end);
        if(s->disjoint)ID3D11Query_Release(s->disjoint);
        memset(s,0,sizeof *s);
    }
    if(pc_gpu_file)fclose(pc_gpu_file);
    pc_gpu_file=NULL;pc_gpu_active=NULL;pc_gpu_enabled=0;
}
static void pc_gpu_poll(void)
{
    for(unsigned i=0;i<PC_GPU_SLOTS;i++) {
        PcGpuSample *s=&pc_gpu_samples[i];D3D11_QUERY_DATA_TIMESTAMP_DISJOINT d;
        UINT64 begin,end;
        if(!s->pending)continue;
        if(ID3D11DeviceContext_GetData(g_ctx,(ID3D11Asynchronous *)s->disjoint,&d,sizeof d,D3D11_ASYNC_GETDATA_DONOTFLUSH)!=S_OK ||
           ID3D11DeviceContext_GetData(g_ctx,(ID3D11Asynchronous *)s->start,&begin,sizeof begin,D3D11_ASYNC_GETDATA_DONOTFLUSH)!=S_OK ||
           ID3D11DeviceContext_GetData(g_ctx,(ID3D11Asynchronous *)s->end,&end,sizeof end,D3D11_ASYNC_GETDATA_DONOTFLUSH)!=S_OK)continue;
        int valid=!d.Disjoint && d.Frequency && end>=begin;
        fprintf(pc_gpu_file,"%u,%.9f,%.6f,%.6f,%d,%.6f,0x%08lX,0x%08lX,%u,%u,%.9f,%u\n",s->id,
            (double)s->enter/pc_gpu_frequency.QuadPart,
            (s->enter-s->begin)*1000.0/pc_gpu_frequency.QuadPart,
            (s->leave-s->enter)*1000.0/pc_gpu_frequency.QuadPart,valid,
            valid?(end-begin)*1000.0/d.Frequency:0.0,(unsigned long)s->present_result,
            (unsigned long)s->stats_result,s->stats_valid?s->stats.PresentCount:0,
            s->stats_valid?s->stats.SyncRefreshCount:0,
            s->stats_valid?(double)s->stats.SyncQPCTime.QuadPart/pc_gpu_frequency.QuadPart:0.0,pc_gpu_skipped);
        s->pending=0;
        if(++pc_gpu_rows%120==0)fflush(pc_gpu_file);
    }
}
void tsfp_d3d11_profile_begin(void)
{
    LARGE_INTEGER now;
    if(!pc_gpu_initialized) {
        const char *path=getenv("TSFP_GPU_TRACE");pc_gpu_initialized=1;
        if(!path || !*path || !g_dev || !g_ctx)return;
        pc_gpu_file=fopen(path,"wb");if(!pc_gpu_file)return;
        QueryPerformanceFrequency(&pc_gpu_frequency);
        for(unsigned i=0;i<PC_GPU_SLOTS;i++) {
            D3D11_QUERY_DESC q={D3D11_QUERY_TIMESTAMP_DISJOINT,0};
            PcGpuSample *s=&pc_gpu_samples[i];
            if(FAILED(ID3D11Device_CreateQuery(g_dev,&q,&s->disjoint))){pc_gpu_close();return;}
            q.Query=D3D11_QUERY_TIMESTAMP;
            if(FAILED(ID3D11Device_CreateQuery(g_dev,&q,&s->start)) ||
               FAILED(ID3D11Device_CreateQuery(g_dev,&q,&s->end))){pc_gpu_close();return;}
        }
        fprintf(pc_gpu_file,"id,present_qpc_seconds,cpu_redraw_ms,present_call_ms,gpu_valid,gpu_span_ms,present_result,stats_result,stats_present_count,stats_sync_refresh,stats_sync_qpc_seconds,skipped_samples\n");
        pc_gpu_enabled=1;
    }
    if(!pc_gpu_enabled)return;
    pc_gpu_poll();
    if(pc_gpu_active || pc_gpu_issued>=PC_GPU_LIMIT)return;
    PcGpuSample *s=&pc_gpu_samples[pc_gpu_next];
    if(s->pending){pc_gpu_skipped++;return;}
    QueryPerformanceCounter(&now);s->begin=now.QuadPart;s->id=++pc_gpu_issued;
    ID3D11DeviceContext_Begin(g_ctx,(ID3D11Asynchronous *)s->disjoint);
    ID3D11DeviceContext_End(g_ctx,(ID3D11Asynchronous *)s->start);
    pc_gpu_active=s;
}
static void pc_gpu_before_present(void)
{
    LARGE_INTEGER now;if(!pc_gpu_active)return;
    ID3D11DeviceContext_End(g_ctx,(ID3D11Asynchronous *)pc_gpu_active->end);
    ID3D11DeviceContext_End(g_ctx,(ID3D11Asynchronous *)pc_gpu_active->disjoint);
    QueryPerformanceCounter(&now);pc_gpu_active->enter=now.QuadPart;
}
static void pc_gpu_after_present(HRESULT result)
{
    LARGE_INTEGER now;PcGpuSample *s=pc_gpu_active;if(!s)return;
    QueryPerformanceCounter(&now);s->leave=now.QuadPart;s->present_result=result;
    memset(&s->stats,0,sizeof s->stats);
    s->stats_result=IDXGISwapChain_GetFrameStatistics(g_swap,&s->stats);
    s->stats_valid=SUCCEEDED(s->stats_result);s->pending=1;
    pc_gpu_active=NULL;pc_gpu_next=(pc_gpu_next+1)%PC_GPU_SLOTS;
}
