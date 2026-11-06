#include "pc_counter_layout.h"
#include "pc_developer.h"
#include "text_bitmap_macos.h"
#include "graphics/pc_developer_canvas.h"
/* Host-only overlay: drawn after scene presentation so guest captures, copied
 * textures, and interpolation history cannot contain the debug text. */
typedef struct NativeDebugLine {
    GLuint texture,fbo;
    char text[384];
    int width;
} NativeDebugLine;
static NativeDebugLine native_debug_lines[2];
static PcDeveloperCanvas native_developer_canvas;
static GLuint native_developer_texture,native_developer_fbo,native_developer_program,native_developer_vao;
static PcDeveloperLine native_developer_layout[PC_DEVELOPER_MAX_DRAW];
/* Thin GL adapter: composite the shared straight-alpha canvas after the scene. */
static void native_developer_composite(GLuint destination,int width,int height,PcDeveloperCanvas *c) {
    if(!native_developer_program) {
        const char *vs="#version 410 core\nout vec2 uv;uniform vec4 rect;void main(){vec2 p=vec2(gl_VertexID&1,gl_VertexID>>1);uv=p;gl_Position=vec4(rect.xy+p*rect.zw,0,1);}";
        const char *fs="#version 410 core\nin vec2 uv;out vec4 color;uniform sampler2D image;void main(){color=texelFetch(image,ivec2(uv*vec2(textureSize(image,0))),0);}";
        GLuint shaders[2]={glCreateShader(GL_VERTEX_SHADER),glCreateShader(GL_FRAGMENT_SHADER)};
        glShaderSource(shaders[0],1,&vs,NULL);glShaderSource(shaders[1],1,&fs,NULL);
        glCompileShader(shaders[0]);glCompileShader(shaders[1]);
        native_developer_program=glCreateProgram();glAttachShader(native_developer_program,shaders[0]);glAttachShader(native_developer_program,shaders[1]);glLinkProgram(native_developer_program);
        glDeleteShader(shaders[0]);glDeleteShader(shaders[1]);GLint ok=0;glGetProgramiv(native_developer_program,GL_LINK_STATUS,&ok);
        if(!ok){char error[512];glGetProgramInfoLog(native_developer_program,sizeof error,NULL,error);fprintf(stderr,"[Developer Tools] GL canvas: %s\n",error);glDeleteProgram(native_developer_program);native_developer_program=0;return;}
        glGenVertexArrays(1,&native_developer_vao);
    }
    GLint program,vao,viewport[4],active,texture,sampler,src,dst,srca,dsta,eq,eqa,mode[2];GLboolean mask[4];
    const GLenum caps[]={GL_BLEND,GL_DEPTH_TEST,GL_STENCIL_TEST,GL_CULL_FACE,GL_FRAMEBUFFER_SRGB,GL_RASTERIZER_DISCARD};GLboolean enabled[6];
    for(int i=0;i<6;i++)enabled[i]=glIsEnabled(caps[i]);
    glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_VIEWPORT,viewport);glGetBooleanv(GL_COLOR_WRITEMASK,mask);
    glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);glGetIntegerv(GL_SAMPLER_BINDING,&sampler);
    glGetIntegerv(GL_BLEND_SRC_RGB,&src);glGetIntegerv(GL_BLEND_DST_RGB,&dst);glGetIntegerv(GL_BLEND_SRC_ALPHA,&srca);glGetIntegerv(GL_BLEND_DST_ALPHA,&dsta);
    glGetIntegerv(GL_BLEND_EQUATION_RGB,&eq);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&eqa);glGetIntegerv(GL_POLYGON_MODE,mode);
    for(int i=1;i<6;i++)glDisable(caps[i]);glEnable(GL_BLEND);glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glViewport(0,0,width,height);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,destination);
    glUseProgram(native_developer_program);glBindVertexArray(native_developer_vao);glBindSampler(0,0);glBindTexture(GL_TEXTURE_2D,native_developer_texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glUniform1i(glGetUniformLocation(native_developer_program,"image"),0);
    glUniform4f(glGetUniformLocation(native_developer_program,"rect"),-1.f+2.f*c->x/width,1.f-2.f*c->y/height,2.f*c->width/width,-2.f*c->height/height);
    glDrawArrays(GL_TRIANGLE_STRIP,0,4);
    glBindTexture(GL_TEXTURE_2D,texture);glBindSampler(0,sampler);glActiveTexture(active);glUseProgram(program);glBindVertexArray(vao);
    glBlendEquationSeparate(eq,eqa);glBlendFuncSeparate(src,dst,srca,dsta);glPolygonMode(GL_FRONT_AND_BACK,mode[0]);glColorMask(mask[0],mask[1],mask[2],mask[3]);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
    for(int i=0;i<6;i++){if(enabled[i])glEnable(caps[i]);else glDisable(caps[i]);}
}
static void native_debug_release(void) {
    for(int i=0;i<2;i++) {
        if(native_debug_lines[i].texture)glDeleteTextures(1,&native_debug_lines[i].texture);
        if(native_debug_lines[i].fbo)glDeleteFramebuffers(1,&native_debug_lines[i].fbo);
    }
    memset(native_debug_lines,0,sizeof native_debug_lines);
    if(native_developer_texture)glDeleteTextures(1,&native_developer_texture);
    if(native_developer_fbo)glDeleteFramebuffers(1,&native_developer_fbo);
    if(native_developer_program)glDeleteProgram(native_developer_program);
    if(native_developer_vao)glDeleteVertexArrays(1,&native_developer_vao);
    native_developer_texture=native_developer_fbo=native_developer_program=native_developer_vao=0;
    pc_developer_canvas_release(&native_developer_canvas);
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
    pc_developer_frame();
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
    if(pc_developer_watermark_visible())native_debug_line(&native_debug_lines[1],pc_debug_watermark(),destination,width,height,1);
    unsigned developer_count=pc_developer_lines(width,height,native_developer_layout,PC_DEVELOPER_MAX_DRAW);
    if(developer_count) {
        PcDeveloperCanvas *canvas=&native_developer_canvas;
        int changed=pc_developer_canvas_update(canvas,native_developer_layout,developer_count,pc_developer_font_bitmap);
        if(changed>=0 && canvas->pixels) {
            if(!native_developer_texture) {glGenTextures(1,&native_developer_texture);glGenFramebuffers(1,&native_developer_fbo);changed=1;}
            glBindTexture(GL_TEXTURE_2D,native_developer_texture);
            if(changed)glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,canvas->width,canvas->height,0,GL_BGRA,GL_UNSIGNED_BYTE,canvas->pixels);
            native_developer_composite(destination,width,height,canvas);
        }
    }
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
