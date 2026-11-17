/* Each logical query collects exact samples from its draws. Bracket each draw
 * inside its rendering scope so uploads/target changes cannot split a query
 * across Vulkan rendering instances. Results are asynchronous and fence-owned. */
#define VISIBILITY_SEGMENTS 64u
struct RhiOcclusion {
    VkQueryPool pool;
    uint64_t serial;
    uint32_t count;
    int active,ended,failed;
};
static RhiOcclusion *v_visibility_active;
static RhiOcclusion *v_occlusion_create(void) {
    if(!V.dev || !V.feat_occlusion_precise)return NULL;
    RhiOcclusion *q=calloc(1,sizeof(*q));if(!q)return NULL;
    VkQueryPoolCreateInfo info={VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    info.queryType=VK_QUERY_TYPE_OCCLUSION;info.queryCount=VISIBILITY_SEGMENTS;
    if(vkCreateQueryPool(V.dev,&info,NULL,&q->pool)!=VK_SUCCESS){free(q);return NULL;}
    return q;
}
static void v_occlusion_destroy(RhiOcclusion *q) {
    if(!q)return;
    if(v_visibility_active==q)v_visibility_active=NULL;
    if(q->serial>V.completed)trash(T_QUERY,(uint64_t)q->pool,NULL);
    else vkDestroyQueryPool(V.dev,q->pool,NULL);
    free(q);
}
static int v_occlusion_begin(RhiOcclusion *q) {
    if(!q || v_visibility_active)return 0;
    /* A prior use must be complete before host reads/reuses these slots. */
    if(q->serial>V.completed) {
        uint64_t completed=0;
        if(vkGetSemaphoreCounterValue(V.dev,V.timeline,&completed)!=VK_SUCCESS || completed<q->serial)return 0;
        V.completed=completed;
    }
    end_rendering();
    vkCmdResetQueryPool(cmd(),q->pool,0,VISIBILITY_SEGMENTS);
    q->count=0;q->failed=0;q->ended=0;q->active=1;q->serial=V.submitted+1;
    v_visibility_active=q;return 1;
}
static int v_occlusion_end(RhiOcclusion *q) {
    if(!q || v_visibility_active!=q || !q->active)return 0;
    v_visibility_active=NULL;q->active=0;q->ended=1;q->serial=V.submitted+1;
    return !q->failed;
}
static int v_occlusion_result(RhiOcclusion *q,uint64_t *samples) {
    uint64_t completed=0,counts[VISIBILITY_SEGMENTS],total=0;
    if(!q || !samples || !q->ended || q->failed)return -1;
    if(vkGetSemaphoreCounterValue(V.dev,V.timeline,&completed)!=VK_SUCCESS)return -1;
    if(completed>V.completed)V.completed=completed;
    if(completed<q->serial)return 0;
    if(q->count && vkGetQueryPoolResults(V.dev,q->pool,0,q->count,
        q->count*sizeof(uint64_t),counts,sizeof(uint64_t),VK_QUERY_RESULT_64_BIT)!=VK_SUCCESS)return -1;
    for(uint32_t i=0;i<q->count;i++) {
        if(UINT64_MAX-total<counts[i])return -1;
        total+=counts[i];
    }
    *samples=total;return 1;
}
static int v_visibility_draw_begin(void) {
    RhiOcclusion *q=v_visibility_active;
    if(!q)return 0;
    if(q->count==VISIBILITY_SEGMENTS){q->failed=1;return 0;}
    vkCmdBeginQuery(cmd(),q->pool,q->count++,VK_QUERY_CONTROL_PRECISE_BIT);return 1;
}
static void v_visibility_draw_end(int begun) {
    if(begun)vkCmdEndQuery(cmd(),v_visibility_active->pool,v_visibility_active->count-1);
}
