/* Native Mac backend derived from ts3FPrecomp d90c929, src/d3d/d3d8_gl.c.
 * Shared HLE and format helpers remain in the pinned, patched runtime. */
/* SDL3/Cocoa + OpenGL 4.1 bring-up renderer. The shared HLE preserves the
 * guest's execution and supplies decoded state, textures and vertex layouts.
 * NV2A vertex programs compile to GLSL, with a portable CPU fallback. The
 * fragment shader evaluates four 2D texture inputs and register combiners.
 * Offscreen targets, screen copies, scissors and 2x interpolation are supported;
 * advanced texture modes, cube/volume resources and movies remain incomplete.
 */

#include "d3d8_xbox.h"
#include "d3d8_fvf.h"
#include "d3d8_swizzle.h"
#ifdef __APPLE__
#include "d3d8_internal.h"
#include "d3d8_vsh.h"
#include "d3d8_combiners.h"
#endif

#include <SDL3/SDL.h>
#include "input_sdl.h"
#include "settings_macos.h"
#include "pc_display.h"
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#include <epoxy/gl.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef D3D_OK
#define D3D_OK ((HRESULT)0)
#endif
#ifndef D3DERR_INVALIDCALL
#define D3DERR_INVALIDCALL ((HRESULT)0x8876086CL)
#endif

/* ======================================================================== */
/* Globals                                                                  */
/* ======================================================================== */

#define MAX_RS    256

/* Window title for the SDL window, configurable per title via
 * xbox_D3D8SetWindowTitle(). Default is a generic name. */
static const char *g_window_title = "Xbox Game";
static UINT g_present_interval = 1;     /* xbox_D3D8SetPresentInterval */

void xbox_D3D8SetWindowTitle(const char *title)
{
    g_window_title = (title && title[0]) ? title : "Xbox Game";
}
#define MAX_TSS   33
#define MAX_TSU   8     /* texture stages */

typedef struct {
    /* Host */
    SDL_Window      *window;
    SDL_GLContext    glctx;
    int              backbuf_w;
    int              backbuf_h;

    /* D3D state cache */
    DWORD            rs[MAX_RS];
    DWORD            tss[MAX_TSU][MAX_TSS];
    D3DMATRIX        m_world, m_view, m_proj;
    D3DVIEWPORT8     viewport;
    D3DCOLOR         tex_factor;

    /* Current bindings */
    IDirect3DBaseTexture8 *textures[MAX_TSU];
    IDirect3DVertexBuffer8 *vb;
    UINT             vb_stride;
    UINT             vb_offset;
    IDirect3DIndexBuffer8 *ib;
    UINT             ib_base;
    DWORD            current_fvf;
    DWORD            current_vsh;
    DWORD            current_psh;

    /* Lighting */
    D3DMATERIAL8     material;
    D3DLIGHT8        lights[8];
    BOOL             light_enabled[8];

    /* GL draw resources */
    GLuint           vao;
    GLuint           stream_vbo;
    GLuint           stream_ibo;

    /* GL fixed shader program */
    GLuint           fallback_texture;
    GLuint           prog;
    GLint            u_mvp;
    GLint            u_use_tex;
    GLint            u_use_xform;
    GLint            u_tex0;
} D3DGL;

static D3DGL g;
static unsigned native_present_count,native_guest_present_count;
static int native_interp_on;
static int native_depth_enabled=1;
static void native_apply_scissor(void);
static UINT native_target_width,native_target_height;
#include "gl_context.inc"
#include "gl_display.inc"
#ifdef __APPLE__
static float *native_shader_vertices(const void *,UINT,UINT);
static void native_shader_attribs(void);
static GLuint native_gpu_prepare(UINT);
static void native_gpu_attribs(UINT);
static void native_select_program(GLuint);
static void native_gpu_delete(int);
static GLuint native_fixed_program;
static void native_set_palette(DWORD,const void *);
#endif

/* Bounded, opt-in per-draw diagnostics for a requested frame. */
static unsigned trace_draw;
static void native_trace(const char *kind,const float *vertices,UINT count) {
    if(!trace_draw || trace_draw>2000)return;
    BYTE pixel[4]={0};glReadPixels(native_target_width/2,native_target_height/2,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    fprintf(stderr,"[native-draw] %u %s pixel=%u,%u,%u,%u vsh=%x ps=%x depth=%u/%u/%u blend=%u/%u/%u cull=%u vp=%u,%u,%u,%u tex=%p,%p,%p,%p\n",trace_draw++,kind,pixel[0],pixel[1],pixel[2],pixel[3],g.current_vsh,g.current_psh,g.rs[7],g.rs[14],g.rs[23],g.rs[27],g.rs[19],g.rs[20],g.rs[22],g.viewport.X,g.viewport.Y,g.viewport.Width,g.viewport.Height,g.textures[0],g.textures[1],g.textures[2],g.textures[3]);
    if(vertices && count) {
        const float *v=vertices;
        fprintf(stderr,"[native-draw] vertex=%g,%g,%g,%g color=%g,%g,%g,%g uv=%g,%g comb=%x final=%x,%x\n",v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8],v[9],g.rs[234],g.rs[208],g.rs[209]);
    }
}

/* ======================================================================== */
/* Tiny helpers                                                             */
/* ======================================================================== */

static void mat4_identity(D3DMATRIX *m)
{
    memset(m, 0, sizeof(*m));
    m->m[0][0] = m->m[1][1] = m->m[2][2] = m->m[3][3] = 1.0f;
}

static void mat4_mul(D3DMATRIX *r, const D3DMATRIX *a, const D3DMATRIX *b)
{
    D3DMATRIX t;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            t.m[i][j] = a->m[i][0]*b->m[0][j] + a->m[i][1]*b->m[1][j]
                      + a->m[i][2]*b->m[2][j] + a->m[i][3]*b->m[3][j];
    *r = t;
}

static GLenum gl_prim(D3DPRIMITIVETYPE t, UINT prim_count, UINT *out_vcount)
{
    switch (t) {
    case D3DPT_POINTLIST:     *out_vcount = prim_count;       return GL_POINTS;
    case D3DPT_LINELIST:      *out_vcount = prim_count * 2;   return GL_LINES;
    case D3DPT_LINESTRIP:     *out_vcount = prim_count + 1;   return GL_LINE_STRIP;
    case D3DPT_TRIANGLELIST:  *out_vcount = prim_count * 3;   return GL_TRIANGLES;
    case D3DPT_TRIANGLESTRIP: *out_vcount = prim_count + 2;   return GL_TRIANGLE_STRIP;
    case D3DPT_TRIANGLEFAN:   *out_vcount = prim_count + 2;   return GL_TRIANGLE_FAN;
    case D3DPT_QUADLIST: *out_vcount = prim_count*4; return GL_TRIANGLES;
    default:                  *out_vcount = prim_count * 3;   return GL_TRIANGLES;
    }
}

static GLenum gl_blend(DWORD b)
{
    switch (b) {
    case D3DBLEND_ZERO:         return GL_ZERO;
    case D3DBLEND_ONE:          return GL_ONE;
    case D3DBLEND_SRCCOLOR:     return GL_SRC_COLOR;
    case D3DBLEND_INVSRCCOLOR:  return GL_ONE_MINUS_SRC_COLOR;
    case D3DBLEND_SRCALPHA:     return GL_SRC_ALPHA;
    case D3DBLEND_INVSRCALPHA:  return GL_ONE_MINUS_SRC_ALPHA;
    case D3DBLEND_DESTALPHA:    return GL_DST_ALPHA;
    case D3DBLEND_INVDESTALPHA: return GL_ONE_MINUS_DST_ALPHA;
    case D3DBLEND_DESTCOLOR:    return GL_DST_COLOR;
    case D3DBLEND_INVDESTCOLOR: return GL_ONE_MINUS_DST_COLOR;
    case D3DBLEND_SRCALPHASAT:  return GL_SRC_ALPHA_SATURATE;
    default:                    return GL_ONE;
    }
}

static GLenum gl_cmp(DWORD f)
{
    switch (f) {
    case D3DCMP_NEVER:        return GL_NEVER;
    case D3DCMP_LESS:         return GL_LESS;
    case D3DCMP_EQUAL:        return GL_EQUAL;
    case D3DCMP_LESSEQUAL:    return GL_LEQUAL;
    case D3DCMP_GREATER:      return GL_GREATER;
    case D3DCMP_NOTEQUAL:     return GL_NOTEQUAL;
    case D3DCMP_GREATEREQUAL: return GL_GEQUAL;
    case D3DCMP_ALWAYS:       return GL_ALWAYS;
    default:                  return GL_LESS;
    }
}

/* ======================================================================== */
/* Fixed GLSL program (position + diffuse + UV, optional MVP transform)     */
/* ======================================================================== */

static const char *VS_SRC =
    "#version 330 core\n"
    "layout(location=0) in vec4 a_pos;\n"
    "layout(location=1) in vec4 a_color;\n"
    "layout(location=2) in vec4 a_uv0;\n"
    "layout(location=3) in vec4 a_uv1;\n"
    "layout(location=4) in vec4 a_uv2;\n"
    "layout(location=5) in vec4 a_uv3;\n"
    "uniform mat4 u_mvp;\n"
    "uniform int  u_use_xform;\n"
    "uniform vec2 u_size;\n"
    "out vec4 v_color;\n"
    "out vec4 v_uv[4];\n"
    "void main() {\n"
    "  if (u_use_xform == 1) {\n"
    "    gl_Position = u_mvp * vec4(a_pos.xyz, 1.0);\n"
    "  } else {\n"
    "    /* XYZRHW: a_pos.w is RHW; emit clip-space directly. The caller is\n"
    "       expected to feed already-clip-space coords. */\n"
    "    gl_Position = a_pos;\n"
    "    if (u_use_xform == 0) { float w = a_pos.w != 0.0 ? 1.0/a_pos.w : 1.0; gl_Position = vec4((a_pos.x+0.5)*2.0/u_size.x-1.0, 1.0-(a_pos.y+0.5)*2.0/u_size.y, a_pos.z*2.0-1.0, 1.0)*w; }\n"
    "  }\n"
    "  v_color = a_color;\n"
    "  v_uv[0]=a_uv0;v_uv[1]=a_uv1;v_uv[2]=a_uv2;v_uv[3]=a_uv3;\n"
    "}\n";

static const char *FS_SRC =
#include "gl_fragment.inc"
;

