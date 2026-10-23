#include "texture_pack_stream.h"
#include "texture_mode.h"
#include <inttypes.h>
static uint64_t native_pack_resident;
static void native_pack_request(GLTexture *t);
static void native_pack_release(GLTexture *t) {
    tsfp_stream_cancel(t);t->pack_checked=0;
    if(t->pack_texture)glDeleteTextures(1,&t->pack_texture);
    native_pack_resident-=t->pack_bytes;t->pack_bytes=0;t->pack_texture=0;
}
static void native_pack_apply(void *owner,uint64_t key,const unsigned char *data,const TsfpDDS *dds) {
    GLTexture *t=owner;
    if(t->pack_key!=key || t->pack_checked<2)return;
    t->pack_checked=1;
    if(!data || t->usage || t->screen_copy || !t->sys_mem ||
       native_pack_resident>TSFP_PACK_RESIDENT_BUDGET || dds->bytes>TSFP_PACK_RESIDENT_BUDGET-native_pack_resident)return;
    /* This legacy adapter only uploads BC formats; retain BGRA8 fallback. */
    if(dds->format==6 || !tsfp_texture_alpha_compatible(t->format,dds->format))return;
    GLint old_binding=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&old_binding);
    GLuint texture;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
    GLenum format=dds->format==12?0x83f1:dds->format==14?0x83f2:0x83f3;
    unsigned w=dds->width,h=dds->height;
    for(unsigned i=0;i<dds->levels;i++) {
        glCompressedTexImage2D(GL_TEXTURE_2D,i,format,w,h,0,dds->size[i],data+dds->offset[i]);
        w=w>1?w>>1:1;h=h>1?h>>1:1;
    }
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,dds->levels-1);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    GLenum error=glGetError();glBindTexture(GL_TEXTURE_2D,(GLuint)old_binding);
    if(error!=GL_NO_ERROR){glDeleteTextures(1,&texture);return;}
    t->pack_texture=texture;t->pack_bytes=dds->bytes;native_pack_resident+=dds->bytes;
    const char *trace=getenv("TSFP_TEXTURE_PACK_TRACE");
    if(trace && *trace && strcmp(trace,"0"))fprintf(stderr,"[TEXTURE-PACK] native loaded %016" PRIx64 " %ux%u -> %ux%u mips=%u resident=%" PRIu64 "\n",key,t->width,t->height,dds->width,dds->height,dds->levels,native_pack_resident);
}
static void native_pack_request(GLTexture *t) {
    const char *root=getenv("TSFP_TEXTURE_PACK_DIR");
    if(!root||!*root){t->pack_checked=1;return;}
    char path[4096];
    if(snprintf(path,sizeof path,"%s/%016" PRIx64 ".dds",root,t->pack_key)>=(int)sizeof path){t->pack_checked=1;return;}
    int queued=tsfp_stream_submit(t,t->pack_key,path,t->width,t->height,native_pack_apply);
    t->pack_checked=queued==0?3:2;
    if(queued<0) {
        TsfpDDS dds;unsigned char *data=tsfp_pack_read(path,t->width,t->height,TSFP_PACK_RESIDENT_BUDGET-native_pack_resident,&dds);
        native_pack_apply(t,t->pack_key,data,&dds);free(data);
    }
}
static void native_pack_upload(GLTexture *t) {
    const char *root=getenv("TSFP_TEXTURE_PACK_DIR");
    if(!root||!*root||t->usage||t->screen_copy||!t->sys_mem||!d3d8_format_is_compressed(t->format))return;
    uint32_t bytes=d3d8_row_pitch(t->format,t->width)*((t->height+3)/4);
    uint64_t key=tsfp_texture_key(t->format,t->width,t->height,t->sys_mem,bytes);
    if(t->pack_checked&&t->pack_key==key)return;
    native_pack_release(t);t->pack_key=key;
    native_pack_request(t);
}
