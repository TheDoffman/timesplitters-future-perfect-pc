#include "opengl_capabilities.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char *advertised[5];
static int extension(const char *name)
{for(unsigned i=0;advertised[i];i++)if(!strcmp(name,advertised[i]))return 1;return 0;}
static const char *missing(int major,int minor)
{return tsfp_gl_missing_capability(tsfp_gl_capabilities(major,minor,extension));}
int main(void)
{
    assert(strstr(missing(4,0),"4.1"));
    assert(strstr(missing(4,1),"S3TC"));
    assert(strstr(missing(4,6),"S3TC")); /* Newer core alone is insufficient. */
    advertised[0]="GL_EXT_texture_compression_dxt1";
    advertised[1]="GL_ARB_texture_mirror_clamp_to_edge";
    assert(strstr(missing(4,1),"S3TC")); /* BC1 alone is insufficient. */
    advertised[0]="GL_EXT_texture_compression_s3tc";advertised[1]=0;
    assert(strstr(missing(4,1),"mirror-once"));
    assert(strstr(missing(4,3),"mirror-once"));
    assert(!missing(4,4) && !missing(4,6) && !missing(5,0));
    const char *mirrors[]={"GL_ARB_texture_mirror_clamp_to_edge","GL_EXT_texture_mirror_clamp","GL_ATI_texture_mirror_once"};
    for(unsigned i=0;i<3;i++) {
        advertised[1]=mirrors[i];
        assert(!missing(4,1));
        assert(strstr(missing(3,3),"4.1"));
    }
    puts("PASS: OpenGL minimum version, complete S3TC and mirror-once capabilities");
    return 0;
}
