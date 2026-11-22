/* Preserve the production HLSL generators and DX constant-buffer layout.
 * Only shader-language/resource bindings change at this backend boundary. */
#include "opengl_shader.h"
#include "rhi_vulkan_bindings.h"
#include "spirv_glsl.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

extern "C" int rhi_gl_dxc_compile(uint32_t,const RhiShaderSource *,uint32_t **,size_t *,char *,size_t);

static std::string semantic(spirv_cross::CompilerGLSL &compiler,spirv_cross::ID id)
{
    if (!compiler.has_decoration(id,spv::DecorationHlslSemanticGOOGLE))
        throw std::runtime_error("DXC omitted the HLSL interface semantic");
    return compiler.get_decoration_string(id,spv::DecorationHlslSemanticGOOGLE);
}
static std::string varying_name(const std::string &value)
{
    std::string name="tsfp_varying_";
    for (unsigned char c:value) {
        if (c>='a' && c<='z')c-=32;
        if (!((c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_'))
            throw std::runtime_error("Unsupported HLSL semantic name");
        name+=c;
    }
    return name;
}
extern "C" void tsfp_gl_shader_text_free(TsfpGlShaderText *text)
{
    if (text) {std::free(text->source);std::memset(text,0,sizeof *text);}
}
extern "C" int tsfp_gl_shader_translate(unsigned stage,const RhiShaderSource *source,
                                        TsfpGlShaderText *out,char *error,size_t error_size)
{
    if (!out)return 0;
    std::memset(out,0,sizeof *out);
    if (!source || (stage!=RHI_STAGE_VERTEX && stage!=RHI_STAGE_PIXEL))return 0;
    uint32_t *words=nullptr;size_t bytes=0;
    if (!rhi_gl_dxc_compile(stage,source,&words,&bytes,error,error_size))return 0;
    std::unique_ptr<uint32_t,decltype(&std::free)> owned(words,std::free);
    try {
        spirv_cross::CompilerGLSL compiler(words,bytes/sizeof(uint32_t));
        auto options=compiler.get_common_options();
        options.version=410;options.es=false;
        options.enable_420pack_extension=false;
        options.vertex.fixup_clipspace=true;
        /* Offscreen row zero is the GL bottom row but the RHI's logical top.
         * Keep uploads/readbacks unflipped; flip only the final screen blit. */
        options.vertex.flip_vert_y=true;
        compiler.set_common_options(options);
        auto resources=compiler.get_shader_resources();
        if (!resources.storage_buffers.empty() || !resources.storage_images.empty())
            throw std::runtime_error("OpenGL recovery does not support storage resources");
        for (const auto &r:resources.uniform_buffers) {
            if (out->block_count>=TSFP_GL_MAX_BLOCKS)throw std::runtime_error("Too many OpenGL uniform blocks");
            unsigned binding=compiler.get_decoration(r.id,spv::DecorationBinding);
            if (binding>=TSFP_GL_MAX_BLOCKS)throw std::runtime_error("Invalid shared uniform register binding");
            auto &block=out->blocks[out->block_count++];block.binding=binding;
            std::snprintf(block.name,sizeof block.name,"tsfp_block_%u",binding);
            compiler.set_name(r.base_type_id,block.name);
            compiler.unset_decoration(r.id,spv::DecorationDescriptorSet);
        }
        if (stage==RHI_STAGE_VERTEX) {
            for (const auto &r:resources.stage_inputs) {
                if (out->input_count>=TSFP_GL_MAX_INPUTS)throw std::runtime_error("Too many OpenGL vertex inputs");
                auto &input=out->inputs[out->input_count++];auto name=semantic(compiler,r.id);
                if (name.size()>=sizeof input.semantic)throw std::runtime_error("Vertex semantic is too long");
                std::memcpy(input.semantic,name.c_str(),name.size()+1);
                input.location=compiler.get_decoration(r.id,spv::DecorationLocation);
            }
        }
        /* Match HLSL semantics rather than dense SPIR-V locations: a pixel
         * shader may consume only a subset of the vertex shader's outputs. */
        const auto &varyings=stage==RHI_STAGE_VERTEX?resources.stage_outputs:resources.stage_inputs;
        for (const auto &r:varyings) {
            compiler.set_name(r.id,varying_name(semantic(compiler,r.id)));
            compiler.unset_decoration(r.id,spv::DecorationLocation);
        }
        auto dummy=compiler.build_dummy_sampler_for_combined_images();
        compiler.build_combined_image_samplers();
        for (const auto &r:compiler.get_combined_image_samplers()) {
            if (out->sampler_count>=TSFP_GL_MAX_SAMPLERS)throw std::runtime_error("Too many combined OpenGL samplers");
            unsigned texture=compiler.get_decoration(r.image_id,spv::DecorationBinding);
            unsigned sampler=r.sampler_id==dummy?~0u:compiler.get_decoration(r.sampler_id,spv::DecorationBinding);
            if (texture<RHI_VK_TEXTURE_BASE || texture>=RHI_VK_TEXTURE_BASE+16 ||
                (sampler!=~0u && (sampler<RHI_VK_SAMPLER_BASE || sampler>=RHI_VK_SAMPLER_BASE+16)))
                throw std::runtime_error("Invalid shared texture/sampler binding");
            auto &slot=out->samplers[out->sampler_count];
            slot.unit=out->sampler_count+++(stage==RHI_STAGE_VERTEX?16:0);
            slot.texture_slot=texture-RHI_VK_TEXTURE_BASE;
            slot.sampler_slot=sampler==~0u?~0u:sampler-RHI_VK_SAMPLER_BASE;
            std::snprintf(slot.name,sizeof slot.name,"tsfp_sampler_%u",slot.unit);
            compiler.set_name(r.combined_id,slot.name);
            compiler.unset_decoration(r.combined_id,spv::DecorationBinding);
            compiler.unset_decoration(r.combined_id,spv::DecorationDescriptorSet);
        }
        std::string glsl=compiler.compile();
        out->source=static_cast<char *>(std::malloc(glsl.size()+1));
        if (!out->source)throw std::bad_alloc();
        std::memcpy(out->source,glsl.c_str(),glsl.size()+1);
        return 1;
    } catch (const std::exception &failure) {
        if (error && error_size)std::snprintf(error,error_size,"OpenGL shader translation: %s",failure.what());
        tsfp_gl_shader_text_free(out);
        return 0;
    }
}
