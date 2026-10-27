/**
 * rhi_vulkan.c -- the Vulkan backend of rhi.h.
 *
 * docs/technical/vulkan-backend.md is the design; the section numbers below
 * are its. The shape, in short:
 *
 *  - Vulkan 1.3 core: dynamic rendering (no render passes or framebuffers),
 *    extended dynamic state (cull, winding, depth, stencil and topology are
 *    not in the pipeline key), synchronization2, maintenance4 (a vertex
 *    program writes float4 texture coordinates that some pixel shaders read
 *    as float3), and push descriptors for the one small descriptor set
 *    (rhi_vulkan_bindings.h).
 *  - HLSL goes to SPIR-V through DXC (rhi_vulkan_dxc.cpp), after the same
 *    d3d8_hlsl_note the D3D11 backend calls.
 *  - The RHI's binding model looks immediate; the pipeline is found or
 *    built at draw time from what is bound (section 4.5).
 *  - Y is flipped with a negative viewport height, and the one winding
 *    inversion that pays for it is in raster_front_face() (section 4.4).
 *  - Barriers are deliberately coarse for bring-up: every image that can be
 *    a target lives in GENERAL, every image that can only be sampled lives
 *    in SHADER_READ_ONLY_OPTIMAL, and every operation outside a rendering
 *    scope is followed by a full memory barrier (section 4.7). Correct
 *    first; section 4.8's batching is a later step.
 *  - Buffers the CPU rewrites are renamed, the way D3D11 renames a
 *    WRITE_DISCARD map: vertex and index buffers get a new version when the
 *    current one is still in flight, and constant buffers are a CPU copy
 *    snapshotted into a per-frame ring when a draw needs them (section 4.6).
 *
 * Reading pixels back (rhi_image_readback) submits and waits, so every probe
 * that looks at pixels works here unchanged.
 */

#if defined(RHI_HAVE_VULKAN)

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#include "volk.h"
#include "vk_mem_alloc.h"

#include <ctype.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rhi_backend.h"
#include "rhi_vulkan_bindings.h"
#include "rhi_shader_cache.h"

/* d3d8_compile.c: every backend shows each HLSL source to it first. */
void d3d8_hlsl_note(const char *src, size_t len, const char *name,
                    const RhiMacro *macros, const char *entry, const char *target);
/* rhi_vulkan_dxc.cpp */
int rhi_vk_dxc_compile(uint32_t stage, const RhiShaderSource *src,
                       uint32_t **spirv, size_t *spirv_bytes, char *err, size_t err_len);

#define FRAMES          2
#define MAX_SC_IMAGES   8
#define RING_CHUNK      (8u << 20)
#define MAX_VB          16
#define MAX_TEX         16
#define MAX_UBO         8
#define MAX_REFL        32

/* ---- objects ------------------------------------------------------------------ */

typedef struct {
    VkBuffer      b;
    VmaAllocation a;
    uint8_t      *p;
    uint64_t      used;             /* the submission that last read it */
} BufVer;

struct RhiBuffer {
    RhiBufferDesc desc;
    int           uniform;
    /* constant buffers: the CPU copy and where the last snapshot went */
    uint8_t      *shadow;
    int           dirty;
    uint64_t      snap_frame;
    VkBuffer      snap_b;
    VkDeviceSize  snap_off, snap_range;
    /* everything else: versions, renamed on a discard */
    BufVer       *ver;
    uint32_t      nver, cap, cur;
};

struct RhiImage {
    RhiImageDesc       desc;
    volatile long      refs;
    int                swapchain;   /* stands for the acquired swap chain image */
    VkImage            image;
    VmaAllocation      alloc;
    VkFormat           format;
    VkImageAspectFlags aspect;
    VkImageLayout      layout;      /* the whole image */
    VkImageLayout      steady;      /* the layout it lives in between operations */
};

struct RhiView {
    uint32_t     kind;
    RhiImage    *image;
    int          swapchain;
    VkImageView  view;
    VkImageViewType type;
    uint32_t     base_mip, base_layer, layers;
    uint32_t     width, height;     /* of the viewed mip */
};

typedef struct {
    uint32_t binding;
    uint32_t kind;                  /* 1 uniform buffer, 2 sampled image, 3 sampler */
    uint32_t dim, arrayed, ms;      /* images: SPIR-V Dim, Arrayed, MS */
    uint32_t block_size;            /* uniform buffers: bytes the shader declares */
} Refl;

typedef struct {
    char     sem[40];
    uint32_t index;
    uint32_t location;
    uint32_t loc_word;              /* where the Location literal sits, for patching */
} ReflIO;

struct RhiShader {
    uint32_t       stage;
    uint64_t       id;
    VkShaderModule module;
    uint32_t      *code;
    size_t         words;
    char           entry[64];
    Refl           b[MAX_REFL];
    uint32_t       nb;
    ReflIO         in[32], out[32];
    uint32_t       nin, nout;
};

struct RhiVertexLayout {
    uint64_t         id;
    RhiVertexElement e[32];
    char             names[32][40];
    uint32_t         n;
};

struct RhiBlendState   { RhiBlendDesc d; };
struct RhiDepthState   { RhiDepthDesc d; };
struct RhiRasterState  { RhiRasterDesc d; };
struct RhiSampler      { VkSampler s; RhiSamplerDesc d; };

/* ---- the device ------------------------------------------------------------------ */

typedef struct {
    VkBuffer      b;
    VmaAllocation a;
    uint8_t      *p;
    VkDeviceSize  size;
} Chunk;

typedef struct {
    Chunk        *c;
    uint32_t      n, cap, cur;
    VkDeviceSize  off;
} Ring;

typedef struct {
    VkCommandPool   pool;
    VkCommandBuffer cmd;
    VkSemaphore     acquire;
    uint64_t        last_serial;
    Ring            ring;
} Frame;

enum { T_BUFFER, T_IMAGE, T_VIEW, T_SAMPLER, T_MODULE };
typedef struct {
    int      kind;
    uint64_t serial;
    uint64_t h;                     /* the Vulkan handle */
    VmaAllocation a;
} Trash;

typedef struct {
    uint64_t    vs, ps, layout;
    RhiBlendDesc blend;
    uint32_t    sample_mask;
    uint32_t    fill, depth_clamp;
    VkFormat    color, depth;
    uint32_t    samples;
    uint32_t    topo_class;
} PipeKey;

typedef struct {
    PipeKey    key;
    VkPipeline pipe;
    Refl       b[MAX_REFL * 2];
    uint32_t   nb;
    uint32_t   vb_slots[MAX_VB];
    uint32_t   nvb;
} Pipe;

static struct {
    VkInstance               instance;
    VkDebugUtilsMessengerEXT messenger;
    VkSurfaceKHR             surface;
    VkPhysicalDevice         pd;
    VkPhysicalDeviceProperties props;
    VkDevice                 dev;
    VkQueue                  queue;
    uint32_t                 qfamily;
    VmaAllocator             vma;
    VkFormat                 depth24;       /* D24S8, or D32S8 where there is none */
    int feat_depth_clamp, feat_fill, feat_bias_clamp, feat_aniso, feat_border;
    int                      validation;

    VkDescriptorSetLayout    set_layout;
    VkPipelineLayout         pipe_layout;
    VkPipelineCache          pcache;

    VkSemaphore              timeline;
    uint64_t                 submitted, completed;
    Frame                    frames[FRAMES];
    uint32_t                 fi;
    uint64_t                 frame_no;
    int                      recording, rendering;
    VkExtent2D               area;

    Trash                   *trash;
    uint32_t                 ntrash, trash_cap;

    Pipe                   **pipes;
    uint32_t                 pipe_cap, npipes;
    uint64_t                 next_id;

    /* dummies for what a draw leaves unbound */
    RhiImage                *dummy_img[5];  /* 2D, 2D array, cube, 3D, 2D MS */
    RhiView                 *dummy_view[5];
    VkBuffer                 zero_b;
    VmaAllocation            zero_a;
    VkSampler                default_sampler;
    VkBuffer                 read_b;
    VmaAllocation            read_a;
    uint8_t                 *read_p;
    VkDeviceSize             read_size;

    /* bound state */
    RhiView         *rt_color, *rt_depth;
    RhiViewport      vp[16];
    uint32_t         nvp;
    RhiRect          scissor;
    uint32_t         topology;
    RhiVertexLayout *layout;
    struct { RhiBuffer *b; uint32_t stride, offset; } vb[MAX_VB];
    RhiBuffer       *ib;
    uint32_t         ib_bits, ib_offset;
    RhiShader       *vs, *ps;
    RhiBuffer       *ubo[2][MAX_UBO];
    RhiView         *tex[MAX_TEX];
    RhiSampler      *smp[MAX_TEX];
    RhiBlendState   *blend;
    float            blend_factor[4];
    uint32_t         sample_mask;
    RhiDepthState   *depth;
    uint32_t         stencil_ref;
    RhiRasterState  *raster;
    VkPipeline       cur_pipe;
    /* The dynamic state the command buffer currently holds, so a draw sets
     * only what changed. Dynamic state lives in the command buffer and
     * persists across pipeline binds (every pipeline here declares the
     * same dynamic states), so this is valid from the first draw of a
     * recording until submit; begin_recording clears it. Setting all of
     * it on every draw was some twenty driver calls a draw, measured at a
     * few percent of Outrun 2's guest thread in a race. */
    struct {
        int                  valid;
        VkViewport           vp;
        VkRect2D             sc;
        VkCullModeFlags      cull;
        VkFrontFace          front_face;
        VkPrimitiveTopology  topo;
        VkBool32             bias_enable;
        float                bias[3];
        VkBool32             depth_test, depth_write;
        VkCompareOp          depth_cmp;
        VkBool32             stencil_test;
        struct { VkStencilOp fail, pass, depth_fail; VkCompareOp cmp; } front, back;
        uint32_t             read_mask, write_mask, ref;
        float                blend_const[4];
    } dyn;
} V;

static struct {
    VkSwapchainKHR   sc;
    VkFormat         format;
    VkExtent2D       extent;
    uint32_t         count, index;
    VkImage          images[MAX_SC_IMAGES];
    VkImageView      views[MAX_SC_IMAGES];
    VkImageLayout    layouts[MAX_SC_IMAGES];
    VkSemaphore      done[MAX_SC_IMAGES];
    int              acquired, wait_pending, stale;
    VkPresentModeKHR mode;
    uint32_t         interval;
    int              vrr;               /* rhi_swapchain_set_vrr */
    RhiImage         proxy_image;
    RhiView          proxy_view;
} SC;

static void full_barrier(VkCommandBuffer cb);
static void end_rendering(void);

#define VKCHECK(call, what) do { VkResult r_ = (call); if (r_ != VK_SUCCESS) { \
    fprintf(stderr, "[RHI] vulkan: %s failed (%d)\n", what, (int)r_); return -1; } } while (0)

/* ---- formats (section 4.9) -------------------------------------------------------- */

static VkFormat vk_format(RhiFormat f)
{
    switch (f) {
    case RHI_FORMAT_R32G32B32A32_FLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
    case RHI_FORMAT_R32G32B32A32_SINT:  return VK_FORMAT_R32G32B32A32_SINT;
    case RHI_FORMAT_R32G32B32_FLOAT:    return VK_FORMAT_R32G32B32_SFLOAT;
    case RHI_FORMAT_R16G16B16A16_FLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;
    case RHI_FORMAT_R16G16B16A16_UNORM: return VK_FORMAT_R16G16B16A16_UNORM;
    case RHI_FORMAT_R16G16B16A16_SNORM: return VK_FORMAT_R16G16B16A16_SNORM;
    case RHI_FORMAT_R32G32_FLOAT:       return VK_FORMAT_R32G32_SFLOAT;
    /* DXGI names packed formats from the low bits up and Vulkan from the
     * high bits down: the layouts agree where the names disagree. */
    case RHI_FORMAT_R10G10B10A2_UNORM:  return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case RHI_FORMAT_R11G11B10_FLOAT:    return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    case RHI_FORMAT_R8G8B8A8_UNORM:     return VK_FORMAT_R8G8B8A8_UNORM;
    case RHI_FORMAT_R8G8B8A8_SNORM:     return VK_FORMAT_R8G8B8A8_SNORM;
    case RHI_FORMAT_R16G16_FLOAT:       return VK_FORMAT_R16G16_SFLOAT;
    case RHI_FORMAT_R16G16_UNORM:       return VK_FORMAT_R16G16_UNORM;
    case RHI_FORMAT_R16G16_SNORM:       return VK_FORMAT_R16G16_SNORM;
    case RHI_FORMAT_D32_FLOAT:          return VK_FORMAT_D32_SFLOAT;
    case RHI_FORMAT_R32_FLOAT:          return VK_FORMAT_R32_SFLOAT;
    case RHI_FORMAT_R32_UINT:           return VK_FORMAT_R32_UINT;
    case RHI_FORMAT_D24_UNORM_S8_UINT:  return V.depth24 ? V.depth24 : VK_FORMAT_D24_UNORM_S8_UINT;
    case RHI_FORMAT_R8G8_UNORM:         return VK_FORMAT_R8G8_UNORM;
    case RHI_FORMAT_R8G8_SNORM:         return VK_FORMAT_R8G8_SNORM;
    case RHI_FORMAT_R16_FLOAT:          return VK_FORMAT_R16_SFLOAT;
    case RHI_FORMAT_D16_UNORM:          return VK_FORMAT_D16_UNORM;
    case RHI_FORMAT_R16_UNORM:          return VK_FORMAT_R16_UNORM;
    case RHI_FORMAT_R16_UINT:           return VK_FORMAT_R16_UINT;
    case RHI_FORMAT_R16_SNORM:          return VK_FORMAT_R16_SNORM;
    case RHI_FORMAT_R8_UNORM:           return VK_FORMAT_R8_UNORM;
    case RHI_FORMAT_A8_UNORM:           return VK_FORMAT_R8_UNORM;         /* swizzled in views */
    case RHI_FORMAT_BC1_UNORM:          return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
    case RHI_FORMAT_BC2_UNORM:          return VK_FORMAT_BC2_UNORM_BLOCK;
    case RHI_FORMAT_BC3_UNORM:          return VK_FORMAT_BC3_UNORM_BLOCK;
    case RHI_FORMAT_BC5_UNORM:          return VK_FORMAT_BC5_UNORM_BLOCK;
    case RHI_FORMAT_B5G6R5_UNORM:       return VK_FORMAT_R5G6B5_UNORM_PACK16;
    case RHI_FORMAT_B5G5R5A1_UNORM:     return VK_FORMAT_A1R5G5B5_UNORM_PACK16;
    case RHI_FORMAT_B8G8R8A8_UNORM:     return VK_FORMAT_B8G8R8A8_UNORM;
    case RHI_FORMAT_B8G8R8X8_UNORM:     return VK_FORMAT_B8G8R8A8_UNORM;   /* alpha forced in views */
    case RHI_FORMAT_B4G4R4A4_UNORM:     return VK_FORMAT_A4R4G4B4_UNORM_PACK16;
    default:                            return VK_FORMAT_UNDEFINED;
    }
}