static GLuint compile_shader(GLenum type, const char *src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024]; GLsizei n = 0;
        glGetShaderInfoLog(s, sizeof(log), &n, log);
        fprintf(stderr, "[d3d8_gl] shader compile failed: %.*s\n", n, log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint link_program(GLuint vs, GLuint fs)
{
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024]; GLsizei n = 0;
        glGetProgramInfoLog(p, sizeof(log), &n, log);
        fprintf(stderr, "[d3d8_gl] program link failed: %.*s\n", n, log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

static int init_gl_pipeline(void)
{
    glGenTextures(1,&g.fallback_texture);
    glBindTexture(GL_TEXTURE_2D,g.fallback_texture);
    const BYTE white[4]={255,255,255,255};
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,white);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glGenVertexArrays(1, &g.vao);
    glBindVertexArray(g.vao);

    glGenBuffers(1, &g.stream_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g.stream_vbo);

    glGenBuffers(1, &g.stream_ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g.stream_ibo);

    GLuint vs = compile_shader(GL_VERTEX_SHADER,   VS_SRC);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, FS_SRC);
    if(!vs || !fs){if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);return 0;}
    g.prog = link_program(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    if(!g.prog)return 0;
    g.u_mvp       = glGetUniformLocation(g.prog, "u_mvp");
    g.u_use_tex   = glGetUniformLocation(g.prog, "u_use_tex");
    g.u_use_xform = glGetUniformLocation(g.prog, "u_use_xform");
    g.u_tex0      = glGetUniformLocation(g.prog, "u_tex0");
    return 1;
}

/* ======================================================================== */
/* Apply current render-state cache to GL                                   */
/* ======================================================================== */

static void apply_render_states(void)
{
    native_apply_scissor();
    /* Depth */
    if (g.rs[D3DRS_ZENABLE] && native_depth_enabled)       glEnable(GL_DEPTH_TEST);
    else                            glDisable(GL_DEPTH_TEST);
    glDepthMask(g.rs[D3DRS_ZWRITEENABLE] ? GL_TRUE : GL_FALSE);
    glDepthFunc(gl_cmp(g.rs[D3DRS_ZFUNC]));

    /* Blend */
    if (g.rs[D3DRS_ALPHABLENDENABLE]) {
        glEnable(GL_BLEND);
        glBlendFunc(gl_blend(g.rs[D3DRS_SRCBLEND]),
                    gl_blend(g.rs[D3DRS_DESTBLEND]));
    } else {
        glDisable(GL_BLEND);
    }

    /* Cull */
    switch (g.rs[D3DRS_CULLMODE]) {
    case D3DCULL_NONE: glDisable(GL_CULL_FACE); break;
    case D3DCULL_CW:   glEnable(GL_CULL_FACE); glFrontFace(GL_CCW); glCullFace(GL_BACK); break;
    case D3DCULL_CCW:  glEnable(GL_CULL_FACE); glFrontFace(GL_CW);  glCullFace(GL_BACK); break;
    }

    /* Color write */
    DWORD cw = g.rs[D3DRS_COLORWRITEENABLE];
    glColorMask((cw & 1) != 0, (cw & 2) != 0, (cw & 4) != 0, (cw & 8) != 0);
}

/* ======================================================================== */
/* VB / IB / Texture wrapper structs (defined here, not in d3d8_internal.h) */
/* ======================================================================== */

typedef struct {
    IDirect3DVertexBuffer8 iface;
    LONG     ref;
    GLuint   gl_buf;
    UINT     size;
    DWORD    fvf;
    DWORD    usage;
    BYTE    *sys_mem;
    BOOL     dirty;
} GLVertexBuffer;

typedef struct {
    IDirect3DIndexBuffer8 iface;
    LONG     ref;
    GLuint   gl_buf;
    UINT     size;
    D3DFORMAT format;
    BYTE    *sys_mem;
    BOOL     dirty;
} GLIndexBuffer;

typedef struct {
    IDirect3DTexture8 iface;
    LONG     ref;
    GLuint   gl_tex;
    UINT     width, height, levels;
    D3DFORMAT format;
    BYTE    *sys_mem;        /* Level 0 staging */
    UINT     pitch;
    BOOL     dirty;
    UINT offsets[16], palette;
    DWORD usage;
    int flipped; /* GPU-rendered textures have GL bottom-up storage. */
    int screen_copy;
    GLuint pack_texture;
    uint64_t pack_key,pack_bytes;
    int pack_checked;
} GLTexture;

typedef struct {
    IDirect3DSurface8 iface;
    LONG     ref;
    UINT     width, height;
    D3DFORMAT format;
    BYTE *readback;
    int backbuffer;
    int image_surface, locked, lock_readonly;
    GLTexture *parent;
    UINT level;
    GLuint color,depth;
} GLSurface;
static const IDirect3DSurface8Vtbl g_sf_vtbl;
static GLSurface *native_target,*native_depth,*native_default_depth;
static GLuint native_framebuffer;
static GLuint surface_color(GLSurface *s) {return s->parent?s->parent->gl_tex:s->color;}


/* ======================================================================== */
/* IDirect3DVertexBuffer8                                                   */
/* ======================================================================== */

static HRESULT __stdcall vb_QueryInterface(IDirect3DVertexBuffer8 *self, const IID *iid, void **pp)
{ NATIVE_GL_SCOPE; (void)self;(void)iid;(void)pp; return D3DERR_INVALIDCALL; }
static ULONG __stdcall vb_AddRef(IDirect3DVertexBuffer8 *self)
{ NATIVE_GL_SCOPE; GLVertexBuffer *v=(GLVertexBuffer*)self; return (ULONG)__atomic_add_fetch(&v->ref,1,__ATOMIC_SEQ_CST); }
static ULONG __stdcall vb_Release(IDirect3DVertexBuffer8 *self)
{ NATIVE_GL_SCOPE;
    GLVertexBuffer *v=(GLVertexBuffer*)self;
    LONG r=__atomic_sub_fetch(&v->ref,1,__ATOMIC_SEQ_CST);
    if (r<=0) { if (v->gl_buf) glDeleteBuffers(1,&v->gl_buf); free(v->sys_mem); free(v); }
    return (ULONG)r;
}
static HRESULT __stdcall vb_GetDevice(IDirect3DVertexBuffer8 *s, IDirect3DDevice8 **pp)
{ NATIVE_GL_SCOPE; (void)s; *pp=xbox_GetD3DDevice(); return D3D_OK; }
static DWORD __stdcall vb_SetPriority(IDirect3DVertexBuffer8 *s, DWORD p) { NATIVE_GL_SCOPE; (void)s;(void)p; return 0; }
static DWORD __stdcall vb_GetPriority(IDirect3DVertexBuffer8 *s) { NATIVE_GL_SCOPE; (void)s; return 0; }
static void  __stdcall vb_PreLoad(IDirect3DVertexBuffer8 *s) { NATIVE_GL_SCOPE; (void)s; }
static DWORD __stdcall vb_GetType(IDirect3DVertexBuffer8 *s) { NATIVE_GL_SCOPE; (void)s; return 0; }
static HRESULT __stdcall vb_Lock(IDirect3DVertexBuffer8 *self, UINT off, UINT sz, BYTE **pp, DWORD flags)
{ NATIVE_GL_SCOPE;
    GLVertexBuffer *v=(GLVertexBuffer*)self; (void)flags;
    if (!v->sys_mem) return D3DERR_INVALIDCALL;
    if (sz == 0) sz = v->size - off;
    *pp = v->sys_mem + off;
    v->dirty = TRUE;
    return D3D_OK;
}
static HRESULT __stdcall vb_Unlock(IDirect3DVertexBuffer8 *self) { NATIVE_GL_SCOPE; (void)self; return D3D_OK; }
static HRESULT __stdcall vb_GetDesc(IDirect3DVertexBuffer8 *s, void *p) { NATIVE_GL_SCOPE; (void)s;(void)p; return D3D_OK; }

static const IDirect3DVertexBuffer8Vtbl g_vb_vtbl = {
    vb_QueryInterface, vb_AddRef, vb_Release,
    vb_GetDevice, vb_SetPriority, vb_GetPriority, vb_PreLoad, vb_GetType,
    vb_Lock, vb_Unlock, vb_GetDesc,
};

/* ======================================================================== */
/* IDirect3DIndexBuffer8                                                    */
/* ======================================================================== */

static HRESULT __stdcall ib_QueryInterface(IDirect3DIndexBuffer8 *s, const IID *iid, void **pp)
{ NATIVE_GL_SCOPE; (void)s;(void)iid;(void)pp; return D3DERR_INVALIDCALL; }
static ULONG __stdcall ib_AddRef(IDirect3DIndexBuffer8 *s)
{ NATIVE_GL_SCOPE; GLIndexBuffer *v=(GLIndexBuffer*)s; return (ULONG)__atomic_add_fetch(&v->ref,1,__ATOMIC_SEQ_CST); }
static ULONG __stdcall ib_Release(IDirect3DIndexBuffer8 *s)
{ NATIVE_GL_SCOPE;
    GLIndexBuffer *v=(GLIndexBuffer*)s;
    LONG r=__atomic_sub_fetch(&v->ref,1,__ATOMIC_SEQ_CST);
    if (r<=0) { if (v->gl_buf) glDeleteBuffers(1,&v->gl_buf); free(v->sys_mem); free(v); }
    return (ULONG)r;
}
static HRESULT __stdcall ib_GetDevice(IDirect3DIndexBuffer8 *s, IDirect3DDevice8 **pp)
{ NATIVE_GL_SCOPE; (void)s; *pp=xbox_GetD3DDevice(); return D3D_OK; }
static DWORD __stdcall ib_SetPriority(IDirect3DIndexBuffer8 *s, DWORD p) { NATIVE_GL_SCOPE; (void)s;(void)p; return 0; }
static DWORD __stdcall ib_GetPriority(IDirect3DIndexBuffer8 *s) { NATIVE_GL_SCOPE; (void)s; return 0; }
static void  __stdcall ib_PreLoad(IDirect3DIndexBuffer8 *s) { NATIVE_GL_SCOPE; (void)s; }
static DWORD __stdcall ib_GetType(IDirect3DIndexBuffer8 *s) { NATIVE_GL_SCOPE; (void)s; return 0; }
static HRESULT __stdcall ib_Lock(IDirect3DIndexBuffer8 *self, UINT off, UINT sz, BYTE **pp, DWORD flags)
{ NATIVE_GL_SCOPE;
    GLIndexBuffer *v=(GLIndexBuffer*)self; (void)flags;
    if (!v->sys_mem) return D3DERR_INVALIDCALL;
    if (sz == 0) sz = v->size - off;
    *pp = v->sys_mem + off;
    v->dirty = TRUE;
    return D3D_OK;
}
static HRESULT __stdcall ib_Unlock(IDirect3DIndexBuffer8 *s) { NATIVE_GL_SCOPE; (void)s; return D3D_OK; }
static HRESULT __stdcall ib_GetDesc(IDirect3DIndexBuffer8 *s, void *p) { NATIVE_GL_SCOPE; (void)s;(void)p; return D3D_OK; }

