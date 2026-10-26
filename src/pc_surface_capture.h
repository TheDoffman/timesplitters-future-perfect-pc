#ifndef PC_SURFACE_CAPTURE_H
#define PC_SURFACE_CAPTURE_H
#include <stdint.h>
#include <string.h>
/* Copied into the deferred renderer queue: no guest pointers survive the call.
 * A TSWATER1 record is these 23 little-endian words followed by three tightly
 * packed 128x128 RGBA8 images: feedback input, first pass, second pass. */
typedef struct PcSurfaceCapture {
    uint32_t resource,frame,level_ticks,scene,level,whole,delta_bits;
    uint32_t textures[3][4]; /* guest VA, data, format, size */
    uint32_t coefficient_bits[4]; /* restoring, acceleration, velocity, damping */
} PcSurfaceCapture;
typedef int (*PcSurfaceRead)(uint32_t address,void *out,unsigned bytes);
static inline int pc_surface_capture_collect(PcSurfaceRead read,uint32_t resource,PcSurfaceCapture *out) {
    if(!read || !out || resource<0x10000u || resource>0x83ffffccu)return 0;
    PcSurfaceCapture s={0};uint32_t fields[6],table,base;
    if(!read(resource+0x1c,fields,sizeof fields) ||
       !read(0x4b85c8u,&table,4) || !read(table,&base,4))return 0;
    if(fields[0]<0x10000u || fields[0]>0x83ffffe0u ||
       fields[1]<0x10000u || fields[1]>0x83fffff0u ||
       base<0x10000u || base>0x83ffffccu)return 0;
    const unsigned order[3]={2,4,3};
    for(unsigned i=0;i<3;i++) {
        uint32_t id,index=fields[order[i]];if(index>=4)return 0;
        if(!read(fields[1]+index*4,&id,4))return 0;
        uint64_t va=(uint64_t)base+(uint64_t)id*0x34u;
        if(va>0x83ffffccu)return 0;
        s.textures[i][0]=(uint32_t)va;
        if(!read((uint32_t)va+4,&s.textures[i][1],4) ||
           !read((uint32_t)va+12,&s.textures[i][2],8))return 0;
        for(unsigned j=0;j<i;j++)if(s.textures[j][0]==va)return 0;
    }
    s.resource=resource;
    if(!read(fields[0]+0x10,s.coefficient_bits,16) ||
       !read(0x7497b8u,&s.frame,4) || !read(0x7de318u,&s.level_ticks,4) ||
       !read(0x79094cu,&s.scene,4) || !read(0x7de30cu,&s.level,4) ||
       !read(0x4e7944u,&s.whole,4) || !read(0x4e794cu,&s.delta_bits,4))return 0;
    *out=s;return 1;
}
#endif
