/* Execute the legacy GL loader with real DDS reads and counted GPU adapters. */
#include "texture_pack_format.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
static void tick(void) {Sleep(1);}
#else
#include <unistd.h>
static void tick(void) {usleep(1000);}
static int _putenv_s(const char *key,const char *value) {return setenv(key,value,1);}
#endif
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
typedef unsigned GLuint,GLenum;
typedef int GLint;
enum {GL_TEXTURE_BINDING_2D=1,GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,
      GL_TEXTURE_MIN_FILTER,GL_TEXTURE_MAG_FILTER,GL_LINEAR_MIPMAP_LINEAR,GL_LINEAR,GL_NO_ERROR=0};
typedef struct {
    unsigned width,height,format,usage,screen_copy,pack_texture,pack_bytes;
    unsigned char *sys_mem;
    uint64_t pack_key;
    int pack_checked;
} GLTexture;
static unsigned binding=77,created,destroyed,mips,next_name=100,error;
static void glGetIntegerv(GLenum name,GLint *value) {CHECK(name==GL_TEXTURE_BINDING_2D);*value=(GLint)binding;}
static void glGenTextures(int n,GLuint *name) {CHECK(n==1);*name=++next_name;created++;}
static void glBindTexture(GLenum kind,GLuint name) {CHECK(kind==GL_TEXTURE_2D);binding=name;}
static void glDeleteTextures(int n,const GLuint *name) {CHECK(n==1 && *name>100);destroyed++;}
static void glCompressedTexImage2D(GLenum kind,unsigned level,GLenum format,unsigned w,unsigned h,
                                  int border,unsigned size,const void *data)
{
    CHECK(kind==GL_TEXTURE_2D && level<3 && format==0x83f1 && !border && data);
    CHECK(w==(4u>>level) && h==w && size==8);mips++;
}
static void glTexParameteri(GLenum kind,GLenum setting,unsigned value) {(void)setting;(void)value;CHECK(kind==GL_TEXTURE_2D);}
static GLenum glGetError(void) {return error;}
static unsigned d3d8_row_pitch(unsigned format,unsigned w) {CHECK(format==12);return ((w+3)/4)*8;}
static int d3d8_format_is_compressed(unsigned format) {return format==12 || format==14 || format==15;}
#include "native/gl_texture_pack.inc"
static void finish(GLTexture *t)
{
    for(int i=0;i<2000 && t->pack_checked>=2;i++) {
        tsfp_stream_pump();
        if(t->pack_checked==3)native_pack_request(t);
        tick();
    }
    CHECK(t->pack_checked==1);
}
static void put(unsigned char *p,uint32_t x)
{p[0]=(unsigned char)x;p[1]=(unsigned char)(x>>8);p[2]=(unsigned char)(x>>16);p[3]=(unsigned char)(x>>24);}
int main(int argc,char **argv)
{
    CHECK(argc==2);_putenv_s("TSFP_TEXTURE_PACK_DIR",argv[1]);
    unsigned char data[8]={0,1,2,3,4,5,6,7},changed[8]={9,1,2,3,4,5,6,7},dds[152]={0};
    GLTexture t={0};t.width=t.height=4;t.format=12;t.sys_mem=data;
    char path[4096];snprintf(path,sizeof path,"%s/%016" PRIx64 ".dds",argv[1],tsfp_texture_key(12,4,4,data,8));
    put(dds,0x20534444);put(dds+4,124);put(dds+12,4);put(dds+16,4);put(dds+28,3);
    put(dds+76,32);put(dds+80,4);put(dds+84,0x31545844);
    FILE *f=fopen(path,"wb");CHECK(f);CHECK(fwrite(dds,1,sizeof dds,f)==sizeof dds);CHECK(!fclose(f));
    native_pack_upload(&t);
    if(strcmp(getenv("TSFP_TEXTURE_STREAMING"),"0"))CHECK(!created && !t.pack_texture);
    finish(&t);CHECK(created==1 && !destroyed && mips==3 && t.pack_bytes==24);
    CHECK(binding==77 && native_pack_resident==24);
    native_pack_upload(&t);finish(&t);CHECK(created==1); /* cached */
    native_pack_release(&t);CHECK(destroyed==1 && !native_pack_resident);
    error=1;native_pack_upload(&t);finish(&t);
    CHECK(created==2 && destroyed==2 && !t.pack_texture && binding==77);
    error=0;native_pack_release(&t);
    /* Cancellation/content reuse cannot restore a released replacement. */
    native_pack_upload(&t);native_pack_release(&t);t.sys_mem=changed;
    native_pack_upload(&t);finish(&t);
    CHECK(!t.pack_texture && created==destroyed && !native_pack_resident);
    t.sys_mem=data;t.screen_copy=1;native_pack_upload(&t);CHECK(!t.pack_texture);
    t.screen_copy=0;t.usage=1;native_pack_upload(&t);CHECK(!t.pack_texture);
    t.usage=0;native_pack_release(&t);
    tsfp_stream_shutdown();CHECK(!remove(path));
    puts("PASS: legacy GL background upload, binding preservation, mipmaps, errors and cancellation");
    return 0;
}
