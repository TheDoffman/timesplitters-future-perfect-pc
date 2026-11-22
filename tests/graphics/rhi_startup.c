#include "graphics/rhi_startup.h"
#include <assert.h>
#include <string.h>
static int primary_result,gl_result,primary_live,gl_live;
static char events[32];static unsigned count;
static int vk_create(const RhiDeviceDesc *d){assert(d->width==64);events[count++]='V';primary_live=1;return primary_result;}
static void vk_destroy(void){events[count++]='v';primary_live=0;}
static int gl_create(const RhiDeviceDesc *d){assert(d->width==64);assert(!primary_live);events[count++]='G';gl_live=1;return gl_result;}
static void gl_destroy(void){events[count++]='g';gl_live=0;}
static const RhiBackend vk={.name="vulkan",.device_create=vk_create,.device_destroy=vk_destroy};
static const RhiBackend gl={.name="opengl",.device_create=gl_create,.device_destroy=gl_destroy};
static const RhiBackend dd={.name="d3d11",.device_create=vk_create,.device_destroy=vk_destroy};
static void reset(int v,int g){primary_result=v;gl_result=g;primary_live=gl_live=0;count=0;memset(events,0,sizeof events);}
int main(void){
    RhiDeviceDesc desc={.width=64,.height=64};const RhiBackend *active=&vk;
    unsigned char before[sizeof desc];memcpy(before,&desc,sizeof desc);
    reset(0,0);assert(!tsfp_rhi_startup(&active,&desc,&gl,1));
    assert(active==&vk && primary_live && !gl_live && !strcmp(events,"V"));
    reset(-1,0);active=&vk;assert(!tsfp_rhi_startup(&active,&desc,&gl,1));
    assert(active==&gl && !primary_live && gl_live && !strcmp(events,"VvG"));
    reset(-1,-1);active=&vk;assert(tsfp_rhi_startup(&active,&desc,&gl,1)<0);
    assert(active==&vk && !primary_live && !gl_live && !strcmp(events,"VvGg"));
    active->device_destroy();assert(!primary_live && !gl_live);
    reset(-1,0);active=&vk;assert(tsfp_rhi_startup(&active,&desc,&gl,0)<0);
    assert(active==&vk && !strcmp(events,"Vv")); /* Unverified host stays disabled. */
    reset(-1,0);active=&vk;assert(tsfp_rhi_startup(&active,&desc,NULL,1)<0);
    assert(!strcmp(events,"Vv")); /* Missing backend cannot be 'accepted'. */
    reset(-1,0);active=&dd;assert(tsfp_rhi_startup(&active,&desc,&gl,1)<0);
    assert(active==&dd && !strcmp(events,"Vv")); /* No alternate primary policy. */
    reset(-1,0);active=&gl;assert(tsfp_rhi_startup(&active,&desc,&gl,1)==0);
    assert(!strcmp(events,"G")); /* Explicit GL must never recurse. */
    reset(-1,-1);active=&gl;assert(tsfp_rhi_startup(&active,&desc,&gl,1)<0);
    assert(!strcmp(events,"Gg") && !gl_live);
    assert(!memcmp(&desc,before,sizeof desc));
    active=NULL;assert(tsfp_rhi_startup(&active,&desc,&gl,1)<0);
    puts("PASS: shared renderer startup ordering, acceptance gate and cleanup");
    return 0;
}
