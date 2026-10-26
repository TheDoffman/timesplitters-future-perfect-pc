/* Real OS protection boundaries, independent of the controlled query fixture. */
#include "pc_guest_memory.h"
static unsigned full_queries;
static PcGuestRegion counted_region(uintptr_t address)
{ ++full_queries;return pc_guest_query_region(address); }
#define pc_guest_query_region counted_region
#include "pc_guest_span.h"
#undef pc_guest_query_region
#include "pc_camera.h"
#include <stdio.h>
#include <string.h>
#ifdef __APPLE__
#include <sys/mman.h>
#include <unistd.h>
#endif
#define CHECK(c) do {if(!(c)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c);return 1;}}while(0)
static int protect(void *p,size_t size,int mode)
{
#ifdef __APPLE__
    return !mprotect(p,size,mode==2?PROT_READ|PROT_WRITE:mode==1?PROT_READ:PROT_NONE);
#else
    DWORD old;
    return VirtualProtect(p,size,mode==2?PAGE_READWRITE:mode==1?PAGE_READONLY:PAGE_NOACCESS,&old)!=0;
#endif
}
int main(void)
{
#ifdef __APPLE__
    size_t page=(size_t)sysconf(_SC_PAGESIZE);
    unsigned char *guest=mmap(NULL,page*4,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);
    CHECK(guest!=MAP_FAILED);
#else
    SYSTEM_INFO info;GetSystemInfo(&info);size_t page=info.dwPageSize;
    unsigned char *guest=VirtualAlloc(NULL,page*4,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    CHECK(guest);
#endif
    ptrdiff_t offset=(ptrdiff_t)guest;
    uint32_t player=(uint32_t)(page*3),camera=(uint32_t)page;
    float *yaw,*pitch,fov;
    memset(guest+player,0,0x118);
    *(float*)(guest+camera+0x384)=55;
    PcGuestRegion region=pc_camera_mouse_region(player,camera,offset);
    CHECK(pc_guest_region_remaining(&region,(uintptr_t)guest+camera+0x384,0)>=4);
    CHECK(pc_camera_fields_in_region(player,camera,offset,&yaw,&pitch,&fov,&region));
    CHECK(fov==55 && yaw==(float*)(guest+player+0x100) && pitch==yaw+1);
#ifndef __APPLE__
    CHECK(full_queries==0); /* Every required field uses fresh resident-page checks. */
#endif
    /* Refresh at every operation; a prior writable span must never survive
     * a protection change. A read-only FOV is still legal with a writable player. */
    CHECK(protect(guest+camera,page,1));
    region=pc_camera_mouse_region(player,camera,offset);
    CHECK(!pc_guest_region_remaining(&region,(uintptr_t)guest+camera+0x384,1));
    CHECK(!pc_guest_region_remaining(&region,(uintptr_t)guest+player,0));
    CHECK(pc_camera_fields_in_region(player,camera,offset,&yaw,&pitch,&fov,&region));
    CHECK(protect(guest+player,page,1));
    region=pc_camera_mouse_region(player,camera,offset);
    CHECK(!pc_camera_fields_in_region(player,camera,offset,&yaw,&pitch,&fov,&region));
    CHECK(protect(guest+player,page,2) && protect(guest+camera,page,0));
    region=pc_camera_mouse_region(player,camera,offset);
    CHECK(!pc_camera_fields_in_region(player,camera,offset,&yaw,&pitch,&fov,&region));
    CHECK(protect(guest+camera,page,2));
    region=pc_camera_mouse_region(player,camera,offset);
    CHECK(pc_camera_fields_in_region(player,camera,offset,&yaw,&pitch,&fov,&region));
    /* Reverse the address order; null inputs remain rejected. */
    *(float*)(guest+player+0x384)=55;
    region=pc_camera_mouse_region(camera,player,offset);
    CHECK(pc_camera_fields_in_region(camera,player,offset,&yaw,&pitch,&fov,&region));
    region=pc_camera_mouse_region(0,camera,offset);CHECK(!region.size);
    CHECK(!pc_camera_fields_in_region(0,camera,offset,&yaw,&pitch,&fov,&region));
#ifdef __APPLE__
    CHECK(!munmap(guest,page*4));
#else
    CHECK(VirtualFree(guest,0,MEM_RELEASE));
#endif
    puts("PASS: camera mapping order and real OS protection changes");
    return 0;
}