static const IDirect3DIndexBuffer8Vtbl g_ib_vtbl = {
    ib_QueryInterface, ib_AddRef, ib_Release,
    ib_GetDevice, ib_SetPriority, ib_GetPriority, ib_PreLoad, ib_GetType,
    ib_Lock, ib_Unlock, ib_GetDesc,
};

#include "gl_texture_pack.inc"

/* ======================================================================== */
/* IDirect3DTexture8                                                        */
/* ======================================================================== */

static HRESULT __stdcall tex_QueryInterface(IDirect3DTexture8 *s, const IID *iid, void **pp)
{ NATIVE_GL_SCOPE; (void)s;(void)iid;(void)pp; return D3DERR_INVALIDCALL; }
static ULONG __stdcall tex_AddRef(IDirect3DTexture8 *s)
{ NATIVE_GL_SCOPE; GLTexture *t=(GLTexture*)s; return (ULONG)__atomic_add_fetch(&t->ref,1,__ATOMIC_SEQ_CST); }
static ULONG __stdcall tex_Release(IDirect3DTexture8 *s)
{ NATIVE_GL_SCOPE;
    GLTexture *t=(GLTexture*)s;
    LONG r=__atomic_sub_fetch(&t->ref,1,__ATOMIC_SEQ_CST);
    if (r<=0) { native_pack_release(t); if (t->gl_tex) glDeleteTextures(1,&t->gl_tex); free(t->sys_mem); free(t); }
    return (ULONG)r;
}
static HRESULT __stdcall tex_GetDevice(IDirect3DTexture8 *s, IDirect3DDevice8 **pp)
{ NATIVE_GL_SCOPE; (void)s; *pp=xbox_GetD3DDevice(); return D3D_OK; }
static DWORD __stdcall tex_SetPriority(IDirect3DTexture8 *s, DWORD p) { NATIVE_GL_SCOPE; (void)s;(void)p; return 0; }
static DWORD __stdcall tex_GetPriority(IDirect3DTexture8 *s) { NATIVE_GL_SCOPE; (void)s; return 0; }
static void  __stdcall tex_PreLoad(IDirect3DTexture8 *s) { NATIVE_GL_SCOPE; (void)s; }
static DWORD __stdcall tex_GetType(IDirect3DTexture8 *s) { NATIVE_GL_SCOPE; (void)s; return 0; }
static DWORD __stdcall tex_GetLevelCount(IDirect3DTexture8 *s)
{ NATIVE_GL_SCOPE; GLTexture *t=(GLTexture*)s; return t->levels; }
static HRESULT __stdcall tex_GetLevelDesc(IDirect3DTexture8 *s, UINT lvl, D3DSURFACE_DESC *pD)
{ NATIVE_GL_SCOPE;
    GLTexture *t=(GLTexture*)s;
    if (!pD || lvl>=t->levels) return D3DERR_INVALIDCALL;
    memset(pD,0,sizeof(*pD));
    pD->Format = t->format;
    pD->Width  = t->width  >> lvl;  if (pD->Width  == 0) pD->Width  = 1;
    pD->Height = t->height >> lvl;  if (pD->Height == 0) pD->Height = 1;
    return D3D_OK;
}
static HRESULT __stdcall tex_GetSurfaceLevel(IDirect3DTexture8 *s, UINT lvl, IDirect3DSurface8 **pp)
{ NATIVE_GL_SCOPE;
    GLTexture *t=(GLTexture*)s;
    if(!pp || lvl>=t->levels)return D3DERR_INVALIDCALL;
    GLSurface *surface=calloc(1,sizeof *surface);if(!surface)return E_FAIL;
    surface->iface.lpVtbl=&g_sf_vtbl;surface->ref=1;surface->parent=t;surface->level=lvl;
    surface->width=t->width>>lvl;if(!surface->width)surface->width=1;
    surface->height=t->height>>lvl;if(!surface->height)surface->height=1;
    surface->format=t->format;tex_AddRef(s);*pp=&surface->iface;return S_OK;
}
static HRESULT __stdcall tex_LockRect(IDirect3DTexture8 *self, UINT lvl, D3DLOCKED_RECT *pLR,
                                       const RECT *r, DWORD flags)
{ NATIVE_GL_SCOPE;
    GLTexture *t=(GLTexture*)self;
    if(!pLR || !t->sys_mem || lvl>=t->levels || r)return D3DERR_INVALIDCALL;
    UINT w=t->width>>lvl;if(!w)w=1;
    pLR->Pitch=d3d8_row_pitch(t->format,w);pLR->pBits=t->sys_mem+t->offsets[lvl];t->dirty=TRUE;return S_OK;
}
static HRESULT __stdcall tex_UnlockRect(IDirect3DTexture8 *self,UINT lvl) { NATIVE_GL_SCOPE;
    GLTexture *t=(GLTexture*)self;if(lvl>=t->levels)return D3DERR_INVALIDCALL;
    t->flipped=0;t->screen_copy=0;
    UINT w=t->width>>lvl,h=t->height>>lvl;if(!w)w=1;if(!h)h=1;
    const BYTE *src=t->sys_mem+t->offsets[lvl];
    glBindTexture(GL_TEXTURE_2D,t->gl_tex);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    if(d3d8_format_is_compressed(t->format)) {
        GLenum f=t->format==D3DFMT_DXT1?0x83F1:t->format==D3DFMT_DXT3?0x83F2:0x83F3;
        glCompressedTexImage2D(GL_TEXTURE_2D,lvl,f,w,h,0,d3d8_row_pitch(t->format,w)*((h+3)/4),src);
    } else {
        BYTE *tmp=NULL,*linear=NULL;const BYTE *pixels=src;
        if(d3d8_format_is_swizzled(t->format)) {linear=malloc((size_t)w*h*d3d8_format_bpp(t->format)/8);if(!linear)return E_FAIL;xbox_unswizzle_rect(linear,src,w,h,d3d8_format_bpp(t->format)/8);pixels=linear;}
        if(d3d8_format_has_conversion(t->format)){tmp=malloc((size_t)w*h*16);if(!tmp){free(linear);return E_FAIL;}d3d8_convert_linear_pixels(t->format,w,h,pixels,tmp,t->palette);pixels=tmp;}
        RhiFormat f=d3d8_to_dxgi_format(t->format);GLenum format=GL_BGRA,type=GL_UNSIGNED_BYTE;
        switch(f){
        case RHI_FORMAT_A8_UNORM:format=GL_RED;break;
        case RHI_FORMAT_B8G8R8A8_UNORM:case RHI_FORMAT_B8G8R8X8_UNORM:break;
        case RHI_FORMAT_R8G8B8A8_UNORM:format=GL_RGBA;break;
        case RHI_FORMAT_B5G6R5_UNORM:format=GL_RGB;type=GL_UNSIGNED_SHORT_5_6_5;break;
        case RHI_FORMAT_B5G5R5A1_UNORM:type=GL_UNSIGNED_SHORT_1_5_5_5_REV;break;
        case RHI_FORMAT_B4G4R4A4_UNORM:type=GL_UNSIGNED_SHORT_4_4_4_4_REV;break;
        default:free(linear);free(tmp);fprintf(stderr,"[native-gl] unsupported texture format 0x%x\n",t->format);return E_NOTIMPL;
        }
        glTexImage2D(GL_TEXTURE_2D,lvl,f==RHI_FORMAT_A8_UNORM?GL_R8:GL_RGBA8,w,h,0,format,type,pixels);
        if(f==RHI_FORMAT_A8_UNORM){GLint swizzle[4]={GL_ONE,GL_ONE,GL_ONE,GL_RED};glTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_SWIZZLE_RGBA,swizzle);}free(linear);free(tmp);
    }
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,t->levels-1);
    if(lvl==0)native_pack_upload(t);t->dirty=FALSE;return S_OK;
}

static const IDirect3DTexture8Vtbl g_tex_vtbl = {
    tex_QueryInterface, tex_AddRef, tex_Release,
    tex_GetDevice, tex_SetPriority, tex_GetPriority, tex_PreLoad, tex_GetType,
    tex_GetLevelCount,
    tex_GetLevelDesc, tex_GetSurfaceLevel, tex_LockRect, tex_UnlockRect,
};

/* ======================================================================== */
/* IDirect3DSurface8 (minimal stub)                                         */
/* ======================================================================== */

