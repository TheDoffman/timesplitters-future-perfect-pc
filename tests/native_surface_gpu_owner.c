/* Asset-free construction failure injection; actual shader pixels/state are
 * covered separately through real RHI adapters. Every RHI allocation stage
 * must unwind all prior owned handles, without modifying renderer bindings. */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "rhi.h"
#include "surface_gpu_fake_rhi.h"
#include "graphics/surface_gpu.h"
int main(void) {
    PcSurfaceGpuConfig config={128,26./255,165./255,249./255,43./255,128./255};
    char errors[256];PcSurfaceGpu *owner=pc_surface_gpu_create(&config,errors,sizeof errors);
    assert(owner && !owner->ready);unsigned stages=attempt;assert(stages>20);
    pc_surface_gpu_destroy(owner);assert(!live);pc_surface_gpu_destroy(NULL);
    for(unsigned stage=1;stage<=stages;stage++) {
        attempt=0;fail_at=stage;owner=pc_surface_gpu_create(&config,errors,sizeof errors);
        assert(!owner && !live);
    }
    attempt=fail_at=0;owner=pc_surface_gpu_create(&config,errors,sizeof errors);
    assert(owner);pc_surface_gpu_destroy(owner);assert(!live);
    printf("PASS: surface GPU owner cleans all %u allocation-failure stages and recreates\n",stages);
    return 0;
}
