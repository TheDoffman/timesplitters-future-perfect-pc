/* Exercise the actual Vulkan visibility adapter with explicit API outcomes.
 * This is a deterministic host-contract fixture, not GPU/driver acceptance. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef uint64_t VkQueryPool;
typedef int VkResult;
typedef struct RhiOcclusion RhiOcclusion;
typedef struct {int sType;unsigned queryType,queryCount;} VkQueryPoolCreateInfo;
enum {VK_SUCCESS=0,VK_NOT_READY=1,VK_ERROR_DEVICE_LOST=-4,
      VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO=5,VK_QUERY_TYPE_OCCLUSION=7,
      VK_QUERY_RESULT_64_BIT=1,VK_QUERY_CONTROL_PRECISE_BIT=2,T_QUERY=3};
static struct {unsigned dev;int feat_occlusion_precise;uint64_t submitted,completed,timeline;} V;
static unsigned created,destroyed,retired,resets,draws,active,get_results;
static VkResult create_code=VK_SUCCESS,counter_code=VK_SUCCESS,result_code=VK_SUCCESS;
static uint64_t counter,fake_counts[64];
static unsigned cmd(void){return 9;}
static void end_rendering(void){}
static void trash(int kind,uint64_t pool,void *allocation){assert(kind==T_QUERY && pool && !allocation);retired++;}
static VkResult vkCreateQueryPool(unsigned dev,const VkQueryPoolCreateInfo *info,void *allocation,VkQueryPool *pool) {
    assert(dev && !allocation && info->sType==VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO &&
           info->queryType==VK_QUERY_TYPE_OCCLUSION && info->queryCount==64);
    if(create_code==VK_SUCCESS)*pool=++created;
    return create_code;
}
static void vkDestroyQueryPool(unsigned dev,VkQueryPool pool,void *allocation){assert(dev && pool && !allocation);destroyed++;}
static VkResult vkGetSemaphoreCounterValue(unsigned dev,uint64_t semaphore,uint64_t *value) {
    assert(dev && semaphore && value);*value=counter;return counter_code;
}
static void vkCmdResetQueryPool(unsigned command,VkQueryPool pool,unsigned first,unsigned count) {
    assert(command==9 && pool && first==0 && count==64);resets++;
}
static VkResult vkGetQueryPoolResults(unsigned dev,VkQueryPool pool,unsigned first,unsigned count,
                                     size_t size,void *data,size_t stride,unsigned flags) {
    assert(dev && pool && first==0 && count>0 && count<=64 && size==count*sizeof(uint64_t) &&
           stride==sizeof(uint64_t) && flags==VK_QUERY_RESULT_64_BIT);
    get_results++;
    memcpy(data,fake_counts,size); /* Partial writes must never publish an answer. */
    return result_code;
}
static void vkCmdBeginQuery(unsigned command,VkQueryPool pool,unsigned index,unsigned flags) {
    assert(command==9 && pool && index<64 && flags==VK_QUERY_CONTROL_PRECISE_BIT && !active);active=1;draws++;
}
static void vkCmdEndQuery(unsigned command,VkQueryPool pool,unsigned index){assert(command==9 && pool && index<64 && active);active=0;}
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#include "graphics/rhi_visibility_vulkan.inc"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

int main(void) {
    uint64_t answer=777;
    assert(!v_occlusion_create());V.dev=1;assert(!v_occlusion_create());
    V.feat_occlusion_precise=1;V.timeline=4;V.submitted=15;
    create_code=VK_ERROR_DEVICE_LOST;assert(!v_occlusion_create());create_code=VK_SUCCESS;
    RhiOcclusion *q=v_occlusion_create();assert(q && created==1);
    assert(v_occlusion_result(q,&answer)==-1 && answer==777);
    assert(v_occlusion_begin(q));assert(!v_occlusion_begin(q));
    assert(v_visibility_draw_begin());v_visibility_draw_end(1);
    assert(v_visibility_draw_begin());v_visibility_draw_end(1);
    assert(v_occlusion_end(q) && !v_visibility_active && draws==2);
    V.submitted=16; /* The recorded scope has now been submitted. */
    counter=15;assert(v_occlusion_result(q,&answer)==0 && answer==777 && !get_results);
    counter=16;fake_counts[0]=100;fake_counts[1]=23;result_code=VK_NOT_READY;
    /* Timeline completion is not permission to reinterpret a pending API result
     * as a failed visibility scope, discard it or publish partially written data. */
    assert(v_occlusion_result(q,&answer)==0 && answer==777 && get_results==1);
    result_code=VK_SUCCESS;assert(v_occlusion_result(q,&answer)==1 && answer==123);
    answer=777;result_code=VK_ERROR_DEVICE_LOST;
    assert(v_occlusion_result(q,&answer)==-1 && answer==777);
    result_code=VK_SUCCESS;fake_counts[0]=UINT64_MAX;fake_counts[1]=1;
    assert(v_occlusion_result(q,&answer)==-1 && answer==777);
    fake_counts[0]=fake_counts[1]=0;assert(v_occlusion_result(q,&answer)==1 && answer==0);
    counter_code=VK_ERROR_DEVICE_LOST;assert(v_occlusion_result(q,&answer)==-1);
    counter_code=VK_SUCCESS;v_occlusion_destroy(q);assert(destroyed==1 && !retired);
    q=v_occlusion_create();assert(q);V.submitted=17;q->serial=17;counter=16;
    assert(!v_occlusion_begin(q));counter=17;assert(v_occlusion_begin(q));
    for(unsigned i=0;i<64;i++){assert(v_visibility_draw_begin());v_visibility_draw_end(1);}
    assert(!v_visibility_draw_begin() && !v_occlusion_end(q));
    V.submitted=18;
    answer=777;assert(v_occlusion_result(q,&answer)==-1 && answer==777);
    v_occlusion_destroy(q);assert(retired==1 && destroyed==1);
    q=v_occlusion_create();assert(v_occlusion_begin(q));
    assert(v_occlusion_end(q));unsigned calls=get_results;
    V.submitted=19;counter=19;
    assert(v_occlusion_result(q,&answer)==1 && !answer && get_results==calls);
    v_occlusion_destroy(q);v_occlusion_destroy(NULL);v_visibility_draw_end(0);
    assert(resets==3 && destroyed==2);
    puts("Vulkan visibility pending/retry, partial output, failure, zero, overflow and retirement passed");
    return 0;
}
