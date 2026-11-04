#include "pc_counter_layout.h"
/* Host-only overlay: drawn after scene presentation so guest captures, copied
 * textures, and interpolation history cannot contain the debug text. */
typedef struct NativeDebugLine {
    GLuint texture,fbo;
    char text[384];
    int width;
} NativeDebugLine;
static NativeDebugLine native_debug_lines[2];
static void native_debug_release(void) {
    for(int i=0;i<2;i++) {
        if(native_debug_lines[i].texture)glDeleteTextures(1,&native_debug_lines[i].texture);
        if(native_debug_lines[i].fbo)glDeleteFramebuffers(1,&native_debug_lines[i].fbo);
    }
    memset(native_debug_lines,0,sizeof native_debug_lines);
}
static void native_debug_line(NativeDebugLine *cache,const char *text,GLuint destination,
                              int width,int height,int bottom) {
    if(!cache->texture){glGenTextures(1,&cache->texture);glGenFramebuffers(1,&cache->fbo);}
    if(strcmp(text,cache->text) || !cache->width) {
        static unsigned char pixels[1024*48*4];
        if(!tsfp_native_debug_bitmap(text,pixels,1024,48,&cache->width))return;
        glBindTexture(GL_TEXTURE_2D,cache->texture);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1024,48,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
        snprintf(cache->text,sizeof cache->text,"%s",text);
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER,cache->fbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,cache->texture,0);
    if(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE) {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,destination);
        float scale=!bottom && pc_counter_enabled()?pc_counter_scale(width,height,cache->width,48):1.f;
        int line_height=(int)(48*scale),margin=(int)(10*scale);
        int shown=width-2*margin<cache->width*scale?width-2*margin:(int)(cache->width*scale);
        int x=bottom?width-margin-shown:margin,y=bottom?margin:height-line_height-margin;
        /* Fit the complete line when the drawable is narrow. CoreText truncates
         * only text exceeding the cache itself, with an explicit ellipsis. */
        glBlitFramebuffer(0,48,cache->width,0,x,y,x+shown,y+line_height,GL_COLOR_BUFFER_BIT,GL_LINEAR);
    }
}
static void native_debug_draw(GLuint destination,int width,int height) {
    if(width<=20 || height<=60)return;
    char text[320],line[384];
    int show=pc_debug_overlay(text,sizeof text,NULL);
    PcDebugOptions options;pc_debug_get(&options);
    snprintf(line,sizeof line,"%s%s",options.show_fps && !options.show_counter?"Game/Shown: ":"",show?text:"");
    GLint read,draw,binding,unpack,row,skip_rows,skip_pixels,pbo;
    GLboolean scissor=glIsEnabled(GL_SCISSOR_TEST);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);
    glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding);glGetIntegerv(GL_UNPACK_ALIGNMENT,&unpack);
    glGetIntegerv(GL_UNPACK_ROW_LENGTH,&row);glGetIntegerv(GL_UNPACK_SKIP_ROWS,&skip_rows);
    glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&skip_pixels);glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
    glDisable(GL_SCISSOR_TEST);
    if(show)native_debug_line(&native_debug_lines[0],line,destination,width,height,0);
    native_debug_line(&native_debug_lines[1],pc_debug_watermark(),destination,width,height,1);
    if(scissor)glEnable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,read);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,draw);
    glBindTexture(GL_TEXTURE_2D,binding);glPixelStorei(GL_UNPACK_ALIGNMENT,unpack);
    glPixelStorei(GL_UNPACK_ROW_LENGTH,row);glPixelStorei(GL_UNPACK_SKIP_ROWS,skip_rows);glPixelStorei(GL_UNPACK_SKIP_PIXELS,skip_pixels);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,pbo);
}
static void native_debug_present(Uint64 now,int guest) {
    static Uint64 start;static unsigned games,shown;
    if(guest)pc_debug_frame();
    if(!start){start=now;return;}
    games+=guest;shown++;
    if(now-start>=500000000) {
        double seconds=(now-start)/1e9;pc_debug_sample(games/seconds,shown/seconds);
        start=now;games=shown=0;
    }
}
