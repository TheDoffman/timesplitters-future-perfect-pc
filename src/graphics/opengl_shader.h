#ifndef TSFP_OPENGL_SHADER_H
#define TSFP_OPENGL_SHADER_H
#include "rhi.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { TSFP_GL_MAX_BLOCKS=16, TSFP_GL_MAX_INPUTS=32, TSFP_GL_MAX_SAMPLERS=16 };
typedef struct { char name[64]; unsigned binding; } TsfpGlBlock;
typedef struct { char semantic[64]; unsigned location; } TsfpGlInput;
typedef struct { char name[64]; unsigned unit,texture_slot,sampler_slot; } TsfpGlSampler;
typedef struct {
    char *source;
    unsigned block_count,input_count,sampler_count;
    TsfpGlBlock blocks[TSFP_GL_MAX_BLOCKS];
    TsfpGlInput inputs[TSFP_GL_MAX_INPUTS];
    TsfpGlSampler samplers[TSFP_GL_MAX_SAMPLERS];
} TsfpGlShaderText;
int tsfp_gl_shader_translate(unsigned stage,const RhiShaderSource *source,
                             TsfpGlShaderText *out,char *error,size_t error_size);
void tsfp_gl_shader_text_free(TsfpGlShaderText *text);
#ifdef __cplusplus
}
#endif
#endif
