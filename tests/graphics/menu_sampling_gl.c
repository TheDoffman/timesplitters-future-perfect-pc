/* Asset-free offscreen shader contract; no SDL, game or personal settings. */
#define GL_SILENCE_DEPRECATION
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <stdio.h>
#include <stdlib.h>
#include "pc_menu_sampling.h"
#include "menu_sampling_cases.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1); } } while(0)
static GLuint program;
static GLuint compile(GLenum kind,const char *source)
{
    GLuint shader=glCreateShader(kind);GLint ok=0;
    glShaderSource(shader,1,&source,NULL);glCompileShader(shader);
    glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok) {char log[4096];glGetShaderInfoLog(shader,sizeof log,NULL,log);fprintf(stderr,"%s\n",log);}
    CHECK(ok);return shader;
}
static void render(float mode,float scale,float *pixels)
{
    glUniform1f(glGetUniformLocation(program,"enabled"),mode);
    glUniform1f(glGetUniformLocation(program,"scale"),scale);
    glDrawArrays(GL_TRIANGLES,0,3);
    glReadPixels(0,0,64,64,GL_RGBA,GL_FLOAT,pixels);
    CHECK(glGetError()==GL_NO_ERROR);
}
int main(void)
{
    CGLPixelFormatAttribute attributes[]={kCGLPFAOpenGLProfile,
        (CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core,(CGLPixelFormatAttribute)0};
    CGLPixelFormatObj format=NULL;CGLContextObj context=NULL;GLint count=0;
    CHECK(CGLChoosePixelFormat(attributes,&format,&count)==kCGLNoError && format);
    CHECK(CGLCreateContext(format,NULL,&context)==kCGLNoError && context);
    CGLDestroyPixelFormat(format);CHECK(CGLSetCurrentContext(context)==kCGLNoError);
    const char *vertex="#version 330 core\nvoid main(){gl_Position=vec4("
        "gl_VertexID==2?3.0:-1.0,gl_VertexID==1?3.0:-1.0,0,1);}";
    const char *fragment="#version 330 core\n" TSFP_MENU_GLSL
        "uniform sampler2D tex;uniform float enabled,scale;out vec4 color;"
        "void main(){color=menu_sample(tex,gl_FragCoord.xy/64.0*scale,enabled);}";
    GLuint vs=compile(GL_VERTEX_SHADER,vertex),fs=compile(GL_FRAGMENT_SHADER,fragment);
    program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);
    glLinkProgram(program);GLint linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);CHECK(linked);
    glDeleteShader(vs);glDeleteShader(fs);glUseProgram(program);
    GLuint textures[2],fbo,vao;
    glGenTextures(2,textures);glBindTexture(GL_TEXTURE_2D,textures[0]);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,64,64,0,GL_RGBA,GL_FLOAT,NULL);
    glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,textures[0],0);
    CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    float texels[16*16*4];menu_sampling_texels(texels);
    glBindTexture(GL_TEXTURE_2D,textures[1]);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,16,16,0,GL_RGBA,GL_FLOAT,texels);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glUniform1i(glGetUniformLocation(program,"tex"),0);
    glGenVertexArrays(1,&vao);glBindVertexArray(vao);glViewport(0,0,64,64);
    CHECK(menu_sampling_cases(render));
    glDeleteVertexArrays(1,&vao);glDeleteFramebuffers(1,&fbo);glDeleteTextures(2,textures);
    glDeleteProgram(program);CGLSetCurrentContext(NULL);CGLDestroyContext(context);
    puts("PASS: GPU menu magnification, alpha, flat colours, bounded edges and unchanged native/minified sampling");
    return 0;
}