static HRESULT __stdcall sf_QueryInterface(IDirect3DSurface8 *s, const IID *iid, void **pp)
{ NATIVE_GL_SCOPE; (void)s;(void)iid;(void)pp; return D3DERR_INVALIDCALL; }
static ULONG __stdcall sf_AddRef(IDirect3DSurface8 *s)
{ NATIVE_GL_SCOPE; GLSurface *t=(GLSurface*)s; return (ULONG)__atomic_add_fetch(&t->ref,1,__ATOMIC_SEQ_CST); }
static ULONG __stdcall sf_Release(IDirect3DSurface8 *s)
{ NATIVE_GL_SCOPE;
    GLSurface *t=(GLSurface*)s;
    LONG r=__atomic_sub_fetch(&t->ref,1,__ATOMIC_SEQ_CST);
    if (r<=0) {
        if(t->parent)tex_Release(&t->parent->iface);
        if(t->color)glDeleteTextures(1,&t->color);
        if(t->depth)glDeleteRenderbuffers(1,&t->depth);
        free(t->readback);free(t);
    }
    return (ULONG)r;
}
static HRESULT __stdcall sf_GetDevice(IDirect3DSurface8 *s, IDirect3DDevice8 **pp)
{ NATIVE_GL_SCOPE; (void)s; *pp=xbox_GetD3DDevice(); return D3D_OK; }
static HRESULT __stdcall sf_GetDesc(IDirect3DSurface8 *s, D3DSURFACE_DESC *pD)
{ NATIVE_GL_SCOPE;
    GLSurface *t=(GLSurface*)s;
    if (!pD) return D3DERR_INVALIDCALL;
    memset(pD,0,sizeof(*pD));
    pD->Format=t->format; pD->Width=t->width; pD->Height=t->height;
    return D3D_OK;
}
static HRESULT __stdcall sf_LockRect(IDirect3DSurface8 *s, D3DLOCKED_RECT *pL, const RECT *r, DWORD f)
{ NATIVE_GL_SCOPE;
    GLSurface *t=(GLSurface*)s;
    if(!pL || t->locked || (r && !t->image_surface) || (!t->backbuffer && !surface_color(t)))return D3DERR_INVALIDCALL;
    if(r && (r->left<0 || r->top<0 || r->right<=r->left || r->bottom<=r->top ||
             r->right>(int)t->width || r->bottom>(int)t->height))return D3DERR_INVALIDCALL;
    size_t row=(size_t)t->width*4, size=row*t->height;
    if(!t->readback)t->readback=malloc(size);
    BYTE *raw=malloc(size);if(!t->readback||!raw){free(raw);return E_FAIL;}
    GLint previous,previous_draw;glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&previous);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&previous_draw);
    GLuint read_fbo=0,read_color=0;
    if(t->backbuffer)read_fbo=native_scene_readback(t->width,t->height,&read_color);
    else {
        glGenFramebuffers(1,&read_fbo);glBindFramebuffer(GL_READ_FRAMEBUFFER,read_fbo);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,surface_color(t),t->level);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
    }
    glReadPixels(0,0,t->width,t->height,t->image_surface?GL_BGRA:GL_RGBA,GL_UNSIGNED_BYTE,raw);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,previous);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,previous_draw);
    if(read_fbo)glDeleteFramebuffers(1,&read_fbo);if(read_color)glDeleteRenderbuffers(1,&read_color);
    for(UINT y=0;y<t->height;y++)memcpy(t->readback+y*row,raw+(t->height-1-y)*row,row);
    free(raw);pL->pBits=t->readback+(r?(size_t)r->top*row+r->left*4:0);pL->Pitch=(INT)row;
    t->locked=1;t->lock_readonly=!!(f&D3DLOCK_READONLY);return S_OK;
}
static HRESULT __stdcall sf_UnlockRect(IDirect3DSurface8 *s) {
    NATIVE_GL_SCOPE;GLSurface *t=(GLSurface*)s;
    if(!t->locked)return D3DERR_INVALIDCALL;
    t->locked=0;
    if(t->image_surface && !t->lock_readonly) {
        size_t row=(size_t)t->width*4;BYTE *raw=malloc(row*t->height);if(!raw)return E_FAIL;
        for(UINT y=0;y<t->height;y++)memcpy(raw+y*row,t->readback+(t->height-1-y)*row,row);
        GLint binding;glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding);
        glBindTexture(GL_TEXTURE_2D,t->color);
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,t->width,t->height,GL_BGRA,GL_UNSIGNED_BYTE,raw);
        glBindTexture(GL_TEXTURE_2D,binding);free(raw);
    }
    return S_OK;
}

static const IDirect3DSurface8Vtbl g_sf_vtbl = {
    sf_QueryInterface, sf_AddRef, sf_Release,
    sf_GetDevice, sf_GetDesc, sf_LockRect, sf_UnlockRect,
};

/* ======================================================================== */
/* IDirect3DDevice8                                                         */
/* ======================================================================== */

static IDirect3DDevice8 g_device;   /* singleton, vtable set at init */

static HRESULT __stdcall dev_QueryInterface(IDirect3DDevice8 *s, const IID *iid, void **pp)
{ NATIVE_GL_SCOPE; (void)s;(void)iid;(void)pp; return D3DERR_INVALIDCALL; }
static ULONG __stdcall dev_AddRef(IDirect3DDevice8 *s) { NATIVE_GL_SCOPE; (void)s; return 1; }
static ULONG __stdcall dev_Release(IDirect3DDevice8 *s) { NATIVE_GL_SCOPE; (void)s; return 0; }

static HRESULT __stdcall dev_GetDirect3D(IDirect3DDevice8 *s, IDirect3D8 **pp)
{ NATIVE_GL_SCOPE; (void)s; (void)pp; return D3D_OK; }
static HRESULT __stdcall dev_GetDeviceCaps(IDirect3DDevice8 *s, void *pC) { NATIVE_GL_SCOPE; (void)s;(void)pC; return D3D_OK; }
static HRESULT __stdcall dev_GetDisplayMode(IDirect3DDevice8 *s, void *pM) { NATIVE_GL_SCOPE; (void)s;(void)pM; return D3D_OK; }
static HRESULT __stdcall dev_GetCreationParameters(IDirect3DDevice8 *s, void *pP) { NATIVE_GL_SCOPE; (void)s;(void)pP; return D3D_OK; }

static HRESULT __stdcall dev_Reset(IDirect3DDevice8 *s, D3DPRESENT_PARAMETERS *pPP)
{ NATIVE_GL_SCOPE; (void)s;(void)pPP; return D3D_OK; }

/* Opt-in CPU wall-clock intervals between completed host presents. This does
 * not wait for the GPU or claim display scanout timing. At most 20,000 rows
 * (under 2 MiB), with no per-frame flush or readback. */
static void native_frame_timing(Uint64 swap_start,Uint64 now) {
    static int checked;
    static FILE *output;
    static Uint64 first,last;
    static unsigned rows;
    if(!checked) {
        checked=1;const char *path=getenv("TSFP_NATIVE_FRAME_PROFILE");
        if(path && *path) {
            output=fopen(path,"w");
            if(output)fputs("frame,elapsed_ms,frame_ms,swap_ms,kind,guest_frame\n",output);
        }
    }
    if(!output)return;
    if(!first)first=now;
    if(last) {
        fprintf(output,"%u,%.3f,%.3f,%.3f,%s,%u\n",native_present_count,
                (now-first)/1e6,(now-last)/1e6,(now-swap_start)/1e6,
                native_interp_on?"interpolated":"guest",native_guest_present_count);
        if(++rows%60==0)fflush(output);
        if(rows==20000){fclose(output);output=NULL;}
    }
    last=now;
}

static HRESULT __stdcall dev_Present(IDirect3DDevice8 *s, const RECT *src, const RECT *dst,
                                     HWND hwnd, void *dirty)
{ NATIVE_GL_SCOPE;
    (void)s;(void)src;(void)dst;(void)hwnd;(void)dirty;
    native_scene_present();
    Uint64 swap_start=SDL_GetTicksNS();
    if(!native_swap_window()) {
        fprintf(stderr,"[native-gl] present failed: %s\n",SDL_GetError());return E_FAIL;
    }
    native_present_count++;native_guest_present_count++;
    native_frame_timing(swap_start,SDL_GetTicksNS());
    if(native_guest_present_count==1 || native_guest_present_count%300==0)
        fprintf(stderr,"[native-gl] presented frame %u, GL error=0x%x\n",native_present_count,glGetError());
    pc_display_frame_done();
    trace_draw=0;
    const char *trace_path=getenv("TSFP_NATIVE_TRACE_RENDER");
    if(trace_path) {
        FILE *f=fopen(trace_path,"r");int requested=0;
        if(f){fscanf(f,"%d",&requested);fclose(f);}
        if(requested){f=fopen(trace_path,"w");if(f)fclose(f);trace_draw=1;fprintf(stderr,"[native-draw] begin frame %u\n",native_present_count);}
    }
    /* Cocoa event pumping stays on the main thread. The guest has no host
     * close-window loop, so quit ends the native process directly. */
    if(SDL_IsMainThread())tsfp_native_input_frame(g.window);
    SDL_Event ev;
    while (SDL_IsMainThread() && SDL_PollEvent(&ev)) {
        tsfp_native_input_event(&ev,g.window);
        if (ev.type == SDL_EVENT_QUIT || ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            fprintf(stderr, "[native-input] window close requested\n");
            exit(0);
        }
    }
    return D3D_OK;
}

static HRESULT __stdcall dev_GetBackBuffer(IDirect3DDevice8 *s, INT i, DWORD t,
                                            IDirect3DSurface8 **pp)
{ NATIVE_GL_SCOPE;
    if(!pp||i)return D3DERR_INVALIDCALL;
    GLSurface *b=calloc(1,sizeof *b);if(!b)return E_FAIL;
    b->iface.lpVtbl=&g_sf_vtbl;b->ref=1;b->width=g.backbuf_w;b->height=g.backbuf_h;b->format=D3DFMT_A8R8G8B8;b->backbuffer=1;*pp=&b->iface;return S_OK;
}

static HRESULT __stdcall dev_BeginScene(IDirect3DDevice8 *s) { NATIVE_GL_SCOPE; (void)s; return D3D_OK; }
static HRESULT __stdcall dev_EndScene(IDirect3DDevice8 *s)   { NATIVE_GL_SCOPE; (void)s; return D3D_OK; }

static HRESULT __stdcall dev_Clear(IDirect3DDevice8 *s, DWORD count, const D3DRECT *rects,
                                    DWORD flags, D3DCOLOR color, float z, DWORD stencil)
{ NATIVE_GL_SCOPE;
    (void)s;if(count && !rects)return D3DERR_INVALIDCALL;
    GLbitfield m = 0;
    if (flags & D3DCLEAR_TARGET) {
        float a = ((color >> 24) & 0xFF) / 255.0f;
        float r = ((color >> 16) & 0xFF) / 255.0f;
        float gr= ((color >>  8) & 0xFF) / 255.0f;
        float b = ((color      ) & 0xFF) / 255.0f;
        glClearColor(r, gr, b, a);
        m |= GL_COLOR_BUFFER_BIT;
    }
    if (flags & D3DCLEAR_ZBUFFER)  { glClearDepth(z);           m |= GL_DEPTH_BUFFER_BIT;   }
    if (flags & D3DCLEAR_STENCIL)  { glClearStencil((GLint)stencil); m |= GL_STENCIL_BUFFER_BIT; }
    if(m) {
        GLint old_box[4],old_stencil;GLboolean scissor=glIsEnabled(GL_SCISSOR_TEST);
        glGetIntegerv(GL_SCISSOR_BOX,old_box);glGetIntegerv(GL_STENCIL_WRITEMASK,&old_stencil);
        glDepthMask(GL_TRUE);glColorMask(1,1,1,1);glStencilMask(~0u);glEnable(GL_SCISSOR_TEST);
        /* D3D clears are clipped to the viewport, even without explicit rects. */
        for(DWORD i=0;i<(count?count:1);i++) {
            int x=g.viewport.X,y=g.viewport.Y,right=x+g.viewport.Width,bottom=y+g.viewport.Height;
            if(count && rects) {
                if(rects[i].x1>x)x=rects[i].x1;if(rects[i].y1>y)y=rects[i].y1;
                if(rects[i].x2<right)right=rects[i].x2;if(rects[i].y2<bottom)bottom=rects[i].y2;
            }
            if(right>x && bottom>y) {
                double sx=native_scene_active?(double)native_scene_w/g.backbuf_w:1;
                double sy=native_scene_active?(double)native_scene_h/g.backbuf_h:1;
                glScissor(x*sx,(native_target_height-bottom)*sy,(right-x)*sx,(bottom-y)*sy);glClear(m);
            }
        }
        glStencilMask(old_stencil);glScissor(old_box[0],old_box[1],old_box[2],old_box[3]);
        if(!scissor)glDisable(GL_SCISSOR_TEST);
        apply_render_states();
    }
    native_trace("clear",NULL,0);
    return D3D_OK;
}

