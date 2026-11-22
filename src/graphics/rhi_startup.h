#ifndef TSFP_RHI_STARTUP_H
#define TSFP_RHI_STARTUP_H
#include "rhi_backend.h"
#include <stdio.h>
#include <string.h>

/* One startup transaction, shared by all hosts. Eligibility comes from the
 * build's platform acceptance gate, never a saved preference or environment
 * variable. Presentation failures after a successful startup do not enter here.
 * Backend destruction must accept incomplete creation and repeated cleanup. */
static int tsfp_rhi_startup(const RhiBackend **active, const RhiDeviceDesc *desc,
                           const RhiBackend *opengl, int accepted)
{
    const RhiBackend *primary;
    if (!active || !(primary=*active) || !desc) return -1;
    if (primary->device_create(desc)==0) return 0;
    primary->device_destroy();
    if (accepted && opengl && primary!=opengl && !strcmp(primary->name,"vulkan") &&
        !strcmp(opengl->name,"opengl")) {
        fprintf(stderr,"[RHI] Vulkan initialization failed; attempting OpenGL startup recovery\n");
        fflush(stderr);
        if (opengl->device_create(desc)==0) {
            *active=opengl;
            fprintf(stderr,"[RHI] startup recovery selected opengl\n");
            fflush(stderr);
            return 0;
        }
        opengl->device_destroy();
        fprintf(stderr,"[RHI] ERROR: Vulkan and OpenGL initialization failed; no renderer is ready\n");
    } else {
        fprintf(stderr,"[RHI] ERROR: %s renderer startup failed; automatic renderer fallback is disabled\n",
                primary->name);
    }
    fflush(stderr);
    return -1;
}
#endif
