/* Apple GL host boundary. Included by renderer_gl.c, whose objects it describes.
 * Programs compile to GLSL; the portable interpreter is the fallback/reference. */
static NV2AVshSlot native_slots[NV2A_VS_MAX_SLOTS];
static NV2AVshProgram native_programs[NV2A_VS_MAX_SLOTS];
static float native_constants[NV2A_VS_MAX_CONSTANTS][4];
static float native_inputs[16][4];
static float native_scale[4], native_offset[4];
static int native_screen;
static DWORD native_palettes[4][256], native_pixel_shader;
const DWORD *d3d8_GetRenderStates(void) { return g.rs; }
const DWORD *d3d8_GetTSS(DWORD s) { return g.tss[s < MAX_TSU ? s : 0]; }
IDirect3DBaseTexture8 *d3d8_GetStageTexture(DWORD s) { return s < MAX_TSU ? g.textures[s] : NULL; }
const D3DMATRIX *d3d8_GetTransform(D3DTRANSFORMSTATETYPE t) { return t == D3DTS_WORLD ? &g.m_world : t == D3DTS_VIEW ? &g.m_view : &g.m_proj; }
const D3DLIGHT8 *d3d8_GetLight(DWORD n) { return n < 8 ? &g.lights[n] : NULL; }
BOOL d3d8_GetLightEnable(DWORD n) { return n < 8 && g.light_enabled[n]; }
const D3DMATERIAL8 *d3d8_GetMaterial(void) { return &g.material; }
UINT d3d8_GetNumLights(void) { return 8; }
const DWORD *d3d8_GetPalette(DWORD s) { return native_palettes[s & 3]; }
static void native_set_palette(DWORD s, const void *p) { if (p && s < 4) memcpy(native_palettes[s], p, 1024); }
void d3d8_base_set_palette(IDirect3DBaseTexture8 *p, UINT s) { if (p) ((GLTexture *)p)->palette = s & 3; }
UINT d3d8_up_vertices_read(D3DPRIMITIVETYPE t, UINT n) { UINT v=0; gl_prim(t,n,&v); return v; }
UINT d3d8_up_indices_read(D3DPRIMITIVETYPE t, UINT n) { return d3d8_up_vertices_read(t,n); }
BOOL d3d8_texture_info(IDirect3DBaseTexture8 *p, D3D8TextureInfo *i) {
    if (!p || (const void *)p->lpVtbl != &g_tex_vtbl) return FALSE;
    GLTexture *t=(GLTexture *)p; *i=(D3D8TextureInfo){t->format,t->width,t->height,t->levels,t->usage}; return TRUE;
}
BOOL d3d8_texture_level(IDirect3DBaseTexture8 *p, UINT l, const BYTE **b, UINT *pitch, UINT *rows) {
    D3D8TextureInfo i; if (!d3d8_texture_info(p,&i) || l >= i.levels) return FALSE;
    GLTexture *t=(GLTexture *)p; UINT w=t->width>>l,h=t->height>>l; if(!w)w=1;if(!h)h=1;
    *b=t->sys_mem+t->offsets[l]; *pitch=d3d8_row_pitch(t->format,w); *rows=d3d8_format_is_compressed(t->format)?(h+3)/4:h;return TRUE;
}
/* Cube/volume resources and movie planes are explicitly unsupported in this
 * first 2D bridge. Their factory calls fail, so no fake resource is returned. */
