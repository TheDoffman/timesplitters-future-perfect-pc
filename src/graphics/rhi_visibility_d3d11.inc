/* Precise depth-tested samples from the host context; never fabricate visibility. */
struct RhiOcclusion { ID3D11Query *query; int active,ended; };
static RhiOcclusion *d_occlusion_create(void) {
    D3D11_QUERY_DESC desc={D3D11_QUERY_OCCLUSION,0};
    RhiOcclusion *q=calloc(1,sizeof(*q));
    if(!q)return NULL;
    if(FAILED(ID3D11Device_CreateQuery(g_dev,&desc,&q->query))) {free(q);return NULL;}
    return q;
}
static void d_occlusion_destroy(RhiOcclusion *q) {
    if(!q)return;
    if(q->active)ID3D11DeviceContext_End(g_ctx,(ID3D11Asynchronous *)q->query);
    ID3D11Query_Release(q->query);free(q);
}
static int d_occlusion_begin(RhiOcclusion *q) {
    if(!q || q->active)return 0;
    q->ended=0;q->active=1;
    ID3D11DeviceContext_Begin(g_ctx,(ID3D11Asynchronous *)q->query);return 1;
}
static int d_occlusion_end(RhiOcclusion *q) {
    if(!q || !q->active)return 0;
    ID3D11DeviceContext_End(g_ctx,(ID3D11Asynchronous *)q->query);
    q->active=0;q->ended=1;return 1;
}
static int d_occlusion_result(RhiOcclusion *q,uint64_t *samples) {
    if(!q || !samples || !q->ended)return -1;
    uint64_t value=0;
    HRESULT result=ID3D11DeviceContext_GetData(g_ctx,(ID3D11Asynchronous *)q->query,
        &value,sizeof(value),D3D11_ASYNC_GETDATA_DONOTFLUSH);
    if(result==S_OK){*samples=value;return 1;}
    return result==S_FALSE?0:-1;
}
