#ifndef TSFP_OPENGL_CAPABILITIES_H
#define TSFP_OPENGL_CAPABILITIES_H

/* Required by the common Xbox frontend, beyond the core4.1 entry points.
 * S3TC is not core even in4.6. Mirror-clamp-to-edge is core in4.4, or the
 * identical0x8743 sampler mode exposed by the ARB/EXT/ATI extensions. */
typedef struct TsfpGlCapabilities {
    int major,minor,s3tc,mirror_once;
} TsfpGlCapabilities;
static TsfpGlCapabilities tsfp_gl_capabilities(int major,int minor,int (*extension)(const char *))
{
    TsfpGlCapabilities caps={major,minor,0,0};
    caps.s3tc=extension("GL_EXT_texture_compression_s3tc");
    caps.mirror_once=major>4 || (major==4 && minor>=4) ||
        extension("GL_ARB_texture_mirror_clamp_to_edge") ||
        extension("GL_EXT_texture_mirror_clamp") || extension("GL_ATI_texture_mirror_once");
    return caps;
}
static const char *tsfp_gl_missing_capability(TsfpGlCapabilities caps)
{
    if(caps.major<4 || (caps.major==4 && caps.minor<1))return "OpenGL4.1 core";
    if(!caps.s3tc)return "BC1/BC2/BC3 S3TC compressed textures";
    if(!caps.mirror_once)return "mirror-once sampler addressing";
    return 0;
}
#endif