BOOL d3d8_cube_info(IDirect3DBaseTexture8 *p,D3D8CubeInfo *i) { return FALSE; }
BOOL d3d8_cube_level(IDirect3DBaseTexture8 *p,UINT f,UINT l,const BYTE **b,UINT *pitch,UINT *rows) { return FALSE; }
void d3d8_movie_clear(void) {}
void d3d8_movie_draw(void) {}
void hle_d3d8_overlay_redraw(void) {}
void d3d8_combiners_set_pixel_shader(DWORD t) { native_pixel_shader=t; }
DWORD d3d8_combiners_get_pixel_shader(void) { return native_pixel_shader; }
HRESULT d3d8_vsh_create_shader(const DWORD *code,int n,DWORD *h) {
    if (!code || !h || n<1 || n>NV2A_VS_MAX_INSTRUCTIONS) return E_INVALIDARG;
    for(int i=0;i<NV2A_VS_MAX_SLOTS;i++) if(!native_slots[i].in_use) {
        NV2AVshSlot *s=&native_slots[i];s->in_use=1;s->length=n;memcpy(s->microcode,code,n*16);
        nv2a_vsh_parse(code,n,&native_programs[i]);*h=0x10000+i;return S_OK;
    }
    return E_FAIL;
}
BOOL d3d8_vsh_is_programmable(DWORD h) { return h>=0x10000 && h<0x10000+NV2A_VS_MAX_SLOTS && native_slots[h-0x10000].in_use; }
HRESULT d3d8_vsh_delete_shader(DWORD h) { if(!d3d8_vsh_is_programmable(h))return E_INVALIDARG;native_gpu_delete(h-0x10000);memset(&native_slots[h-0x10000],0,sizeof native_slots[0]);return S_OK; }
HRESULT d3d8_vsh_set_declaration(DWORD h,const D3D8VshInput *d,int n) {
    if(!d3d8_vsh_is_programmable(h)||n<0||n>16||(n&&!d))return E_INVALIDARG;
    NV2AVshSlot *s=&native_slots[h-0x10000];s->decl_count=n;if(n)memcpy(s->decl,d,n*sizeof *d);return S_OK;
}
void d3d8_vsh_set_constant(int r,const float *p,int n) { if(r>=0 && n>=0 && r+n<=NV2A_VS_MAX_CONSTANTS && p)memcpy(native_constants[r],p,n*16); }
const float *d3d8_vsh_constants(void) { return &native_constants[0][0]; }
void d3d8_vsh_set_vertex_data(int r,const float p[4]) { if(r>=0&&r<16&&p)memcpy(native_inputs[r],p,16); }
void d3d8_vsh_get_vertex_data(int r,float p[4]) { if(r>=0&&r<16&&p)memcpy(p,native_inputs[r],16); }
void d3d8_vsh_set_screenspace(const float s[4],const float o[4]) { memcpy(native_scale,s,16);memcpy(native_offset,o,16);native_screen=1; }
void d3d8_vsh_get_screenspace(float s[4],float o[4],int *e) { if(s)memcpy(s,native_scale,16);if(o)memcpy(o,native_offset,16);if(e)*e=native_screen; }
BOOL d3d8_vsh_get_slot(int i,DWORD *h,const DWORD **m,int *n,const D3D8VshInput **d,int *c) {
    if(i<0||i>=NV2A_VS_MAX_SLOTS||!native_slots[i].in_use)return FALSE;
    NV2AVshSlot *s=&native_slots[i];if(h)*h=0x10000+i;if(m)*m=s->microcode;if(n)*n=s->length;if(d)*d=s->decl;if(c)*c=s->decl_count;return TRUE;
}
/* Only constants actually consumed by the bound program identify its matrices. */
void d3d8_vsh_interp_registers(int *p,int *a,int *n) {
    *p=native_interp_projection;*a=native_interp_affine;*n=native_interp_count;
    if(*p<0||*p+4>192)*p=-1;
    if(*a<0||*n<4||*a+*n>192){*a=-1;*n=0;}
}
uint32_t d3d8_vsh_bound_reads(int first,int count) {
    if(!d3d8_vsh_is_programmable(g.current_vsh))return 0;
    const NV2AVshProgram *p=&native_programs[g.current_vsh-0x10000];
    static const unsigned reads[]={0,1,3,5,7,3,3,3,3,3,3,3,3,1};
    uint32_t mask=0;
    for(int i=0;i<p->length;i++) {
        const NV2AVshInstruction *in=&p->insns[i];
        unsigned used=in->mac_op<14?reads[in->mac_op]:0;
        for(int j=0;j<4;j++) {
            const NV2AVshSrcOperand *s=j==3?&in->ilu_src:&in->mac_src[j];
            if(j==3?in->ilu_op==0:!(used&(1u<<j)))continue;
            if(s->reg_type!=NV2A_VSH_REG_CONST)continue;
            for(int group=0;group*4<count&&group<32;group++)
                if(s->rel_addr||(s->reg_index>=first+group*4&&s->reg_index<first+group*4+4))mask|=1u<<group;
        }
    }
    return mask;
}
int d3d8_vsh_bound_uses_projection(void) {
    return native_interp_projection>=0 && d3d8_vsh_bound_reads(native_interp_projection,4)!=0;
}

