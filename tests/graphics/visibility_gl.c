/* Compile the actual legacy GL adapter against explicit API spies. GPU pixel
 * acceptance is separate; this checks context entry, query API and scaling. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
typedef unsigned int GLuint;
typedef uint64_t GLuint64;
#define GL_SAMPLES_PASSED 1
#define GL_QUERY_RESULT_AVAILABLE 2
#define GL_QUERY_RESULT 3
static unsigned scope_calls,created,deleted,active,available;
static uint64_t fake_samples;
#define NATIVE_GL_SCOPE int entered=(scope_calls++,1);(void)entered
static struct {unsigned backbuf_w,backbuf_h;} g={640,480};
static int native_scene_active=1;static unsigned native_scene_w=1280,native_scene_h=960;
static void glGenQueries(int n,GLuint *id){assert(n==1);*id=++created;}
static void glDeleteQueries(int n,const GLuint *id){assert(n==1 && *id && active!=*id);deleted++;}
static void glBeginQuery(GLuint type,GLuint id){assert(type==GL_SAMPLES_PASSED && !active);active=id;}
static void glEndQuery(GLuint type){assert(type==GL_SAMPLES_PASSED && active);active=0;}
static void glGetQueryObjectuiv(GLuint id,GLuint key,GLuint *value){assert(id && key==GL_QUERY_RESULT_AVAILABLE);*value=available;}
static void glGetQueryObjectui64v(GLuint id,GLuint key,GLuint64 *value){assert(id && key==GL_QUERY_RESULT);*value=fake_samples;}
#include "native/gl_visibility.inc"
int main(void) {
    uint32_t pixels=77;
    assert(xbox_D3D8VisibilityBegin());assert(active);xbox_D3D8VisibilityEnd(0,1);assert(!active);
    assert(!xbox_D3D8VisibilityResult(0,&pixels) && pixels==77);
    available=1;fake_samples=400;
    assert(xbox_D3D8VisibilityResult(0,&pixels)==1 && pixels==100);
    native_scene_active=0;
    assert(xbox_D3D8VisibilityBegin());xbox_D3D8VisibilityEnd(1,1);fake_samples=40;
    assert(xbox_D3D8VisibilityResult(1,&pixels)==1 && pixels==40);
    assert(xbox_D3D8VisibilityBegin());xbox_D3D8VisibilityEnd(2,1);fake_samples=0;
    assert(xbox_D3D8VisibilityResult(2,&pixels)==1 && !pixels);
    assert(created==1 && scope_calls>=9);
    assert(xbox_D3D8VisibilityBegin());assert(active);
    {NATIVE_GL_SCOPE;native_visibility_reset();}
    assert(!active && created==deleted);
    puts("production GL visibility API, context entry, pending/zero and area scaling passed");return 0;
}