static HRESULT __stdcall dev_SetTransform(IDirect3DDevice8 *s, D3DTRANSFORMSTATETYPE st,
                                          const D3DMATRIX *m)
{ NATIVE_GL_SCOPE;
    (void)s; if (!m) return D3DERR_INVALIDCALL;
    switch (st) {
    case D3DTS_WORLD:      g.m_world = *m; break;
    case D3DTS_VIEW:       g.m_view  = *m; break;
    case D3DTS_PROJECTION: g.m_proj  = *m; break;
    default: break;
    }
    return D3D_OK;
}
static HRESULT __stdcall dev_GetTransform(IDirect3DDevice8 *s, D3DTRANSFORMSTATETYPE st,
                                          D3DMATRIX *m)
{ NATIVE_GL_SCOPE;
    (void)s; if (!m) return D3DERR_INVALIDCALL;
    switch (st) {
    case D3DTS_WORLD:      *m = g.m_world; break;
    case D3DTS_VIEW:       *m = g.m_view;  break;
    case D3DTS_PROJECTION: *m = g.m_proj;  break;
    default: mat4_identity(m); break;
    }
    return D3D_OK;
}

static HRESULT __stdcall dev_SetRenderState(IDirect3DDevice8 *s, D3DRENDERSTATETYPE st, DWORD v)
{ NATIVE_GL_SCOPE;
    (void)s;
    if ((unsigned)st < MAX_RS) g.rs[st] = v;
    if (st == D3DRS_TEXTUREFACTOR) g.tex_factor = v;
    return D3D_OK;
}
static HRESULT __stdcall dev_GetRenderState(IDirect3DDevice8 *s, D3DRENDERSTATETYPE st, DWORD *pv)
{ NATIVE_GL_SCOPE;
    (void)s; if (!pv) return D3DERR_INVALIDCALL;
    *pv = ((unsigned)st < MAX_RS) ? g.rs[st] : 0;
    return D3D_OK;
}

static HRESULT __stdcall dev_SetTextureStageState(IDirect3DDevice8 *s, DWORD st,
                                                  D3DTEXTURESTAGESTATETYPE ty, DWORD v)
{ NATIVE_GL_SCOPE;
    (void)s;
    if (st < MAX_TSU && (unsigned)ty < MAX_TSS) g.tss[st][ty] = v;
    return D3D_OK;
}
static HRESULT __stdcall dev_GetTextureStageState(IDirect3DDevice8 *s, DWORD st,
                                                  D3DTEXTURESTAGESTATETYPE ty, DWORD *pv)
{ NATIVE_GL_SCOPE;
    (void)s; if (!pv) return D3DERR_INVALIDCALL;
    *pv = (st < MAX_TSU && (unsigned)ty < MAX_TSS) ? g.tss[st][ty] : 0;
    return D3D_OK;
}

static HRESULT __stdcall dev_SetTexture(IDirect3DDevice8 *s, DWORD st, IDirect3DBaseTexture8 *t)
{ NATIVE_GL_SCOPE; (void)s; if (st < MAX_TSU) g.textures[st] = t; return D3D_OK; }
static HRESULT __stdcall dev_GetTexture(IDirect3DDevice8 *s, DWORD st, IDirect3DBaseTexture8 **pp)
{ NATIVE_GL_SCOPE; (void)s; if (!pp) return D3DERR_INVALIDCALL; *pp = (st < MAX_TSU) ? g.textures[st] : NULL; return D3D_OK; }

static HRESULT __stdcall dev_SetStreamSource(IDirect3DDevice8 *s, UINT sn,
                                             IDirect3DVertexBuffer8 *pVB, UINT stride)
{ NATIVE_GL_SCOPE; (void)s;(void)sn; g.vb = pVB; g.vb_stride = stride; g.vb_offset = 0; return D3D_OK; }
static HRESULT __stdcall dev_GetStreamSource(IDirect3DDevice8 *s, UINT sn,
                                             IDirect3DVertexBuffer8 **pp, UINT *st)
{ NATIVE_GL_SCOPE; (void)s;(void)sn; if(pp) *pp=g.vb; if(st) *st=g.vb_stride; return D3D_OK; }
static HRESULT __stdcall dev_SetIndices(IDirect3DDevice8 *s, IDirect3DIndexBuffer8 *pIB, UINT base)
{ NATIVE_GL_SCOPE; (void)s; g.ib = pIB; g.ib_base = base; return D3D_OK; }
static HRESULT __stdcall dev_GetIndices(IDirect3DDevice8 *s, IDirect3DIndexBuffer8 **pp, UINT *pb)
{ NATIVE_GL_SCOPE; (void)s; if(pp) *pp=g.ib; if(pb) *pb=g.ib_base; return D3D_OK; }

/* Vertex layout setup for the FVF the recompiled RW driver actually emits at
 * this point: XYZ (or XYZRHW) + DIFFUSE + optional TEX1. */
static void setup_fvf_attribs(DWORD fvf, UINT stride)
{
    GLboolean xyzrhw = d3d8_fvf_transformed(fvf) ? GL_TRUE : GL_FALSE;
    GLboolean has_diff = (fvf & D3DFVF_DIFFUSE) ? GL_TRUE : GL_FALSE;
    UINT off = 0;
    /* Position: 3 floats (XYZ) or 4 floats (XYZRHW) */
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, xyzrhw ? 4 : 3, GL_FLOAT, GL_FALSE, stride,
                          (const void *)(uintptr_t)off);
    off = d3d8_fvf_position_bytes(fvf);
    if (fvf & D3DFVF_NORMAL) off += 12;
    /* Diffuse: D3DCOLOR (BGRA byte order, normalised to 0..1) */
    if (has_diff) {
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, GL_BGRA, GL_UNSIGNED_BYTE, GL_TRUE, stride,
                              (const void *)(uintptr_t)off);
        off += 4;
    } else {
        glDisableVertexAttribArray(1);
        glVertexAttrib4f(1, 1.f, 1.f, 1.f, 1.f);
    }
    /* Specular */
    if (fvf & D3DFVF_SPECULAR) off += 4;
    unsigned count=(fvf&D3DFVF_TEXCOUNT_MASK)>>D3DFVF_TEXCOUNT_SHIFT;
    for(unsigned stage=0;stage<4;stage++) {
        unsigned encoded=(fvf>>(16+stage*2))&3;
        unsigned components=encoded==0?2:encoded==1?3:encoded==2?4:1;
        if(stage<count && off+components*4<=stride) {
            glEnableVertexAttribArray(2+stage);
            glVertexAttribPointer(2+stage,components,GL_FLOAT,GL_FALSE,stride,(void*)(uintptr_t)off);
            off+=components*4;
        } else {glDisableVertexAttribArray(2+stage);glVertexAttrib4f(2+stage,0,0,0,1);}
    }
}

static GLenum native_wrap(DWORD mode) {
    return mode==D3DTADDRESS_CLAMP?GL_CLAMP_TO_EDGE:mode==D3DTADDRESS_MIRROR?GL_MIRRORED_REPEAT:
           mode==D3DTADDRESS_BORDER?GL_CLAMP_TO_BORDER:GL_REPEAT;
}
static GLuint native_samplers[4];
static void bind_texture0(void)
{
    if(!native_samplers[0])glGenSamplers(4,native_samplers);
    GLint used[4],flipped[4],modes[4];GLfloat scale[8];
    DWORD token=d3d8_combiners_get_pixel_shader(),tm=g.rs[D3DRS_PSTEXTUREMODES];
    static unsigned warned;
    for(int stage=0;stage<4;stage++) {
        GLTexture *t=(GLTexture*)g.textures[stage];
        glActiveTexture(GL_TEXTURE0+stage);glBindSampler(stage,native_samplers[stage]);used[stage]=t && t->gl_tex;
        glBindTexture(GL_TEXTURE_2D,used[stage]?(t->pack_texture?t->pack_texture:t->gl_tex):g.fallback_texture);
        glSamplerParameteri(native_samplers[stage],GL_TEXTURE_MIN_FILTER,g.tss[stage][D3DTSS_MINFILTER]==D3DTEXF_POINT?GL_NEAREST:(t&&t->pack_texture?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR));
        glSamplerParameteri(native_samplers[stage],GL_TEXTURE_MAG_FILTER,g.tss[stage][D3DTSS_MAGFILTER]==D3DTEXF_POINT?GL_NEAREST:GL_LINEAR);
        glSamplerParameteri(native_samplers[stage],GL_TEXTURE_WRAP_S,native_wrap(g.tss[stage][D3DTSS_ADDRESSU]));
        glSamplerParameteri(native_samplers[stage],GL_TEXTURE_WRAP_T,native_wrap(g.tss[stage][D3DTSS_ADDRESSV]));
        flipped[stage]=t && t->flipped;
        scale[stage*2]=scale[stage*2+1]=1;
        if(token && t && d3d8_format_is_linear(t->format)) {
            scale[stage*2]=1.0f/t->width;scale[stage*2+1]=1.0f/t->height;
        }
        /* Internal modes: 0 absent, 1 projected 2D, 4 passthrough,
           32 ordinary 2D. Other Xbox modes remain explicit. */
        if(!token)modes[stage]=32;
        else if((token>>8)&0xffff) {
            unsigned m=(token>>(8+stage*4))&15;modes[stage]=m==0?32:m==3?0:31;
        } else if(tm&D3D8_PSTEXTUREMODES_XBOX)modes[stage]=(tm>>(5*stage))&31;
        else {unsigned m=(tm>>(4*stage))&15;modes[stage]=m==0?32:m==3?0:31;}
        unsigned mode=modes[stage];
        if(mode!=0 && mode!=1 && mode!=4 && mode!=32 && !(warned&(1u<<mode))) {
            warned|=1u<<mode;fprintf(stderr,"[native-gl] texture shader mode %u remains unsupported\n",mode);
        }
        char name[24];snprintf(name,sizeof name,"u_tex%d",stage);
        glUniform1i(glGetUniformLocation(g.prog,name),stage);
    }
    glActiveTexture(GL_TEXTURE0);
    glUniform1iv(glGetUniformLocation(g.prog,"u_used"),4,used);
    glUniform1iv(glGetUniformLocation(g.prog,"u_flip"),4,flipped);
    glUniform1iv(glGetUniformLocation(g.prog,"u_mode"),4,modes);
    glUniform2fv(glGetUniformLocation(g.prog,"u_scale"),4,scale);
    glUniform1i(glGetUniformLocation(g.prog,"u_combiner"),token!=0);
    glUniform1uiv(glGetUniformLocation(g.prog,"u_rs"),256,g.rs);
}

