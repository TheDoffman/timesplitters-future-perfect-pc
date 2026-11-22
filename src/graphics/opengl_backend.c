/* Shared OpenGL implementation of the production RHI. Guest decoding, shaders,
 * textures, display and gameplay stay in the existing common frontend. */
#include "rhi_backend.h"
#include "opengl_api.h"
#include "opengl_host.h"
#include "opengl_shader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {GLenum internal,format,type;unsigned bytes,block;int depth,stencil,integer;} GlFormat;
struct RhiBuffer {GLuint id;RhiBufferDesc desc;unsigned refs;};
struct RhiImage {GLuint id;GLenum target;RhiImageDesc desc;unsigned refs;int immutable;};
struct RhiView {RhiImage *image;GLuint texture;GLenum target;RhiViewDesc desc;unsigned kind,refs;int own_texture,stencil;};
struct RhiShader {GLuint id;unsigned stage,refs;TsfpGlShaderText text;};
typedef struct {unsigned location,slot,offset;GlFormat format;unsigned components;} GlAttribute;
struct RhiVertexLayout {unsigned count;GlAttribute attributes[32];unsigned refs;};
struct RhiBlendState {RhiBlendDesc desc;};
struct RhiDepthState {RhiDepthDesc desc;};
struct RhiRasterState {RhiRasterDesc desc;};
struct RhiSampler {GLuint id;unsigned refs;};
struct RhiOcclusion {GLuint id;int active,ended;};
typedef struct GlProgram {GLuint id,vs,ps;GLint samples[32];struct GlProgram *next;} GlProgram;
static TsfpGlApi G;
static struct {
    TsfpGlHost *host;int ready,failed,storage,views,storage_ms,stencil_texture,max_samples;
    GLuint framebuffer,readbuffer,writebuffer,vao;
    RhiImage *swap_image;RhiView *swap_view,*color,*depth;
    RhiShader *shaders[2];RhiVertexLayout *layout;
    RhiBuffer *vertices[16],*indices,*uniforms[16];unsigned strides[16],offsets[16],index_bits,index_offset;
    RhiView *textures[16];RhiSampler *samplers[16];
    RhiBlendDesc blend;RhiDepthDesc depth_state;RhiRasterDesc raster;
    float factor[4];unsigned mask,stencil_ref,topology;
    RhiViewport viewports[16];unsigned viewport_count;RhiRect scissor;
    GlProgram *programs,*pipeline;GLuint current_program;
    GLuint zero_texture;RhiOcclusion *query;
} S;
extern int rhi_vk_dxc_available(void);
static void g_device_destroy(void);
static void g_image_destroy(RhiImage *image);
static void g_view_destroy(RhiView *view);
static void g_set_render_target(RhiView *color,RhiView *depth);
static void g_shader_destroy(RhiShader *shader);
static void g_buffer_destroy(RhiBuffer *buffer);
static void g_sampler_destroy(RhiSampler *sampler);
static void g_layout_destroy(RhiVertexLayout *layout);
static void g_image_update(RhiImage *,uint32_t,const RhiBox *,const void *,uint32_t,uint32_t);
static int g_resolve(RhiImage *,uint32_t,RhiImage *,uint32_t);
static unsigned level_size(unsigned size,unsigned mip){return size>>mip?size>>mip:1;}
static int gl_ok(const char *operation)
{
    GLenum error=glGetError();if(error==GL_NO_ERROR)return 1;
    fprintf(stderr,"[RHI] ERROR: opengl %s (0x%04x)\n",operation,error);
    while(glGetError()!=GL_NO_ERROR){}S.failed=1;return 0;
}
static int extension(const char *name)
{
    GLint count=0;glGetIntegerv(GL_NUM_EXTENSIONS,&count);
    for(GLint i=0;i<count;i++)if(!strcmp((const char *)G.GetStringi(GL_EXTENSIONS,i),name))return 1;
    return 0;
}
static void APIENTRY debug_message(GLenum source,GLenum type,GLuint id,GLenum severity,
                                  GLsizei length,const GLchar *message,const void *user)
{
    (void)source;(void)id;(void)length;(void)user;
    if(type==GL_DEBUG_TYPE_ERROR || severity==GL_DEBUG_SEVERITY_HIGH)
        fprintf(stderr,"[RHI] ERROR: opengl driver: %s\n",message);
}
static GlFormat format(RhiFormat value)
{
    GlFormat f={0};
#define F(v,i,e,t,b) case v:f=(GlFormat){i,e,t,b,0,0,0,0};break
    switch(value) {
    F(RHI_FORMAT_R32G32B32A32_FLOAT,GL_RGBA32F,GL_RGBA,GL_FLOAT,16);
    F(RHI_FORMAT_R32G32B32A32_SINT,GL_RGBA32I,GL_RGBA_INTEGER,GL_INT,16);
    F(RHI_FORMAT_R32G32B32_FLOAT,GL_RGB32F,GL_RGB,GL_FLOAT,12);
    F(RHI_FORMAT_R16G16B16A16_FLOAT,GL_RGBA16F,GL_RGBA,GL_HALF_FLOAT,8);
    F(RHI_FORMAT_R16G16B16A16_UNORM,GL_RGBA16,GL_RGBA,GL_UNSIGNED_SHORT,8);
    F(RHI_FORMAT_R16G16B16A16_SNORM,GL_RGBA16_SNORM,GL_RGBA,GL_SHORT,8);
    F(RHI_FORMAT_R32G32_FLOAT,GL_RG32F,GL_RG,GL_FLOAT,8);
    F(RHI_FORMAT_R10G10B10A2_UNORM,GL_RGB10_A2,GL_RGBA,GL_UNSIGNED_INT_2_10_10_10_REV,4);
    F(RHI_FORMAT_R11G11B10_FLOAT,GL_R11F_G11F_B10F,GL_RGB,GL_UNSIGNED_INT_10F_11F_11F_REV,4);
    F(RHI_FORMAT_R8G8B8A8_UNORM,GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE,4);
    F(RHI_FORMAT_R8G8B8A8_SNORM,GL_RGBA8_SNORM,GL_RGBA,GL_BYTE,4);
    F(RHI_FORMAT_R16G16_FLOAT,GL_RG16F,GL_RG,GL_HALF_FLOAT,4);
    F(RHI_FORMAT_R16G16_UNORM,GL_RG16,GL_RG,GL_UNSIGNED_SHORT,4);
    F(RHI_FORMAT_R16G16_SNORM,GL_RG16_SNORM,GL_RG,GL_SHORT,4);
    F(RHI_FORMAT_R32_FLOAT,GL_R32F,GL_RED,GL_FLOAT,4);
    F(RHI_FORMAT_R32_UINT,GL_R32UI,GL_RED_INTEGER,GL_UNSIGNED_INT,4);
    F(RHI_FORMAT_R8G8_UNORM,GL_RG8,GL_RG,GL_UNSIGNED_BYTE,2);
    F(RHI_FORMAT_R8G8_SNORM,GL_RG8_SNORM,GL_RG,GL_BYTE,2);
    F(RHI_FORMAT_R16_FLOAT,GL_R16F,GL_RED,GL_HALF_FLOAT,2);
    F(RHI_FORMAT_R16_UNORM,GL_R16,GL_RED,GL_UNSIGNED_SHORT,2);
    F(RHI_FORMAT_R16_UINT,GL_R16UI,GL_RED_INTEGER,GL_UNSIGNED_SHORT,2);
    F(RHI_FORMAT_R16_SNORM,GL_R16_SNORM,GL_RED,GL_SHORT,2);
    F(RHI_FORMAT_R8_UNORM,GL_R8,GL_RED,GL_UNSIGNED_BYTE,1);
    F(RHI_FORMAT_A8_UNORM,GL_R8,GL_RED,GL_UNSIGNED_BYTE,1);
    F(RHI_FORMAT_B5G6R5_UNORM,GL_RGB565,GL_RGB,GL_UNSIGNED_SHORT_5_6_5,2);
    F(RHI_FORMAT_B5G5R5A1_UNORM,GL_RGB5_A1,GL_BGRA,GL_UNSIGNED_SHORT_1_5_5_5_REV,2);
    F(RHI_FORMAT_B8G8R8A8_UNORM,GL_RGBA8,GL_BGRA,GL_UNSIGNED_BYTE,4);
    F(RHI_FORMAT_B8G8R8X8_UNORM,GL_RGBA8,GL_BGRA,GL_UNSIGNED_BYTE,4);
    F(RHI_FORMAT_B4G4R4A4_UNORM,GL_RGBA4,GL_BGRA,GL_UNSIGNED_SHORT_4_4_4_4_REV,2);
    F(RHI_FORMAT_D32_FLOAT,GL_DEPTH_COMPONENT32F,GL_DEPTH_COMPONENT,GL_FLOAT,4);
    F(RHI_FORMAT_D24_UNORM_S8_UINT,GL_DEPTH24_STENCIL8,GL_DEPTH_STENCIL,GL_UNSIGNED_INT_24_8,4);
    F(RHI_FORMAT_D16_UNORM,GL_DEPTH_COMPONENT16,GL_DEPTH_COMPONENT,GL_UNSIGNED_SHORT,2);
    case RHI_FORMAT_BC1_UNORM:f.internal=GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;f.block=8;break;
    case RHI_FORMAT_BC2_UNORM:f.internal=GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;f.block=16;break;
    case RHI_FORMAT_BC3_UNORM:f.internal=GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;f.block=16;break;
    case RHI_FORMAT_BC5_UNORM:f.internal=GL_COMPRESSED_RG_RGTC2;f.block=16;break;
    default:break;
    }
#undef F
    f.depth=value==RHI_FORMAT_D32_FLOAT || value==RHI_FORMAT_D24_UNORM_S8_UINT || value==RHI_FORMAT_D16_UNORM;
    f.stencil=value==RHI_FORMAT_D24_UNORM_S8_UINT;
    f.integer=value==RHI_FORMAT_R32_UINT || value==RHI_FORMAT_R16_UINT || value==RHI_FORMAT_R32G32B32A32_SINT;
    return f;
}
static RhiBuffer *g_buffer_create(const RhiBufferDesc *desc,const void *data)
{
    if(!desc || !desc->size || desc->size>512u*1024*1024)return NULL;
    RhiBuffer *b=calloc(1,sizeof *b);if(!b)return NULL;b->refs=1;b->desc=*desc;
    G.GenBuffers(1,&b->id);G.BindBuffer(GL_COPY_WRITE_BUFFER,b->id);
    G.BufferData(GL_COPY_WRITE_BUFFER,desc->size,data,desc->usage==RHI_USAGE_DYNAMIC?GL_DYNAMIC_DRAW:GL_STATIC_DRAW);
    if(!gl_ok("buffer creation")){g_buffer_destroy(b);return NULL;}return b;
}
static void g_buffer_destroy(RhiBuffer *b){if(b && !--b->refs){G.DeleteBuffers(1,&b->id);free(b);}}
static void *g_buffer_map(RhiBuffer *b,uint32_t mode)
{
    if(!b)return NULL;G.BindBuffer(GL_COPY_WRITE_BUFFER,b->id);
    GLbitfield flags=GL_MAP_WRITE_BIT|(mode==RHI_MAP_WRITE_DISCARD?GL_MAP_INVALIDATE_BUFFER_BIT:GL_MAP_UNSYNCHRONIZED_BIT);
    return G.MapBufferRange(GL_COPY_WRITE_BUFFER,0,b->desc.size,flags);
}
static void g_buffer_unmap(RhiBuffer *b){if(b){G.BindBuffer(GL_COPY_WRITE_BUFFER,b->id);G.UnmapBuffer(GL_COPY_WRITE_BUFFER);}}
static void g_buffer_update(RhiBuffer *b,const void *data)
{if(b && data){G.BindBuffer(GL_COPY_WRITE_BUFFER,b->id);G.BufferSubData(GL_COPY_WRITE_BUFFER,0,b->desc.size,data);}}
static void texture_parameters(GLenum target,RhiFormat value,unsigned levels)
{
    glTexParameteri(target,GL_TEXTURE_BASE_LEVEL,0);glTexParameteri(target,GL_TEXTURE_MAX_LEVEL,levels-1);
    glTexParameteri(target,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(target,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(target,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(target,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    if(value==RHI_FORMAT_A8_UNORM){glTexParameteri(target,GL_TEXTURE_SWIZZLE_R,GL_ZERO);glTexParameteri(target,GL_TEXTURE_SWIZZLE_G,GL_ZERO);glTexParameteri(target,GL_TEXTURE_SWIZZLE_B,GL_ZERO);glTexParameteri(target,GL_TEXTURE_SWIZZLE_A,GL_RED);}
    if(value==RHI_FORMAT_B8G8R8X8_UNORM)glTexParameteri(target,GL_TEXTURE_SWIZZLE_A,GL_ONE);
}
static int g_sample_count(RhiFormat value,uint32_t samples)
{
    GlFormat f=format(value);if(!f.internal || f.block || !samples || (samples&(samples-1)))return 0;
    if(samples==1)return 1;
    /* Probe the real internal format; GL_MAX_SAMPLES alone overstates support
     * for integer or floating targets on some drivers. No bound state changes. */
    GLint previous=0;GLuint id=0;glGetIntegerv(GL_TEXTURE_BINDING_2D_MULTISAMPLE,&previous);
    glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D_MULTISAMPLE,id);
    G.TexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE,samples,f.internal,4,4,GL_TRUE);
    GLenum error=glGetError();glBindTexture(GL_TEXTURE_2D_MULTISAMPLE,(GLuint)previous);glDeleteTextures(1,&id);
    while(glGetError()!=GL_NO_ERROR){}return error==GL_NO_ERROR;
}
static RhiImage *g_image_create(const RhiImageDesc *input,const RhiSubresourceData *data)
{
    if(!input)return NULL;RhiImageDesc d=*input;GlFormat f=format(d.format);
    if(!f.internal || !d.width || !d.height || d.width>16384 || d.height>16384)return NULL;
    if(!d.depth)d.depth=1;if(!d.mip_levels)d.mip_levels=1;if(!d.samples)d.samples=1;
    if(d.depth>2048 || d.mip_levels>15 || (d.cube && (d.depth!=6 || d.type==RHI_IMAGE_3D)) ||
       (d.samples>1 && (d.mip_levels!=1 || d.type==RHI_IMAGE_3D || d.cube || f.block)))return NULL;
    RhiImage *image=calloc(1,sizeof *image);if(!image)return NULL;image->refs=1;image->desc=d;
    image->target=d.type==RHI_IMAGE_3D?GL_TEXTURE_3D:d.cube?GL_TEXTURE_CUBE_MAP:
        d.samples>1?(d.depth>1?GL_TEXTURE_2D_MULTISAMPLE_ARRAY:GL_TEXTURE_2D_MULTISAMPLE):
        d.depth>1?GL_TEXTURE_2D_ARRAY:GL_TEXTURE_2D;
    glGenTextures(1,&image->id);glBindTexture(image->target,image->id);
    if(d.samples>1) {
        if(S.storage_ms) {
            if(d.depth>1)G.TexStorage3DMultisample(image->target,d.samples,f.internal,d.width,d.height,d.depth,GL_TRUE);
            else G.TexStorage2DMultisample(image->target,d.samples,f.internal,d.width,d.height,GL_TRUE);
            image->immutable=1;
        } else if(d.depth>1)G.TexImage3DMultisample(image->target,d.samples,f.internal,d.width,d.height,d.depth,GL_TRUE);
        else G.TexImage2DMultisample(image->target,d.samples,f.internal,d.width,d.height,GL_TRUE);
    } else if(S.storage) {
        if(image->target==GL_TEXTURE_3D || image->target==GL_TEXTURE_2D_ARRAY)
            G.TexStorage3D(image->target,d.mip_levels,f.internal,d.width,d.height,d.depth);
        else G.TexStorage2D(image->target,d.mip_levels,f.internal,d.width,d.height);
        image->immutable=1;
    } else {
        for(unsigned m=0;m<d.mip_levels;m++) {
            unsigned w=level_size(d.width,m),h=level_size(d.height,m),z=d.type==RHI_IMAGE_3D?level_size(d.depth,m):d.depth;
            GLsizei size=(GLsizei)(rhi_format_row_pitch(d.format,w)*rhi_format_rows(d.format,h));
            if(image->target==GL_TEXTURE_3D || image->target==GL_TEXTURE_2D_ARRAY) {
                if(f.block)G.CompressedTexImage3D(image->target,m,f.internal,w,h,z,0,size*z,NULL);
                else G.TexImage3D(image->target,m,f.internal,w,h,z,0,f.format,f.type,NULL);
            } else for(unsigned face=0;face<(d.cube?6u:1u);face++) {
                GLenum target=d.cube?GL_TEXTURE_CUBE_MAP_POSITIVE_X+face:image->target;
                if(f.block)G.CompressedTexImage2D(target,m,f.internal,w,h,0,size,NULL);
                else glTexImage2D(target,m,f.internal,w,h,0,f.format,f.type,NULL);
            }
        }
    }
    if(d.samples==1)texture_parameters(image->target,d.format,d.mip_levels);
    if(!gl_ok("image allocation")){g_image_destroy(image);return NULL;}
    if(data)for(unsigned layer=0;layer<(d.type==RHI_IMAGE_3D?1u:d.depth);layer++)for(unsigned m=0;m<d.mip_levels;m++) {
        unsigned sub=layer*d.mip_levels+m;g_image_update(image,sub,NULL,data[sub].data,data[sub].row_pitch,data[sub].slice_pitch);
    }
    return image;
}
static RhiImage *g_image_retain(RhiImage *image){if(image)image->refs++;return image;}
static void g_image_destroy(RhiImage *image){if(image && !--image->refs){glDeleteTextures(1,&image->id);free(image);}}
static void g_image_desc(const RhiImage *image,RhiImageDesc *out){if(image && out)*out=image->desc;}
static void g_image_update(RhiImage *image,uint32_t sub,const RhiBox *box,const void *data,uint32_t row,uint32_t slice)
{
    if(!image || !data || image->desc.samples>1)return;
    RhiImageDesc *d=&image->desc;GlFormat f=format(d->format);
    unsigned mip=sub%d->mip_levels,layer=sub/d->mip_levels,w=level_size(d->width,mip),h=level_size(d->height,mip);
    unsigned depth=d->type==RHI_IMAGE_3D?level_size(d->depth,mip):1;
    if((d->type==RHI_IMAGE_3D && layer) || (d->type!=RHI_IMAGE_3D && layer>=d->depth))return;
    RhiBox full={0,0,0,w,h,depth};if(!box)box=&full;
    if(box->left>=box->right || box->top>=box->bottom || box->front>=box->back ||
       box->right>w || box->bottom>h || box->back>depth)return;
    w=box->right-box->left;h=box->bottom-box->top;depth=box->back-box->front;
    unsigned tight=rhi_format_row_pitch(d->format,w),rows=rhi_format_rows(d->format,h);
    if(!row)row=tight;if(!slice)slice=row*rows;
    if(row<tight || slice<(uint64_t)row*rows || (uint64_t)tight*rows*depth>512u*1024*1024)return;
    unsigned char *packed=malloc((size_t)tight*rows*depth);if(!packed)return;
    for(unsigned z=0;z<depth;z++)for(unsigned y=0;y<rows;y++)
        memcpy(packed+((size_t)z*rows+y)*tight,(const char *)data+(size_t)z*slice+(size_t)y*row,tight);
    if(d->format==RHI_FORMAT_D24_UNORM_S8_UINT)for(size_t i=0;i<(size_t)w*h*depth;i++) {
        uint32_t value;memcpy(&value,packed+i*4,4);value=(value<<8)|(value>>24);memcpy(packed+i*4,&value,4);
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_IMAGE_HEIGHT,0);
    glBindTexture(image->target,image->id);
    if(image->target==GL_TEXTURE_3D || image->target==GL_TEXTURE_2D_ARRAY) {
        unsigned z=d->type==RHI_IMAGE_3D?box->front:layer;
        if(f.block)G.CompressedTexSubImage3D(image->target,mip,box->left,box->top,z,w,h,depth,f.internal,tight*rows*depth,packed);
        else G.TexSubImage3D(image->target,mip,box->left,box->top,z,w,h,depth,f.format,f.type,packed);
    } else {
        GLenum target=d->cube?GL_TEXTURE_CUBE_MAP_POSITIVE_X+layer:image->target;
        if(f.block)G.CompressedTexSubImage2D(target,mip,box->left,box->top,w,h,f.internal,tight*rows,packed);
        else glTexSubImage2D(target,mip,box->left,box->top,w,h,f.format,f.type,packed);
    }
    free(packed);gl_ok("image upload");
}
static RhiView *g_view_create(RhiImage *image,uint32_t kind,const RhiViewDesc *input)
{
    if(!image || kind>RHI_VIEW_DEPTH)return NULL;
    RhiImageDesc *d=&image->desc;RhiViewDesc v={0};
    v.format=d->format;v.mip_count=d->mip_levels;v.layer_count=d->depth;
    v.dim=d->type==RHI_IMAGE_3D?RHI_VIEW_DIM_3D:d->cube?RHI_VIEW_DIM_CUBE:
        d->samples>1?RHI_VIEW_DIM_2D_MS:d->depth>1?RHI_VIEW_DIM_2D_ARRAY:RHI_VIEW_DIM_2D;
    if(input){v=*input;if(!v.format)v.format=d->format;if(!v.mip_count)v.mip_count=d->mip_levels-v.base_mip;
        if(!v.layer_count)v.layer_count=d->depth-v.base_layer;}
    if(v.base_mip>=d->mip_levels || !v.mip_count || v.mip_count>d->mip_levels-v.base_mip ||
       (d->type!=RHI_IMAGE_3D && (v.base_layer>=d->depth || !v.layer_count || v.layer_count>d->depth-v.base_layer)))return NULL;
    GlFormat f=format(d->format);int stencil=v.format==RHI_FORMAT_X24_TYPELESS_G8_UINT;
    if((stencil && (!f.stencil || !S.stencil_texture)) || (!stencil && v.format!=d->format))return NULL;
    RhiView *view=calloc(1,sizeof *view);if(!view)return NULL;
    view->refs=1;view->image=g_image_retain(image);view->kind=kind;view->desc=v;
    view->texture=image->id;view->target=image->target;view->stencil=stencil;
    int subset=v.base_mip || v.mip_count!=d->mip_levels || (d->type!=RHI_IMAGE_3D &&
        (v.base_layer || v.layer_count!=d->depth)) ||
        (d->cube && v.dim!=RHI_VIEW_DIM_CUBE) ||
        (v.dim==RHI_VIEW_DIM_2D_ARRAY && image->target!=GL_TEXTURE_2D_ARRAY);
    if(kind==RHI_VIEW_SAMPLED && (subset || stencil)) {
        if(!S.views || !image->immutable){g_view_destroy(view);return NULL;}
        GLenum target=v.dim==RHI_VIEW_DIM_CUBE?GL_TEXTURE_CUBE_MAP:v.dim==RHI_VIEW_DIM_3D?GL_TEXTURE_3D:
            d->samples>1?(v.layer_count>1?GL_TEXTURE_2D_MULTISAMPLE_ARRAY:GL_TEXTURE_2D_MULTISAMPLE):
            v.dim==RHI_VIEW_DIM_2D_ARRAY?GL_TEXTURE_2D_ARRAY:GL_TEXTURE_2D;
        glGenTextures(1,&view->texture);view->own_texture=1;view->target=target;
        G.TextureView(view->texture,target,image->id,f.internal,v.base_mip,v.mip_count,v.base_layer,
            d->type==RHI_IMAGE_3D?1:v.layer_count);
        glBindTexture(target,view->texture);
        if(d->samples==1)texture_parameters(target,d->format,v.mip_count);
        if(stencil)glTexParameteri(target,GL_DEPTH_STENCIL_TEXTURE_MODE,GL_STENCIL_INDEX);
        if(!gl_ok("sampled image view")){g_view_destroy(view);return NULL;}
    }
    return view;
}
static void g_view_destroy(RhiView *v)
{if(v && !--v->refs){if(v->own_texture)glDeleteTextures(1,&v->texture);g_image_destroy(v->image);free(v);}}
static RhiImage *g_view_image(const RhiView *v){return v?v->image:NULL;}
static void attach(GLenum framebuffer,GLenum attachment,RhiImage *image,unsigned mip,unsigned layer)
{
    if(!image){G.FramebufferTexture2D(framebuffer,attachment,GL_TEXTURE_2D,0,0);return;}
    if(image->target==GL_TEXTURE_3D || image->target==GL_TEXTURE_2D_ARRAY || image->target==GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
        G.FramebufferTextureLayer(framebuffer,attachment,image->id,mip,layer);
    else G.FramebufferTexture2D(framebuffer,attachment,image->desc.cube?GL_TEXTURE_CUBE_MAP_POSITIVE_X+layer:image->target,image->id,mip);
}
static void attach_view(GLenum target,GLenum attachment,RhiView *view)
{attach(target,attachment,view?view->image:NULL,view?view->desc.base_mip:0,view?view->desc.base_layer:0);}
static int work_framebuffer(GLuint framebuffer,RhiImage *image,unsigned sub,int depth)
{
    G.BindFramebuffer(GL_FRAMEBUFFER,framebuffer);
    attach(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,NULL,0,0);attach(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,NULL,0,0);
    GlFormat f=format(image->desc.format);
    attach(GL_FRAMEBUFFER,depth?(f.stencil?GL_DEPTH_STENCIL_ATTACHMENT:GL_DEPTH_ATTACHMENT):GL_COLOR_ATTACHMENT0,
        image,sub%image->desc.mip_levels,sub/image->desc.mip_levels);
    glDrawBuffer(depth?GL_NONE:GL_COLOR_ATTACHMENT0);glReadBuffer(depth?GL_NONE:GL_COLOR_ATTACHMENT0);
    return G.CheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
}
static int g_resolve(RhiImage *source,uint32_t src,RhiImage *dest,uint32_t dst)
{
    if(!source || !dest || source==dest || source->desc.format!=dest->desc.format || dest->desc.samples!=1 ||
       source->desc.type!=RHI_IMAGE_2D || dest->desc.type!=RHI_IMAGE_2D ||
       src>=source->desc.mip_levels*source->desc.depth || dst>=dest->desc.mip_levels*dest->desc.depth)return -1;
    unsigned w=level_size(source->desc.width,src%source->desc.mip_levels),h=level_size(source->desc.height,src%source->desc.mip_levels);
    if(w!=level_size(dest->desc.width,dst%dest->desc.mip_levels) || h!=level_size(dest->desc.height,dst%dest->desc.mip_levels))return -1;
    GlFormat f=format(source->desc.format);if(f.block)return -1;
    int complete=work_framebuffer(S.readbuffer,source,src,f.depth) && work_framebuffer(S.writebuffer,dest,dst,f.depth);
    if(complete){G.BindFramebuffer(GL_READ_FRAMEBUFFER,S.readbuffer);G.BindFramebuffer(GL_DRAW_FRAMEBUFFER,S.writebuffer);
        glDisable(GL_SCISSOR_TEST);G.BlitFramebuffer(0,0,w,h,0,0,w,h,f.depth?(GL_DEPTH_BUFFER_BIT|(f.stencil?GL_STENCIL_BUFFER_BIT:0)):GL_COLOR_BUFFER_BIT,GL_NEAREST);}
    G.BindFramebuffer(GL_FRAMEBUFFER,S.framebuffer);return complete && gl_ok("image resolve")?0:-1;
}
static int g_image_readback(RhiImage *image,uint32_t sub,void *dst,uint32_t pitch)
{
    if(!image || !dst || image->desc.type!=RHI_IMAGE_2D || sub>=image->desc.mip_levels*image->desc.depth)return -1;
    unsigned mip=sub%image->desc.mip_levels,w=level_size(image->desc.width,mip),h=level_size(image->desc.height,mip);
    GlFormat f=format(image->desc.format);unsigned row=rhi_format_row_pitch(image->desc.format,w),rows=rhi_format_rows(image->desc.format,h);
    if(pitch<row)return -1;
    if(image->desc.samples>1) {
        RhiImageDesc d=image->desc;d.width=w;d.height=h;d.depth=d.mip_levels=d.samples=1;d.cube=0;
        RhiImage *single=g_image_create(&d,NULL);if(!single)return -1;
        int result=g_resolve(image,sub,single,0);if(!result)result=g_image_readback(single,0,dst,pitch);
        g_image_destroy(single);return result;
    }
    unsigned layers=f.block && image->target==GL_TEXTURE_2D_ARRAY?image->desc.depth:1;
    if((uint64_t)row*rows*layers>512u*1024*1024)return -1;
    unsigned char *pixels=malloc((size_t)row*rows*layers);if(!pixels)return -1;
    glPixelStorei(GL_PACK_ALIGNMENT,1);glPixelStorei(GL_PACK_ROW_LENGTH,0);
    int complete=1;
    if(f.block) {glBindTexture(image->target,image->id);G.GetCompressedTexImage(image->desc.cube?GL_TEXTURE_CUBE_MAP_POSITIVE_X+sub/image->desc.mip_levels:image->target,mip,pixels);}
    else {complete=work_framebuffer(S.readbuffer,image,sub,f.depth);if(complete)glReadPixels(0,0,w,h,f.format,f.type,pixels);}
    /* The RHI readback door completes all preceding work, including queries
     * whose draws had color writes disabled and did not touch these pixels. */
    if(complete)glFinish();
    if(complete && image->desc.format==RHI_FORMAT_D24_UNORM_S8_UINT)for(size_t i=0;i<(size_t)w*h;i++) {
        uint32_t value;memcpy(&value,pixels+i*4,4);value=(value>>8)|(value<<24);memcpy(pixels+i*4,&value,4);
    }
    size_t base=layers>1?(size_t)(sub/image->desc.mip_levels)*row*rows:0;
    if(complete)for(unsigned y=0;y<rows;y++)memcpy((char *)dst+(size_t)y*pitch,pixels+base+(size_t)y*row,row);
    free(pixels);G.BindFramebuffer(GL_FRAMEBUFFER,S.framebuffer);
    return complete && gl_ok("image readback")?0:-1;
}

static void retain_view(RhiView **slot,RhiView *value)
{if(value)value->refs++;g_view_destroy(*slot);*slot=value;}
static void retain_buffer(RhiBuffer **slot,RhiBuffer *value)
{if(value)value->refs++;g_buffer_destroy(*slot);*slot=value;}
static void bind_output(void)
{
    G.BindFramebuffer(GL_FRAMEBUFFER,S.framebuffer);
    attach_view(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,S.color);
    attach_view(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,NULL);
    if(S.depth)attach_view(GL_FRAMEBUFFER,format(S.depth->image->desc.format).stencil?GL_DEPTH_STENCIL_ATTACHMENT:GL_DEPTH_ATTACHMENT,S.depth);
    glDrawBuffer(S.color?GL_COLOR_ATTACHMENT0:GL_NONE);glReadBuffer(S.color?GL_COLOR_ATTACHMENT0:GL_NONE);
}
static void g_set_render_target(RhiView *color,RhiView *depth)
{
    retain_view(&S.color,color);retain_view(&S.depth,depth);bind_output();
    if((color || depth) && G.CheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr,"[RHI] ERROR: opengl incomplete render target\n");S.failed=1;
    }
}
static void g_output_save(RhiOutputState *state)
{
    if(!state)return;memset(state,0,sizeof *state);state->color=S.color;state->depth=S.depth;
    if(S.color)S.color->refs++;if(S.depth)S.depth->refs++;
    state->viewport_count=S.viewport_count;memcpy(state->viewports,S.viewports,sizeof S.viewports);
}
static void g_output_restore(RhiOutputState *state)
{
    if(!state)return;g_set_render_target(state->color,state->depth);
    S.viewport_count=state->viewport_count;memcpy(S.viewports,state->viewports,sizeof S.viewports);
    g_view_destroy(state->color);g_view_destroy(state->depth);memset(state,0,sizeof *state);
}
static int g_output_color_is(const RhiOutputState *state,const RhiView *view)
{return state && state->color==view;}
static void g_viewports(uint32_t count,const RhiViewport *viewports)
{if(count<=16 && (viewports || !count)){S.viewport_count=count;if(count)memcpy(S.viewports,viewports,count*sizeof *viewports);}}
static void g_scissor(const RhiRect *rect){if(rect)S.scissor=*rect;}
static void g_topology(uint32_t topology){S.topology=topology;}
static RhiShader *g_shader_create(uint32_t stage,const RhiShaderSource *src,char *error,size_t size)
{
    RhiShader *shader=calloc(1,sizeof *shader);if(!shader)return NULL;
    shader->refs=1;shader->stage=stage;
    if(!tsfp_gl_shader_translate(stage,src,&shader->text,error,size)){free(shader);return NULL;}
    shader->id=G.CreateShader(stage==RHI_STAGE_VERTEX?GL_VERTEX_SHADER:GL_FRAGMENT_SHADER);
    G.ShaderSource(shader->id,1,(const GLchar *const *)&shader->text.source,NULL);G.CompileShader(shader->id);
    GLint compiled=0;G.GetShaderiv(shader->id,GL_COMPILE_STATUS,&compiled);
    if(!compiled){if(error && size)G.GetShaderInfoLog(shader->id,(GLsizei)size,NULL,error);g_shader_destroy(shader);return NULL;}
    return shader;
}
static void g_shader_destroy(RhiShader *shader)
{
    if(!shader || --shader->refs)return;
    GlProgram **next=&S.programs;
    while(*next) {
        GlProgram *p=*next;
        if(p->vs==shader->id || p->ps==shader->id) {
            if(S.pipeline==p)S.pipeline=NULL;
            if(S.current_program==p->id){G.UseProgram(0);S.current_program=0;}
            G.DeleteProgram(p->id);*next=p->next;free(p);
        } else next=&p->next;
    }
    if(shader->id)G.DeleteShader(shader->id);tsfp_gl_shader_text_free(&shader->text);free(shader);
}
static void g_set_shader(uint32_t stage,RhiShader *shader)
{
    if(stage>1 || (shader && shader->stage!=stage))return;
    if(shader)shader->refs++;g_shader_destroy(S.shaders[stage]);S.shaders[stage]=shader;
}
static int semantic_equal(const char *hlsl,const char *name,unsigned index)
{
    char left[80],right[80];snprintf(left,sizeof left,"%s",hlsl);snprintf(right,sizeof right,"%s%u",name,index);
    size_t n=strlen(left);if(n && (left[n-1]<'0' || left[n-1]>'9') && n+1<sizeof left){left[n]='0';left[n+1]=0;}
    for(unsigned i=0;left[i];i++)if(left[i]>='a' && left[i]<='z')left[i]-=32;
    for(unsigned i=0;right[i];i++)if(right[i]>='a' && right[i]<='z')right[i]-=32;
    return !strcmp(left,right);
}
static RhiVertexLayout *g_layout_create(const RhiVertexElement *elements,uint32_t count,const RhiShader *vs)
{
    if(!vs || vs->stage!=RHI_STAGE_VERTEX || count>32 || (count && !elements))return NULL;
    RhiVertexLayout *layout=calloc(1,sizeof *layout);if(!layout)return NULL;layout->refs=1;
    unsigned offsets[16]={0};GLint limit=0;glGetIntegerv(GL_MAX_VERTEX_ATTRIBS,&limit);
    for(unsigned i=0;i<count;i++) {
        const RhiVertexElement *e=&elements[i];GlFormat f=format(e->format);
        if(e->slot>=16 || !f.bytes || f.depth || !e->semantic){free(layout);return NULL;}
        unsigned offset=e->offset==RHI_APPEND?offsets[e->slot]:e->offset;offsets[e->slot]=offset+f.bytes;
        for(unsigned j=0;j<vs->text.input_count;j++)if(semantic_equal(vs->text.inputs[j].semantic,e->semantic,e->semantic_index)) {
            if(vs->text.inputs[j].location>=(unsigned)limit){free(layout);return NULL;}
            GlAttribute *a=&layout->attributes[layout->count++];a->location=vs->text.inputs[j].location;
            a->slot=e->slot;a->offset=offset;a->format=f;
            a->components=f.format==GL_RED || f.format==GL_RED_INTEGER?1:f.format==GL_RG?2:f.format==GL_RGB?3:4;
        }
    }
    return layout;
}
static void g_layout_destroy(RhiVertexLayout *layout){if(layout && !--layout->refs)free(layout);}
static void g_set_layout(RhiVertexLayout *layout)
{if(layout)layout->refs++;g_layout_destroy(S.layout);S.layout=layout;}
static void g_vertex_buffer(uint32_t slot,RhiBuffer *buffer,uint32_t stride,uint32_t offset)
{if(slot<16){retain_buffer(&S.vertices[slot],buffer);S.strides[slot]=stride;S.offsets[slot]=offset;}}
static void g_index_buffer(RhiBuffer *buffer,uint32_t bits,uint32_t offset)
{retain_buffer(&S.indices,buffer);S.index_bits=bits;S.index_offset=offset;}
static void g_uniforms(uint32_t stage,uint32_t slot,uint32_t count,RhiBuffer *const *buffers)
{if(stage<2 && slot<=8 && count<=8-slot)for(unsigned i=0;i<count;i++)retain_buffer(&S.uniforms[stage*8+slot+i],buffers?buffers[i]:NULL);}
static void g_textures(uint32_t slot,uint32_t count,RhiView *const *views)
{if(slot<=16 && count<=16-slot)for(unsigned i=0;i<count;i++)retain_view(&S.textures[slot+i],views?views[i]:NULL);}
static GLenum address(unsigned value)
{switch(value){case RHI_ADDRESS_WRAP:return GL_REPEAT;case RHI_ADDRESS_MIRROR:return GL_MIRRORED_REPEAT;
    case RHI_ADDRESS_BORDER:return GL_CLAMP_TO_BORDER;case RHI_ADDRESS_MIRROR_ONCE:return GL_MIRROR_CLAMP_TO_EDGE;default:return GL_CLAMP_TO_EDGE;}}
static GLenum comparison(unsigned value)
{static const GLenum values[]={GL_NEVER,GL_NEVER,GL_LESS,GL_EQUAL,GL_LEQUAL,GL_GREATER,GL_NOTEQUAL,GL_GEQUAL,GL_ALWAYS};return value<9?values[value]:GL_ALWAYS;}
static RhiSampler *g_sampler_create(const RhiSamplerDesc *desc)
{
    if(!desc)return NULL;RhiSampler *sampler=calloc(1,sizeof *sampler);if(!sampler)return NULL;sampler->refs=1;
    unsigned filter=desc->filter;GLint min=(filter&0x10)?((filter&1)?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR_MIPMAP_NEAREST):
        ((filter&1)?GL_NEAREST_MIPMAP_LINEAR:GL_NEAREST_MIPMAP_NEAREST);
    G.GenSamplers(1,&sampler->id);G.SamplerParameteri(sampler->id,GL_TEXTURE_MIN_FILTER,min);
    G.SamplerParameteri(sampler->id,GL_TEXTURE_MAG_FILTER,(filter&4)?GL_LINEAR:GL_NEAREST);
    G.SamplerParameteri(sampler->id,GL_TEXTURE_WRAP_S,address(desc->address_u));
    G.SamplerParameteri(sampler->id,GL_TEXTURE_WRAP_T,address(desc->address_v));
    G.SamplerParameteri(sampler->id,GL_TEXTURE_WRAP_R,address(desc->address_w));
    G.SamplerParameterf(sampler->id,GL_TEXTURE_LOD_BIAS,desc->mip_lod_bias);
    G.SamplerParameterf(sampler->id,GL_TEXTURE_MIN_LOD,desc->min_lod);
    G.SamplerParameterf(sampler->id,GL_TEXTURE_MAX_LOD,desc->max_lod);
    G.SamplerParameterfv(sampler->id,GL_TEXTURE_BORDER_COLOR,desc->border);
    G.SamplerParameteri(sampler->id,GL_TEXTURE_COMPARE_MODE,(filter&0x80)?GL_COMPARE_REF_TO_TEXTURE:GL_NONE);
    G.SamplerParameteri(sampler->id,GL_TEXTURE_COMPARE_FUNC,comparison(desc->compare));
    if(extension("GL_EXT_texture_filter_anisotropic")) {
        GLfloat limit=1;glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,&limit);
        G.SamplerParameterf(sampler->id,GL_TEXTURE_MAX_ANISOTROPY_EXT,(filter&0x40)?fminf(limit,fmaxf(1,(float)desc->max_anisotropy)):1);
    }
    if(!gl_ok("sampler creation")){g_sampler_destroy(sampler);return NULL;}return sampler;
}
static void g_sampler_destroy(RhiSampler *sampler){if(sampler && !--sampler->refs){G.DeleteSamplers(1,&sampler->id);free(sampler);}}
static void g_samplers(uint32_t slot,uint32_t count,RhiSampler *const *samplers)
{if(slot<=16 && count<=16-slot)for(unsigned i=0;i<count;i++){RhiSampler *s=samplers?samplers[i]:NULL;if(s)s->refs++;g_sampler_destroy(S.samplers[slot+i]);S.samplers[slot+i]=s;}}
#define STATE_CREATE(name,type,description) static type *name(const description *d){if(!d)return NULL;type *s=malloc(sizeof *s);if(s)s->desc=*d;return s;}
STATE_CREATE(g_blend_create,RhiBlendState,RhiBlendDesc)
STATE_CREATE(g_depth_create,RhiDepthState,RhiDepthDesc)
STATE_CREATE(g_raster_create,RhiRasterState,RhiRasterDesc)
#undef STATE_CREATE
static void g_blend_destroy(RhiBlendState *state){free(state);}
static void g_depth_destroy(RhiDepthState *state){free(state);}
static void g_raster_destroy(RhiRasterState *state){free(state);}
static void g_blend(RhiBlendState *state,const float factor[4],uint32_t mask)
{if(state)S.blend=state->desc;else S.blend=(RhiBlendDesc){.src=RHI_BLEND_ONE,.dst=RHI_BLEND_ZERO,.op=RHI_BLEND_OP_ADD,.src_alpha=RHI_BLEND_ONE,.dst_alpha=RHI_BLEND_ZERO,.op_alpha=RHI_BLEND_OP_ADD,.write_mask=RHI_WRITE_ALL};
    if(factor)memcpy(S.factor,factor,sizeof S.factor);else for(unsigned i=0;i<4;i++)S.factor[i]=1;S.mask=mask;}
static void g_depth(RhiDepthState *state,uint32_t reference)
{if(state)S.depth_state=state->desc;else S.depth_state=(RhiDepthDesc){.depth_enable=1,.depth_write=1,.depth_func=RHI_CMP_LESS};S.stencil_ref=reference;}
static void g_raster(RhiRasterState *state)
{if(state)S.raster=state->desc;else S.raster=(RhiRasterDesc){.fill=RHI_FILL_SOLID,.cull=RHI_CULL_BACK,.depth_clip=1};}
static GLenum blend_factor(unsigned value,int alpha)
{
    switch(value) {case RHI_BLEND_ZERO:return GL_ZERO;case RHI_BLEND_ONE:return GL_ONE;
    case RHI_BLEND_SRC_COLOR:return GL_SRC_COLOR;case RHI_BLEND_INV_SRC_COLOR:return GL_ONE_MINUS_SRC_COLOR;
    case RHI_BLEND_SRC_ALPHA:return GL_SRC_ALPHA;case RHI_BLEND_INV_SRC_ALPHA:return GL_ONE_MINUS_SRC_ALPHA;
    case RHI_BLEND_DEST_ALPHA:return GL_DST_ALPHA;case RHI_BLEND_INV_DEST_ALPHA:return GL_ONE_MINUS_DST_ALPHA;
    case RHI_BLEND_DEST_COLOR:return GL_DST_COLOR;case RHI_BLEND_INV_DEST_COLOR:return GL_ONE_MINUS_DST_COLOR;
    case RHI_BLEND_SRC_ALPHA_SAT:return GL_SRC_ALPHA_SATURATE;
    case RHI_BLEND_FACTOR:return alpha?GL_CONSTANT_ALPHA:GL_CONSTANT_COLOR;
    case RHI_BLEND_INV_FACTOR:return alpha?GL_ONE_MINUS_CONSTANT_ALPHA:GL_ONE_MINUS_CONSTANT_COLOR;
    default:return GL_ONE;}
}
static GLenum blend_op(unsigned value)
{switch(value){case RHI_BLEND_OP_SUBTRACT:return GL_FUNC_SUBTRACT;case RHI_BLEND_OP_REV_SUBTRACT:return GL_FUNC_REVERSE_SUBTRACT;case RHI_BLEND_OP_MIN:return GL_MIN;case RHI_BLEND_OP_MAX:return GL_MAX;default:return GL_FUNC_ADD;}}
static GLenum stencil_op(unsigned value)
{switch(value){case RHI_STENCIL_ZERO:return GL_ZERO;case RHI_STENCIL_REPLACE:return GL_REPLACE;case RHI_STENCIL_INCR_SAT:return GL_INCR;
    case RHI_STENCIL_DECR_SAT:return GL_DECR;case RHI_STENCIL_INVERT:return GL_INVERT;case RHI_STENCIL_INCR:return GL_INCR_WRAP;case RHI_STENCIL_DECR:return GL_DECR_WRAP;default:return GL_KEEP;}}
static void toggle(GLenum cap,int enable){if(enable)glEnable(cap);else glDisable(cap);}
static void apply_state(void)
{
    RhiBlendDesc *b=&S.blend;RhiDepthDesc *d=&S.depth_state;RhiRasterDesc *r=&S.raster;
    toggle(GL_BLEND,b->enable);G.BlendFuncSeparate(blend_factor(b->src,0),blend_factor(b->dst,0),blend_factor(b->src_alpha,1),blend_factor(b->dst_alpha,1));
    G.BlendEquationSeparate(blend_op(b->op),blend_op(b->op_alpha));G.BlendColor(S.factor[0],S.factor[1],S.factor[2],S.factor[3]);
    glColorMask(!!(b->write_mask&1),!!(b->write_mask&2),!!(b->write_mask&4),!!(b->write_mask&8));
    toggle(GL_SAMPLE_ALPHA_TO_COVERAGE,b->alpha_to_coverage);toggle(GL_SAMPLE_MASK,S.mask!=~0u);G.SampleMaski(0,S.mask);
    toggle(GL_DEPTH_TEST,d->depth_enable);glDepthMask(d->depth_write?GL_TRUE:GL_FALSE);glDepthFunc(comparison(d->depth_func));
    toggle(GL_STENCIL_TEST,d->stencil_enable);
    for(unsigned i=0;i<2;i++){RhiStencilFace *s=i?&d->back:&d->front;GLenum face=i?GL_BACK:GL_FRONT;
        G.StencilFuncSeparate(face,comparison(s->func),S.stencil_ref,d->stencil_read_mask);
        G.StencilOpSeparate(face,stencil_op(s->fail),stencil_op(s->depth_fail),stencil_op(s->pass));G.StencilMaskSeparate(face,d->stencil_write_mask);}
    toggle(GL_CULL_FACE,r->cull!=RHI_CULL_NONE);glCullFace(r->cull==RHI_CULL_FRONT?GL_FRONT:GL_BACK);
    /* The shader flips clip Y for logical top-down offscreen rows. GL facing
     * is evaluated before the final screen flip, so its winding reverses. */
    glFrontFace(r->front_ccw?GL_CW:GL_CCW);glPolygonMode(GL_FRONT_AND_BACK,r->fill==RHI_FILL_WIREFRAME?GL_LINE:GL_FILL);
    toggle(GL_DEPTH_CLAMP,!r->depth_clip);toggle(GL_POLYGON_OFFSET_FILL,r->depth_bias!=0 || r->slope_scaled_depth_bias!=0);
    glPolygonOffset(r->slope_scaled_depth_bias,(float)r->depth_bias);
    toggle(GL_SCISSOR_TEST,r->scissor);if(r->scissor)glScissor(S.scissor.left,S.scissor.top,
        S.scissor.right>S.scissor.left?S.scissor.right-S.scissor.left:0,S.scissor.bottom>S.scissor.top?S.scissor.bottom-S.scissor.top:0);
    if(S.viewport_count){GLfloat boxes[16*4];GLdouble ranges[16*2];
        for(unsigned i=0;i<S.viewport_count;i++){RhiViewport *v=&S.viewports[i];boxes[i*4]=v->x;boxes[i*4+1]=v->y;boxes[i*4+2]=v->width;boxes[i*4+3]=v->height;ranges[i*2]=v->min_depth;ranges[i*2+1]=v->max_depth;}
        G.ViewportArrayv(0,S.viewport_count,boxes);G.DepthRangeArrayv(0,S.viewport_count,ranges);}
}
static GLuint program(void)
{
    if(!S.shaders[0] || !S.shaders[1])return 0;
    for(GlProgram *p=S.programs;p;p=p->next)if(p->vs==S.shaders[0]->id && p->ps==S.shaders[1]->id){S.pipeline=p;return p->id;}
    GlProgram *p=calloc(1,sizeof *p);if(!p)return 0;p->id=G.CreateProgram();p->vs=S.shaders[0]->id;p->ps=S.shaders[1]->id;
    G.AttachShader(p->id,p->vs);G.AttachShader(p->id,p->ps);G.LinkProgram(p->id);GLint linked=0;G.GetProgramiv(p->id,GL_LINK_STATUS,&linked);
    if(!linked){char error[4096]={0};G.GetProgramInfoLog(p->id,sizeof error,NULL,error);fprintf(stderr,"[RHI] ERROR: opengl shader link: %s\nVS:\n%s\nPS:\n%s\n",error,S.shaders[0]->text.source,S.shaders[1]->text.source);G.DeleteProgram(p->id);free(p);S.failed=1;return 0;}
    G.UseProgram(p->id);S.current_program=p->id;
    for(unsigned i=0;i<32;i++)p->samples[i]=-1;
    for(unsigned stage=0;stage<2;stage++){
        TsfpGlShaderText *text=&S.shaders[stage]->text;
        for(unsigned i=0;i<text->block_count;i++){TsfpGlBlock *block=&text->blocks[i];GLuint index=G.GetUniformBlockIndex(p->id,block->name);if(index!=GL_INVALID_INDEX)G.UniformBlockBinding(p->id,index,block->binding);}
        for(unsigned i=0;i<text->sampler_count;i++){TsfpGlSampler *sampler=&text->samplers[i];GLint loc=G.GetUniformLocation(p->id,sampler->name);if(loc>=0)G.Uniform1i(loc,sampler->unit);
            char name[64];snprintf(name,sizeof name,"tsfp_samples_%u",sampler->unit);p->samples[sampler->unit]=G.GetUniformLocation(p->id,name);}
    }
    p->next=S.programs;S.programs=p;S.pipeline=p;return p->id;
}
static int prepare_draw(void)
{
    if(!S.ready || S.failed || (!S.color && !S.depth))return 0;
    GLuint id=program();if(!id)return 0;if(S.current_program!=id){G.UseProgram(id);S.current_program=id;}
    apply_state();G.BindVertexArray(S.vao);
    GLint max=0;glGetIntegerv(GL_MAX_VERTEX_ATTRIBS,&max);for(GLint i=0;i<max;i++)G.DisableVertexAttribArray(i);
    if(S.layout)for(unsigned i=0;i<S.layout->count;i++) {
        GlAttribute *a=&S.layout->attributes[i];RhiBuffer *buffer=S.vertices[a->slot];if(!buffer)continue;
        G.BindBuffer(GL_ARRAY_BUFFER,buffer->id);const void *offset=(const void *)(uintptr_t)(S.offsets[a->slot]+a->offset);
        if(a->format.integer)G.VertexAttribIPointer(a->location,a->components,a->format.type,S.strides[a->slot],offset);
        else G.VertexAttribPointer(a->location,a->components,a->format.type,a->format.type!=GL_FLOAT && a->format.type!=GL_HALF_FLOAT,S.strides[a->slot],offset);
        G.EnableVertexAttribArray(a->location);
    }
    G.BindBuffer(GL_ELEMENT_ARRAY_BUFFER,S.indices?S.indices->id:0);
    for(unsigned i=0;i<16;i++)G.BindBufferBase(GL_UNIFORM_BUFFER,i,S.uniforms[i]?S.uniforms[i]->id:0);
    for(unsigned stage=0;stage<2;stage++)for(unsigned i=0;i<S.shaders[stage]->text.sampler_count;i++) {
        TsfpGlSampler *mapping=&S.shaders[stage]->text.samplers[i];RhiView *view=S.textures[mapping->texture_slot];
        RhiSampler *sampler=mapping->sampler_slot<16?S.samplers[mapping->sampler_slot]:NULL;
        G.ActiveTexture(GL_TEXTURE0+mapping->unit);glBindTexture(view?view->target:GL_TEXTURE_2D,view?view->texture:S.zero_texture);
        G.BindSampler(mapping->unit,sampler?sampler->id:0);
        if(S.pipeline->samples[mapping->unit]>=0)G.Uniform1i(S.pipeline->samples[mapping->unit],view?view->image->desc.samples:1);
    }
    G.ActiveTexture(GL_TEXTURE0);return gl_ok("draw bindings");
}
static GLenum topology(void)
{switch(S.topology){case RHI_TOPOLOGY_POINTS:return GL_POINTS;case RHI_TOPOLOGY_LINES:return GL_LINES;case RHI_TOPOLOGY_LINE_STRIP:return GL_LINE_STRIP;case RHI_TOPOLOGY_TRIANGLE_STRIP:return GL_TRIANGLE_STRIP;default:return GL_TRIANGLES;}}
static void g_draw(uint32_t count,uint32_t first)
{if(prepare_draw()){glDrawArrays(topology(),first,count);gl_ok("draw");}}
static void g_draw_indexed(uint32_t count,uint32_t first,int32_t base)
{if(S.indices && prepare_draw()){G.DrawElementsBaseVertex(topology(),count,S.index_bits==32?GL_UNSIGNED_INT:GL_UNSIGNED_SHORT,
    (const void *)(uintptr_t)(S.index_offset+(size_t)first*(S.index_bits==32?4:2)),base);gl_ok("indexed draw");}}
static void g_clear_color(RhiView *view,const float rgba[4])
{
    if(!view || !rgba)return;GlFormat f=format(view->image->desc.format);
    if(work_framebuffer(S.writebuffer,view->image,view->desc.base_layer*view->image->desc.mip_levels+view->desc.base_mip,0)) {
        glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        if(f.integer){GLint value[4];for(unsigned i=0;i<4;i++)value[i]=(GLint)rgba[i];if(f.type==GL_INT)G.ClearBufferiv(GL_COLOR,0,value);else G.ClearBufferuiv(GL_COLOR,0,(GLuint *)value);}
        else G.ClearBufferfv(GL_COLOR,0,rgba);
    }
    G.BindFramebuffer(GL_FRAMEBUFFER,S.framebuffer);gl_ok("color clear");
}
static void g_clear_depth(RhiView *view,uint32_t flags,float depth,uint8_t stencil)
{
    if(!view)return;
    if(work_framebuffer(S.writebuffer,view->image,view->desc.base_layer*view->image->desc.mip_levels+view->desc.base_mip,1)) {
        glDisable(GL_SCISSOR_TEST);glDepthMask(GL_TRUE);glStencilMask(255);glClearDepth(depth);glClearStencil(stencil);
        glClear(((flags&RHI_CLEAR_DEPTH)?GL_DEPTH_BUFFER_BIT:0)|((flags&RHI_CLEAR_STENCIL)?GL_STENCIL_BUFFER_BIT:0));
    }
    G.BindFramebuffer(GL_FRAMEBUFFER,S.framebuffer);gl_ok("depth clear");
}
static RhiOcclusion *g_query_create(void)
{RhiOcclusion *q=calloc(1,sizeof *q);if(q)G.GenQueries(1,&q->id);return q;}
static void g_query_destroy(RhiOcclusion *q)
{if(q){if(S.query==q){G.EndQuery(GL_SAMPLES_PASSED);S.query=NULL;}G.DeleteQueries(1,&q->id);free(q);}}
static int g_query_begin(RhiOcclusion *q)
{if(!q || S.query)return 0;G.BeginQuery(GL_SAMPLES_PASSED,q->id);q->active=1;q->ended=0;S.query=q;return gl_ok("query begin");}
static int g_query_end(RhiOcclusion *q)
{if(!q || S.query!=q)return 0;G.EndQuery(GL_SAMPLES_PASSED);S.query=NULL;q->active=0;q->ended=1;glFlush();return gl_ok("query end");}
static int g_query_result(RhiOcclusion *q,uint64_t *samples)
{if(!q || !q->ended || !samples)return -1;GLuint ready=0;G.GetQueryObjectuiv(q->id,GL_QUERY_RESULT_AVAILABLE,&ready);if(!ready)return 0;
    GLuint64 count=0;G.GetQueryObjectui64v(q->id,GL_QUERY_RESULT,&count);if(!gl_ok("query result"))return -1;*samples=count;return 1;}

static int g_resize(uint32_t width,uint32_t height)
{
    if(!width || !height)return -1;
    RhiImageDesc desc={0};desc.width=width;desc.height=height;desc.depth=desc.mip_levels=desc.samples=1;
    desc.format=RHI_FORMAT_R8G8B8A8_UNORM;desc.bind=RHI_BIND_RENDER_TARGET|RHI_BIND_SAMPLED;
    RhiImage *image=g_image_create(&desc,NULL);if(!image)return -1;
    RhiView *view=g_view_create(image,RHI_VIEW_RENDER_TARGET,NULL);if(!view){g_image_destroy(image);return -1;}
    int bound=S.color==S.swap_view && S.swap_view;
    if(bound)g_set_render_target(view,S.depth);
    g_view_destroy(S.swap_view);g_image_destroy(S.swap_image);S.swap_image=image;S.swap_view=view;
    return 0;
}
static int32_t g_present(uint32_t interval)
{
    if(!S.ready || S.failed || !S.swap_image)return -1;
    unsigned w=S.swap_image->desc.width,h=S.swap_image->desc.height;
    if(!work_framebuffer(S.readbuffer,S.swap_image,0,0))return -1;
    G.BindFramebuffer(GL_READ_FRAMEBUFFER,S.readbuffer);G.BindFramebuffer(GL_DRAW_FRAMEBUFFER,0);glDrawBuffer(GL_BACK);
    glDisable(GL_SCISSOR_TEST);G.BlitFramebuffer(0,0,w,h,0,h,w,0,GL_COLOR_BUFFER_BIT,GL_NEAREST);
    int ok=gl_ok("present blit");int result=ok?tsfp_gl_host_present(S.host,interval):-1;
    G.BindFramebuffer(GL_FRAMEBUFFER,S.framebuffer);return result;
}
static int g_swap_readback(void *dst,uint32_t pitch,uint32_t *w,uint32_t *h)
{if(!S.swap_image || !w || !h)return -1;*w=S.swap_image->desc.width;*h=S.swap_image->desc.height;return g_image_readback(S.swap_image,0,dst,pitch);}
static RhiView *g_swap_view(void){return S.swap_view;}
static int g_ready(void){return S.ready;}
static void g_vrr(int on){(void)on;/* Swap interval is applied by the host at present. */}
static int g_device_create(const RhiDeviceDesc *desc)
{
    if(!desc || S.host)return -1;
    if(!rhi_vk_dxc_available()){fprintf(stderr,"[RHI] opengl: shared DXC shader compiler is unavailable\n");return -1;}
    memset(&S,0,sizeof S);memset(&G,0,sizeof G);S.host=tsfp_gl_host_create(desc->window);if(!S.host)return -1;
#define LOAD(type,name) G.name=(type)tsfp_gl_host_proc(S.host,"gl" #name);if(!G.name){fprintf(stderr,"[RHI] opengl: missing gl" #name "\n");g_device_destroy();return -1;}
    TSFP_GL_FUNCTIONS(LOAD)
#undef LOAD
#define LOAD_OPTIONAL(type,name) G.name=(type)tsfp_gl_host_proc(S.host,"gl" #name);
    TSFP_GL_OPTIONAL(LOAD_OPTIONAL)
#undef LOAD_OPTIONAL
    if(G.DebugMessageCallback && extension("GL_KHR_debug")) {
        glEnable(GL_DEBUG_OUTPUT);glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        G.DebugMessageCallback(debug_message,NULL);
    }
    S.storage=G.TexStorage2D && G.TexStorage3D && extension("GL_ARB_texture_storage");
    S.storage_ms=G.TexStorage2DMultisample && G.TexStorage3DMultisample && extension("GL_ARB_texture_storage_multisample");
    S.views=G.TextureView && extension("GL_ARB_texture_view");S.stencil_texture=extension("GL_ARB_stencil_texturing");
    G.GenFramebuffers(1,&S.framebuffer);G.GenFramebuffers(1,&S.readbuffer);G.GenFramebuffers(1,&S.writebuffer);
    G.GenVertexArrays(1,&S.vao);G.BindVertexArray(S.vao);
    glGenTextures(1,&S.zero_texture);glBindTexture(GL_TEXTURE_2D,S.zero_texture);unsigned char zero[4]={0};
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,zero);texture_parameters(GL_TEXTURE_2D,RHI_FORMAT_R8G8B8A8_UNORM,1);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);glEnable(GL_MULTISAMPLE);glEnable(GL_PROGRAM_POINT_SIZE);glDisable(GL_DITHER);glDisable(GL_FRAMEBUFFER_SRGB);
    g_blend(NULL,NULL,~0u);g_depth(NULL,0);g_raster(NULL);S.topology=RHI_TOPOLOGY_TRIANGLES;
    if(g_resize(desc->width,desc->height)!=0 || !gl_ok("device initialization")){
        fprintf(stderr,"[RHI] opengl: drawable target initialization failed\n");g_device_destroy();return -1;}
    S.ready=1;fprintf(stderr,"[RHI] opengl: shared Xbox frontend, core4.1, texture views %s\n",S.views?"available":"unavailable");return 0;
}
static void g_device_destroy(void)
{
    if(!S.host){memset(&S,0,sizeof S);return;}
    if(S.framebuffer) {
        g_set_render_target(NULL,NULL);g_set_shader(0,NULL);g_set_shader(1,NULL);g_set_layout(NULL);g_index_buffer(NULL,16,0);
        for(unsigned i=0;i<16;i++) {
            retain_view(&S.textures[i],NULL);retain_buffer(&S.vertices[i],NULL);retain_buffer(&S.uniforms[i],NULL);
            g_sampler_destroy(S.samplers[i]);S.samplers[i]=NULL;
        }
        g_view_destroy(S.swap_view);g_image_destroy(S.swap_image);
        while(S.programs){GlProgram *p=S.programs;S.programs=p->next;G.DeleteProgram(p->id);free(p);}
        G.DeleteFramebuffers(1,&S.framebuffer);G.DeleteFramebuffers(1,&S.readbuffer);G.DeleteFramebuffers(1,&S.writebuffer);
        G.DeleteVertexArrays(1,&S.vao);glDeleteTextures(1,&S.zero_texture);
    }
    tsfp_gl_host_destroy(S.host);memset(&S,0,sizeof S);memset(&G,0,sizeof G);
}
/* The title can create its device on one guest thread and render on another.
 * One context lease per public RHI operation serializes GL state and makes that
 * handoff explicit. Internal calls do not reacquire. No guest/gameplay fork. */