static int is_depth_format(VkFormat f)
{
    return f == VK_FORMAT_D16_UNORM || f == VK_FORMAT_D32_SFLOAT ||
           f == VK_FORMAT_D24_UNORM_S8_UINT || f == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

static int has_stencil(VkFormat f)
{
    return f == VK_FORMAT_D24_UNORM_S8_UINT || f == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

static VkImageAspectFlags aspect_of(VkFormat f)
{
    if (!is_depth_format(f))
        return VK_IMAGE_ASPECT_COLOR_BIT;
    return VK_IMAGE_ASPECT_DEPTH_BIT | (has_stencil(f) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
}

static int format_ok(VkFormat f, VkFormatFeatureFlags need)
{
    VkFormatProperties p;
    vkGetPhysicalDeviceFormatProperties(V.pd, f, &p);
    return (p.optimalTilingFeatures & need) == need;
}

/* ---- deferred destruction ---------------------------------------------------------- */

/* A handle the GPU may still be reading goes here with the serial of the
 * next submission; it is destroyed once that submission has finished. */
static void trash(int kind, uint64_t h, VmaAllocation a)
{
    Trash *t;
    if (!h)
        return;
    if (V.ntrash == V.trash_cap) {
        uint32_t cap = V.trash_cap ? V.trash_cap * 2 : 256;
        Trash *n = realloc(V.trash, cap * sizeof *n);
        if (!n)
            return;
        V.trash = n;
        V.trash_cap = cap;
    }
    t = &V.trash[V.ntrash++];
    t->kind = kind;
    t->serial = V.submitted + 1;
    t->h = h;
    t->a = a;
}

static void destroy_now(const Trash *t)
{
    switch (t->kind) {
    case T_BUFFER:  vmaDestroyBuffer(V.vma, (VkBuffer)t->h, t->a); break;
    case T_IMAGE:   vmaDestroyImage(V.vma, (VkImage)t->h, t->a); break;
    case T_VIEW:    vkDestroyImageView(V.dev, (VkImageView)t->h, NULL); break;
    case T_SAMPLER: vkDestroySampler(V.dev, (VkSampler)t->h, NULL); break;
    case T_MODULE:  vkDestroyShaderModule(V.dev, (VkShaderModule)t->h, NULL); break;
    }
}

static void update_completed(void)
{
    uint64_t v = 0;
    if (V.timeline && vkGetSemaphoreCounterValue(V.dev, V.timeline, &v) == VK_SUCCESS)
        V.completed = v;
}

static void trash_collect(void)
{
    uint32_t i, k = 0;
    update_completed();
    for (i = 0; i < V.ntrash; i++) {
        if (V.trash[i].serial <= V.completed)
            destroy_now(&V.trash[i]);
        else
            V.trash[k++] = V.trash[i];
    }
    V.ntrash = k;
}

static void wait_serial(uint64_t s)
{
    VkSemaphoreWaitInfo wi = { VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
    if (!s || s <= V.completed)
        return;
    wi.semaphoreCount = 1;
    wi.pSemaphores = &V.timeline;
    wi.pValues = &s;
    vkWaitSemaphores(V.dev, &wi, UINT64_MAX);
    update_completed();
}

/* ---- memory ------------------------------------------------------------------------ */

static int host_buffer(VkDeviceSize size, VkBufferUsageFlags usage, int readback,
                       VkBuffer *b, VmaAllocation *a, uint8_t **p)
{
    VkBufferCreateInfo bi = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    VmaAllocationCreateInfo ai;
    VmaAllocationInfo info;

    memset(&ai, 0, sizeof ai);
    bi.size = size ? size : 16;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ai.usage = VMA_MEMORY_USAGE_AUTO;
    ai.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
               (readback ? VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
                         : VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    if (vmaCreateBuffer(V.vma, &bi, &ai, b, a, &info) != VK_SUCCESS)
        return 0;
    *p = (uint8_t *)info.pMappedData;
    return 1;
}

#define RING_USAGE (VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | \
                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT)

static uint8_t *ring_alloc(VkDeviceSize size, VkDeviceSize align, VkBuffer *buf, VkDeviceSize *off)
{
    Ring *r = &V.frames[V.fi].ring;

    if (align < 16)
        align = 16;
    for (;;) {
        if (r->cur < r->n) {
            Chunk *c = &r->c[r->cur];
            VkDeviceSize o = (r->off + align - 1) & ~(align - 1);
            if (o + size <= c->size) {
                *buf = c->b;
                *off = o;
                r->off = o + size;
                return c->p + o;
            }
            r->cur++;
            r->off = 0;
            continue;
        }
        if (r->n == r->cap) {
            uint32_t cap = r->cap ? r->cap * 2 : 4;
            Chunk *n = realloc(r->c, cap * sizeof *n);
            if (!n)
                return NULL;
            r->c = n;
            r->cap = cap;
        }
        {
            Chunk *c = &r->c[r->n];
            c->size = size > RING_CHUNK ? size : RING_CHUNK;
            if (!host_buffer(c->size, RING_USAGE, 0, &c->b, &c->a, &c->p))
                return NULL;
            r->n++;
        }
    }
}

static void ring_flush(Ring *r)
{
    uint32_t i;
    for (i = 0; i < r->n && i <= r->cur; i++)
        vmaFlushAllocation(V.vma, r->c[i].a, 0, VK_WHOLE_SIZE);
}

/* ---- command recording and submission ------------------------------------------------ */

static void begin_recording(void)
{
    Frame *f = &V.frames[V.fi];
    VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };

    wait_serial(f->last_serial);
    vkResetCommandPool(V.dev, f->pool, 0);
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(f->cmd, &bi);
    V.recording = 1;
    V.rendering = 0;
    V.cur_pipe = VK_NULL_HANDLE;
    V.dyn.valid = 0;                    /* a new command buffer holds no dynamic state */
}

static VkCommandBuffer cmd(void)
{
    if (!V.recording)
        begin_recording();
    return V.frames[V.fi].cmd;
}

/* Ends the recording and submits it. The swap chain image's acquire is
 * waited on by the first submission after it; `signal` is the semaphore a
 * present waits on. */
static void submit(VkSemaphore signal)
{
    Frame *f = &V.frames[V.fi];
    VkSemaphoreSubmitInfo waits[1], signals[2];
    VkCommandBufferSubmitInfo cbi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
    VkSubmitInfo2 si = { VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };

    if (!V.recording)
        begin_recording();
    end_rendering();
    vkEndCommandBuffer(f->cmd);
    ring_flush(&f->ring);

    memset(waits, 0, sizeof waits);
    memset(signals, 0, sizeof signals);
    cbi.commandBuffer = f->cmd;
    si.commandBufferInfoCount = 1;
    si.pCommandBufferInfos = &cbi;
    if (SC.wait_pending) {
        waits[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        waits[0].semaphore = f->acquire;
        waits[0].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        si.waitSemaphoreInfoCount = 1;
        si.pWaitSemaphoreInfos = waits;
        SC.wait_pending = 0;
    }
    signals[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signals[0].semaphore = V.timeline;
    signals[0].value = ++V.submitted;
    signals[0].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    si.signalSemaphoreInfoCount = 1;
    if (signal) {
        signals[1].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signals[1].semaphore = signal;
        signals[1].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        si.signalSemaphoreInfoCount = 2;
    }
    si.pSignalSemaphoreInfos = signals;
    if (vkQueueSubmit2(V.queue, 1, &si, VK_NULL_HANDLE) != VK_SUCCESS)
        fprintf(stderr, "[RHI] vulkan: vkQueueSubmit2 failed\n");
    f->last_serial = V.submitted;
    V.recording = 0;
}

/* Everything recorded so far, finished: what a synchronous readback needs. */
static void flush_wait(void)
{
    if (V.recording)
        submit(VK_NULL_HANDLE);
    wait_serial(V.submitted);
    trash_collect();
}

static void full_barrier(VkCommandBuffer cb)
{
    VkMemoryBarrier2 mb = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
    VkDependencyInfo d = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };

    mb.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    mb.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    mb.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_2_HOST_BIT;
    mb.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT |
                       VK_ACCESS_2_HOST_READ_BIT;
    d.memoryBarrierCount = 1;
    d.pMemoryBarriers = &mb;
    vkCmdPipelineBarrier2(cb, &d);
}

static void layout_barrier(VkCommandBuffer cb, VkImage img, VkImageAspectFlags aspect,
                           VkImageLayout from, VkImageLayout to)
{
    VkImageMemoryBarrier2 b = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    VkDependencyInfo d = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };

    b.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    b.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    b.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    b.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    b.oldLayout = from;
    b.newLayout = to;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img;
    b.subresourceRange.aspectMask = aspect;
    b.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    b.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    d.imageMemoryBarrierCount = 1;
    d.pImageMemoryBarriers = &b;
    vkCmdPipelineBarrier2(cb, &d);
}

/* ---- the swap chain (section 4.10) ------------------------------------------------------ */

static const char *mode_name(VkPresentModeKHR m)
{
    switch (m) {
    case VK_PRESENT_MODE_IMMEDIATE_KHR:    return "IMMEDIATE";
    case VK_PRESENT_MODE_MAILBOX_KHR:      return "MAILBOX";
    case VK_PRESENT_MODE_FIFO_KHR:         return "FIFO";
    case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "FIFO_RELAXED";
    default:                               return "other";
    }
}

/* Interval 0 must not block: a title paces its loader on frames presented,
 * and a vsync wait here once throttled Burnout 2 from 136 frames a second
 * to 27. So MAILBOX, then IMMEDIATE, then FIFO_RELAXED, and FIFO only when
 * there is nothing else -- said, because it is that regression again. */
static VkPresentModeKHR choose_mode(uint32_t interval)
{
    static const VkPresentModeKHR order[] = {
        VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR,
        VK_PRESENT_MODE_FIFO_RELAXED_KHR, VK_PRESENT_MODE_FIFO_KHR
    };
    /* For a variable-refresh display (rhi_swapchain_set_vrr): IMMEDIATE
     * hands each frame to the display as it is made, which is what lets the
     * display refresh when a frame arrives -- a 60 fps title shown at an
     * even 60 on a 144 Hz screen rather than for two refreshes, then three.
     * MAILBOX holds the frame for the display's next fixed refresh, which is
     * the judder variable refresh exists to remove, so it comes after.
     * Still none that blocks, for the reason above. */
    static const VkPresentModeKHR order_vrr[] = {
        VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_RELAXED_KHR,
        VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_KHR
    };
    const VkPresentModeKHR *pick = SC.vrr ? order_vrr : order;
    VkPresentModeKHR modes[16];
    uint32_t n = 16, i, k;

    if (interval)
        return VK_PRESENT_MODE_FIFO_KHR;
    vkGetPhysicalDeviceSurfacePresentModesKHR(V.pd, V.surface, &n, modes);
    /* RECOMP_VK_PRESENT=immediate|mailbox|fifo_relaxed|fifo puts that mode
     * first. On Windows a windowed MAILBOX swap chain is paced by the
     * compositor at the display's refresh rate, so measuring what a frame
     * costs, rather than what the screen shows, wants IMMEDIATE. */
    {
        const char *want = getenv("RECOMP_VK_PRESENT");
        VkPresentModeKHR m = VK_PRESENT_MODE_MAX_ENUM_KHR;
        if (want && !strcmp(want, "immediate"))    m = VK_PRESENT_MODE_IMMEDIATE_KHR;
        if (want && !strcmp(want, "mailbox"))      m = VK_PRESENT_MODE_MAILBOX_KHR;
        if (want && !strcmp(want, "fifo_relaxed")) m = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
        if (want && !strcmp(want, "fifo"))         m = VK_PRESENT_MODE_FIFO_KHR;
        for (i = 0; i < n; i++)
            if (modes[i] == m)
                return m;
    }
    for (k = 0; k < sizeof order / sizeof order[0]; k++)
        for (i = 0; i < n; i++)
            if (modes[i] == pick[k])
                return pick[k];
    return VK_PRESENT_MODE_FIFO_KHR;
}

static void v_swapchain_set_vrr(int on)
{
    on = on ? 1 : 0;
    if (on == SC.vrr)
        return;
    SC.vrr = on;
    if (!V.dev)
        return;
    SC.mode = choose_mode(SC.interval);
    SC.stale = 1;                       /* the next acquire recreates it */
    fprintf(stderr, "[RHI] vulkan: variable refresh %s, present mode %s\n",
            on ? "on" : "off", mode_name(SC.mode));
    fflush(stderr);
}

static void destroy_swapchain_views(void)
{
    uint32_t i;
    for (i = 0; i < SC.count; i++) {
        if (SC.views[i]) vkDestroyImageView(V.dev, SC.views[i], NULL);
        if (SC.done[i]) vkDestroySemaphore(V.dev, SC.done[i], NULL);
        SC.views[i] = VK_NULL_HANDLE;
        SC.done[i] = VK_NULL_HANDLE;
    }
    SC.count = 0;
}

static int create_swapchain(uint32_t w, uint32_t h)
{
    VkSurfaceCapabilitiesKHR caps;
    VkSwapchainCreateInfoKHR ci = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    VkSemaphoreCreateInfo sci = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkSwapchainKHR old = SC.sc;
    VkExtent2D ext;
    uint32_t i;

    VKCHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(V.pd, V.surface, &caps), "surface capabilities");
    ext = caps.currentExtent;
    if (ext.width == 0xFFFFFFFFu) {
        ext.width = w;
        ext.height = h;
    }
    if (!ext.width || !ext.height)
        return -1;                      /* minimised: nothing to draw into */
    ci.surface = V.surface;
    ci.minImageCount = caps.minImageCount + 1;
    if (caps.maxImageCount && ci.minImageCount > caps.maxImageCount)
        ci.minImageCount = caps.maxImageCount;
    ci.imageFormat = SC.format;
    ci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    ci.imageExtent = ext;
    ci.imageArrayLayers = 1;
    /* TRANSFER as well: clears, and the readback behind replay --present. */
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                    ((VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT) &
                     caps.supportedUsageFlags);
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = SC.mode;
    ci.clipped = VK_TRUE;
    ci.oldSwapchain = old;
    VKCHECK(vkCreateSwapchainKHR(V.dev, &ci, NULL, &SC.sc), "vkCreateSwapchainKHR");
    destroy_swapchain_views();
    if (old)
        vkDestroySwapchainKHR(V.dev, old, NULL);
    SC.extent = ext;
    SC.count = MAX_SC_IMAGES;
    VKCHECK(vkGetSwapchainImagesKHR(V.dev, SC.sc, &SC.count, SC.images), "swap chain images");
    for (i = 0; i < SC.count; i++) {
        VkImageViewCreateInfo vci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        vci.image = SC.images[i];
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = SC.format;
        vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vci.subresourceRange.levelCount = 1;
        vci.subresourceRange.layerCount = 1;
        VKCHECK(vkCreateImageView(V.dev, &vci, NULL, &SC.views[i]), "swap chain view");
        VKCHECK(vkCreateSemaphore(V.dev, &sci, NULL, &SC.done[i]), "semaphore");
        SC.layouts[i] = VK_IMAGE_LAYOUT_UNDEFINED;
    }
    SC.proxy_image.desc.width = SC.proxy_view.width = ext.width;
    SC.proxy_image.desc.height = SC.proxy_view.height = ext.height;
    SC.stale = 0;
    return 0;
}

static int recreate_swapchain(void)
{
    if (SC.acquired) {
        /* An acquired image may be dropped unpresented once the queue has
         * finished with it. */
        flush_wait();
        SC.acquired = 0;
    }
    vkDeviceWaitIdle(V.dev);
    update_completed();
    return create_swapchain(SC.extent.width, SC.extent.height);
}

static int acquire(void)
{
    VkResult r;
    int tries;

    if (SC.acquired)
        return 1;
    if (!SC.sc)
        return 0;
    for (tries = 0; tries < 2; tries++) {
        if (SC.stale && recreate_swapchain() != 0)
            return 0;
        /* The frame's acquire semaphore is free: its last wait was in a
         * submission begin_recording has already waited for. */
        cmd();
        r = vkAcquireNextImageKHR(V.dev, SC.sc, UINT64_MAX, V.frames[V.fi].acquire,
                                  VK_NULL_HANDLE, &SC.index);
        if (r == VK_ERROR_OUT_OF_DATE_KHR) {
            SC.stale = 1;
            continue;
        }
        if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR)
            return 0;
        if (r == VK_SUBOPTIMAL_KHR)
            SC.stale = 1;               /* after this frame is presented */
        SC.acquired = 1;
        SC.wait_pending = 1;
        /* DXGI's DISCARD swap effect: the old contents are not kept. */
        layout_barrier(cmd(), SC.images[SC.index], VK_IMAGE_ASPECT_COLOR_BIT,
                       VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        SC.layouts[SC.index] = VK_IMAGE_LAYOUT_GENERAL;
        return 1;
    }
    return 0;
}

/* The Vulkan objects behind a handle that may stand for the swap chain. */
static VkImage image_handle(const RhiImage *img)
{
    if (img->swapchain)
        return acquire() ? SC.images[SC.index] : VK_NULL_HANDLE;
    return img->image;
}

static VkImageView view_handle(const RhiView *v)
{
    if (v->swapchain)
        return acquire() ? SC.views[SC.index] : VK_NULL_HANDLE;
    return v->view;
}

static VkFormat image_format(const RhiImage *img)
{
    return img->swapchain ? SC.format : img->format;
}

/* ---- rendering scopes (section 4.8) -------------------------------------------------------- */

static void end_rendering(void)
{
    if (!V.rendering)
        return;
    vkCmdEndRendering(V.frames[V.fi].cmd);
    full_barrier(V.frames[V.fi].cmd);
    V.rendering = 0;
}

static int begin_rendering(void)
{
    VkRenderingAttachmentInfo ca = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    VkRenderingAttachmentInfo da = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    VkRenderingInfo ri = { VK_STRUCTURE_TYPE_RENDERING_INFO };
    RhiView *c = V.rt_color, *d = V.rt_depth;
    VkImageView cv = VK_NULL_HANDLE, dv = VK_NULL_HANDLE;
    uint32_t w = 0, h = 0;

    if (V.rendering)
        return 1;
    if (!c && !d)
        return 0;
    cmd();
    if (c && (cv = view_handle(c)) == VK_NULL_HANDLE)
        return 0;
    if (d)
        dv = d->view;
    if (c) {
        w = c->width;
        h = c->height;
    }
    if (d) {
        if (!c || d->width < w) w = d->width;
        if (!c || d->height < h) h = d->height;
        if (c && d->width > c->width) w = c->width;
        if (c && d->height > c->height) h = c->height;
    }
    ri.renderArea.extent.width = w;
    ri.renderArea.extent.height = h;
    ri.layerCount = 1;
    if (c) {
        ca.imageView = cv;
        ca.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        ca.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        ca.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        ri.colorAttachmentCount = 1;
        ri.pColorAttachments = &ca;
    }
    if (d) {
        da.imageView = dv;
        da.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        da.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        da.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        ri.pDepthAttachment = &da;
        if (has_stencil(d->image->format))
            ri.pStencilAttachment = &da;
    }
    vkCmdBeginRendering(V.frames[V.fi].cmd, &ri);
    V.rendering = 1;
    V.area.width = w;
    V.area.height = h;
    return 1;
}

/* ---- the device ------------------------------------------------------------------------------ */

static int g_validation_errors;

static VKAPI_ATTR VkBool32 VKAPI_CALL on_debug(VkDebugUtilsMessageSeverityFlagBitsEXT sev,
                                               VkDebugUtilsMessageTypeFlagsEXT types,
                                               const VkDebugUtilsMessengerCallbackDataEXT *data,
                                               void *user)
{
    (void)types; (void)user;
    if ((sev & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) && g_validation_errors++ < 50)
        fprintf(stderr, "[VKVAL] %s\n", data->pMessage);
    else if ((sev & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) && g_validation_errors < 50)
        fprintf(stderr, "[VKVAL warning] %s\n", data->pMessage);
    return VK_FALSE;
}

static int has_instance_layer(const char *name)
{
    uint32_t n = 0, i;
    VkLayerProperties *p;
    int found = 0;
    vkEnumerateInstanceLayerProperties(&n, NULL);
    p = calloc(n ? n : 1, sizeof *p);
    if (!p)
        return 0;
    vkEnumerateInstanceLayerProperties(&n, p);
    for (i = 0; i < n; i++)
        if (!strcmp(p[i].layerName, name))
            found = 1;
    free(p);
    return found;
}

static int has_device_ext(const char *name)
{
    uint32_t n = 0, i;
    VkExtensionProperties *p;
    int found = 0;
    vkEnumerateDeviceExtensionProperties(V.pd, NULL, &n, NULL);
    p = calloc(n ? n : 1, sizeof *p);
    if (!p)
        return 0;
    vkEnumerateDeviceExtensionProperties(V.pd, NULL, &n, p);
    for (i = 0; i < n; i++)
        if (!strcmp(p[i].extensionName, name))
            found = 1;
    free(p);
    return found;
}

static int env_on(const char *name)
{
    const char *v = getenv(name);
    return v && *v && strcmp(v, "0") != 0;
}

/* RECOMP_VK_TRACE=1: every image, upload and sampler as it is made, for
 * telling a format or sampling difference from a drawing one when a
 * replay differs between backends. The first 400 lines, or as many as a
 * number larger than 1 says. */
static int trace_on(void)
{
    static int limit = -1, lines;
    if (limit < 0) {
        const char *v = getenv("RECOMP_VK_TRACE");
        limit = !env_on("RECOMP_VK_TRACE") ? 0 : atoi(v) > 1 ? atoi(v) : 400;
    }
    return lines++ < limit;
}

static int create_instance(int want_validation)
{
    const char *layer = "VK_LAYER_KHRONOS_validation";
    const char *exts[4];
    uint32_t next = 0;
    VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    VkDebugUtilsMessengerCreateInfoEXT dci = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };

    V.validation = want_validation && has_instance_layer(layer);
    if (want_validation && !V.validation)
        fprintf(stderr, "[RHI] vulkan: validation asked for, but the layer is not installed\n");
    exts[next++] = VK_KHR_SURFACE_EXTENSION_NAME;
#if defined(_WIN32)
    exts[next++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
#endif
    if (V.validation)
        exts[next++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    app.pApplicationName = "xboxrecomp";
    app.pEngineName = "xboxrecomp";
    app.apiVersion = VK_API_VERSION_1_3;
    ici.pApplicationInfo = &app;
    ici.enabledExtensionCount = next;
    ici.ppEnabledExtensionNames = exts;
    if (V.validation) {
        ici.enabledLayerCount = 1;
        ici.ppEnabledLayerNames = &layer;
        dci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        dci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        dci.pfnUserCallback = on_debug;
        ici.pNext = &dci;
    }
    VKCHECK(vkCreateInstance(&ici, NULL, &V.instance), "vkCreateInstance");
    volkLoadInstance(V.instance);
    if (V.validation)
        vkCreateDebugUtilsMessengerEXT(V.instance, &dci, NULL, &V.messenger);
    return 0;
}

static int pick_physical_device(void)
{
    VkPhysicalDevice pds[8];
    uint32_t n = 8, i, pick = 0;
    const char *want = getenv("RECOMP_VK_DEVICE");

    VKCHECK(vkEnumeratePhysicalDevices(V.instance, &n, pds), "vkEnumeratePhysicalDevices");
    if (!n) {
        fprintf(stderr, "[RHI] vulkan: no Vulkan device\n");
        return -1;
    }
    if (want && *want && (uint32_t)atoi(want) < n) {
        pick = (uint32_t)atoi(want);
    } else {
        for (i = 0; i < n; i++) {
            VkPhysicalDeviceProperties p;
            vkGetPhysicalDeviceProperties(pds[i], &p);
            if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                pick = i;
                break;
            }
        }
    }
    V.pd = pds[pick];
    vkGetPhysicalDeviceProperties(V.pd, &V.props);
    if (V.props.apiVersion < VK_API_VERSION_1_3) {
        fprintf(stderr, "[RHI] vulkan: %s is Vulkan %u.%u; this backend needs 1.3\n",
                V.props.deviceName, VK_API_VERSION_MAJOR(V.props.apiVersion),
                VK_API_VERSION_MINOR(V.props.apiVersion));
        return -1;
    }
    return 0;
}

static int create_device(void)
{
    VkQueueFamilyProperties q[16];
    uint32_t nq = 16, i;
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    VkPhysicalDeviceFeatures2 have = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    VkPhysicalDeviceVulkan12Features have12 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    VkPhysicalDeviceVulkan13Features have13 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    VkPhysicalDeviceCustomBorderColorFeaturesEXT have_cb = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT };
    VkPhysicalDeviceFeatures2 f2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    VkPhysicalDeviceVulkan12Features f12 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    VkPhysicalDeviceVulkan13Features f13 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    VkPhysicalDeviceCustomBorderColorFeaturesEXT fcb = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT };
    VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    const char *exts[3];
    uint32_t next = 0;
    int border_ext = has_device_ext(VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME);

    if (!has_device_ext(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME)) {
        fprintf(stderr, "[RHI] vulkan: %s has no push descriptors, which this backend needs\n",
                V.props.deviceName);
        return -1;
    }
    vkGetPhysicalDeviceQueueFamilyProperties(V.pd, &nq, q);
    for (i = 0; i < nq; i++) {
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(V.pd, i, V.surface, &present);
        if ((q[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present)
            break;
    }
    if (i == nq) {
        fprintf(stderr, "[RHI] vulkan: no queue both draws and presents to this window\n");
        return -1;
    }
    V.qfamily = i;
    qci.queueFamilyIndex = i;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;

    have.pNext = &have12;
    have12.pNext = &have13;
    have13.pNext = border_ext ? &have_cb : NULL;
    vkGetPhysicalDeviceFeatures2(V.pd, &have);
    if (!have13.dynamicRendering || !have13.synchronization2 || !have13.maintenance4 ||
        !have12.timelineSemaphore) {
        fprintf(stderr, "[RHI] vulkan: %s lacks dynamic rendering, synchronization2, "
                        "maintenance4 or timeline semaphores\n", V.props.deviceName);
        return -1;
    }
    f2.features.fillModeNonSolid = V.feat_fill = have.features.fillModeNonSolid;
    f2.features.depthClamp = V.feat_depth_clamp = have.features.depthClamp;
    f2.features.depthBiasClamp = V.feat_bias_clamp = have.features.depthBiasClamp;
    f2.features.samplerAnisotropy = V.feat_aniso = have.features.samplerAnisotropy;
    f12.timelineSemaphore = VK_TRUE;
    f12.scalarBlockLayout = have12.scalarBlockLayout;
    f12.samplerMirrorClampToEdge = have12.samplerMirrorClampToEdge;
    f13.dynamicRendering = VK_TRUE;
    f13.synchronization2 = VK_TRUE;
    /* The vertex programs write every texture coordinate as float4 and some
     * pixel shaders read float3: D3D11 links that, core Vulkan only with
     * maintenance4. */
    f13.maintenance4 = VK_TRUE;
    f2.pNext = &f12;
    f12.pNext = &f13;
    exts[next++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    exts[next++] = VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME;
    if (border_ext && have_cb.customBorderColors && have_cb.customBorderColorWithoutFormat) {
        fcb.customBorderColors = VK_TRUE;
        fcb.customBorderColorWithoutFormat = VK_TRUE;
        f13.pNext = &fcb;
        exts[next++] = VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME;
        V.feat_border = 1;
    }
    dci.pNext = &f2;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = next;
    dci.ppEnabledExtensionNames = exts;
    VKCHECK(vkCreateDevice(V.pd, &dci, NULL, &V.dev), "vkCreateDevice");
    volkLoadDevice(V.dev);
    vkGetDeviceQueue(V.dev, V.qfamily, 0, &V.queue);

    /* Section 4.9: AMD has no D24S8, so the depth format is the device's. */
    V.depth24 = format_ok(VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
                ? VK_FORMAT_D24_UNORM_S8_UINT : VK_FORMAT_D32_SFLOAT_S8_UINT;
    return 0;
}

static int create_allocator(void)
{
    VmaVulkanFunctions fn;
    VmaAllocatorCreateInfo ai;

    memset(&fn, 0, sizeof fn);
    memset(&ai, 0, sizeof ai);
    fn.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    fn.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    ai.vulkanApiVersion = VK_API_VERSION_1_3;
    ai.physicalDevice = V.pd;
    ai.device = V.dev;
    ai.instance = V.instance;
    ai.pVulkanFunctions = &fn;
    VKCHECK(vmaCreateAllocator(&ai, &V.vma), "vmaCreateAllocator");
    return 0;
}

/* The one descriptor set (rhi_vulkan_bindings.h): vertex b0-b3 and b7,
 * pixel b0, b1 and b7, t0-t3, t8 and t9, s0-s3, s8 and s9 -- every slot
 * the renderer binds (section 4.2). */
static const uint32_t g_ubo_bindings[] = {
    RHI_VK_VS_UNIFORM_BASE + 0, RHI_VK_VS_UNIFORM_BASE + 1, RHI_VK_VS_UNIFORM_BASE + 2,
    RHI_VK_VS_UNIFORM_BASE + 3, RHI_VK_VS_UNIFORM_BASE + 7,
    RHI_VK_PS_UNIFORM_BASE + 0, RHI_VK_PS_UNIFORM_BASE + 1, RHI_VK_PS_UNIFORM_BASE + 7,
};
static const uint32_t g_tex_slots[] = { 0, 1, 2, 3, 8, 9 };

static int binding_declared(uint32_t binding, uint32_t kind)
{
    uint32_t i;
    if (kind == 1) {
        for (i = 0; i < sizeof g_ubo_bindings / sizeof g_ubo_bindings[0]; i++)
            if (g_ubo_bindings[i] == binding)
                return 1;
        return 0;
    }
    for (i = 0; i < sizeof g_tex_slots / sizeof g_tex_slots[0]; i++)
        if (binding == (kind == 2 ? RHI_VK_TEXTURE_BASE : RHI_VK_SAMPLER_BASE) + g_tex_slots[i])
            return 1;
    return 0;
}

/* The pipeline cache, kept between runs beside the shader cache
 * (rhi_shader_cache.c). Pipelines are built on first use of each draw-state
 * combination, and each build is a driver compile -- a hitch the shader
 * cache does not remove. The driver keys the cache's entries by the shaders'
 * contents, so it is valid across runs on the same device and driver; the
 * header is checked against this device first and a mismatch starts empty. */
#define PIPELINE_CACHE_DIR  "vulkan-pipelines"
#define PIPELINE_CACHE_FILE "pipelines.bin"

static void *pcache_load(size_t *bytes)
{
    char path[1024];
    FILE *f;
    long n;
    unsigned char *data = NULL;

    *bytes = 0;
    if (!rhi_cache_file_path(PIPELINE_CACHE_DIR, PIPELINE_CACHE_FILE, path, sizeof path))
        return NULL;
    f = fopen(path, "rb");
    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) == 0 && (n = ftell(f)) >= 32 && n < 256L * 1024 * 1024 &&
        fseek(f, 0, SEEK_SET) == 0 && (data = malloc((size_t)n)) != NULL &&
        fread(data, 1, (size_t)n, f) == (size_t)n) {
        uint32_t hsize, hver, vendor, device;

        memcpy(&hsize, data + 0, 4);
        memcpy(&hver, data + 4, 4);
        memcpy(&vendor, data + 8, 4);
        memcpy(&device, data + 12, 4);
        if (hsize >= 32 && hver == VK_PIPELINE_CACHE_HEADER_VERSION_ONE &&
            vendor == V.props.vendorID && device == V.props.deviceID &&
            memcmp(data + 16, V.props.pipelineCacheUUID, VK_UUID_SIZE) == 0) {
            *bytes = (size_t)n;
        } else {
            fprintf(stderr, "[RHI] vulkan: pipeline cache on disk is for another "
                    "device or driver; starting empty\n");
            free(data);
            data = NULL;
        }
    }
    fclose(f);
    if (data)
        fprintf(stderr, "[RHI] vulkan: pipeline cache loaded (%zu bytes)\n", *bytes);
    return data;
}

static void pcache_save(void)
{
    char path[1024], tmp[1100];
    size_t n = 0;
    void *data;
    FILE *f;
    int ok;

    if (!V.pcache || !rhi_cache_file_path(PIPELINE_CACHE_DIR, PIPELINE_CACHE_FILE,
                                          path, sizeof path))
        return;
    if (vkGetPipelineCacheData(V.dev, V.pcache, &n, NULL) != VK_SUCCESS || !n)
        return;
    data = malloc(n);
    if (!data)
        return;
    if (vkGetPipelineCacheData(V.dev, V.pcache, &n, data) != VK_SUCCESS) {
        free(data);
        return;
    }
    snprintf(tmp, sizeof tmp, "%s.%lu.tmp", path, (unsigned long)GetCurrentThreadId());
    f = fopen(tmp, "wb");
    if (f) {
        ok = fwrite(data, 1, n, f) == n;
        ok = (fclose(f) == 0) && ok;
        if (!ok || !MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING))
            DeleteFileA(tmp);
    }
    free(data);
}

static int create_layouts(void)
{
    VkDescriptorSetLayoutBinding b[32];
    VkDescriptorSetLayoutCreateInfo ci = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    VkPipelineLayoutCreateInfo lci = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    VkPipelineCacheCreateInfo pci = { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
    uint32_t n = 0, i;

    memset(b, 0, sizeof b);
    for (i = 0; i < sizeof g_ubo_bindings / sizeof g_ubo_bindings[0]; i++, n++) {
        b[n].binding = g_ubo_bindings[i];
        b[n].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }
    for (i = 0; i < sizeof g_tex_slots / sizeof g_tex_slots[0]; i++) {
        b[n].binding = RHI_VK_TEXTURE_BASE + g_tex_slots[i];
        b[n++].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        b[n].binding = RHI_VK_SAMPLER_BASE + g_tex_slots[i];
        b[n++].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    }
    for (i = 0; i < n; i++) {
        b[i].descriptorCount = 1;
        b[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    ci.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    ci.bindingCount = n;
    ci.pBindings = b;
    VKCHECK(vkCreateDescriptorSetLayout(V.dev, &ci, NULL, &V.set_layout), "descriptor set layout");
    lci.setLayoutCount = 1;
    lci.pSetLayouts = &V.set_layout;
    VKCHECK(vkCreatePipelineLayout(V.dev, &lci, NULL, &V.pipe_layout), "pipeline layout");
    {
        size_t initial = 0;
        void *data = pcache_load(&initial);
        VkResult r;

        pci.initialDataSize = initial;
        pci.pInitialData = data;
        r = vkCreatePipelineCache(V.dev, &pci, NULL, &V.pcache);
        if (r != VK_SUCCESS && data) {
            /* The driver refused what it wrote last time: start empty. */
            pci.initialDataSize = 0;
            pci.pInitialData = NULL;
            r = vkCreatePipelineCache(V.dev, &pci, NULL, &V.pcache);
        }
        free(data);
        VKCHECK(r, "pipeline cache");
    }
    return 0;
}

static int create_frames(void)
{
    VkSemaphoreTypeCreateInfo tci = { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
    VkSemaphoreCreateInfo sci = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    uint32_t i;

    tci.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    sci.pNext = &tci;
    VKCHECK(vkCreateSemaphore(V.dev, &sci, NULL, &V.timeline), "timeline semaphore");
    sci.pNext = NULL;
    for (i = 0; i < FRAMES; i++) {
        VkCommandPoolCreateInfo pci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        VkCommandBufferAllocateInfo ai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        pci.queueFamilyIndex = V.qfamily;
        VKCHECK(vkCreateCommandPool(V.dev, &pci, NULL, &V.frames[i].pool), "command pool");
        ai.commandPool = V.frames[i].pool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        VKCHECK(vkAllocateCommandBuffers(V.dev, &ai, &V.frames[i].cmd), "command buffer");
        VKCHECK(vkCreateSemaphore(V.dev, &sci, NULL, &V.frames[i].acquire), "semaphore");
    }
    return 0;
}

static RhiImage *v_image_create(const RhiImageDesc *d, const RhiSubresourceData *initial);
static RhiView  *v_view_create(RhiImage *img, uint32_t kind, const RhiViewDesc *d);
static void      v_image_destroy(RhiImage *img);
static void      v_view_destroy(RhiView *v);

static int create_dummies(void)
{
    static const uint8_t zero[4 * 6 * 4] = { 0 };
    RhiSubresourceData sd[6];
    RhiImageDesc d;
    VkSamplerCreateInfo si = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    uint8_t *p;
    int k;

    for (k = 0; k < 6; k++) {
        sd[k].data = zero;
        sd[k].row_pitch = 4;
        sd[k].slice_pitch = 4;
    }
    for (k = 0; k < 5; k++) {
        memset(&d, 0, sizeof d);
        d.type = k == 3 ? RHI_IMAGE_3D : RHI_IMAGE_2D;
        d.width = d.height = 1;
        d.depth = k == 1 ? 2 : k == 2 ? 6 : 1;
        d.mip_levels = 1;
        d.format = RHI_FORMAT_R8G8B8A8_UNORM;
        d.samples = k == 4 ? 4 : 1;
        d.bind = RHI_BIND_SAMPLED | (k == 4 ? RHI_BIND_RENDER_TARGET : 0);
        d.cube = k == 2;
        V.dummy_img[k] = v_image_create(&d, k == 4 ? NULL : sd);
        if (!V.dummy_img[k])
            continue;
        {
            RhiViewDesc vd;
            memset(&vd, 0, sizeof vd);
            vd.dim = k == 0 ? RHI_VIEW_DIM_2D : k == 1 ? RHI_VIEW_DIM_2D_ARRAY :
                     k == 2 ? RHI_VIEW_DIM_CUBE : k == 3 ? RHI_VIEW_DIM_3D : RHI_VIEW_DIM_2D_MS;
            vd.mip_count = 1;
            vd.layer_count = d.depth;
            V.dummy_view[k] = v_view_create(V.dummy_img[k], RHI_VIEW_SAMPLED, &vd);
        }
    }
    if (!host_buffer(65536, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 0, &V.zero_b, &V.zero_a, &p))
        return -1;
    memset(p, 0, 65536);
    vmaFlushAllocation(V.vma, V.zero_a, 0, VK_WHOLE_SIZE);
    /* D3D11's default sampler: linear, clamped. */
    si.magFilter = si.minFilter = VK_FILTER_LINEAR;
    si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.maxLod = VK_LOD_CLAMP_NONE;
    VKCHECK(vkCreateSampler(V.dev, &si, NULL, &V.default_sampler), "sampler");
    return 0;
}

static int v_device_create(const RhiDeviceDesc *dd)
{
    VkSurfaceFormatKHR formats[64];
    uint32_t nf = 64, i;

    if (volkInitialize() != VK_SUCCESS) {
        fprintf(stderr, "[RHI] vulkan: no Vulkan loader on this machine\n");
        return -1;
    }
    memset(&V, 0, sizeof V);
    memset(&SC, 0, sizeof SC);
    V.sample_mask = 0xFFFFFFFFu;
    V.topology = RHI_TOPOLOGY_TRIANGLES;
    if (create_instance(dd->debug || env_on("RECOMP_VK_VALIDATION")) != 0)
        return -1;
#if defined(_WIN32)
    {
        VkWin32SurfaceCreateInfoKHR wci = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
        wci.hinstance = GetModuleHandleA(NULL);
        wci.hwnd = (HWND)dd->window;
        VKCHECK(vkCreateWin32SurfaceKHR(V.instance, &wci, NULL, &V.surface), "window surface");
    }
#else
    fprintf(stderr, "[RHI] vulkan: no window surface on this platform yet\n");
    return -1;
#endif
    if (pick_physical_device() != 0 || create_device() != 0 || create_allocator() != 0 ||
        create_layouts() != 0 || create_frames() != 0)
        return -1;

    /* R8G8B8A8 like the D3D11 swap chain when the surface has it, so a
     * presented frame reads back the same way under both. */
    SC.format = VK_FORMAT_UNDEFINED;
    vkGetPhysicalDeviceSurfaceFormatsKHR(V.pd, V.surface, &nf, formats);
    for (i = 0; i < nf; i++)
        if (formats[i].format == VK_FORMAT_R8G8B8A8_UNORM &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            SC.format = VK_FORMAT_R8G8B8A8_UNORM;
    if (!SC.format)
        for (i = 0; i < nf; i++)
            if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM)
                SC.format = VK_FORMAT_B8G8R8A8_UNORM;
    if (!SC.format && nf)
        SC.format = formats[0].format;
    SC.interval = 0;
    SC.mode = choose_mode(0);
    SC.proxy_image.swapchain = 1;
    SC.proxy_image.refs = 1 << 30;
    SC.proxy_image.desc.type = RHI_IMAGE_2D;
    SC.proxy_image.desc.depth = 1;
    SC.proxy_image.desc.mip_levels = 1;
    SC.proxy_image.desc.samples = 1;
    SC.proxy_image.desc.format = SC.format == VK_FORMAT_B8G8R8A8_UNORM ? RHI_FORMAT_B8G8R8A8_UNORM
                                                                       : RHI_FORMAT_R8G8B8A8_UNORM;
    SC.proxy_image.desc.bind = RHI_BIND_RENDER_TARGET;
    SC.proxy_image.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    SC.proxy_view.kind = RHI_VIEW_RENDER_TARGET;
    SC.proxy_view.image = &SC.proxy_image;
    SC.proxy_view.swapchain = 1;
    SC.proxy_view.type = VK_IMAGE_VIEW_TYPE_2D;
    SC.proxy_view.layers = 1;
    if (create_swapchain(dd->width, dd->height) != 0)
        return -1;
    if (create_dummies() != 0)
        return -1;
    fprintf(stderr, "[RHI] vulkan: %s, Vulkan %u.%u, present mode %s%s, depth %s%s\n",
            V.props.deviceName, VK_API_VERSION_MAJOR(V.props.apiVersion),
            VK_API_VERSION_MINOR(V.props.apiVersion), mode_name(SC.mode),
            SC.mode == VK_PRESENT_MODE_FIFO_KHR ? " (blocks: the Burnout 2 throttle)" : "",
            V.depth24 == VK_FORMAT_D24_UNORM_S8_UINT ? "D24S8" : "D32S8",
            V.validation ? ", validation on" : "");
    return 0;
}

static void v_device_destroy(void)
{
    uint32_t i, k;

    if (!V.dev)
        return;
    if (V.recording)
        submit(VK_NULL_HANDLE);
    vkDeviceWaitIdle(V.dev);
    for (k = 0; k < 5; k++) {
        if (V.dummy_view[k]) v_view_destroy(V.dummy_view[k]);
        if (V.dummy_img[k]) v_image_destroy(V.dummy_img[k]);
    }
    for (i = 0; i < V.ntrash; i++)      /* the queue is idle: all of it can go */
        destroy_now(&V.trash[i]);
    free(V.trash);
    for (i = 0; i < V.pipe_cap; i++)
        if (V.pipes && V.pipes[i]) {
            vkDestroyPipeline(V.dev, V.pipes[i]->pipe, NULL);
            free(V.pipes[i]);
        }
    free(V.pipes);
    for (i = 0; i < FRAMES; i++) {
        Frame *f = &V.frames[i];
        for (k = 0; k < f->ring.n; k++)
            vmaDestroyBuffer(V.vma, f->ring.c[k].b, f->ring.c[k].a);
        free(f->ring.c);
        vkDestroySemaphore(V.dev, f->acquire, NULL);
        vkDestroyCommandPool(V.dev, f->pool, NULL);
    }
    if (V.read_b) vmaDestroyBuffer(V.vma, V.read_b, V.read_a);
    if (V.zero_b) vmaDestroyBuffer(V.vma, V.zero_b, V.zero_a);
    vkDestroySampler(V.dev, V.default_sampler, NULL);
    destroy_swapchain_views();
    if (SC.sc) vkDestroySwapchainKHR(V.dev, SC.sc, NULL);
    vkDestroySemaphore(V.dev, V.timeline, NULL);
    pcache_save();
    vkDestroyPipelineCache(V.dev, V.pcache, NULL);
    vkDestroyPipelineLayout(V.dev, V.pipe_layout, NULL);
    vkDestroyDescriptorSetLayout(V.dev, V.set_layout, NULL);
    vmaDestroyAllocator(V.vma);
    vkDestroyDevice(V.dev, NULL);
    vkDestroySurfaceKHR(V.instance, V.surface, NULL);
    if (V.messenger) vkDestroyDebugUtilsMessengerEXT(V.instance, V.messenger, NULL);
    vkDestroyInstance(V.instance, NULL);
    memset(&V, 0, sizeof V);
    memset(&SC, 0, sizeof SC);
}

static int v_device_ready(void)
{
    return V.dev != VK_NULL_HANDLE;
}

static RhiView *v_swapchain_view(void)
{
    return SC.sc ? &SC.proxy_view : NULL;
}

static int v_swapchain_resize(uint32_t w, uint32_t h)
{
    if (!SC.sc)
        return -1;
    end_rendering();
    SC.extent.width = w;
    SC.extent.height = h;
    return recreate_swapchain();
}

static void next_frame(void)
{
    V.frame_no++;
    V.fi = (V.fi + 1) % FRAMES;
    wait_serial(V.frames[V.fi].last_serial);
    V.frames[V.fi].ring.cur = 0;
    V.frames[V.fi].ring.off = 0;
    trash_collect();
}

static int32_t v_present(uint32_t interval)
{
    VkPresentInfoKHR pi = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    VkResult r;
    uint32_t idx;

    if (!V.dev)
        return -1;
    if ((interval != 0) != (SC.interval != 0)) {
        SC.interval = interval;
        SC.mode = choose_mode(interval);
        SC.stale = 1;                   /* the next acquire recreates it */
        fprintf(stderr, "[RHI] vulkan: present interval %u, present mode %s\n",
                interval, mode_name(SC.mode));
    }
    if (!SC.acquired) {
        /* Nothing was drawn to the screen this frame: show black, as a
         * cleared DISCARD back buffer would. */
        VkClearColorValue black;
        VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        if (!acquire()) {
            if (V.recording)
                submit(VK_NULL_HANDLE);
            next_frame();
            return 0;
        }
        end_rendering();
        memset(&black, 0, sizeof black);
        vkCmdClearColorImage(cmd(), SC.images[SC.index], VK_IMAGE_LAYOUT_GENERAL, &black, 1, &range);
    }
    end_rendering();
    idx = SC.index;
    layout_barrier(cmd(), SC.images[idx], VK_IMAGE_ASPECT_COLOR_BIT,
                   SC.layouts[idx], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    SC.layouts[idx] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    submit(SC.done[idx]);
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &SC.done[idx];
    pi.swapchainCount = 1;
    pi.pSwapchains = &SC.sc;
    pi.pImageIndices = &idx;
    r = vkQueuePresentKHR(V.queue, &pi);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR)
        SC.stale = 1;
    SC.acquired = 0;
    next_frame();
    return 0;
}

static int v_image_readback(RhiImage *img, uint32_t sub, void *dst, uint32_t dst_pitch);

static int v_swapchain_readback(void *dst, uint32_t pitch, uint32_t *w, uint32_t *h)
{
    int rc;

    if (!SC.sc || !SC.acquired)
        return -1;
    if (w) *w = SC.extent.width;
    if (h) *h = SC.extent.height;
    if (!dst)
        return 0;
    rc = v_image_readback(&SC.proxy_image, 0, dst, pitch);
    if (rc == 0 && SC.format == VK_FORMAT_B8G8R8A8_UNORM) {
        uint32_t x, y;
        for (y = 0; y < SC.extent.height; y++) {
            uint8_t *p = (uint8_t *)dst + (size_t)y * pitch;
            for (x = 0; x < SC.extent.width; x++, p += 4) {
                uint8_t t = p[0];
                p[0] = p[2];
                p[2] = t;
            }
        }
    }
    return rc;
}

/* ---- buffers (section 4.6) ------------------------------------------------------------------- */

static BufVer *new_version(RhiBuffer *b)
{
    BufVer *v;
    if (b->nver == b->cap) {
        uint32_t cap = b->cap ? b->cap * 2 : 2;
        BufVer *n = realloc(b->ver, cap * sizeof *n);
        if (!n)
            return NULL;
        b->ver = n;
        b->cap = cap;
    }
    v = &b->ver[b->nver];
    memset(v, 0, sizeof *v);
    if (!host_buffer(b->desc.size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     0, &v->b, &v->a, &v->p))
        return NULL;
    b->cur = b->nver++;
    return v;
}

/* A version the GPU has finished with, for new contents. */
static BufVer *rename_buffer(RhiBuffer *b)
{
    uint32_t i;
    if (b->nver && b->ver[b->cur].used <= V.completed)
        return &b->ver[b->cur];
    for (i = 0; i < b->nver; i++)
        if (b->ver[i].used <= V.completed) {
            b->cur = i;
            return &b->ver[i];
        }
    return new_version(b);
}

static RhiBuffer *v_buffer_create(const RhiBufferDesc *d, const void *initial)
{
    RhiBuffer *b = calloc(1, sizeof *b);

    if (!b)
        return NULL;
    b->desc = *d;
    b->uniform = (d->bind & RHI_BIND_UNIFORM) != 0;
    if (b->uniform) {
        b->shadow = calloc(1, d->size ? d->size : 16);
        if (!b->shadow) {
            free(b);
            return NULL;
        }
        if (initial)
            memcpy(b->shadow, initial, d->size);
        b->dirty = 1;
        return b;
    }
    if (!new_version(b)) {
        free(b);
        return NULL;
    }
    if (initial) {
        memcpy(b->ver[0].p, initial, d->size);
        vmaFlushAllocation(V.vma, b->ver[0].a, 0, VK_WHOLE_SIZE);
    }
    return b;
}

static void v_buffer_destroy(RhiBuffer *b)
{
    uint32_t i;
    for (i = 0; i < b->nver; i++)
        trash(T_BUFFER, (uint64_t)b->ver[i].b, b->ver[i].a);
    for (i = 0; i < MAX_VB; i++)
        if (V.vb[i].b == b)
            V.vb[i].b = NULL;
    if (V.ib == b)
        V.ib = NULL;
    for (i = 0; i < MAX_UBO; i++) {
        if (V.ubo[0][i] == b) V.ubo[0][i] = NULL;
        if (V.ubo[1][i] == b) V.ubo[1][i] = NULL;
    }
    free(b->ver);
    free(b->shadow);
    free(b);
}

static void *v_buffer_map(RhiBuffer *b, uint32_t mode)
{
    BufVer *v;
    if (b->uniform)
        return b->shadow;
    if (mode == RHI_MAP_WRITE_NO_OVERWRITE && b->nver)
        return b->ver[b->cur].p;
    v = rename_buffer(b);
    return v ? v->p : NULL;
}

static void v_buffer_unmap(RhiBuffer *b)
{
    if (b->uniform)
        b->dirty = 1;
    else if (b->nver)
        vmaFlushAllocation(V.vma, b->ver[b->cur].a, 0, VK_WHOLE_SIZE);
}

static void v_buffer_update(RhiBuffer *b, const void *data)
{
    void *p = v_buffer_map(b, RHI_MAP_WRITE_DISCARD);
    if (p) {
        memcpy(p, data, b->desc.size);
        v_buffer_unmap(b);
    }
}

/* ---- images --------------------------------------------------------------------------------- */

static void clear_image(RhiImage *img, uint32_t base_mip, uint32_t mips, uint32_t base_layer,
                        uint32_t layers, VkImageAspectFlags aspect, const float *rgba,
                        float z, uint32_t s);
static void v_image_update(RhiImage *img, uint32_t sub, const RhiBox *box,
                           const void *data, uint32_t row_pitch, uint32_t slice_pitch);

static RhiImage *v_image_create(const RhiImageDesc *d, const RhiSubresourceData *initial)
{
    VkImageCreateInfo ci = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    VmaAllocationCreateInfo ai;
    RhiImage *img;
    VkFormat f = vk_format(d->format);
    int target = (d->bind & (RHI_BIND_RENDER_TARGET | RHI_BIND_DEPTH)) != 0;

    if (f == VK_FORMAT_UNDEFINED) {
        fprintf(stderr, "[RHI] vulkan: no Vulkan format for DXGI format %u\n", (unsigned)d->format);
        return NULL;
    }
    img = calloc(1, sizeof *img);
    if (!img)
        return NULL;
    img->desc = *d;
    if (!img->desc.mip_levels) img->desc.mip_levels = 1;
    if (!img->desc.depth) img->desc.depth = 1;
    if (!img->desc.samples) img->desc.samples = 1;
    img->refs = 1;
    img->format = f;
    img->aspect = aspect_of(f);
    img->steady = target ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    ci.imageType = d->type == RHI_IMAGE_3D ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
    ci.format = f;
    ci.extent.width = d->width ? d->width : 1;
    ci.extent.height = d->height ? d->height : 1;
    ci.extent.depth = d->type == RHI_IMAGE_3D ? img->desc.depth : 1;
    ci.mipLevels = img->desc.mip_levels;
    ci.arrayLayers = d->type == RHI_IMAGE_3D ? 1 : img->desc.depth;
    ci.samples = (VkSampleCountFlagBits)img->desc.samples;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (d->bind & RHI_BIND_SAMPLED)       ci.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (d->bind & RHI_BIND_RENDER_TARGET) ci.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (d->bind & RHI_BIND_DEPTH)         ci.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (!(ci.usage & (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)))
        ci.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (d->cube)
        ci.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    memset(&ai, 0, sizeof ai);
    ai.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    if (vmaCreateImage(V.vma, &ci, &ai, &img->image, &img->alloc, NULL) != VK_SUCCESS) {
        fprintf(stderr, "[RHI] vulkan: image %ux%ux%u format %u could not be made\n",
                ci.extent.width, ci.extent.height, ci.arrayLayers, (unsigned)d->format);
        free(img);
        return NULL;
    }
    end_rendering();
    layout_barrier(cmd(), img->image, img->aspect, VK_IMAGE_LAYOUT_UNDEFINED, img->steady);
    img->layout = img->steady;
    if (target) {
        /* Drivers hand D3D11 zeroed targets; do the same, so a frame that
         * reads a target before writing it reads the same either way. */
        static const float zero[4] = { 0 };
        clear_image(img, 0, img->desc.mip_levels, 0, ci.arrayLayers, img->aspect, zero, 0.0f, 0);
    } else {
        full_barrier(cmd());
    }
    if (initial) {
        uint32_t n = img->desc.mip_levels * (d->type == RHI_IMAGE_3D ? 1 : img->desc.depth), i;
        for (i = 0; i < n; i++)
            if (initial[i].data)
                v_image_update(img, i, NULL, initial[i].data, initial[i].row_pitch,
                               initial[i].slice_pitch);
    }
    if (trace_on())
        fprintf(stderr, "[VKTRACE] image %p %ux%ux%u mips %u fmt %u samples %u bind 0x%X%s\n",
                (void *)img, d->width, d->height, img->desc.depth, img->desc.mip_levels,
                (unsigned)d->format, img->desc.samples, d->bind, d->cube ? " cube" : "");
    return img;
}

static RhiImage *v_image_retain(RhiImage *img)
{
    img->refs++;
    return img;
}

static void v_image_destroy(RhiImage *img)
{
    if (img->swapchain || --img->refs > 0)
        return;
    trash(T_IMAGE, (uint64_t)img->image, img->alloc);
    free(img);
}

static void v_image_get_desc(const RhiImage *img, RhiImageDesc *out)
{
    *out = img->desc;
}

static int v_sample_count_supported(RhiFormat f, uint32_t samples)
{
    VkImageFormatProperties p;
    VkFormat vf = vk_format(f);
    VkImageUsageFlags u = is_depth_format(vf) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                                              : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (vf == VK_FORMAT_UNDEFINED ||
        vkGetPhysicalDeviceImageFormatProperties(V.pd, vf, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                                                 u, 0, &p) != VK_SUCCESS)
        return 0;
    return (p.sampleCounts & samples) != 0;
}

static void to_layout(RhiImage *img, VkImageLayout to)
{
    VkImageLayout *cur = img->swapchain ? &SC.layouts[SC.index] : &img->layout;
    if (*cur == to)
        return;
    layout_barrier(cmd(), image_handle(img), img->aspect, *cur, to);
    *cur = to;
}

static void v_image_update(RhiImage *img, uint32_t sub, const RhiBox *box,
                           const void *data, uint32_t row_pitch, uint32_t slice_pitch)
{
    uint32_t mips = img->desc.mip_levels, mip = sub % mips, layer, w, h, dz;
    uint32_t x = 0, y = 0, z = 0, bw, bh, bd, row_bytes, rows, r, k;
    VkBuffer staging;
    VkDeviceSize off;
    VkBufferImageCopy region;
    uint8_t *p;

    if (img->swapchain || !data)
        return;
    if (trace_on())
        fprintf(stderr, "[VKTRACE] update %p sub %u box %s pitch %u slice %u\n", (void *)img, sub,
                box ? "yes" : "no", row_pitch, slice_pitch);
    layer = img->desc.type == RHI_IMAGE_3D ? 0 : sub / mips;
    w = img->desc.width >> mip;  if (!w) w = 1;
    h = img->desc.height >> mip; if (!h) h = 1;
    dz = img->desc.type == RHI_IMAGE_3D ? img->desc.depth >> mip : 1; if (!dz) dz = 1;
    bw = w; bh = h; bd = dz;
    if (box) {
        x = box->left; y = box->top; z = box->front;
        bw = box->right > box->left ? box->right - box->left : 0;
        bh = box->bottom > box->top ? box->bottom - box->top : 0;
        bd = box->back > box->front ? box->back - box->front : 0;
        if (x + bw > w) bw = x < w ? w - x : 0;
        if (y + bh > h) bh = y < h ? h - y : 0;
        if (z + bd > dz) bd = z < dz ? dz - z : 0;
    }
    if (!bw || !bh || !bd)
        return;
    row_bytes = rhi_format_row_pitch(img->desc.format, bw);
    rows = rhi_format_rows(img->desc.format, bh);
    if (!row_bytes || !rows) {
        static int said;
        if (!said++)
            fprintf(stderr, "[RHI] vulkan: no size known for DXGI format %u; upload skipped\n",
                    (unsigned)img->desc.format);
        return;
    }
    if (!row_pitch)
        row_pitch = row_bytes;
    if (!slice_pitch)
        slice_pitch = row_pitch * rows;
    p = ring_alloc((VkDeviceSize)row_bytes * rows * bd, 16, &staging, &off);
    if (!p)
        return;
    for (k = 0; k < bd; k++)
        for (r = 0; r < rows; r++)
            memcpy(p + ((size_t)k * rows + r) * row_bytes,
                   (const uint8_t *)data + (size_t)k * slice_pitch + (size_t)r * row_pitch,
                   row_bytes < row_pitch ? row_bytes : row_pitch);

    end_rendering();
    if (img->steady != VK_IMAGE_LAYOUT_GENERAL)
        to_layout(img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    memset(&region, 0, sizeof region);
    region.bufferOffset = off;
    region.imageSubresource.aspectMask = img->aspect & ~VK_IMAGE_ASPECT_STENCIL_BIT;
    region.imageSubresource.mipLevel = mip;
    region.imageSubresource.baseArrayLayer = layer;
    region.imageSubresource.layerCount = 1;
    region.imageOffset.x = (int32_t)x;
    region.imageOffset.y = (int32_t)y;
    region.imageOffset.z = (int32_t)z;
    region.imageExtent.width = bw;
    region.imageExtent.height = bh;
    region.imageExtent.depth = bd;
    vkCmdCopyBufferToImage(cmd(), staging, img->image, img->layout, 1, &region);
    to_layout(img, img->steady);
    full_barrier(cmd());
}

static int ensure_readback(VkDeviceSize size)
{
    if (V.read_size >= size)
        return 1;
    if (V.read_b) {
        flush_wait();
        vmaDestroyBuffer(V.vma, V.read_b, V.read_a);
        V.read_b = VK_NULL_HANDLE;
    }
    if (!host_buffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, 1, &V.read_b, &V.read_a, &V.read_p))
        return 0;
    V.read_size = size;
    return 1;
}

static int v_image_readback(RhiImage *img, uint32_t sub, void *dst, uint32_t dst_pitch)
{
    uint32_t mips, mip, layer, w, h, row_bytes, rows, r;
    VkImage src, resolved = VK_NULL_HANDLE;
    VmaAllocation resolved_a = VK_NULL_HANDLE;
    VkImageLayout src_layout;
    VkBufferImageCopy region;
    VkFormat f;

    if (!img || img->desc.type != RHI_IMAGE_2D || !dst)
        return -1;
    f = image_format(img);
    if (is_depth_format(f))
        return -1;
    mips = img->desc.mip_levels ? img->desc.mip_levels : 1;
    mip = sub % mips;
    layer = sub / mips;
    w = img->desc.width >> mip;  if (!w) w = 1;
    h = img->desc.height >> mip; if (!h) h = 1;
    row_bytes = rhi_format_row_pitch(img->desc.format, w);
    rows = rhi_format_rows(img->desc.format, h);
    if (!row_bytes || !ensure_readback((VkDeviceSize)row_bytes * rows))
        return -1;
    end_rendering();
    if ((src = image_handle(img)) == VK_NULL_HANDLE)
        return -1;
    if (!img->swapchain && img->steady != VK_IMAGE_LAYOUT_GENERAL)
        to_layout(img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    src_layout = img->swapchain ? SC.layouts[SC.index] : img->layout;

    if (img->desc.samples > 1) {
        /* A multisampled image is resolved into a plain one first. */
        VkImageCreateInfo ci = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        VmaAllocationCreateInfo ai;
        VkImageResolve rv;

        ci.imageType = VK_IMAGE_TYPE_2D;
        ci.format = f;
        ci.extent.width = w;
        ci.extent.height = h;
        ci.extent.depth = 1;
        ci.mipLevels = 1;
        ci.arrayLayers = 1;
        ci.samples = VK_SAMPLE_COUNT_1_BIT;
        ci.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        memset(&ai, 0, sizeof ai);
        ai.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        if (vmaCreateImage(V.vma, &ci, &ai, &resolved, &resolved_a, NULL) != VK_SUCCESS)
            return -1;
        layout_barrier(cmd(), resolved, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                       VK_IMAGE_LAYOUT_GENERAL);
        memset(&rv, 0, sizeof rv);
        rv.srcSubresource.aspectMask = rv.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        rv.srcSubresource.mipLevel = mip;
        rv.srcSubresource.baseArrayLayer = layer;
        rv.srcSubresource.layerCount = rv.dstSubresource.layerCount = 1;
        rv.extent = ci.extent;
        vkCmdResolveImage(cmd(), src, src_layout, resolved, VK_IMAGE_LAYOUT_GENERAL, 1, &rv);
        full_barrier(cmd());
        src = resolved;
        src_layout = VK_IMAGE_LAYOUT_GENERAL;
        mip = layer = 0;
    }
    memset(&region, 0, sizeof region);
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = mip;
    region.imageSubresource.baseArrayLayer = layer;
    region.imageSubresource.layerCount = 1;
    region.imageExtent.width = w;
    region.imageExtent.height = h;
    region.imageExtent.depth = 1;
    vkCmdCopyImageToBuffer(cmd(), src, src_layout, V.read_b, 1, &region);
    if (!img->swapchain)
        to_layout(img, img->steady);
    full_barrier(cmd());
    flush_wait();
    if (resolved)
        vmaDestroyImage(V.vma, resolved, resolved_a);
    vmaInvalidateAllocation(V.vma, V.read_a, 0, VK_WHOLE_SIZE);
    for (r = 0; r < rows; r++)
        memcpy((uint8_t *)dst + (size_t)r * dst_pitch, V.read_p + (size_t)r * row_bytes,
               row_bytes < dst_pitch ? row_bytes : dst_pitch);
    return 0;
}

/* ---- views -------------------------------------------------------------------------------------- */

static RhiView *v_view_create(RhiImage *img, uint32_t kind, const RhiViewDesc *d)
{
    VkImageViewCreateInfo ci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    RhiView *v;
    RhiFormat rf = (d && d->format) ? d->format : img->desc.format;
    uint32_t layers_total = img->desc.type == RHI_IMAGE_3D ? 1 : img->desc.depth;
    uint32_t dim;

    if (img->swapchain)
        return kind == RHI_VIEW_RENDER_TARGET ? &SC.proxy_view : NULL;
    v = calloc(1, sizeof *v);
    if (!v)
        return NULL;
    v->kind = kind;
    v->image = img;
    if (d) {
        dim = d->dim;
    } else if (img->desc.type == RHI_IMAGE_3D) {
        dim = RHI_VIEW_DIM_3D;
    } else if (img->desc.samples > 1) {
        dim = RHI_VIEW_DIM_2D_MS;
    } else if (img->desc.cube && kind == RHI_VIEW_SAMPLED) {
        dim = RHI_VIEW_DIM_CUBE;
    } else {
        dim = layers_total > 1 ? RHI_VIEW_DIM_2D_ARRAY : RHI_VIEW_DIM_2D;
    }
    ci.image = img->image;
    ci.format = vk_format(rf);
    ci.subresourceRange.aspectMask = img->aspect;
    if (kind == RHI_VIEW_SAMPLED && is_depth_format(ci.format))
        ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (kind == RHI_VIEW_SAMPLED) {
        ci.subresourceRange.baseMipLevel = d ? d->base_mip : 0;
        ci.subresourceRange.levelCount = (d && d->mip_count && d->mip_count != 0xFFFFFFFFu)
                                         ? d->mip_count : VK_REMAINING_MIP_LEVELS;
    } else {
        ci.subresourceRange.baseMipLevel = d ? d->base_mip : 0;
        ci.subresourceRange.levelCount = 1;
    }
    switch (dim) {
    case RHI_VIEW_DIM_3D:
        ci.viewType = VK_IMAGE_VIEW_TYPE_3D;
        ci.subresourceRange.layerCount = 1;
        break;
    case RHI_VIEW_DIM_CUBE:
        ci.viewType = kind == RHI_VIEW_SAMPLED ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
        ci.subresourceRange.baseArrayLayer = d ? d->base_layer : 0;
        ci.subresourceRange.layerCount = kind == RHI_VIEW_SAMPLED ? 6 : 1;
        break;
    case RHI_VIEW_DIM_2D_ARRAY:
        ci.subresourceRange.baseArrayLayer = d ? d->base_layer : 0;
        ci.subresourceRange.layerCount = (d && d->layer_count) ? d->layer_count : layers_total;
        /* A target is one layer of the array, the way the renderer uses it. */
        if (kind != RHI_VIEW_SAMPLED)
            ci.subresourceRange.layerCount = 1;
        ci.viewType = (kind == RHI_VIEW_SAMPLED && ci.subresourceRange.layerCount > 1)
                      ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        if (kind == RHI_VIEW_SAMPLED && d && d->dim == RHI_VIEW_DIM_2D_ARRAY)
            ci.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        break;
    default:
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.subresourceRange.layerCount = 1;
        break;
    }
    if (kind == RHI_VIEW_SAMPLED) {
        /* The formats Vulkan has no exact twin for (section 4.9). */
        if (rf == RHI_FORMAT_A8_UNORM) {
            ci.components.r = ci.components.g = ci.components.b = VK_COMPONENT_SWIZZLE_ZERO;
            ci.components.a = VK_COMPONENT_SWIZZLE_R;
        } else if (rf == RHI_FORMAT_B8G8R8X8_UNORM) {
            ci.components.a = VK_COMPONENT_SWIZZLE_ONE;
        }
    }
    if (vkCreateImageView(V.dev, &ci, NULL, &v->view) != VK_SUCCESS) {
        free(v);
        return NULL;
    }
    v->type = ci.viewType;
    v->base_mip = ci.subresourceRange.baseMipLevel;
    v->base_layer = ci.subresourceRange.baseArrayLayer;
    v->layers = ci.subresourceRange.layerCount;
    v->width = img->desc.width >> v->base_mip;   if (!v->width) v->width = 1;
    v->height = img->desc.height >> v->base_mip; if (!v->height) v->height = 1;
    img->refs++;                        /* a view holds its image, as in D3D11 */
    return v;
}

static void v_view_destroy(RhiView *v)
{
    uint32_t i;
    if (!v || v->swapchain)
        return;
    if (V.rt_color == v || V.rt_depth == v) {
        end_rendering();
        if (V.rt_color == v) V.rt_color = NULL;
        if (V.rt_depth == v) V.rt_depth = NULL;
    }
    for (i = 0; i < MAX_TEX; i++)
        if (V.tex[i] == v)
            V.tex[i] = NULL;
    trash(T_VIEW, (uint64_t)v->view, NULL);
    v_image_destroy(v->image);
    free(v);
}

static RhiImage *v_view_image(const RhiView *v)
{
    return v->image;
}

/* ---- clears ---------------------------------------------------------------------------------------- */

static void clear_image(RhiImage *img, uint32_t base_mip, uint32_t mips, uint32_t base_layer,
                        uint32_t layers, VkImageAspectFlags aspect, const float *rgba,
                        float z, uint32_t s)
{
    VkImageSubresourceRange range;
    VkImage h;

    end_rendering();
    if ((h = image_handle(img)) == VK_NULL_HANDLE)
        return;
    range.aspectMask = aspect;
    range.baseMipLevel = base_mip;
    range.levelCount = mips;
    range.baseArrayLayer = base_layer;
    range.layerCount = layers;
    if (aspect & VK_IMAGE_ASPECT_COLOR_BIT) {
        VkClearColorValue c;
        memcpy(c.float32, rgba, sizeof c.float32);
        vkCmdClearColorImage(cmd(), h, img->swapchain ? SC.layouts[SC.index] : img->layout,
                             &c, 1, &range);
    } else {
        VkClearDepthStencilValue ds;
        ds.depth = z;
        ds.stencil = s;
        vkCmdClearDepthStencilImage(cmd(), h, img->layout, &ds, 1, &range);
    }
    full_barrier(cmd());
}

/* A clear of the bound target is an attachment clear inside the rendering
 * scope; anything else is an image clear outside one. Either way it covers
 * the whole view and ignores the scissor, as ClearRenderTargetView does. */
static int clear_in_scope(RhiView *v, VkImageAspectFlags aspect, const float *rgba, float z, uint32_t s)
{
    VkClearAttachment a;
    VkClearRect r;

    if (!(aspect & VK_IMAGE_ASPECT_COLOR_BIT) ? V.rt_depth != v : V.rt_color != v)
        return 0;
    if (!begin_rendering() || V.area.width != v->width || V.area.height != v->height)
        return 0;
    memset(&a, 0, sizeof a);
    a.aspectMask = aspect;
    /* VkClearValue is a union: colour or depth, never both. */
    if (aspect & VK_IMAGE_ASPECT_COLOR_BIT) {
        memcpy(a.clearValue.color.float32, rgba, sizeof a.clearValue.color.float32);
    } else {
        a.clearValue.depthStencil.depth = z;
        a.clearValue.depthStencil.stencil = s;
    }
    memset(&r, 0, sizeof r);
    r.rect.extent = V.area;
    r.layerCount = 1;
    vkCmdClearAttachments(cmd(), 1, &a, 1, &r);
    return 1;
}

static void v_clear_color(RhiView *v, const float *rgba)
{
    if (!v || v->kind != RHI_VIEW_RENDER_TARGET)
        return;
    if (trace_on())
        fprintf(stderr, "[VKTRACE] clear color %p (%s) %.3f %.3f %.3f %.3f\n", (void *)v,
                V.rt_color == v ? "bound" : "not bound", rgba[0], rgba[1], rgba[2], rgba[3]);
    if (clear_in_scope(v, VK_IMAGE_ASPECT_COLOR_BIT, rgba, 0.0f, 0))
        return;
    clear_image(v->image, v->base_mip, 1, v->base_layer, v->layers ? v->layers : 1,
                VK_IMAGE_ASPECT_COLOR_BIT, rgba, 0.0f, 0);
}

static void v_clear_depth(RhiView *v, uint32_t flags, float z, uint8_t s)
{
    VkImageAspectFlags aspect = 0;
    if (!v || v->kind != RHI_VIEW_DEPTH)
        return;
    if (flags & RHI_CLEAR_DEPTH)
        aspect |= VK_IMAGE_ASPECT_DEPTH_BIT;
    if ((flags & RHI_CLEAR_STENCIL) && has_stencil(v->image->format))
        aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    if (!aspect)
        return;
    if (clear_in_scope(v, aspect, NULL, z, s))
        return;
    clear_image(v->image, v->base_mip, 1, v->base_layer, v->layers ? v->layers : 1,
                aspect, NULL, z, s);
}

/* ---- SPIR-V reflection ------------------------------------------------------------------------------ */

enum {
    OP_NAME = 5, OP_TYPE_BOOL = 20, OP_TYPE_INT = 21, OP_TYPE_FLOAT = 22, OP_TYPE_VECTOR = 23,
    OP_TYPE_MATRIX = 24, OP_TYPE_IMAGE = 25, OP_TYPE_SAMPLER = 26, OP_TYPE_ARRAY = 28,
    OP_TYPE_STRUCT = 30, OP_TYPE_POINTER = 32, OP_CONSTANT = 43, OP_VARIABLE = 59,
    OP_DECORATE = 71, OP_MEMBER_DECORATE = 72
};
enum { DEC_BLOCK = 2, DEC_ARRAY_STRIDE = 6, DEC_MATRIX_STRIDE = 7, DEC_BUILTIN = 11,
       DEC_LOCATION = 30, DEC_BINDING = 33, DEC_OFFSET = 35 };
enum { SC_UNIFORM_CONSTANT = 0, SC_INPUT = 1, SC_UNIFORM = 2, SC_OUTPUT = 3 };

typedef struct {
    uint16_t    op;
    uint8_t     block, builtin;
    uint32_t    a, b, c;            /* per-opcode operands */
    int32_t     binding, location;
    uint32_t    loc_word, array_stride, cval;
    const char *name;
} SpvId;

typedef struct { uint32_t id, member, offset, mstride; } SpvMember;

typedef struct {
    const uint32_t *w;
    SpvId          *ids;
    uint32_t        bound;
    SpvMember      *m;
    uint32_t        nm, mcap;
} Spv;

static SpvMember *member_of(Spv *s, uint32_t id, uint32_t member)
{
    uint32_t i;
    for (i = 0; i < s->nm; i++)
        if (s->m[i].id == id && s->m[i].member == member)
            return &s->m[i];
    if (s->nm == s->mcap) {
        uint32_t cap = s->mcap ? s->mcap * 2 : 64;
        SpvMember *n = realloc(s->m, cap * sizeof *n);
        if (!n)
            return NULL;
        s->m = n;
        s->mcap = cap;
    }
    memset(&s->m[s->nm], 0, sizeof s->m[0]);
    s->m[s->nm].id = id;
    s->m[s->nm].member = member;
    return &s->m[s->nm++];
}

static uint32_t type_size(Spv *s, uint32_t id, uint32_t mstride, int depth)
{
    SpvId *t;
    if (id >= s->bound || depth > 16)
        return 0;
    t = &s->ids[id];
    switch (t->op) {
    case OP_TYPE_BOOL:   return 4;
    case OP_TYPE_INT:
    case OP_TYPE_FLOAT:  return t->a / 8;
    case OP_TYPE_VECTOR: return t->b * type_size(s, t->a, 0, depth + 1);
    case OP_TYPE_MATRIX: return t->b * (mstride ? mstride : 16);
    case OP_TYPE_ARRAY: {
        uint32_t len = t->b < s->bound ? s->ids[t->b].cval : 0;
        uint32_t stride = t->array_stride ? t->array_stride : type_size(s, t->a, mstride, depth + 1);
        return len * stride;
    }
    case OP_TYPE_STRUCT: {
        uint32_t i, end = 0;
        for (i = 0; i < t->b; i++) {
            SpvMember *m = member_of(s, id, i);
            uint32_t e = (m ? m->offset : 0) +
                         type_size(s, s->w[t->a + i], m ? m->mstride : 0, depth + 1);
            if (e > end)
                end = e;
        }
        return end;
    }
    default:
        return 0;
    }
}

/* "in.var.TEXCOORD3" -> TEXCOORD, 3; "out.var.COLOR" -> COLOR, 0. */
static void semantic_of(const char *name, char *sem, size_t n, uint32_t *index)
{
    const char *dot = strrchr(name, '.');
    size_t len, k;
    const char *p = dot ? dot + 1 : name;

    len = strlen(p);
    while (len && isdigit((unsigned char)p[len - 1]))
        len--;
    *index = (uint32_t)atoi(p + len);
    if (len >= n)
        len = n - 1;
    for (k = 0; k < len; k++)
        sem[k] = (char)toupper((unsigned char)p[k]);
    sem[len] = 0;
}

static int reflect(RhiShader *sh)
{
    Spv s;
    size_t i;
    uint32_t id;

    memset(&s, 0, sizeof s);
    if (sh->words < 5 || sh->code[0] != 0x07230203u)
        return 0;
    s.w = sh->code;
    s.bound = sh->code[3];
    s.ids = calloc(s.bound ? s.bound : 1, sizeof *s.ids);
    if (!s.ids)
        return 0;
    for (id = 0; id < s.bound; id++)
        s.ids[id].binding = s.ids[id].location = -1;
    for (i = 5; i < sh->words; ) {
        const uint32_t *in = sh->code + i;
        uint32_t len = in[0] >> 16, op = in[0] & 0xFFFFu;
        if (!len || i + len > sh->words)
            break;
        switch (op) {
        case OP_NAME:
            if (in[1] < s.bound) s.ids[in[1]].name = (const char *)(in + 2);
            break;
        case OP_DECORATE:
            if (in[1] >= s.bound || len < 3) break;
            switch (in[2]) {
            case DEC_BINDING:      s.ids[in[1]].binding = (int32_t)in[3]; break;
            case DEC_LOCATION:     s.ids[in[1]].location = (int32_t)in[3];
                                   s.ids[in[1]].loc_word = (uint32_t)(i + 3); break;
            case DEC_BLOCK:        s.ids[in[1]].block = 1; break;
            case DEC_BUILTIN:      s.ids[in[1]].builtin = 1; break;
            case DEC_ARRAY_STRIDE: s.ids[in[1]].array_stride = in[3]; break;
            }
            break;
        case OP_MEMBER_DECORATE:
            if (len >= 5 && (in[3] == DEC_OFFSET || in[3] == DEC_MATRIX_STRIDE)) {
                SpvMember *m = member_of(&s, in[1], in[2]);
                if (m) {
                    if (in[3] == DEC_OFFSET) m->offset = in[4];
                    else m->mstride = in[4];
                }
            }
            break;
        case OP_TYPE_BOOL: case OP_TYPE_SAMPLER:
            if (in[1] < s.bound) s.ids[in[1]].op = (uint16_t)op;
            break;
        case OP_TYPE_INT: case OP_TYPE_FLOAT:
            if (in[1] < s.bound) { s.ids[in[1]].op = (uint16_t)op; s.ids[in[1]].a = in[2]; }
            break;
        case OP_TYPE_VECTOR: case OP_TYPE_MATRIX: case OP_TYPE_ARRAY:
            if (in[1] < s.bound) { s.ids[in[1]].op = (uint16_t)op; s.ids[in[1]].a = in[2]; s.ids[in[1]].b = in[3]; }
            break;
        case OP_TYPE_IMAGE:
            if (in[1] < s.bound && len >= 9) {
                s.ids[in[1]].op = (uint16_t)op;
                s.ids[in[1]].a = in[3];     /* Dim */
                s.ids[in[1]].b = in[5];     /* Arrayed */
                s.ids[in[1]].c = in[6];     /* MS */
            }
            break;
        case OP_TYPE_STRUCT:
            if (in[1] < s.bound) {
                s.ids[in[1]].op = (uint16_t)op;
                s.ids[in[1]].a = (uint32_t)(i + 2);     /* where the member types start */
                s.ids[in[1]].b = len - 2;
            }
            break;
        case OP_TYPE_POINTER:
            if (in[1] < s.bound) { s.ids[in[1]].op = (uint16_t)op; s.ids[in[1]].a = in[2]; s.ids[in[1]].b = in[3]; }
            break;
        case OP_CONSTANT:
            if (len >= 4 && in[2] < s.bound) s.ids[in[2]].cval = in[3];
            break;
        case OP_VARIABLE:
            if (in[2] < s.bound) { s.ids[in[2]].op = (uint16_t)op; s.ids[in[2]].a = in[1]; s.ids[in[2]].b = in[3]; }
            break;
        }
        i += len;
    }

    for (id = 0; id < s.bound; id++) {
        SpvId *v = &s.ids[id], *ptr, *t;
        uint32_t pointee;
        if (v->op != OP_VARIABLE || v->a >= s.bound)
            continue;
        ptr = &s.ids[v->a];
        pointee = ptr->b;
        if (pointee >= s.bound)
            continue;
        t = &s.ids[pointee];
        if ((v->b == SC_UNIFORM || v->b == SC_UNIFORM_CONSTANT) && v->binding >= 0 && sh->nb < MAX_REFL) {
            Refl *r = &sh->b[sh->nb];
            memset(r, 0, sizeof *r);
            r->binding = (uint32_t)v->binding;
            if (t->op == OP_TYPE_STRUCT) {
                r->kind = 1;
                r->block_size = type_size(&s, pointee, 0, 0);
            } else if (t->op == OP_TYPE_IMAGE) {
                r->kind = 2;
                r->dim = t->a;
                r->arrayed = t->b;
                r->ms = t->c;
            } else if (t->op == OP_TYPE_SAMPLER) {
                r->kind = 3;
            } else {
                continue;
            }
            sh->nb++;
        } else if ((v->b == SC_INPUT || v->b == SC_OUTPUT) && v->location >= 0 && !v->builtin) {
            ReflIO *io;
            if (v->b == SC_INPUT) {
                if (sh->nin >= 32) continue;
                io = &sh->in[sh->nin++];
            } else {
                if (sh->nout >= 32) continue;
                io = &sh->out[sh->nout++];
            }
            semantic_of(v->name ? v->name : "", io->sem, sizeof io->sem, &io->index);
            io->location = (uint32_t)v->location;
            io->loc_word = v->loc_word;
        }
    }
    free(s.ids);
    free(s.m);
    return 1;
}

/* ---- shaders and layouts -------------------------------------------------------------------------------- */

/* Names the compiler and its settings in the disk cache's key: DXC to
 * SPIR-V with rhi_vk_dxc_compile's arguments, which bake in the binding
 * shifts below. Bump the trailing number if those arguments change. */
#define VK_CACHE_STR2(x) #x
#define VK_CACHE_STR(x)  VK_CACHE_STR2(x)
#define VK_CACHE_TAG "vulkan-dxc-1-b" VK_CACHE_STR(RHI_VK_PS_UNIFORM_BASE) \
                     "-t" VK_CACHE_STR(RHI_VK_TEXTURE_BASE) "-s" VK_CACHE_STR(RHI_VK_SAMPLER_BASE)

static RhiShader *v_shader_create(uint32_t stage, const RhiShaderSource *src, char *err, size_t err_len)
{
    VkShaderModuleCreateInfo ci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    RhiShader *s;
    uint32_t *code = NULL;
    size_t bytes = 0;

    d3d8_hlsl_note(src->hlsl, src->len, src->name, src->macros, src->entry, src->target);
    /* A blob from an earlier run (rhi_shader_cache.c) skips DXC, which a
     * fully warm run then never loads. */
    if (!rhi_shader_cache_get(src, VK_CACHE_TAG, (void **)&code, &bytes) ||
        bytes < 20 || (bytes & 3)) {
        free(code);
        code = NULL;
        if (!rhi_vk_dxc_compile(stage, src, &code, &bytes, err, err_len))
            return NULL;
        rhi_shader_cache_put(src, VK_CACHE_TAG, code, bytes);
    }
    s = calloc(1, sizeof *s);
    if (!s) {
        free(code);
        return NULL;
    }
    s->stage = stage;
    s->id = ++V.next_id;
    s->code = code;
    s->words = bytes / 4;
    snprintf(s->entry, sizeof s->entry, "%s", src->entry ? src->entry : "main");
    ci.codeSize = bytes;
    ci.pCode = code;
    if (vkCreateShaderModule(V.dev, &ci, NULL, &s->module) != VK_SUCCESS || !reflect(s)) {
        if (err && err_len)
            snprintf(err, err_len, "vkCreateShaderModule failed");
        if (s->module) vkDestroyShaderModule(V.dev, s->module, NULL);
        free(code);
        free(s);
        return NULL;
    }
    return s;
}

static void v_shader_destroy(RhiShader *s)
{
    if (V.vs == s) V.vs = NULL;
    if (V.ps == s) V.ps = NULL;
    trash(T_MODULE, (uint64_t)s->module, NULL);
    free(s->code);
    free(s);
}

static RhiVertexLayout *v_vertex_layout_create(const RhiVertexElement *e, uint32_t n, const RhiShader *vs)
{
    RhiVertexLayout *l;
    uint32_t i;

    (void)vs;
    if (n > 32)
        return NULL;
    l = calloc(1, sizeof *l);
    if (!l)
        return NULL;
    l->id = ++V.next_id;
    l->n = n;
    for (i = 0; i < n; i++) {
        l->e[i] = e[i];
        snprintf(l->names[i], sizeof l->names[i], "%s", e[i].semantic ? e[i].semantic : "");
        l->e[i].semantic = l->names[i];
    }
    return l;
}

static void v_vertex_layout_destroy(RhiVertexLayout *l)
{
    if (V.layout == l)
        V.layout = NULL;
    free(l);
}

/* ---- state objects -------------------------------------------------------------------------------------- */

static RhiBlendState *v_blend_state_create(const RhiBlendDesc *d)
{
    RhiBlendState *s = calloc(1, sizeof *s);
    if (s) s->d = *d;
    return s;
}

static RhiDepthState *v_depth_state_create(const RhiDepthDesc *d)
{
    RhiDepthState *s = calloc(1, sizeof *s);
    if (s) s->d = *d;
    return s;
}

static RhiRasterState *v_raster_state_create(const RhiRasterDesc *d)
{
    RhiRasterState *s = calloc(1, sizeof *s);
    if (s) s->d = *d;
    return s;
}

static VkSamplerAddressMode vk_address(uint32_t a)
{
    switch (a) {
    case RHI_ADDRESS_MIRROR:      return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case RHI_ADDRESS_CLAMP:       return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case RHI_ADDRESS_BORDER:      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    case RHI_ADDRESS_MIRROR_ONCE: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    default:                      return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

static RhiSampler *v_sampler_create(const RhiSamplerDesc *d)
{
    VkSamplerCreateInfo si = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    VkSamplerCustomBorderColorCreateInfoEXT cb = { VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT };
    RhiSampler *s = calloc(1, sizeof *s);

    if (!s)
        return NULL;
    /* D3D11_FILTER: bit 0 mip linear, bit 2 mag linear, bit 4 min linear,
     * bit 6 anisotropic, bit 7 comparison. */
    si.minFilter = (d->filter & 0x10) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    si.magFilter = (d->filter & 0x04) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    si.mipmapMode = (d->filter & 0x01) ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    if ((d->filter & 0x40) && V.feat_aniso) {
        si.anisotropyEnable = VK_TRUE;
        si.maxAnisotropy = (float)(d->max_anisotropy ? d->max_anisotropy : 1);
        if (si.maxAnisotropy > V.props.limits.maxSamplerAnisotropy)
            si.maxAnisotropy = V.props.limits.maxSamplerAnisotropy;
    }
    si.compareEnable = (d->filter & 0x80) ? VK_TRUE : VK_FALSE;
    si.compareOp = d->compare ? (VkCompareOp)(d->compare - 1) : VK_COMPARE_OP_NEVER;
    si.addressModeU = vk_address(d->address_u);
    si.addressModeV = vk_address(d->address_v);
    si.addressModeW = vk_address(d->address_w);
    si.mipLodBias = d->mip_lod_bias;
    if (si.mipLodBias > V.props.limits.maxSamplerLodBias)
        si.mipLodBias = V.props.limits.maxSamplerLodBias;
    if (si.mipLodBias < -V.props.limits.maxSamplerLodBias)
        si.mipLodBias = -V.props.limits.maxSamplerLodBias;
    si.minLod = d->min_lod;
    si.maxLod = d->max_lod >= FLT_MAX ? VK_LOD_CLAMP_NONE : d->max_lod;
    if (V.feat_border) {
        memcpy(cb.customBorderColor.float32, d->border, sizeof cb.customBorderColor.float32);
        cb.format = VK_FORMAT_UNDEFINED;
        si.borderColor = VK_BORDER_COLOR_FLOAT_CUSTOM_EXT;
        si.pNext = &cb;
    } else if (d->border[3] < 0.5f) {
        si.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    } else {
        si.borderColor = (d->border[0] + d->border[1] + d->border[2] > 1.5f)
                         ? VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE : VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    }
    s->d = *d;
    if (vkCreateSampler(V.dev, &si, NULL, &s->s) != VK_SUCCESS) {
        free(s);
        return NULL;
    }
    if (trace_on())
        fprintf(stderr, "[VKTRACE] sampler filter 0x%X address %u/%u/%u bias %.2f lod %.1f..%.1f\n",
                d->filter, d->address_u, d->address_v, d->address_w, d->mip_lod_bias,
                d->min_lod, d->max_lod);
    return s;
}

static void v_blend_state_destroy(RhiBlendState *s)   { if (V.blend == s) V.blend = NULL; free(s); }
static void v_depth_state_destroy(RhiDepthState *s)   { if (V.depth == s) V.depth = NULL; free(s); }
static void v_raster_state_destroy(RhiRasterState *s) { if (V.raster == s) V.raster = NULL; free(s); }

static void v_sampler_destroy(RhiSampler *s)
{
    uint32_t i;
    for (i = 0; i < MAX_TEX; i++)
        if (V.smp[i] == s)
            V.smp[i] = NULL;
    trash(T_SAMPLER, (uint64_t)s->s, NULL);
    free(s);
}

/* ---- binding --------------------------------------------------------------------------------------------- */

static void v_set_render_target(RhiView *color, RhiView *depth)
{
    if (color == V.rt_color && depth == V.rt_depth)
        return;
    end_rendering();
    V.rt_color = color;
    V.rt_depth = depth;
}

static void v_output_save(RhiOutputState *s)
{
    uint32_t i;
    s->color = V.rt_color;
    s->depth = V.rt_depth;
    s->viewport_count = V.nvp;
    for (i = 0; i < V.nvp && i < 16; i++)
        s->viewports[i] = V.vp[i];
}

static void v_set_viewports(uint32_t n, const RhiViewport *vps)
{
    uint32_t i;
    if (n > 16) n = 16;
    for (i = 0; i < n; i++)
        V.vp[i] = vps[i];
    V.nvp = n;
}

static void v_output_restore(RhiOutputState *s)
{
    v_set_render_target((RhiView *)s->color, (RhiView *)s->depth);
    if (s->viewport_count)
        v_set_viewports(s->viewport_count, s->viewports);
    s->color = s->depth = NULL;
}

static int v_output_color_is(const RhiOutputState *s, const RhiView *v)
{
    return v && s->color == (const void *)v;
}

static void v_set_scissor(const RhiRect *r)                  { V.scissor = *r; }
static void v_set_topology(uint32_t t)                       { V.topology = t; }
static void v_set_vertex_layout(RhiVertexLayout *l)          { V.layout = l; }

static void v_set_vertex_buffer(uint32_t slot, RhiBuffer *b, uint32_t stride, uint32_t offset)
{
    if (slot >= MAX_VB)
        return;
    V.vb[slot].b = b;
    V.vb[slot].stride = stride;
    V.vb[slot].offset = offset;
}

static void v_set_index_buffer(RhiBuffer *b, uint32_t bits, uint32_t offset)
{
    V.ib = b;
    V.ib_bits = bits;
    V.ib_offset = offset;
}

static void v_set_shader(uint32_t stage, RhiShader *s)
{
    if (stage == RHI_STAGE_VERTEX) V.vs = s;
    else V.ps = s;
}

static void v_set_uniform_buffers(uint32_t stage, uint32_t slot, uint32_t n, RhiBuffer *const *b)
{
    uint32_t i;
    for (i = 0; i < n && slot + i < MAX_UBO; i++)
        V.ubo[stage == RHI_STAGE_VERTEX ? 0 : 1][slot + i] = b[i];
}

static void v_set_textures(uint32_t slot, uint32_t n, RhiView *const *v)
{
    uint32_t i;
    for (i = 0; i < n && slot + i < MAX_TEX; i++)
        V.tex[slot + i] = v[i];
}

static void v_set_samplers(uint32_t slot, uint32_t n, RhiSampler *const *s)
{
    uint32_t i;
    for (i = 0; i < n && slot + i < MAX_TEX; i++)
        V.smp[slot + i] = s[i];
}

static void v_set_blend_state(RhiBlendState *s, const float *factor, uint32_t mask)
{
    V.blend = s;
    if (factor)
        memcpy(V.blend_factor, factor, sizeof V.blend_factor);
    else
        V.blend_factor[0] = V.blend_factor[1] = V.blend_factor[2] = V.blend_factor[3] = 1.0f;
    V.sample_mask = mask;
}

static void v_set_depth_state(RhiDepthState *s, uint32_t ref) { V.depth = s; V.stencil_ref = ref; }
static void v_set_raster_state(RhiRasterState *s)             { V.raster = s; }

/* ---- pipelines (section 4.5) ------------------------------------------------------------------------------ */

static VkBlendFactor vk_blend(uint32_t b)
{
    switch (b) {
    case RHI_BLEND_ZERO:           return VK_BLEND_FACTOR_ZERO;
    case RHI_BLEND_SRC_COLOR:      return VK_BLEND_FACTOR_SRC_COLOR;
    case RHI_BLEND_INV_SRC_COLOR:  return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case RHI_BLEND_SRC_ALPHA:      return VK_BLEND_FACTOR_SRC_ALPHA;
    case RHI_BLEND_INV_SRC_ALPHA:  return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case RHI_BLEND_DEST_ALPHA:     return VK_BLEND_FACTOR_DST_ALPHA;
    case RHI_BLEND_INV_DEST_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case RHI_BLEND_DEST_COLOR:     return VK_BLEND_FACTOR_DST_COLOR;
    case RHI_BLEND_INV_DEST_COLOR: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case RHI_BLEND_SRC_ALPHA_SAT:  return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    case RHI_BLEND_FACTOR:         return VK_BLEND_FACTOR_CONSTANT_COLOR;
    case RHI_BLEND_INV_FACTOR:     return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    default:                       return VK_BLEND_FACTOR_ONE;
    }
}

static VkBlendOp vk_blend_op(uint32_t op)
{
    switch (op) {
    case RHI_BLEND_OP_SUBTRACT:     return VK_BLEND_OP_SUBTRACT;
    case RHI_BLEND_OP_REV_SUBTRACT: return VK_BLEND_OP_REVERSE_SUBTRACT;
    case RHI_BLEND_OP_MIN:          return VK_BLEND_OP_MIN;
    case RHI_BLEND_OP_MAX:          return VK_BLEND_OP_MAX;
    default:                        return VK_BLEND_OP_ADD;
    }
}

/* RHI_CMP_* and RHI_STENCIL_* are D3D11's numbers, one above Vulkan's. */
static VkCompareOp vk_cmp(uint32_t c)       { return c ? (VkCompareOp)(c - 1) : VK_COMPARE_OP_ALWAYS; }
static VkStencilOp vk_stencil(uint32_t o)   { return o ? (VkStencilOp)(o - 1) : VK_STENCIL_OP_KEEP; }

static uint32_t topo_class(uint32_t t)
{
    return t == RHI_TOPOLOGY_POINTS ? 0 : (t == RHI_TOPOLOGY_LINES || t == RHI_TOPOLOGY_LINE_STRIP) ? 1 : 2;
}

static VkPrimitiveTopology vk_topology(uint32_t t)
{
    switch (t) {
    case RHI_TOPOLOGY_POINTS:         return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case RHI_TOPOLOGY_LINES:          return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case RHI_TOPOLOGY_LINE_STRIP:     return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case RHI_TOPOLOGY_TRIANGLE_STRIP: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    default:                          return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }
}

static const RhiBlendDesc *blend_desc(void)
{
    static RhiBlendDesc def;
    if (V.blend)
        return &V.blend->d;
    def.enable = 0;
    def.src = def.src_alpha = RHI_BLEND_ONE;
    def.dst = def.dst_alpha = RHI_BLEND_ZERO;
    def.op = def.op_alpha = RHI_BLEND_OP_ADD;
    def.write_mask = RHI_WRITE_ALL;
    return &def;
}

static const RhiRasterDesc *raster_desc(void)
{
    static RhiRasterDesc def;
    if (V.raster)
        return &V.raster->d;
    def.fill = RHI_FILL_SOLID;
    def.cull = RHI_CULL_BACK;
    def.depth_clip = 1;
    return &def;
}

static const RhiDepthDesc *depth_desc(void)
{
    static RhiDepthDesc def;
    if (V.depth)
        return &V.depth->d;
    def.depth_enable = 1;
    def.depth_write = 1;
    def.depth_func = RHI_CMP_LESS;
    def.stencil_read_mask = def.stencil_write_mask = 0xFF;
    def.front.fail = def.front.depth_fail = def.front.pass = RHI_STENCIL_KEEP;
    def.front.func = RHI_CMP_ALWAYS;
    def.back = def.front;
    return &def;
}

/* Section 4.4 -- and where it was wrong. The raster description's
 * front_ccw keeps D3D11's meaning (d3d8_states.c's CW/CCW-to-cull mapping
 * belongs to the D3D8 semantics and is not touched). The negative viewport
 * height in apply_dynamic_state makes the framebuffer image the one D3D
 * draws, so a triangle winds on the screen exactly as it does under D3D11,
 * and Vulkan decides facing in those same framebuffer coordinates (y down):
 * its CLOCKWISE is D3D's clockwise. No inversion is needed; the plan's
 * "invert frontFace to pay for the viewport" held only against an unflipped
 * viewport, and adding it culled TimeSplitters 2's tunnel walls away. */
static VkFrontFace raster_front_face(const RhiRasterDesc *r)
{
    return r->front_ccw ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
}

static uint64_t hash_key(const PipeKey *k)
{
    const uint8_t *p = (const uint8_t *)k;
    uint64_t h = 1469598103934665603ull;
    size_t i;
    for (i = 0; i < sizeof *k; i++) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}

static Pipe **pipe_slot(const PipeKey *k)
{
    uint64_t h = hash_key(k);
    uint32_t i = (uint32_t)h & (V.pipe_cap - 1);
    for (;;) {
        Pipe **s = &V.pipes[i];
        if (!*s || memcmp(&(*s)->key, k, sizeof *k) == 0)
            return s;
        i = (i + 1) & (V.pipe_cap - 1);
    }
}

static int pipes_grow(void)
{
    Pipe **old = V.pipes;
    uint32_t cap = V.pipe_cap, i;

    V.pipe_cap = cap ? cap * 2 : 256;
    V.pipes = calloc(V.pipe_cap, sizeof *V.pipes);
    if (!V.pipes) {
        V.pipes = old;
        V.pipe_cap = cap;
        return 0;
    }
    for (i = 0; i < cap; i++)
        if (old[i])
            *pipe_slot(&old[i]->key) = old[i];
    free(old);
    return 1;
}

static int same_semantic(const ReflIO *a, const char *sem, uint32_t index)
{
    return a->index == index && strcmp(a->sem, sem) == 0;
}

/* The pixel shader's inputs, moved to the locations the vertex shader
 * writes the same semantics at. DXC numbers each stage's varyings in its
 * own declaration order, and D3D11 links by semantic, so a pixel shader
 * that declares fewer inputs, or in another order, would otherwise read
 * the wrong ones. */
static VkShaderModule linked_ps_module(const RhiShader *vs, const RhiShader *ps)
{
    VkShaderModuleCreateInfo ci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    VkShaderModule m = VK_NULL_HANDLE;
    uint32_t *code;
    uint32_t i, k, spare = 31;
    int changed = 0;

    code = malloc(ps->words * 4);
    if (!code)
        return VK_NULL_HANDLE;
    memcpy(code, ps->code, ps->words * 4);
    for (i = 0; i < ps->nin; i++) {
        const ReflIO *in = &ps->in[i];
        uint32_t loc = spare;
        for (k = 0; k < vs->nout; k++)
            if (same_semantic(&vs->out[k], in->sem, in->index)) {
                loc = vs->out[k].location;
                break;
            }
        if (k == vs->nout)
            spare--;                    /* read, never written: somewhere nothing aliases */
        if (in->loc_word && code[in->loc_word] != loc) {
            code[in->loc_word] = loc;
            changed = 1;
        }
    }
    if (trace_on())
        fprintf(stderr, "[VKTRACE] link vs %llu ps %llu: %s\n", (unsigned long long)vs->id,
                (unsigned long long)ps->id, changed ? "pixel inputs moved" : "already agree");
    if (!changed) {
        free(code);
        return VK_NULL_HANDLE;          /* the pixel shader's own module will do */
    }
    ci.codeSize = ps->words * 4;
    ci.pCode = code;
    vkCreateShaderModule(V.dev, &ci, NULL, &m);
    free(code);
    return m;
}

static void add_refl(Pipe *p, const RhiShader *s)
{
    uint32_t i, k;
    if (!s)
        return;
    for (i = 0; i < s->nb; i++) {
        for (k = 0; k < p->nb; k++)
            if (p->b[k].binding == s->b[i].binding)
                break;
        if (k < p->nb) {
            if (s->b[i].block_size > p->b[k].block_size)
                p->b[k].block_size = s->b[i].block_size;
            continue;
        }
        if (p->nb < MAX_REFL * 2)
            p->b[p->nb++] = s->b[i];
    }
}

static Pipe *build_pipeline(const PipeKey *key)
{
    VkPipelineShaderStageCreateInfo st[2];
    VkVertexInputBindingDescription vb[MAX_VB];
    VkVertexInputAttributeDescription va[32];
    VkPipelineVertexInputStateCreateInfo vi = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    VkPipelineInputAssemblyStateCreateInfo ia = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    VkPipelineViewportStateCreateInfo vps = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    VkPipelineRasterizationStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    VkPipelineMultisampleStateCreateInfo ms = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    VkPipelineDepthStencilStateCreateInfo ds = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    VkPipelineColorBlendAttachmentState ba;
    VkPipelineColorBlendStateCreateInfo cb = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    static const VkDynamicState dyn[] = {
        VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT, VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
        VK_DYNAMIC_STATE_CULL_MODE, VK_DYNAMIC_STATE_FRONT_FACE, VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
        VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE, VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
        VK_DYNAMIC_STATE_DEPTH_COMPARE_OP, VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE,
        VK_DYNAMIC_STATE_STENCIL_OP, VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
        VK_DYNAMIC_STATE_STENCIL_WRITE_MASK, VK_DYNAMIC_STATE_STENCIL_REFERENCE,
        VK_DYNAMIC_STATE_DEPTH_BIAS, VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE,
        VK_DYNAMIC_STATE_BLEND_CONSTANTS, VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE,
        VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE,
    };
    VkPipelineDynamicStateCreateInfo dy = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    VkPipelineRenderingCreateInfo ri = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    VkGraphicsPipelineCreateInfo gci = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    VkShaderModule linked = VK_NULL_HANDLE;
    const RhiShader *vs = V.vs, *ps = V.ps;
    const RhiVertexLayout *l = V.layout;
    uint32_t nst = 0, nva = 0, i, k;
    uint32_t slot_off[MAX_VB];
    Pipe *p = calloc(1, sizeof *p);

    if (!p)
        return NULL;
    p->key = *key;
    add_refl(p, vs);
    add_refl(p, ps);
    for (i = 0; i < p->nb; i++)
        if (!binding_declared(p->b[i].binding, p->b[i].kind)) {
            fprintf(stderr, "[RHI] vulkan: a shader uses binding %u, which the descriptor set "
                            "does not declare (rhi_vulkan_bindings.h)\n", p->b[i].binding);
            return p;                   /* p->pipe stays null: the draw is skipped */
        }

    memset(st, 0, sizeof st);
    st[nst].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    st[nst].stage = VK_SHADER_STAGE_VERTEX_BIT;
    st[nst].module = vs->module;
    st[nst++].pName = vs->entry;
    if (ps) {
        linked = linked_ps_module(vs, ps);
        st[nst].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        st[nst].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        st[nst].module = linked ? linked : ps->module;
        st[nst++].pName = ps->entry;
    }

    /* Vertex input: each element goes to the location the vertex shader
     * reads its semantic at; strides are dynamic state. */
    memset(slot_off, 0, sizeof slot_off);
    for (i = 0; l && i < l->n && nva < 32; i++) {
        const RhiVertexElement *e = &l->e[i];
        char sem[40];
        uint32_t idx = e->semantic_index, off, loc = 0xFFFFFFFFu, j;
        size_t n;
        if (e->slot >= MAX_VB)
            continue;
        off = e->offset == RHI_APPEND ? slot_off[e->slot] : e->offset;
        slot_off[e->slot] = off + rhi_format_row_pitch(e->format, 1);
        n = strlen(e->semantic);
        if (n >= sizeof sem) n = sizeof sem - 1;
        for (j = 0; j < n; j++)
            sem[j] = (char)toupper((unsigned char)e->semantic[j]);
        sem[n] = 0;
        for (k = 0; k < vs->nin; k++)
            if (same_semantic(&vs->in[k], sem, idx)) {
                loc = vs->in[k].location;
                break;
            }
        if (trace_on())
            fprintf(stderr, "[VKTRACE] pipeline input %s%u slot %u fmt %u offset %u -> location %d\n",
                    sem, idx, e->slot, (unsigned)e->format, off, (int)loc);
        if (loc == 0xFFFFFFFFu)
            continue;                   /* D3D11 lets a layout carry what the shader ignores */
        va[nva].location = loc;
        va[nva].binding = e->slot;
        va[nva].format = vk_format(e->format);
        va[nva].offset = off;
        nva++;
        for (k = 0; k < p->nvb; k++)
            if (p->vb_slots[k] == e->slot)
                break;
        if (k == p->nvb)
            p->vb_slots[p->nvb++] = e->slot;
    }
    for (k = 0; k < p->nvb; k++) {
        vb[k].binding = p->vb_slots[k];
        vb[k].stride = 0;
        vb[k].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    }
    vi.vertexBindingDescriptionCount = p->nvb;
    vi.pVertexBindingDescriptions = vb;
    vi.vertexAttributeDescriptionCount = nva;
    vi.pVertexAttributeDescriptions = va;
    ia.topology = key->topo_class == 0 ? VK_PRIMITIVE_TOPOLOGY_POINT_LIST
                : key->topo_class == 1 ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST
                                       : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    rs.polygonMode = key->fill == RHI_FILL_WIREFRAME && V.feat_fill ? VK_POLYGON_MODE_LINE
                                                                    : VK_POLYGON_MODE_FILL;
    rs.depthClampEnable = key->depth_clamp && V.feat_depth_clamp;
    rs.lineWidth = 1.0f;
    ms.rasterizationSamples = (VkSampleCountFlagBits)(key->samples ? key->samples : 1);
    ms.pSampleMask = &p->key.sample_mask;
    ms.alphaToCoverageEnable = key->blend.alpha_to_coverage ? VK_TRUE : VK_FALSE;
    memset(&ba, 0, sizeof ba);
    ba.blendEnable = key->blend.enable ? VK_TRUE : VK_FALSE;
    ba.srcColorBlendFactor = vk_blend(key->blend.src);
    ba.dstColorBlendFactor = vk_blend(key->blend.dst);
    ba.colorBlendOp = vk_blend_op(key->blend.op);
    ba.srcAlphaBlendFactor = vk_blend(key->blend.src_alpha);
    ba.dstAlphaBlendFactor = vk_blend(key->blend.dst_alpha);
    ba.alphaBlendOp = vk_blend_op(key->blend.op_alpha);
    /* No pixel shader writes no colour, as under D3D11. */
    ba.colorWriteMask = key->ps ? (key->blend.write_mask & 0xF) : 0;
    cb.attachmentCount = key->color ? 1 : 0;
    cb.pAttachments = &ba;
    dy.dynamicStateCount = sizeof dyn / sizeof dyn[0];
    dy.pDynamicStates = dyn;
    ri.colorAttachmentCount = key->color ? 1 : 0;
    ri.pColorAttachmentFormats = &p->key.color;
    ri.depthAttachmentFormat = key->depth;
    ri.stencilAttachmentFormat = has_stencil(key->depth) ? key->depth : VK_FORMAT_UNDEFINED;
    gci.pNext = &ri;
    gci.stageCount = nst;
    gci.pStages = st;
    gci.pVertexInputState = &vi;
    gci.pInputAssemblyState = &ia;
    gci.pViewportState = &vps;
    gci.pRasterizationState = &rs;
    gci.pMultisampleState = &ms;
    gci.pDepthStencilState = &ds;
    gci.pColorBlendState = &cb;
    gci.pDynamicState = &dy;
    gci.layout = V.pipe_layout;
    if (vkCreateGraphicsPipelines(V.dev, V.pcache, 1, &gci, NULL, &p->pipe) != VK_SUCCESS) {
        static int said;
        if (said++ < 10)
            fprintf(stderr, "[RHI] vulkan: a pipeline could not be built; its draws are skipped\n");
        p->pipe = VK_NULL_HANDLE;
    }
    if (linked)
        vkDestroyShaderModule(V.dev, linked, NULL);
    return p;
}

static Pipe *find_pipeline(void)
{
    PipeKey k;
    Pipe **s;
    const RhiRasterDesc *r = raster_desc();

    memset(&k, 0, sizeof k);
    k.vs = V.vs->id;
    k.ps = V.ps ? V.ps->id : 0;
    k.layout = V.layout ? V.layout->id : 0;
    k.blend = *blend_desc();
    k.sample_mask = V.sample_mask;
    k.fill = r->fill;
    k.depth_clamp = !r->depth_clip;
    k.color = V.rt_color ? image_format(V.rt_color->image) : VK_FORMAT_UNDEFINED;
    k.depth = V.rt_depth ? V.rt_depth->image->format : VK_FORMAT_UNDEFINED;
    k.samples = V.rt_color ? V.rt_color->image->desc.samples
                           : V.rt_depth ? V.rt_depth->image->desc.samples : 1;
    k.topo_class = topo_class(V.topology);
    if (!k.color)
        memset(&k.blend, 0, sizeof k.blend);
    if (V.npipes * 10 >= V.pipe_cap * 7 && !pipes_grow())
        return NULL;
    s = pipe_slot(&k);
    if (!*s) {
        *s = build_pipeline(&k);
        if (*s) {
            V.npipes++;
            /* Saved as it grows too, not only at shutdown: a run that is
             * killed or crashes never reaches the destroy path. */
            if (V.npipes % 32 == 0)
                pcache_save();
        }
    }
    return *s;
}

/* ---- drawing ------------------------------------------------------------------------------------------------ */

/* A constant buffer's contents where this draw can read them: snapshotted
 * into this frame's ring when they changed or when the last snapshot was
 * in an older frame's ring. `need` is the size the shader declares, which
 * can exceed the buffer's (the fixed-function path binds its 256-byte
 * lighting buffer where a vertex program declares 3,072; section 4.2). */
static int ubo_range(RhiBuffer *b, uint32_t need, VkDescriptorBufferInfo *out)
{
    VkDeviceSize range;
    if (!b) {
        out->buffer = V.zero_b;
        out->offset = 0;
        out->range = need ? need : 16;
        return 1;
    }
    range = b->desc.size > need ? b->desc.size : need;
    if (b->dirty || b->snap_frame != V.frame_no + 1 || b->snap_range < range) {
        uint8_t *p = ring_alloc(range, V.props.limits.minUniformBufferOffsetAlignment,
                                &b->snap_b, &b->snap_off);
        if (!p)
            return 0;
        memcpy(p, b->shadow, b->desc.size);
        if (range > b->desc.size)
            memset(p + b->desc.size, 0, (size_t)(range - b->desc.size));
        b->snap_range = range;
        b->snap_frame = V.frame_no + 1;
        b->dirty = 0;
    }
    out->buffer = b->snap_b;
    out->offset = b->snap_off;
    out->range = range;
    return 1;
}

static RhiView *dummy_for(const Refl *r)
{
    if (r->ms)
        return V.dummy_view[4];
    if (r->dim == 3 /* Cube */)
        return V.dummy_view[2];
    if (r->dim == 2 /* 3D */)
        return V.dummy_view[3];
    return r->arrayed ? V.dummy_view[1] : V.dummy_view[0];
}

static int view_fits(const RhiView *v, const Refl *r)
{
    if (!v || v->kind != RHI_VIEW_SAMPLED || v->swapchain)
        return 0;
    if ((v->image->desc.samples > 1) != (r->ms != 0))
        return 0;
    switch (v->type) {
    case VK_IMAGE_VIEW_TYPE_CUBE:     return r->dim == 3 && !r->arrayed;
    case VK_IMAGE_VIEW_TYPE_3D:       return r->dim == 2;
    case VK_IMAGE_VIEW_TYPE_2D_ARRAY: return r->dim == 1 && r->arrayed;
    case VK_IMAGE_VIEW_TYPE_2D:       return r->dim == 1 && !r->arrayed;
    default:                          return 0;
    }
}

static int push_descriptors(const Pipe *p)
{
    VkWriteDescriptorSet w[MAX_REFL * 2];
    VkDescriptorBufferInfo bi[MAX_REFL * 2];
    VkDescriptorImageInfo ii[MAX_REFL * 2];
    uint32_t i, n = 0;

    for (i = 0; i < p->nb; i++) {
        const Refl *r = &p->b[i];
        memset(&w[n], 0, sizeof w[n]);
        w[n].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[n].dstBinding = r->binding;
        w[n].descriptorCount = 1;
        if (r->kind == 1) {
            uint32_t stage = r->binding >= RHI_VK_PS_UNIFORM_BASE ? 1 : 0;
            uint32_t slot = r->binding - (stage ? RHI_VK_PS_UNIFORM_BASE : RHI_VK_VS_UNIFORM_BASE);
            if (!ubo_range(slot < MAX_UBO ? V.ubo[stage][slot] : NULL, r->block_size, &bi[n]))
                return 0;
            if (trace_on()) {
                RhiBuffer *ub = slot < MAX_UBO ? V.ubo[stage][slot] : NULL;
                fprintf(stderr, "[VKTRACE] draw binds %s b%u: %s size %u, shader declares %u\n",
                        stage ? "pixel" : "vertex", slot, ub ? "buffer" : "NONE",
                        ub ? ub->desc.size : 0, r->block_size);
            }
            w[n].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            w[n].pBufferInfo = &bi[n];
        } else if (r->kind == 2) {
            uint32_t slot = r->binding - RHI_VK_TEXTURE_BASE;
            RhiView *v = slot < MAX_TEX ? V.tex[slot] : NULL;
            /* D3D11 unbinds a view that is also the render target; reading
             * it as zeros is what the title saw there. */
            if (v && ((V.rt_color && v->image == V.rt_color->image) ||
                      (V.rt_depth && v->image == V.rt_depth->image)))
                v = NULL;
            if (trace_on())
                fprintf(stderr, "[VKTRACE] draw binds t%u: %s image %p %ux%u fmt %u\n", slot,
                        !v ? "NULL" : view_fits(v, r) ? "view" : "MISFIT", v ? (void *)v->image : NULL,
                        v ? v->image->desc.width : 0, v ? v->image->desc.height : 0,
                        v ? (unsigned)v->image->desc.format : 0);
            if (!view_fits(v, r))
                v = dummy_for(r);
            if (!v)
                return 0;
            ii[n].sampler = VK_NULL_HANDLE;
            ii[n].imageView = v->view;
            ii[n].imageLayout = v->image->steady;
            w[n].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            w[n].pImageInfo = &ii[n];
        } else {
            uint32_t slot = r->binding - RHI_VK_SAMPLER_BASE;
            RhiSampler *s = slot < MAX_TEX ? V.smp[slot] : NULL;
            ii[n].sampler = s ? s->s : V.default_sampler;
            if (trace_on())
                fprintf(stderr, "[VKTRACE] draw binds sampler s%u: %s filter 0x%X address %u/%u\n",
                        slot, s ? "bound" : "DEFAULT", s ? s->d.filter : 0x15u,
                        s ? s->d.address_u : 3u, s ? s->d.address_v : 3u);
            ii[n].imageView = VK_NULL_HANDLE;
            ii[n].imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            w[n].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            w[n].pImageInfo = &ii[n];
        }
        n++;
    }
    if (n)
        vkCmdPushDescriptorSetKHR(cmd(), VK_PIPELINE_BIND_POINT_GRAPHICS, V.pipe_layout, 0, n, w);
    return 1;
}

static void apply_dynamic_state(VkCommandBuffer cb)
{
    const RhiRasterDesc *r = raster_desc();
    const RhiDepthDesc *d = depth_desc();
    const RhiViewport *vp = &V.vp[0];
    VkViewport v;
    VkRect2D sc;
    int32_t x0, y0, x1, y1;

    /* Section 4.4: a negative height turns Vulkan's downward Y into D3D's.
     * Depth needs nothing: both use [0, 1]. */
    v.x = vp->x;
    v.y = vp->y + vp->height;
    v.width = vp->width;
    v.height = -vp->height;
    v.minDepth = vp->min_depth;
    v.maxDepth = vp->max_depth;

    if (r->scissor) {
        x0 = V.scissor.left; y0 = V.scissor.top; x1 = V.scissor.right; y1 = V.scissor.bottom;
    } else {
        x0 = 0; y0 = 0; x1 = (int32_t)V.area.width; y1 = (int32_t)V.area.height;
    }
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;
    sc.offset.x = x0;
    sc.offset.y = y0;
    sc.extent.width = (uint32_t)(x1 - x0);
    sc.extent.height = (uint32_t)(y1 - y0);

    /* Each piece of state is set only when it differs from what the command
     * buffer already holds (V.dyn, cleared at begin_recording). The first
     * draw of a recording sets everything. */
#define DYN_CHANGED(field, value) \
    ((!V.dyn.valid || memcmp(&V.dyn.field, &(value), sizeof(value)) != 0) \
        ? (memcpy(&V.dyn.field, &(value), sizeof(value)), 1) : 0)
#define DYN_SET(field, value, ...) \
    do { if (DYN_CHANGED(field, value)) { __VA_ARGS__; } } while (0)
    {
        VkCullModeFlags cull = r->cull == RHI_CULL_FRONT ? VK_CULL_MODE_FRONT_BIT
                             : r->cull == RHI_CULL_BACK  ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
        VkFrontFace ff = raster_front_face(r);
        VkPrimitiveTopology topo = vk_topology(V.topology);
        VkBool32 bias_enable = (r->depth_bias || r->slope_scaled_depth_bias) ? VK_TRUE : VK_FALSE;
        float bias[3];
        VkBool32 depth_test = d->depth_enable ? VK_TRUE : VK_FALSE;
        VkBool32 depth_write = (d->depth_enable && d->depth_write) ? VK_TRUE : VK_FALSE;
        VkCompareOp depth_cmp = vk_cmp(d->depth_func);
        VkBool32 stencil_test = d->stencil_enable ? VK_TRUE : VK_FALSE;
        struct { VkStencilOp fail, pass, depth_fail; VkCompareOp cmp; } front, back;
        uint32_t read_mask = d->stencil_read_mask, write_mask = d->stencil_write_mask;
        uint32_t ref = V.stencil_ref;

        bias[0] = (float)r->depth_bias;
        bias[1] = V.feat_bias_clamp ? r->depth_bias_clamp : 0.0f;
        bias[2] = r->slope_scaled_depth_bias;
        front.fail = vk_stencil(d->front.fail); front.pass = vk_stencil(d->front.pass);
        front.depth_fail = vk_stencil(d->front.depth_fail); front.cmp = vk_cmp(d->front.func);
        back.fail = vk_stencil(d->back.fail); back.pass = vk_stencil(d->back.pass);
        back.depth_fail = vk_stencil(d->back.depth_fail); back.cmp = vk_cmp(d->back.func);

        DYN_SET(vp, v, vkCmdSetViewportWithCount(cb, 1, &v));
        DYN_SET(sc, sc, vkCmdSetScissorWithCount(cb, 1, &sc));
        DYN_SET(cull, cull, vkCmdSetCullMode(cb, cull));
        DYN_SET(front_face, ff, vkCmdSetFrontFace(cb, ff));
        DYN_SET(topo, topo, vkCmdSetPrimitiveTopology(cb, topo));
        if (!V.dyn.valid)
            vkCmdSetPrimitiveRestartEnable(cb, VK_FALSE);
        DYN_SET(bias_enable, bias_enable, vkCmdSetDepthBiasEnable(cb, bias_enable));
        DYN_SET(bias, bias, vkCmdSetDepthBias(cb, bias[0], bias[1], bias[2]));
        DYN_SET(depth_test, depth_test, vkCmdSetDepthTestEnable(cb, depth_test));
        DYN_SET(depth_write, depth_write, vkCmdSetDepthWriteEnable(cb, depth_write));
        DYN_SET(depth_cmp, depth_cmp, vkCmdSetDepthCompareOp(cb, depth_cmp));
        DYN_SET(stencil_test, stencil_test, vkCmdSetStencilTestEnable(cb, stencil_test));
        DYN_SET(front, front, vkCmdSetStencilOp(cb, VK_STENCIL_FACE_FRONT_BIT, front.fail, front.pass,
                                                front.depth_fail, front.cmp));
        DYN_SET(back, back, vkCmdSetStencilOp(cb, VK_STENCIL_FACE_BACK_BIT, back.fail, back.pass,
                                              back.depth_fail, back.cmp));
        DYN_SET(read_mask, read_mask, vkCmdSetStencilCompareMask(cb, VK_STENCIL_FACE_FRONT_AND_BACK, read_mask));
        DYN_SET(write_mask, write_mask, vkCmdSetStencilWriteMask(cb, VK_STENCIL_FACE_FRONT_AND_BACK, write_mask));
        DYN_SET(ref, ref, vkCmdSetStencilReference(cb, VK_STENCIL_FACE_FRONT_AND_BACK, ref));
        DYN_SET(blend_const, V.blend_factor, vkCmdSetBlendConstants(cb, V.blend_factor));
        V.dyn.valid = 1;
    }
#undef DYN_SET
#undef DYN_CHANGED
}

static int prepare_draw(void)
{
    VkCommandBuffer cb;
    Pipe *p;
    uint32_t k;

    if (!V.vs || (!V.rt_color && !V.rt_depth) || !V.nvp ||
        V.vp[0].width <= 0.0f || V.vp[0].height <= 0.0f)
        return 0;
    if (!begin_rendering())
        return 0;
    p = find_pipeline();
    if (!p || !p->pipe)
        return 0;
    cb = cmd();
    if (p->pipe != V.cur_pipe) {
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, p->pipe);
        V.cur_pipe = p->pipe;
    }
    apply_dynamic_state(cb);
    for (k = 0; k < p->nvb; k++) {
        uint32_t slot = p->vb_slots[k];
        RhiBuffer *b = V.vb[slot].b;
        VkBuffer buf = V.zero_b;
        VkDeviceSize off = 0, stride = 0;
        if (b && b->nver) {
            BufVer *v = &b->ver[b->cur];
            v->used = V.submitted + 1;
            buf = v->b;
            off = V.vb[slot].offset;
            stride = V.vb[slot].stride;
        }
        vkCmdBindVertexBuffers2(cb, slot, 1, &buf, &off, NULL, &stride);
    }
    if (!p->nvb) {
        /* Dynamic stride wants a stride bound even when nothing is read. */
        VkDeviceSize off = 0, stride = 0;
        vkCmdBindVertexBuffers2(cb, 0, 1, &V.zero_b, &off, NULL, &stride);
    }
    return push_descriptors(p);
}

static void v_draw(uint32_t n, uint32_t first)
{
    if (n && prepare_draw())
        vkCmdDraw(cmd(), n, 1, first, 0);
}

static void v_draw_indexed(uint32_t n, uint32_t first, int32_t base)
{
    BufVer *v;
    if (!n || !V.ib || !V.ib->nver || !prepare_draw())
        return;
    v = &V.ib->ver[V.ib->cur];
    v->used = V.submitted + 1;
    vkCmdBindIndexBuffer(cmd(), v->b, V.ib_offset,
                         V.ib_bits == 32 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd(), n, 1, first, base, 0);
}

const RhiBackend rhi_vulkan_backend = {
    "vulkan",
    v_device_create, v_device_destroy, v_device_ready, v_swapchain_view, v_swapchain_resize,
    v_present, v_swapchain_readback, v_swapchain_set_vrr,
    v_buffer_create, v_buffer_destroy, v_buffer_map, v_buffer_unmap, v_buffer_update,
    v_image_create, v_image_retain, v_image_destroy, v_image_get_desc, v_image_update, v_image_readback,
    v_view_create, v_view_destroy, v_view_image,
    v_sample_count_supported,
    v_shader_create, v_shader_destroy, v_vertex_layout_create, v_vertex_layout_destroy,
    v_blend_state_create, v_depth_state_create, v_raster_state_create, v_sampler_create,
    v_blend_state_destroy, v_depth_state_destroy, v_raster_state_destroy, v_sampler_destroy,
    v_set_render_target, v_output_save, v_output_restore, v_output_color_is, v_set_viewports, v_set_scissor,
    v_set_topology, v_set_vertex_layout, v_set_vertex_buffer, v_set_index_buffer, v_set_shader,
    v_set_uniform_buffers, v_set_textures, v_set_samplers, v_set_blend_state, v_set_depth_state,
    v_set_raster_state, v_draw, v_draw_indexed, v_clear_color, v_clear_depth,
};

#endif /* RHI_HAVE_VULKAN */
