#ifdef NDEBUG
#undef NDEBUG
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <SDL3/SDL_opengl.h>
#ifndef _WIN32
#include <SDL3/SDL.h>
#endif
#include "opengl_host.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#define LOAD(type,name) auto name=(type)tsfp_gl_host_proc(host,#name);assert(name)
int main(int argc,char **argv)
{
    assert(argc==2);void *window=nullptr;
#ifdef _WIN32
    window=CreateWindowExA(0,"STATIC","OpenGL shader pixel contract",WS_POPUP,0,0,64,64,NULL,NULL,GetModuleHandleA(NULL),NULL);
#else
    assert(SDL_Init(SDL_INIT_VIDEO));
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    window=SDL_CreateWindow("OpenGL shader pixel contract",64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
#endif
    assert(window);TsfpGlHost *host=tsfp_gl_host_create(window);assert(host);
    LOAD(PFNGLCREATESHADERPROC,glCreateShader);LOAD(PFNGLSHADERSOURCEPROC,glShaderSource);
    LOAD(PFNGLCOMPILESHADERPROC,glCompileShader);LOAD(PFNGLGETSHADERIVPROC,glGetShaderiv);
    LOAD(PFNGLGETSHADERINFOLOGPROC,glGetShaderInfoLog);LOAD(PFNGLCREATEPROGRAMPROC,glCreateProgram);
    LOAD(PFNGLATTACHSHADERPROC,glAttachShader);LOAD(PFNGLLINKPROGRAMPROC,glLinkProgram);
    LOAD(PFNGLGETPROGRAMIVPROC,glGetProgramiv);LOAD(PFNGLGETPROGRAMINFOLOGPROC,glGetProgramInfoLog);
    LOAD(PFNGLUSEPROGRAMPROC,glUseProgram);LOAD(PFNGLGETUNIFORMBLOCKINDEXPROC,glGetUniformBlockIndex);
    LOAD(PFNGLUNIFORMBLOCKBINDINGPROC,glUniformBlockBinding);LOAD(PFNGLGENBUFFERSPROC,glGenBuffers);
    LOAD(PFNGLBINDBUFFERPROC,glBindBuffer);LOAD(PFNGLBUFFERDATAPROC,glBufferData);
    LOAD(PFNGLBINDBUFFERBASEPROC,glBindBufferBase);LOAD(PFNGLGENVERTEXARRAYSPROC,glGenVertexArrays);
    LOAD(PFNGLBINDVERTEXARRAYPROC,glBindVertexArray);LOAD(PFNGLVERTEXATTRIBPOINTERPROC,glVertexAttribPointer);
    LOAD(PFNGLENABLEVERTEXATTRIBARRAYPROC,glEnableVertexAttribArray);
    LOAD(PFNGLGETUNIFORMLOCATIONPROC,glGetUniformLocation);LOAD(PFNGLUNIFORM1IPROC,glUniform1i);
    LOAD(PFNGLDELETESHADERPROC,glDeleteShader);LOAD(PFNGLDELETEPROGRAMPROC,glDeleteProgram);
    LOAD(PFNGLDELETEBUFFERSPROC,glDeleteBuffers);LOAD(PFNGLDELETEVERTEXARRAYSPROC,glDeleteVertexArrays);
    auto shader=[&](GLenum stage,const char *name){
        std::ifstream file(std::string(argv[1])+"/"+name);std::stringstream stream;stream<<file.rdbuf();
        std::string source=stream.str();assert(!source.empty());const char *text=source.c_str();
        GLuint id=glCreateShader(stage);glShaderSource(id,1,&text,nullptr);glCompileShader(id);
        GLint ok=0;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);
        if(!ok){char error[4096]={};glGetShaderInfoLog(id,sizeof error,nullptr,error);std::fprintf(stderr,"%s\n",error);}
        assert(ok);return id;
    };
    GLuint vs=shader(GL_VERTEX_SHADER,"interface.vert"),ps=shader(GL_FRAGMENT_SHADER,"interface.frag");
    GLuint program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,ps);glLinkProgram(program);
    GLint ok=0;glGetProgramiv(program,GL_LINK_STATUS,&ok);
    if(!ok){char error[4096]={};glGetProgramInfoLog(program,sizeof error,nullptr,error);std::fprintf(stderr,"%s\n",error);}
    assert(ok);glUseProgram(program);
    float matrix[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},tint[4]={.5f,.25f,.125f,1};
    GLuint buffers[3]={};glGenBuffers(3,buffers);
    for(unsigned i=0;i<2;i++) {
        glBindBuffer(GL_UNIFORM_BUFFER,buffers[i]);glBufferData(GL_UNIFORM_BUFFER,i?sizeof tint:sizeof matrix,i?tint:matrix,GL_STATIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER,i?10:0,buffers[i]);
        glUniformBlockBinding(program,glGetUniformBlockIndex(program,i?"tsfp_block_10":"tsfp_block_0"),i?10:0);
    }
    GLuint texture;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
    unsigned char color[4]={204,204,204,255};glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,color);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glUniform1i(glGetUniformLocation(program,"tsfp_sampler_0"),0);
    GLint bound=glGetUniformLocation(program,"tsfp_bound_0");assert(bound>=0);glUniform1i(bound,1);
    float vertices[18]={-1,-1,.5,1,0,0, -1,3,.5,1,0,2, 3,-1,.5,1,2,0};
    GLuint vao;glGenVertexArrays(1,&vao);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,buffers[2]);
    glBufferData(GL_ARRAY_BUFFER,sizeof vertices,vertices,GL_STATIC_DRAW);
    glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,6*sizeof(float),nullptr);glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,6*sizeof(float),(const void *)(4*sizeof(float)));glEnableVertexAttribArray(1);
    glViewport(0,0,64,64);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
    glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,3);
    float pixel[4]={};glReadPixels(32,32,1,1,GL_RGBA,GL_FLOAT,pixel);
    assert(glGetError()==GL_NO_ERROR);
    for(unsigned i=0;i<3;i++)assert(std::fabs(pixel[i]-.8f*tint[i])<.01f);
    assert(pixel[3]>.99f);
    glUniform1i(bound,0);glDrawArrays(GL_TRIANGLES,0,3);glReadPixels(32,32,1,1,GL_RGBA,GL_FLOAT,pixel);
    for(unsigned i=0;i<3;i++)assert(std::fabs(pixel[i])<.001f);
    glUniform1i(bound,1);glDrawArrays(GL_TRIANGLES,0,3);assert(tsfp_gl_host_present(host,0)==0);
    /* Compile both less common language paths on the actual GL driver too. */
    GLuint fetch=shader(GL_FRAGMENT_SHADER,"fetch.frag"),legacy=shader(GL_FRAGMENT_SHADER,"legacy.frag");
    glDeleteShader(fetch);glDeleteShader(legacy);glDeleteShader(vs);glDeleteShader(ps);glUseProgram(0);glDeleteProgram(program);
    glDeleteBuffers(3,buffers);glDeleteVertexArrays(1,&vao);glDeleteTextures(1,&texture);
    tsfp_gl_host_destroy(host);
#ifdef _WIN32
    DestroyWindow((HWND)window);
#else
    SDL_DestroyWindow((SDL_Window *)window);SDL_Quit();
#endif
    std::puts("PASS: generated GLSL linked and rendered exact sampled/tinted pixels on hardware");
}