static int enter_backend(void)
{
    tsfp_gl_host_lock();
    if(S.host && !tsfp_gl_host_make_current(S.host)) {
        fprintf(stderr,"[RHI] ERROR: opengl context handoff failed\n");
        S.failed=1;tsfp_gl_host_unlock();return 0;
    }
    return 1;
}
static void leave_backend(void)
{tsfp_gl_host_release(S.host);tsfp_gl_host_unlock();}
#define GL_VOID(type,name,params,call,args) \
 static void lease_##name params {if(!enter_backend())return;call args;leave_backend();}
#define GL_VALUE(type,name,params,call,args,failure) \
 static type lease_##name params {if(!enter_backend())return failure;type value=call args;leave_backend();return value;}
GL_VALUE(int, device_create, (const RhiDeviceDesc * a0), g_device_create, (a0), -1)
GL_VOID(void, device_destroy, (void), g_device_destroy, ())
GL_VALUE(int, device_ready, (void), g_ready, (), 0)
GL_VALUE(RhiView *, swapchain_view, (void), g_swap_view, (), NULL)
GL_VALUE(int, swapchain_resize, (uint32_t a0, uint32_t a1), g_resize, (a0, a1), -1)
GL_VALUE(int32_t, present, (uint32_t a0), g_present, (a0), -1)
GL_VALUE(int, swapchain_readback, (void * a0, uint32_t a1, uint32_t * a2, uint32_t * a3), g_swap_readback, (a0, a1, a2, a3), -1)
GL_VOID(void, swapchain_set_vrr, (int a0), g_vrr, (a0))
GL_VALUE(RhiBuffer *, buffer_create, (const RhiBufferDesc * a0, const void * a1), g_buffer_create, (a0, a1), NULL)
GL_VOID(void, buffer_destroy, (RhiBuffer * a0), g_buffer_destroy, (a0))
GL_VALUE(void *, buffer_map, (RhiBuffer * a0, uint32_t a1), g_buffer_map, (a0, a1), NULL)
GL_VOID(void, buffer_unmap, (RhiBuffer * a0), g_buffer_unmap, (a0))
GL_VOID(void, buffer_update, (RhiBuffer * a0, const void * a1), g_buffer_update, (a0, a1))
GL_VALUE(RhiImage *, image_create, (const RhiImageDesc * a0, const RhiSubresourceData * a1), g_image_create, (a0, a1), NULL)
GL_VALUE(RhiImage *, image_retain, (RhiImage * a0), g_image_retain, (a0), NULL)
GL_VOID(void, image_destroy, (RhiImage * a0), g_image_destroy, (a0))
GL_VOID(void, image_get_desc, (const RhiImage * a0, RhiImageDesc * a1), g_image_desc, (a0, a1))
GL_VOID(void, image_update, (RhiImage * a0, uint32_t a1, const RhiBox * a2, const void * a3, uint32_t a4, uint32_t a5), g_image_update, (a0, a1, a2, a3, a4, a5))
GL_VALUE(int, image_readback, (RhiImage * a0, uint32_t a1, void * a2, uint32_t a3), g_image_readback, (a0, a1, a2, a3), -1)
GL_VALUE(RhiView  *, view_create, (RhiImage * a0, uint32_t a1, const RhiViewDesc * a2), g_view_create, (a0, a1, a2), NULL)
GL_VOID(void, view_destroy, (RhiView * a0), g_view_destroy, (a0))
GL_VALUE(RhiImage *, view_image, (const RhiView * a0), g_view_image, (a0), NULL)
GL_VALUE(int, sample_count_supported, (RhiFormat a0, uint32_t a1), g_sample_count, (a0, a1), 0)
GL_VALUE(RhiShader *, shader_create, (uint32_t a0, const RhiShaderSource * a1, char * a2, size_t a3), g_shader_create, (a0, a1, a2, a3), NULL)
GL_VOID(void, shader_destroy, (RhiShader * a0), g_shader_destroy, (a0))
GL_VALUE(RhiVertexLayout *, vertex_layout_create, (const RhiVertexElement * a0, uint32_t a1, const RhiShader * a2), g_layout_create, (a0, a1, a2), NULL)
GL_VOID(void, vertex_layout_destroy, (RhiVertexLayout * a0), g_layout_destroy, (a0))
GL_VALUE(RhiBlendState  *, blend_state_create, (const RhiBlendDesc * a0), g_blend_create, (a0), NULL)
GL_VALUE(RhiDepthState  *, depth_state_create, (const RhiDepthDesc * a0), g_depth_create, (a0), NULL)
GL_VALUE(RhiRasterState *, raster_state_create, (const RhiRasterDesc * a0), g_raster_create, (a0), NULL)
GL_VALUE(RhiSampler     *, sampler_create, (const RhiSamplerDesc * a0), g_sampler_create, (a0), NULL)
GL_VOID(void, blend_state_destroy, (RhiBlendState * a0), g_blend_destroy, (a0))
GL_VOID(void, depth_state_destroy, (RhiDepthState * a0), g_depth_destroy, (a0))
GL_VOID(void, raster_state_destroy, (RhiRasterState * a0), g_raster_destroy, (a0))
GL_VOID(void, sampler_destroy, (RhiSampler * a0), g_sampler_destroy, (a0))
GL_VOID(void, set_render_target, (RhiView * a0, RhiView * a1), g_set_render_target, (a0, a1))
GL_VOID(void, output_save, (RhiOutputState * a0), g_output_save, (a0))
GL_VOID(void, output_restore, (RhiOutputState * a0), g_output_restore, (a0))
GL_VALUE(int, output_color_is, (const RhiOutputState * a0, const RhiView * a1), g_output_color_is, (a0, a1), 0)
GL_VOID(void, set_viewports, (uint32_t a0, const RhiViewport * a1), g_viewports, (a0, a1))
GL_VOID(void, set_scissor, (const RhiRect * a0), g_scissor, (a0))
GL_VOID(void, set_topology, (uint32_t a0), g_topology, (a0))
GL_VOID(void, set_vertex_layout, (RhiVertexLayout * a0), g_set_layout, (a0))
GL_VOID(void, set_vertex_buffer, (uint32_t a0, RhiBuffer * a1, uint32_t a2, uint32_t a3), g_vertex_buffer, (a0, a1, a2, a3))
GL_VOID(void, set_index_buffer, (RhiBuffer * a0, uint32_t a1, uint32_t a2), g_index_buffer, (a0, a1, a2))
GL_VOID(void, set_shader, (uint32_t a0, RhiShader * a1), g_set_shader, (a0, a1))
GL_VOID(void, set_uniform_buffers, (uint32_t a0, uint32_t a1, uint32_t a2, RhiBuffer *const * a3), g_uniforms, (a0, a1, a2, a3))
GL_VOID(void, set_textures, (uint32_t a0, uint32_t a1, RhiView *const * a2), g_textures, (a0, a1, a2))
GL_VOID(void, set_samplers, (uint32_t a0, uint32_t a1, RhiSampler *const * a2), g_samplers, (a0, a1, a2))
GL_VOID(void, set_blend_state, (RhiBlendState * a0, const float * a1, uint32_t a2), g_blend, (a0, a1, a2))
GL_VOID(void, set_depth_state, (RhiDepthState * a0, uint32_t a1), g_depth, (a0, a1))
GL_VOID(void, set_raster_state, (RhiRasterState * a0), g_raster, (a0))
GL_VOID(void, draw, (uint32_t a0, uint32_t a1), g_draw, (a0, a1))
GL_VOID(void, draw_indexed, (uint32_t a0, uint32_t a1, int32_t a2), g_draw_indexed, (a0, a1, a2))
GL_VOID(void, clear_color, (RhiView * a0, const float * a1), g_clear_color, (a0, a1))
GL_VOID(void, clear_depth, (RhiView * a0, uint32_t a1, float a2, uint8_t a3), g_clear_depth, (a0, a1, a2, a3))
GL_VALUE(int, image_resolve, (RhiImage * a0, uint32_t a1, RhiImage * a2, uint32_t a3), g_resolve, (a0, a1, a2, a3), -1)
GL_VALUE(RhiOcclusion *, occlusion_create, (void), g_query_create, (), NULL)
GL_VOID(void, occlusion_destroy, (RhiOcclusion * a0), g_query_destroy, (a0))
GL_VALUE(int, occlusion_begin, (RhiOcclusion * a0), g_query_begin, (a0), 0)
GL_VALUE(int, occlusion_end, (RhiOcclusion * a0), g_query_end, (a0), 0)
GL_VALUE(int, occlusion_result, (RhiOcclusion * a0, uint64_t * a1), g_query_result, (a0, a1), -1)
#undef GL_VOID
#undef GL_VALUE
const RhiBackend rhi_opengl_backend={
    .name="opengl",.device_create=lease_device_create,.device_destroy=lease_device_destroy,.device_ready=lease_device_ready,
    .swapchain_view=lease_swapchain_view,.swapchain_resize=lease_swapchain_resize,.present=lease_present,.swapchain_readback=lease_swapchain_readback,.swapchain_set_vrr=lease_swapchain_set_vrr,
    .buffer_create=lease_buffer_create,.buffer_destroy=lease_buffer_destroy,.buffer_map=lease_buffer_map,.buffer_unmap=lease_buffer_unmap,.buffer_update=lease_buffer_update,
    .image_create=lease_image_create,.image_retain=lease_image_retain,.image_destroy=lease_image_destroy,.image_get_desc=lease_image_get_desc,.image_update=lease_image_update,.image_readback=lease_image_readback,
    .view_create=lease_view_create,.view_destroy=lease_view_destroy,.view_image=lease_view_image,.sample_count_supported=lease_sample_count_supported,
    .shader_create=lease_shader_create,.shader_destroy=lease_shader_destroy,.vertex_layout_create=lease_vertex_layout_create,.vertex_layout_destroy=lease_vertex_layout_destroy,
    .blend_state_create=lease_blend_state_create,.depth_state_create=lease_depth_state_create,.raster_state_create=lease_raster_state_create,.sampler_create=lease_sampler_create,
    .blend_state_destroy=lease_blend_state_destroy,.depth_state_destroy=lease_depth_state_destroy,.raster_state_destroy=lease_raster_state_destroy,.sampler_destroy=lease_sampler_destroy,
    .set_render_target=lease_set_render_target,.output_save=lease_output_save,.output_restore=lease_output_restore,.output_color_is=lease_output_color_is,
    .set_viewports=lease_set_viewports,.set_scissor=lease_set_scissor,.set_topology=lease_set_topology,.set_vertex_layout=lease_set_vertex_layout,.set_vertex_buffer=lease_set_vertex_buffer,
    .set_index_buffer=lease_set_index_buffer,.set_shader=lease_set_shader,.set_uniform_buffers=lease_set_uniform_buffers,.set_textures=lease_set_textures,.set_samplers=lease_set_samplers,
    .set_blend_state=lease_set_blend_state,.set_depth_state=lease_set_depth_state,.set_raster_state=lease_set_raster_state,.draw=lease_draw,.draw_indexed=lease_draw_indexed,.clear_color=lease_clear_color,.clear_depth=lease_clear_depth,
    .image_resolve=lease_image_resolve,.occlusion_create=lease_occlusion_create,.occlusion_destroy=lease_occlusion_destroy,.occlusion_begin=lease_occlusion_begin,.occlusion_end=lease_occlusion_end,.occlusion_result=lease_occlusion_result
};