static void update_mvp(DWORD fvf)
{
    D3DMATRIX wv, mvp;
    mat4_mul(&wv, &g.m_world, &g.m_view);
    mat4_mul(&mvp, &wv, &g.m_proj);
    /* GL is column-major; D3D MATRIX is row-major. Tell GL to transpose. */
    glUniformMatrix4fv(g.u_mvp, 1, GL_TRUE, (const GLfloat *)mvp.m);
    glUniform1i(g.u_use_xform, d3d8_fvf_transformed(fvf) ? 0 : 1);
    glUniform2f(glGetUniformLocation(g.prog,"u_size"),native_target_width,native_target_height);
}

static HRESULT __stdcall dev_DrawPrimitiveUP(IDirect3DDevice8 *s, D3DPRIMITIVETYPE pt,
                                              UINT prim_count, const void *verts, UINT stride)
{ NATIVE_GL_SCOPE;
    (void)s;
    if (!verts || prim_count == 0 || stride == 0) return D3D_OK;
    UINT vcount = 0;
    GLenum mode = gl_prim(pt, prim_count, &vcount);
    native_place_draw(verts,stride,vcount);
    float *converted=NULL;
    GLuint gpu=native_gpu_prepare(stride);
    if(!gpu && d3d8_vsh_is_programmable(g.current_vsh)) {
        converted=native_shader_vertices(verts,vcount,stride);if(!converted)return E_FAIL;
        verts=converted;stride=96;
    }
    GLsizeiptr size = (GLsizeiptr)vcount * stride;
    native_select_program(gpu?gpu:native_fixed_program);
    glBindVertexArray(g.vao);
    glBindBuffer(GL_ARRAY_BUFFER, g.stream_vbo);
    /* Replace the store for each transient draw. Updating offset zero of the
     * previous store made Apple's GL driver wait for earlier GPU draws. */
    glBufferData(GL_ARRAY_BUFFER, size, verts, GL_STREAM_DRAW);
    if(!converted && !gpu)setup_fvf_attribs(g.current_fvf, stride);
    bind_texture0();
    update_mvp(g.current_fvf);
    if(converted)native_shader_attribs();
    if(gpu)native_gpu_attribs(stride);
    apply_render_states();
    if(pt==D3DPT_QUADLIST) {
        GLuint *indices=malloc((size_t)prim_count*6*sizeof *indices);
        if(!indices){free(converted);return E_FAIL;}
        for(UINT q=0;q<prim_count;q++){GLuint v=q*4;GLuint quad[6]={v,v+1,v+2,v,v+2,v+3};memcpy(indices+q*6,quad,sizeof quad);}
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,g.stream_ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,(size_t)prim_count*6*sizeof *indices,indices,GL_STREAM_DRAW);
        glDrawElements(GL_TRIANGLES,prim_count*6,GL_UNSIGNED_INT,0);free(indices);
    } else glDrawArrays(mode, 0, (GLsizei)vcount);
    native_trace("draw",converted,vcount);
    free(converted);
    return D3D_OK;
}

static HRESULT __stdcall dev_DrawIndexedPrimitiveUP(IDirect3DDevice8 *s, D3DPRIMITIVETYPE pt,
        UINT min_vi, UINT nv, UINT prim_count, const void *idx, D3DFORMAT idx_fmt,
        const void *verts, UINT stride)
{ NATIVE_GL_SCOPE;
    (void)s; (void)min_vi;
    if (!verts || !idx || prim_count == 0) return D3D_OK;
    UINT icount = 0;
    GLenum mode = gl_prim(pt, prim_count, &icount);
    GLenum itype = (idx_fmt == D3DFMT_INDEX32) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
    GLsizeiptr ibytes = (GLsizeiptr)icount * ((itype == GL_UNSIGNED_INT) ? 4 : 2);
    native_place_draw(verts,stride,nv);
    float *converted=NULL;
    GLuint gpu=native_gpu_prepare(stride);
    if(!gpu && d3d8_vsh_is_programmable(g.current_vsh)) {
        converted=native_shader_vertices(verts,nv,stride);if(!converted)return E_FAIL;
        verts=converted;stride=96;
    }
    GLsizeiptr vbytes = (GLsizeiptr)nv * stride;
    native_select_program(gpu?gpu:native_fixed_program);
    glBindVertexArray(g.vao);
    glBindBuffer(GL_ARRAY_BUFFER, g.stream_vbo);
    glBufferData(GL_ARRAY_BUFFER, vbytes, verts, GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g.stream_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, ibytes, idx, GL_STREAM_DRAW);
    if(!converted && !gpu)setup_fvf_attribs(g.current_fvf, stride);
    bind_texture0();
    update_mvp(g.current_fvf);
    if(converted)native_shader_attribs();
    if(gpu)native_gpu_attribs(stride);
    apply_render_states();
    glDrawElements(mode, (GLsizei)icount, itype, 0);
    native_trace("indexed",converted,nv);
    free(converted);
    return D3D_OK;
}

static HRESULT __stdcall dev_DrawPrimitive(IDirect3DDevice8 *s, D3DPRIMITIVETYPE pt,
                                            UINT start, UINT prim_count)
{ NATIVE_GL_SCOPE;
    (void)s;
    GLVertexBuffer *vb = (GLVertexBuffer *)g.vb;
    if (!vb || !vb->sys_mem) return D3D_OK;
    UINT vcount = 0;
    GLenum mode = gl_prim(pt, prim_count, &vcount);
    const BYTE *base = vb->sys_mem + start * g.vb_stride;
    return dev_DrawPrimitiveUP(s, pt, prim_count, base, g.vb_stride);
    (void)mode; (void)vcount;
}

static HRESULT __stdcall dev_DrawIndexedPrimitive(IDirect3DDevice8 *s, D3DPRIMITIVETYPE pt,
        UINT min_vi, UINT nv, UINT start_idx, UINT prim_count)
{ NATIVE_GL_SCOPE;
    (void)s;
    GLVertexBuffer *vb = (GLVertexBuffer *)g.vb;
    GLIndexBuffer  *ib = (GLIndexBuffer  *)g.ib;
    if (!vb || !ib || !vb->sys_mem || !ib->sys_mem) return D3D_OK;
    D3DFORMAT ifmt = ib->format;
    UINT istride = (ifmt == D3DFMT_INDEX32) ? 4 : 2;
    const BYTE *iptr = ib->sys_mem + start_idx * istride;
    return dev_DrawIndexedPrimitiveUP(s, pt, min_vi, nv, prim_count,
                                       iptr, ifmt, vb->sys_mem, g.vb_stride);
}

static HRESULT __stdcall dev_CreateTexture(IDirect3DDevice8 *s, UINT w, UINT h, UINT lvls,
        DWORD usage, D3DFORMAT fmt, D3DPOOL pool, IDirect3DTexture8 **pp)
{ NATIVE_GL_SCOPE;
    (void)s; (void)usage; (void)pool;
    if (!pp) return D3DERR_INVALIDCALL;
    GLTexture *t = (GLTexture *)calloc(1, sizeof(*t));
    if(!t)return E_FAIL;
    t->iface.lpVtbl = &g_tex_vtbl;
    t->ref = 1; t->width = w; t->height = h; t->format = fmt;
    t->levels = lvls ? lvls : 1;
    if(t->levels>16 || !w || !h || w>4096 || h>4096 || !d3d8_format_bpp(fmt)) {free(t);return D3DERR_INVALIDCALL;}
    t->usage=usage;t->pitch=d3d8_row_pitch(fmt,w);size_t bytes=0;
    for(UINT l=0;l<t->levels;l++){UINT lw=w>>l,lh=h>>l;if(!lw)lw=1;if(!lh)lh=1;t->offsets[l]=bytes;bytes+=d3d8_row_pitch(fmt,lw)*(d3d8_format_is_compressed(fmt)?(lh+3)/4:lh);}
    t->sys_mem=calloc(1,bytes);if(!t->sys_mem){free(t);return E_FAIL;}
    glGenTextures(1, &t->gl_tex);
    glBindTexture(GL_TEXTURE_2D,t->gl_tex);
    if(usage&D3DUSAGE_RENDERTARGET) {
        for(UINT l=0;l<t->levels;l++) {
            UINT lw=w>>l,lh=h>>l;if(!lw)lw=1;if(!lh)lh=1;
            glTexImage2D(GL_TEXTURE_2D,l,GL_RGBA8,lw,lh,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
        }
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,t->levels-1);
    }
    *pp = &t->iface;
    return D3D_OK;
}

static HRESULT __stdcall dev_CreateVertexBuffer(IDirect3DDevice8 *s, UINT len, DWORD usage,
                                                 DWORD fvf, D3DPOOL pool,
                                                 IDirect3DVertexBuffer8 **pp)
{ NATIVE_GL_SCOPE;
    (void)s; (void)pool;
    if (!pp) return D3DERR_INVALIDCALL;
    GLVertexBuffer *v = (GLVertexBuffer *)calloc(1, sizeof(*v));
    v->iface.lpVtbl = &g_vb_vtbl;
    v->ref = 1; v->size = len; v->fvf = fvf; v->usage = usage;
    v->sys_mem = (BYTE *)calloc(1, len ? len : 1);
    glGenBuffers(1, &v->gl_buf);
    *pp = &v->iface;
    return D3D_OK;
}

static HRESULT __stdcall dev_CreateIndexBuffer(IDirect3DDevice8 *s, UINT len, DWORD usage,
                                                D3DFORMAT fmt, D3DPOOL pool,
                                                IDirect3DIndexBuffer8 **pp)
{ NATIVE_GL_SCOPE;
    (void)s; (void)usage; (void)pool;
    if (!pp) return D3DERR_INVALIDCALL;
    GLIndexBuffer *v = (GLIndexBuffer *)calloc(1, sizeof(*v));
    v->iface.lpVtbl = &g_ib_vtbl;
    v->ref = 1; v->size = len; v->format = fmt;
    v->sys_mem = (BYTE *)calloc(1, len ? len : 1);
    glGenBuffers(1, &v->gl_buf);
    *pp = &v->iface;
    return D3D_OK;
}

