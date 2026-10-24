/* Optional D3D11 hardware diagnostic. No pixel readback or query polling inside
 * the measured sequence. Timestamp spans can include GPU submission bubbles;
 * they are neither whole-game frame times nor monitor presentation intervals. */
#ifndef SURFACE_BENCHMARK
#define SURFACE_BENCHMARK 0
#endif
static ID3D11Device *surface_timing_device;
static ID3D11DeviceContext *surface_timing_context;
typedef struct SurfaceTiming {
    ID3D11Query *disjoint,*start[144],*end[144];
    unsigned count;
} SurfaceTiming;
static void surface_timing_begin(SurfaceTiming *t,unsigned count) {
    memset(t,0,sizeof *t);if(!SURFACE_BENCHMARK)return;
    CHECK(count && count<=144);t->count=count;
    D3D11_QUERY_DESC desc={D3D11_QUERY_TIMESTAMP_DISJOINT,0};
    CHECK(SUCCEEDED(ID3D11Device_CreateQuery(surface_timing_device,&desc,&t->disjoint)));
    desc.Query=D3D11_QUERY_TIMESTAMP;
    for(unsigned i=0;i<count;i++) {
        CHECK(SUCCEEDED(ID3D11Device_CreateQuery(surface_timing_device,&desc,&t->start[i])));
        CHECK(SUCCEEDED(ID3D11Device_CreateQuery(surface_timing_device,&desc,&t->end[i])));
    }
    ID3D11DeviceContext_Begin(surface_timing_context,(ID3D11Asynchronous*)t->disjoint);
}
static void surface_timing_mark(SurfaceTiming *t,unsigned frame,int end) {
    if(!SURFACE_BENCHMARK)return;
    CHECK(frame<t->count);
    ID3D11DeviceContext_End(surface_timing_context,(ID3D11Asynchronous*)(end?t->end[frame]:t->start[frame]));
}
static void surface_timing_get(ID3D11Query *q,void *data,UINT size) {
    ULONGLONG deadline=GetTickCount64()+10000;HRESULT result;
    while((result=ID3D11DeviceContext_GetData(surface_timing_context,
        (ID3D11Asynchronous*)q,data,size,D3D11_ASYNC_GETDATA_DONOTFLUSH))==S_FALSE) {
        CHECK(GetTickCount64()<deadline);Sleep(1);
    }
    CHECK(result==S_OK);
}
static int surface_timing_compare(const void *a,const void *b) {
    double x=*(const double*)a,y=*(const double*)b;return (x>y)-(x<y);
}
static void surface_timing_finish(SurfaceTiming *t,const char *path,unsigned smoothing,unsigned fps,unsigned jitter) {
    if(!SURFACE_BENCHMARK)return;
    ID3D11DeviceContext_End(surface_timing_context,(ID3D11Asynchronous*)t->disjoint);
    ID3D11DeviceContext_Flush(surface_timing_context);
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT clock;
    surface_timing_get(t->disjoint,&clock,sizeof clock);
    CHECK(!clock.Disjoint && clock.Frequency);
    double spans[144],total=0;
    for(unsigned i=0;i<t->count;i++) {
        UINT64 a,b;surface_timing_get(t->start[i],&a,sizeof a);surface_timing_get(t->end[i],&b,sizeof b);
        CHECK(b>=a);spans[i]=(b-a)*1000.0/clock.Frequency;total+=spans[i];
        ID3D11Query_Release(t->start[i]);ID3D11Query_Release(t->end[i]);
    }
    qsort(spans,t->count,sizeof spans[0],surface_timing_compare);
    printf("surface-gpu-span path=%s smoothing=%u fps=%u jitter=%u frames=%u mean_ms=%.6f p50_ms=%.6f p95_ms=%.6f max_ms=%.6f total_ms=%.6f\n",
        path,smoothing,fps,jitter,t->count,total/t->count,spans[t->count/2],spans[(t->count*95-1)/100],spans[t->count-1],total);
    ID3D11Query_Release(t->disjoint);
}
