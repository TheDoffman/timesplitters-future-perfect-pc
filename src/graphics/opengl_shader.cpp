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
#include <vector>

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
struct Varying {std::string local,wire,type,wide;unsigned components,count;};
static std::string normalize_varyings(std::string glsl,const std::vector<Varying> &varyings,bool vertex)
{
    std::string transfers;
    for(const auto &v:varyings) {
        auto at=glsl.find(" "+v.local);if(at==std::string::npos)throw std::runtime_error("Missing GLSL interface declaration");
        auto begin=glsl.rfind('\n',at);begin=begin==std::string::npos?0:begin+1;
        auto end=glsl.find('\n',at);if(end==std::string::npos)throw std::runtime_error("Invalid GLSL interface declaration");
        std::string line=glsl.substr(begin,end-begin),direction=vertex?"out ":"in ";
        auto word=line.find(direction);if(word==std::string::npos)throw std::runtime_error("Invalid GLSL interface qualifier");
        std::string array=v.count>1?"["+std::to_string(v.count)+"]":"";
        std::string wire=line.substr(0,word)+direction+v.wide+" "+v.wire+array+";\n";
        std::string local=v.type+" "+v.local+array+";";
        glsl.replace(begin,end-begin,wire+local);
        for(unsigned i=0;i<v.count;i++) {
            std::string suffix=v.count>1?"["+std::to_string(i)+"]":"";
            if(vertex) {
                transfers+=v.wire+suffix+"="+v.wide+"("+v.local+suffix;
                for(unsigned j=v.components;j<4;j++)transfers+=",0";
                transfers+=");\n";
            } else {
                transfers+=v.local+suffix+"="+v.wire+suffix;
                if(v.components<4)transfers+="."+std::string("xyzw",v.components);
                transfers+=";\n";
            }
        }
    }
    if(!varyings.empty()) {
        auto main=glsl.find("void main(");if(main==std::string::npos)throw std::runtime_error("Missing GLSL main");
        glsl.replace(main,10,"void tsfp_shader_main(");
        glsl+="\nvoid main(){\n"+(vertex?"tsfp_shader_main();\n"+transfers:transfers+"tsfp_shader_main();\n")+"}\n";
    }
    return glsl;
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
        std::vector<Varying> wires;
        for (const auto &r:varyings) {
            std::string wire=varying_name(semantic(compiler,r.id));
            auto type=compiler.get_type(r.type_id);
            std::string scalar=type.basetype==spirv_cross::SPIRType::Float?"float":
                type.basetype==spirv_cross::SPIRType::Int?"int":type.basetype==spirv_cross::SPIRType::UInt?"uint":"";
            if(scalar.empty() || type.columns!=1 || type.vecsize>4 || type.array.size()>1)
                throw std::runtime_error("Unsupported HLSL varying type");
            std::string prefix=scalar=="float"?"vec":scalar=="int"?"ivec":"uvec";
            unsigned count=type.array.empty()?1:type.array[0];
            if(!count || count>32)throw std::runtime_error("Unsupported HLSL varying array");
            wires.push_back({wire+"_local",wire,type.vecsize==1?scalar:prefix+std::to_string(type.vecsize),prefix+"4",type.vecsize,count});
            compiler.set_name(r.id,wire+"_local");
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
        std::string glsl=normalize_varyings(compiler.compile(),wires,stage==RHI_STAGE_VERTEX);
        /*4.1 has textureSize, but sample/level-count queries are later GLSL
         * features. The RHI already knows exact bound-view counts on all hosts. */
        std::string query_uniforms;
        const char *queries[]={"textureSamples","textureQueryLevels"};
        const char *prefixes[]={"tsfp_samples_","tsfp_levels_"};
        const char *extensions[]={"GL_ARB_texture_query_samples","GL_ARB_texture_query_levels"};
        for(unsigned query=0;query<2;query++) {
            for(unsigned i=0;i<out->sampler_count;i++) {
                const auto &sampler=out->samplers[i];
                std::string call=std::string(queries[query])+"("+sampler.name+")";
                std::string name=std::string(prefixes[query])+std::to_string(sampler.unit);
                bool used=false;size_t at;
                while((at=glsl.find(call))!=std::string::npos){glsl.replace(at,call.size(),name);used=true;}
                if(used)query_uniforms+="uniform int "+name+";\n";
            }
            if(glsl.find(std::string(queries[query])+"(")!=std::string::npos)
                throw std::runtime_error("Unsupported texture-query sampler expression");
            auto ext=glsl.find(std::string("#extension ")+extensions[query]);
            if(ext!=std::string::npos)glsl.erase(ext,glsl.find('\n',ext)-ext+1);
        }
        if(!query_uniforms.empty()) {
            size_t last=glsl.rfind("#extension");
            size_t at=glsl.find('\n',last==std::string::npos?0:last)+1;
            glsl.insert(at,query_uniforms);
        }
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