static HRESULT __stdcall dev_CreateRenderTarget(IDirect3DDevice8 *s, UINT w, UINT h,
        D3DFORMAT fmt, D3DMULTISAMPLE_TYPE ms, BOOL lockable, IDirect3DSurface8 **pp)
{ NATIVE_GL_SCOPE;
    (void)s; (void)ms; (void)lockable;
    if (!pp || !w || !h || w>4096 || h>4096) return D3DERR_INVALIDCALL;
    GLSurface *t = (GLSurface *)calloc(1, sizeof(*t));
    if(!t)return E_FAIL;
    t->iface.lpVtbl = &g_sf_vtbl;
    t->ref = 1; t->width = w; t->height = h; t->format = fmt;
    glGenTextures(1,&t->color);glBindTexture(GL_TEXTURE_2D,t->color);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    *pp = &t->iface;
    return D3D_OK;
}
static HRESULT __stdcall dev_CreateDepthStencilSurface(IDirect3DDevice8 *s, UINT w, UINT h,
        D3DFORMAT fmt, D3DMULTISAMPLE_TYPE ms, IDirect3DSurface8 **pp)
{ NATIVE_GL_SCOPE;
    (void)s;(void)ms;
    if(!pp || !w || !h || w>4096 || h>4096)return D3DERR_INVALIDCALL;
    GLSurface *t=calloc(1,sizeof *t);if(!t)return E_FAIL;
    t->iface.lpVtbl=&g_sf_vtbl;t->ref=1;t->width=w;t->height=h;t->format=fmt;
    glGenRenderbuffers(1,&t->depth);glBindRenderbuffer(GL_RENDERBUFFER,t->depth);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,w,h);
    *pp=&t->iface;return S_OK;
}

static int native_bind_target(GLSurface *color,GLSurface *depth)
{
    if(!color){glBindFramebuffer(GL_FRAMEBUFFER,native_scene_fbo);return 1;}
    if(!native_framebuffer)glGenFramebuffers(1,&native_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER,native_framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,surface_color(color),color->level);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,depth?depth->depth:0);
    return glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
}
static HRESULT __stdcall dev_SetRenderTarget(IDirect3DDevice8 *s, IDirect3DSurface8 *rt,
                                              IDirect3DSurface8 *zs)
{ NATIVE_GL_SCOPE;
    (void)s;GLSurface *color=(GLSurface*)rt,*depth=(GLSurface*)zs;
    if(color && color->backbuffer)color=NULL;
    UINT w=color?color->width:g.backbuf_w,h=color?color->height:g.backbuf_h;
    if(depth && (depth->width!=w || depth->height!=h))return D3DERR_INVALIDCALL;
    if(color && (!surface_color(color) || (depth && !depth->depth)))return D3DERR_INVALIDCALL;
    if(!native_bind_target(color,depth)) {
        native_bind_target(native_target,native_depth); /* Failed changes preserve the old target. */
        return E_FAIL;
    }
    if(color && color->parent)color->parent->flipped=1;
    if(color)sf_AddRef(&color->iface);if(depth)sf_AddRef(&depth->iface);
    if(native_target)sf_Release(&native_target->iface);if(native_depth)sf_Release(&native_depth->iface);
    native_scene_active=color==NULL;
    native_target=color;native_depth=depth;native_depth_enabled=depth!=NULL;
    native_target_width=w;native_target_height=h;
    g.viewport=(D3DVIEWPORT8){0,0,w,h,0,1};native_viewport();glDepthRange(0,1);
    if(trace_draw && trace_draw<=2000)fprintf(stderr,"[native-draw] target %ux%u texture=%p depth=%p\n",w,h,color?color->parent:NULL,depth);
    return D3D_OK;
}
static HRESULT __stdcall dev_GetRenderTarget(IDirect3DDevice8 *s, IDirect3DSurface8 **pp)
{ NATIVE_GL_SCOPE;
    if(!pp)return D3DERR_INVALIDCALL;
    if(!native_target)return dev_GetBackBuffer(s,0,0,pp);
    *pp=&native_target->iface;sf_AddRef(*pp);return S_OK;
}
static HRESULT __stdcall dev_GetDepthStencilSurface(IDirect3DDevice8 *s, IDirect3DSurface8 **pp)
{ NATIVE_GL_SCOPE;
    (void)s;if(!pp)return D3DERR_INVALIDCALL;
    *pp=native_depth?&native_depth->iface:NULL;if(*pp)sf_AddRef(*pp);return S_OK;
}

static HRESULT __stdcall dev_SetViewport(IDirect3DDevice8 *s, const D3DVIEWPORT8 *vp)
{ NATIVE_GL_SCOPE;
    (void)s; if (!vp) return D3DERR_INVALIDCALL;
    g.viewport = *vp;
    native_viewport();
    glDepthRange(vp->MinZ, vp->MaxZ);
    return D3D_OK;
}
static HRESULT __stdcall dev_GetViewport(IDirect3DDevice8 *s, D3DVIEWPORT8 *vp)
{ NATIVE_GL_SCOPE; (void)s; if (!vp) return D3DERR_INVALIDCALL; *vp = g.viewport; return D3D_OK; }

static HRESULT __stdcall dev_SetMaterial(IDirect3DDevice8 *s, const D3DMATERIAL8 *m)
{ NATIVE_GL_SCOPE; (void)s; if (m) g.material = *m; return D3D_OK; }
static HRESULT __stdcall dev_GetMaterial(IDirect3DDevice8 *s, D3DMATERIAL8 *m)
{ NATIVE_GL_SCOPE; (void)s; if (m) *m = g.material; return D3D_OK; }
static HRESULT __stdcall dev_SetLight(IDirect3DDevice8 *s, DWORD i, const D3DLIGHT8 *l)
{ NATIVE_GL_SCOPE; (void)s; if (i < 8 && l) g.lights[i] = *l; return D3D_OK; }
static HRESULT __stdcall dev_GetLight(IDirect3DDevice8 *s, DWORD i, D3DLIGHT8 *l)
{ NATIVE_GL_SCOPE; (void)s; if (i < 8 && l) *l = g.lights[i]; return D3D_OK; }
static HRESULT __stdcall dev_LightEnable(IDirect3DDevice8 *s, DWORD i, BOOL en)
{ NATIVE_GL_SCOPE; (void)s; if (i < 8) g.light_enabled[i] = en; return D3D_OK; }

static HRESULT __stdcall dev_SetVertexShader(IDirect3DDevice8 *s, DWORD h)
{ NATIVE_GL_SCOPE;
    (void)s; g.current_vsh = h;
    /* Handle is either a real shader ID (high bit set) or an FVF code. */
    if (!d3d8_vsh_is_programmable(h)) g.current_fvf = h;
    return D3D_OK;
}
static HRESULT __stdcall dev_GetVertexShader(IDirect3DDevice8 *s, DWORD *p)
{ NATIVE_GL_SCOPE; (void)s; if (p) *p = g.current_vsh; return D3D_OK; }
static HRESULT __stdcall dev_SetVertexShaderConstant(IDirect3DDevice8 *s, INT reg,
                                                      const void *p, DWORD n)
{ NATIVE_GL_SCOPE; (void)s;(void)reg;(void)p;(void)n; return D3D_OK; }
static HRESULT __stdcall dev_SetPixelShader(IDirect3DDevice8 *s, DWORD h)
{ NATIVE_GL_SCOPE; (void)s; g.current_psh = h; return D3D_OK; }
static HRESULT __stdcall dev_GetPixelShader(IDirect3DDevice8 *s, DWORD *p)
{ NATIVE_GL_SCOPE; (void)s; if (p) *p = g.current_psh; return D3D_OK; }
static HRESULT __stdcall dev_SetPixelShaderConstant(IDirect3DDevice8 *s, INT reg,
                                                     const void *p, DWORD n)
{ NATIVE_GL_SCOPE; (void)s;(void)reg;(void)p;(void)n; return D3D_OK; }

static void __stdcall dev_SetGammaRamp(IDirect3DDevice8 *s, DWORD f, const D3DGAMMARAMP *r)
{ NATIVE_GL_SCOPE; (void)s;(void)f;(void)r; }
static void __stdcall dev_GetGammaRamp(IDirect3DDevice8 *s, D3DGAMMARAMP *r)
{ NATIVE_GL_SCOPE; (void)s;(void)r; }

static HRESULT __stdcall dev_SetPalette(IDirect3DDevice8 *s, DWORD pn, const void *e)
{ NATIVE_GL_SCOPE; (void)s; native_set_palette(pn,e); return D3D_OK; }

static HRESULT __stdcall dev_BeginPush(IDirect3DDevice8 *s, DWORD c, DWORD **pp)
{ NATIVE_GL_SCOPE; (void)s;(void)c; if (pp) *pp = NULL; return D3D_OK; }
static HRESULT __stdcall dev_EndPush(IDirect3DDevice8 *s, DWORD *p)
{ NATIVE_GL_SCOPE; (void)s;(void)p; return D3D_OK; }

static HRESULT __stdcall dev_Swap(IDirect3DDevice8 *s, DWORD flags)
{ NATIVE_GL_SCOPE; return dev_Present(s, NULL, NULL, NULL, NULL); (void)flags; }

static HRESULT __stdcall dev_CreateImageSurface(IDirect3DDevice8 *s,UINT w,UINT h,D3DFORMAT f,IDirect3DSurface8 **p) {
    NATIVE_GL_SCOPE;if(!p)return D3DERR_INVALIDCALL;*p=NULL;
    if(f!=D3DFMT_A8R8G8B8 && f!=D3DFMT_X8R8G8B8 && f!=D3DFMT_LIN_A8R8G8B8 && f!=D3DFMT_LIN_X8R8G8B8)return E_NOTIMPL;
    HRESULT result=dev_CreateRenderTarget(s,w,h,f,0,TRUE,p);
    if(SUCCEEDED(result))((GLSurface*)*p)->image_surface=1;
    return result;
}
static HRESULT __stdcall dev_CreateCubeTexture(IDirect3DDevice8 *s,UINT e,UINT l,DWORD u,D3DFORMAT f,D3DPOOL pool,IDirect3DCubeTexture8 **p) { NATIVE_GL_SCOPE;if(p)*p=NULL;return E_NOTIMPL;}
static HRESULT __stdcall dev_CreateVolumeTexture(IDirect3DDevice8 *s,UINT w,UINT h,UINT d,UINT l,DWORD u,D3DFORMAT f,D3DPOOL pool,IDirect3DVolumeTexture8 **p) { NATIVE_GL_SCOPE;if(p)*p=NULL;return E_NOTIMPL;}