static unsigned native_read_input(const BYTE *p,UINT available,RhiFormat f,float v[4]) {
    unsigned n=f==RHI_FORMAT_R32G32B32A32_FLOAT?4:f==RHI_FORMAT_R32G32B32_FLOAT?3:f==RHI_FORMAT_R32G32_FLOAT?2:f==RHI_FORMAT_R32_FLOAT?1:0;
    if(n) { if(available<n*4)return 0;memcpy(v,p,n*4);return 1; }
    if(f==RHI_FORMAT_R8G8B8A8_UNORM || f==RHI_FORMAT_B8G8R8A8_UNORM) {
        if(available<4)return 0;for(int j=0;j<4;j++)v[j]=p[j]/255.f;
        if(f==RHI_FORMAT_B8G8R8A8_UNORM){float x=v[0];v[0]=v[2];v[2]=x;}return 1;
    }
    return 0;
}
/* Use the shared output-mask decoder, including NV2A's unusual scalar fog
 * mask, and the shared unwritten marker used by the Windows combiner. */
static int native_program_writes_fog(const NV2AVshProgram *p) {
    for(int i=0;i<p->length;i++) {
        const NV2AVshInstruction *in=&p->insns[i];
        if(in->mac_op>NV2A_VSH_MAC_NOP && in->mac_op<NV2A_VSH_MAC_ARL &&
           in->mac_dst.output_reg==NV2A_VSH_OUT_FOG && nv2a_vsh_output_write_mask(&in->mac_dst))return 1;
        if(in->ilu_op>NV2A_VSH_ILU_NOP && in->ilu_op<=NV2A_VSH_ILU_LIT &&
           in->ilu_dst.output_reg==NV2A_VSH_OUT_FOG && nv2a_vsh_output_write_mask(&in->ilu_dst))return 1;
        if(in->is_final)break;
    }
    return 0;
}
static float *native_shader_vertices(const void *data,UINT count,UINT stride) {
    if(!d3d8_vsh_is_programmable(g.current_vsh)||!count||count>1000000)return NULL;
    NV2AVshSlot *s=&native_slots[g.current_vsh-0x10000];
    float *out=malloc((size_t)count*25*sizeof(float));if(!out)return NULL;
    int writes_fog=native_program_writes_fog(&native_programs[g.current_vsh-0x10000]);
    /* Shared shader literal is parenthesized; parse its numeric payload once. */
    float unwritten_fog=strtof(D3D8_VSH_FOG_UNWRITTEN+1,NULL);
    for(UINT i=0;i<count;i++) {
        float inputs[16][4];memcpy(inputs,native_inputs,sizeof inputs);
        for(int j=0;j<s->decl_count;j++) {
            D3D8VshInput *d=&s->decl[j];
            if(d->reg<0||d->reg>=16||d->offset>=stride||!native_read_input((const BYTE*)data+(size_t)i*stride+d->offset,stride-d->offset,d->format,inputs[d->reg])) {
                static int said;if(!said++){fprintf(stderr,"[native-gl] unsupported vertex declaration format %u; draw skipped\n",d->format);}free(out);return NULL;
            }
        }
        NV2AVshState st;nv2a_vsh_execute(&native_programs[g.current_vsh-0x10000],inputs,native_constants,NV2A_VS_MAX_CONSTANTS,&st);
        float *v=out+i*25;memcpy(v,st.out[NV2A_VSH_OUT_POS],16);
        if(native_screen) {
            for(int k=0;k<4;k++)v[k]=(v[k]-native_offset[k])/native_scale[k];
            if(v[3]==0)v[3]=1;for(int k=0;k<3;k++)v[k]*=v[3];
        }
        v[2]=2*v[2]-v[3];
        memcpy(v+4,st.out[NV2A_VSH_OUT_D0],16);memcpy(v+8,st.out[NV2A_VSH_OUT_T0],64);
        v[24]=writes_fog?st.out[NV2A_VSH_OUT_FOG][0]:unwritten_fog;
    }
    return out;
}
static void native_shader_attribs(void) {
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,100,0);
    glEnableVertexAttribArray(1);glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,100,(void*)16);
    for(int i=0;i<4;i++){glEnableVertexAttribArray(2+i);glVertexAttribPointer(2+i,4,GL_FLOAT,GL_FALSE,100,(void*)(uintptr_t)(32+i*16));}
    glEnableVertexAttribArray(6);glVertexAttribPointer(6,1,GL_FLOAT,GL_FALSE,100,(void*)96);
    glUniform1i(g.u_use_xform,2);
}

