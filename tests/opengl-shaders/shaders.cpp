#ifdef NDEBUG
#undef NDEBUG
#endif
#include "opengl_shader.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <string>

static TsfpGlShaderText translate(unsigned stage,const char *hlsl)
{
    RhiShaderSource src={hlsl,std::strlen(hlsl),"OpenGL shader contract",nullptr,"main",
        stage==RHI_STAGE_VERTEX?"vs_5_0":"ps_5_0",1};
    TsfpGlShaderText text={};char error[4096]={};
    int ok=tsfp_gl_shader_translate(stage,&src,&text,error,sizeof error);
    if(!ok)std::fprintf(stderr,"%s\n",error);
    assert(ok && text.source && std::strstr(text.source,"#version 410"));
    return text;
}
int main(int argc,char **argv)
{
    assert(argc==2);
    auto vertex=translate(RHI_STAGE_VERTEX,
        "cbuffer Transform:register(b0){float4x4 transform;}"
        "struct In {float4 p:POSITION0;float2 uv:TEXCOORD0;};"
        "struct Out {float4 p:SV_POSITION;float2 first:TEXCOORD0;float2 second:TEXCOORD1;};"
        "Out main(In v){Out o;o.p=mul(transform,v.p);o.first=v.uv;o.second=v.uv*.5;return o;}");
    assert(vertex.block_count==1 && vertex.blocks[0].binding==0);
    assert(vertex.input_count==2);
    bool position=false,uv=false;
    for(unsigned i=0;i<vertex.input_count;i++) {
        position|=!std::strcmp(vertex.inputs[i].semantic,"POSITION0");
        uv|=!std::strcmp(vertex.inputs[i].semantic,"TEXCOORD0");
    }
    assert(position && uv && std::strstr(vertex.source,"tsfp_varying_TEXCOORD1"));
    auto pixel=translate(RHI_STAGE_PIXEL,
        "Texture2D image:register(t5);SamplerState filtering:register(s3);"
        "cbuffer Params:register(b2){float4 tint;}"
        "float4 main(float2 second:TEXCOORD1):SV_TARGET {return image.Sample(filtering,second)*tint;}");
    assert(pixel.block_count==1 && pixel.blocks[0].binding==10);
    assert(pixel.sampler_count==1 && pixel.samplers[0].texture_slot==5 && pixel.samplers[0].sampler_slot==3);
    assert(std::strstr(pixel.source,"tsfp_varying_TEXCOORD1"));
    auto load=translate(RHI_STAGE_PIXEL,
        "Texture2D image:register(t8);float4 main(float4 p:SV_POSITION):SV_TARGET{return image.Load(int3(p.xy,0));}");
    assert(load.sampler_count==1 && load.samplers[0].texture_slot==8 && load.samplers[0].sampler_slot==~0u);
    auto legacy=translate(RHI_STAGE_PIXEL,
        "float4 main(float4 p:SV_POSITION):SV_TARGET{float3 a=float3(p.x,128,256);return float4(a>=128?float3(.2,.4,.6):float3(.8,.1,.3),1);}");
    std::ofstream(std::string(argv[1])+"/interface.vert")<<vertex.source;
    std::ofstream(std::string(argv[1])+"/interface.frag")<<pixel.source;
    std::ofstream(std::string(argv[1])+"/fetch.frag")<<load.source;
    std::ofstream(std::string(argv[1])+"/legacy.frag")<<legacy.source;
    tsfp_gl_shader_text_free(&vertex);tsfp_gl_shader_text_free(&pixel);
    tsfp_gl_shader_text_free(&load);tsfp_gl_shader_text_free(&legacy);
    assert(!vertex.source && !vertex.block_count);
    const char *invalid="float4 main(:";RhiShaderSource bad={invalid,std::strlen(invalid),"invalid",nullptr,"main","ps_5_0",1};
    TsfpGlShaderText failed={};char error[256]={};
    assert(!tsfp_gl_shader_translate(RHI_STAGE_PIXEL,&bad,&failed,error,sizeof error));
    assert(!failed.source && error[0]);tsfp_gl_shader_text_free(&failed);
    std::puts("PASS: production HLSL to GLSL410, semantic linking, resource registers and compiler failures");
}