static const IDirect3DDevice8Vtbl g_device_vtbl = {
    dev_QueryInterface, dev_AddRef, dev_Release,
    dev_GetDirect3D, dev_GetDeviceCaps, dev_GetDisplayMode, dev_GetCreationParameters,
    dev_Reset, dev_Present, dev_GetBackBuffer,
    dev_BeginScene, dev_EndScene, dev_Clear,
    dev_SetTransform, dev_GetTransform,
    dev_SetRenderState, dev_GetRenderState,
    dev_SetTextureStageState, dev_GetTextureStageState,
    dev_SetTexture, dev_GetTexture,
    dev_SetStreamSource, dev_GetStreamSource,
    dev_SetIndices, dev_GetIndices,
    dev_DrawPrimitive, dev_DrawIndexedPrimitive,
    dev_DrawPrimitiveUP, dev_DrawIndexedPrimitiveUP,
    dev_CreateTexture, dev_CreateVertexBuffer, dev_CreateIndexBuffer,
    dev_CreateRenderTarget, dev_CreateDepthStencilSurface,
    dev_SetRenderTarget, dev_GetRenderTarget, dev_GetDepthStencilSurface,
    dev_SetViewport, dev_GetViewport,
    dev_SetMaterial, dev_GetMaterial,
    dev_SetLight, dev_GetLight, dev_LightEnable,
    dev_SetVertexShader, dev_GetVertexShader, dev_SetVertexShaderConstant,
    dev_SetPixelShader, dev_GetPixelShader, dev_SetPixelShaderConstant,
    dev_SetGammaRamp, dev_GetGammaRamp,
    dev_SetPalette,
    dev_BeginPush, dev_EndPush,
    dev_Swap,
    dev_CreateImageSurface, dev_CreateCubeTexture, dev_CreateVolumeTexture,
};

/* ======================================================================== */
/* IDirect3D8 (factory)                                                     */
/* ======================================================================== */

static HRESULT __stdcall d3d_QueryInterface(IDirect3D8 *s, const IID *iid, void **pp)
{ NATIVE_GL_SCOPE; (void)s;(void)iid;(void)pp; return D3DERR_INVALIDCALL; }
static ULONG __stdcall d3d_AddRef(IDirect3D8 *s)  { NATIVE_GL_SCOPE; (void)s; return 1; }
static ULONG __stdcall d3d_Release(IDirect3D8 *s) { NATIVE_GL_SCOPE; (void)s; return 0; }

static HRESULT __stdcall d3d_CreateDevice(IDirect3D8 *s, UINT adapter, DWORD devtype,
        HWND hwnd, DWORD flags, D3DPRESENT_PARAMETERS *pPP, IDirect3DDevice8 **pp)
{ NATIVE_GL_SCOPE;
    (void)s; (void)adapter; (void)devtype; (void)hwnd; (void)flags;
    if (!pp || !pPP) return D3DERR_INVALIDCALL;

    /* Cocoa requires window creation on the main thread. */
    if(!SDL_IsMainThread())return D3DERR_INVALIDCALL;
    if (!SDL_WasInit(SDL_INIT_VIDEO) && !SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        fprintf(stderr,"[native-gl] video init failed: %s\n",SDL_GetError());return E_FAIL;
    }

    g.backbuf_w = (int)pPP->BackBufferWidth;
    g.backbuf_h = (int)pPP->BackBufferHeight;
    if (g.backbuf_w <= 0) g.backbuf_w = 640;
    if (g.backbuf_h <= 0) g.backbuf_h = 480;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,  SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,  24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    g.window = SDL_CreateWindow(g_window_title, g.backbuf_w, g.backbuf_h,
                                SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!g.window) {
        fprintf(stderr, "[d3d8_gl] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return D3DERR_INVALIDCALL;
    }
    SDL_RaiseWindow(g.window);
    native_target_width=g.backbuf_w;native_target_height=g.backbuf_h;
    g.glctx = SDL_GL_CreateContext(g.window);
    if (!g.glctx) {
        fprintf(stderr, "[d3d8_gl] SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(g.window);
        g.window = NULL;
        return D3DERR_INVALIDCALL;
    }
    SDL_GL_MakeCurrent(g.window, g.glctx);
    SDL_GL_SetSwapInterval(g_present_interval);

    fprintf(stderr, "[d3d8_gl] GL %s / GLSL %s\n",
            glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));

    if(!init_gl_pipeline())return E_FAIL;
    native_default_depth=calloc(1,sizeof *native_default_depth);
    if(!native_default_depth)return E_FAIL;
    native_default_depth->iface.lpVtbl=&g_sf_vtbl;native_default_depth->ref=2; /* device owner + current binding */
    native_default_depth->width=g.backbuf_w;native_default_depth->height=g.backbuf_h;
    native_default_depth->format=D3DFMT_D24S8;native_depth=native_default_depth;
    for(int i=0;i<16;i++){float v[4]={0,0,0,1};if(i==3)v[0]=v[1]=v[2]=1;d3d8_vsh_set_vertex_data(i,v);}

    /* Reasonable default states */
    mat4_identity(&g.m_world);
    mat4_identity(&g.m_view);
    mat4_identity(&g.m_proj);
    g.rs[D3DRS_ZENABLE]            = 1;
    g.rs[D3DRS_ZWRITEENABLE]       = 1;
    g.rs[D3DRS_ZFUNC]              = D3DCMP_LESSEQUAL;
    g.rs[D3DRS_ALPHABLENDENABLE]   = 0;
    g.rs[D3DRS_SRCBLEND]           = D3DBLEND_SRCALPHA;
    g.rs[D3DRS_DESTBLEND]          = D3DBLEND_INVSRCALPHA;
    g.rs[D3DRS_CULLMODE]           = D3DCULL_CCW;
    g.rs[D3DRS_COLORWRITEENABLE]   = 0x0F;

    g.viewport=(D3DVIEWPORT8){0,0,g.backbuf_w,g.backbuf_h,0,1};
    PcDisplayMode mode=pc_display_current();
    if(!native_scene_resize(mode.width,mode.height))return E_FAIL;
    native_viewport();
    tsfp_native_settings_attach(g.window);

    g_device.lpVtbl = &g_device_vtbl;
    *pp = &g_device;
    return D3D_OK;
}

static const IDirect3D8Vtbl g_d3d8_vtbl = {
    d3d_QueryInterface, d3d_AddRef, d3d_Release,
    d3d_CreateDevice,
};

static IDirect3D8 g_d3d8 = { &g_d3d8_vtbl };

/* ======================================================================== */
/* Public API                                                               */
/* ======================================================================== */

IDirect3D8 *xbox_Direct3DCreate8(UINT SDKVersion)
{
    (void)SDKVersion;
    return &g_d3d8;
}

IDirect3DDevice8 *xbox_GetD3DDevice(void)
{
    return &g_device;
}

/* The guest's presentation size (see d3d8_device.c); this backend has no
 * separate host size yet, so the getters answer the same. */
static UINT g_guest_width, g_guest_height;
void xbox_D3D8SetGuestSize(UINT width, UINT height)
{
    g_guest_width = width;
    g_guest_height = height;
}
UINT d3d8_GetBackBufferWidth(void)  { return g_guest_width; }
UINT d3d8_GetBackBufferHeight(void) { return g_guest_height; }
UINT d3d8_GetGuestWidth(void)  { return g_guest_width; }
UINT d3d8_GetGuestHeight(void) { return g_guest_height; }

/* Match the Windows path: the first inclusive rectangle is supported. */
static UINT g_scissor_count;
static BOOL g_scissor_exclusive;
static D3DRECT g_scissor_rect;
static void native_apply_scissor(void) {
    if(!g_scissor_count || g_scissor_exclusive){glDisable(GL_SCISSOR_TEST);return;}
    double sx=native_scene_active?(double)native_scene_w/g.backbuf_w:1;
    double sy=native_scene_active?(double)native_scene_h/g.backbuf_h:1;
    double left=g_scissor_rect.x1*sx,right=g_scissor_rect.x2*sx;
    if(native_scene_active){double anchor=native_scene_w*native_hud_anchor;
        left=anchor+(left-anchor)*native_hud_k;right=anchor+(right-anchor)*native_hud_k;}
    int top=(int)(g_scissor_rect.y1*sy),bottom=(int)(g_scissor_rect.y2*sy);
    int x=(int)left,r=(int)right,height=(int)(native_target_height*sy);
    glEnable(GL_SCISSOR_TEST);glScissor(x,height-bottom,r>x?r-x:0,bottom>top?bottom-top:0);
}
void xbox_D3D8SetScissors(UINT count,BOOL exclusive,const D3DRECT *rects) {
    NATIVE_GL_SCOPE;
    g_scissor_count=rects?count:0;g_scissor_exclusive=exclusive;
    memset(&g_scissor_rect,0,sizeof g_scissor_rect);if(g_scissor_count)g_scissor_rect=rects[0];
    static int said;
    if((count>1 || (count&&exclusive)) && !said++)
        fprintf(stderr,"[native-gl] scissor supports the first inclusive rectangle only\n");
}
#include "gl_screen_copy.inc"

/* No Hor+ on this backend, so nothing for a title to widen. */
float xbox_D3D8ClaimHorPlus(void) { return 1.0f; }
/* SDL owns this backend's window and its fullscreen; nothing to follow. */
void xbox_D3D8SetFullscreen(BOOL on) { if(g.window && SDL_IsMainThread())SDL_SetWindowFullscreen(g.window,on); }
/* Nor widescreen by screen. */
void xbox_D3D8SetWideFrames(BOOL wide) { (void)wide; }
#include "gl_interpolation.inc"

/* The shared HLE records these tags during interpolation too. */
void xbox_D3D8SetTwoDPlacement(int placement, uint32_t tag)
{
    native_placement=placement;native_placement_tag=tag;
}

int xbox_D3D8GetTwoDPlacement(uint32_t *tag)
{
    if(tag)*tag=native_placement_tag;
    return native_placement;
}

BOOL xbox_D3D8GetScissors(UINT *count, BOOL *exclusive, D3DRECT *rect)
{
    if (count)     *count = g_scissor_count;
    if (exclusive) *exclusive = g_scissor_exclusive;
    if (rect)      *rect = g_scissor_rect;
    return g_scissor_count != 0;
}

void xbox_D3D8SetPresentInterval(UINT interval)
{
    g_present_interval = interval;
    if (g.glctx)
        {NATIVE_GL_SCOPE;SDL_GL_SetSwapInterval((int)interval);}
}

void d3d8_PresentFrame(void)
{
    dev_Present(&g_device, NULL, NULL, NULL, NULL);
}

/* Alias used by recomp_manual.c via d3d8_internal.h on both backends. */
IDirect3DDevice8 *d3d8_GetDevice(void)
{
    return &g_device;
}


#ifdef __APPLE__
#include "gl_bridge.inc"
#endif

#include "renderer_selftest.inc"