#include "gl_vertex.inc"

#include "pc_screen_space.h"
#include "pc_ui_layout.h"
static void native_place_draw(const void *vertices,UINT stride,UINT count) {
    int pos=-1;unsigned offset=0;
    if(d3d8_vsh_is_programmable(g.current_vsh)) {
        NV2AVshProgram *p=&native_programs[g.current_vsh-0x10000];
        for(int i=0;i<p->length;i++)pos=pc_screen_position_write(&p->insns[i],pos);
        NV2AVshSlot *slot=&native_slots[g.current_vsh-0x10000];
        int found=0;
        for(int i=0;i<slot->decl_count;i++)if(slot->decl[i].reg==pos &&
            (slot->decl[i].format==RHI_FORMAT_R32G32B32A32_FLOAT || slot->decl[i].format==RHI_FORMAT_R32G32B32_FLOAT || slot->decl[i].format==RHI_FORMAT_R32G32_FLOAT)){
            offset=slot->decl[i].offset;found=1;break;
        }
        if(!found)pos=-1;
    } else if(d3d8_fvf_transformed(g.current_fvf))pos=0;
    float k=1,ky=1,anchor=.5f;
    if(native_scene_active && pos>=0 && pc_display_wide_now() && offset+8<=stride && count) {
        float left=INFINITY,right=-INFINITY;
        for(UINT i=0;i<count;i++){
            float x;memcpy(&x,(const BYTE *)vertices+(size_t)i*stride+offset,4);
            left=fminf(left,x);right=fmaxf(right,x);
        }
        k=pc_display_hud_scale();ky=pc_ui_scale_y(k);k=pc_ui_scale_x(k);
        int copied=0;for(int stage=0;stage<4;stage++)if(g.textures[stage] &&
            ((GLTexture*)g.textures[stage])->screen_copy)copied=1;
        if(copied || native_placement==XBOX_D3D8_2D_STRETCH ||
           (native_placement==XBOX_D3D8_2D_AUTO && left<=1 && right>=g.backbuf_w-1)){k=1;ky=1;}
        if(native_placement==XBOX_D3D8_2D_LEFT)anchor=0;
        if(native_placement==XBOX_D3D8_2D_RIGHT)anchor=1;
        if(native_placement==XBOX_D3D8_2D_SIDE)anchor=pc_ui_side_anchor(left,right,(float)g.backbuf_w);
    }
    if(k!=native_hud_k || ky!=native_hud_ky || anchor!=native_hud_anchor){native_hud_ky=ky;native_hud_k=k;native_hud_anchor=anchor;native_viewport();}
}

void d3d8_combiners_set_float_constant(unsigned bank,unsigned stage,const float *value)
{pc_combiner_constant_set(&native_float_constants,bank,stage,value);}
int d3d8_combiners_get_float_constant(unsigned bank,unsigned stage,float *value)
{return pc_combiner_constant_get(&native_float_constants,bank,stage,value);}

/* Legacy GL cannot allocate the half-float feedback targets yet. */
HRESULT d3d8_PreserveFeedbackTexture(IDirect3DTexture8 *dst, IDirect3DTexture8 *src)
{ (void)dst;(void)src;return E_NOTIMPL; }
