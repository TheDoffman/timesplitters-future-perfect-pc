/* Copy the current host scene, not the window's previous presentation. Keep
 * guest rectangle coordinates even when the scene uses a larger target. */
static unsigned long native_screen_copies;
static HRESULT native_copy_screen(IDirect3DTexture8 *dst, const RECT *src, const POINT *at)
{
    NATIVE_GL_SCOPE;
    if(!dst || dst->lpVtbl!=&g_tex_vtbl || !native_scene_fbo)return D3DERR_INVALIDCALL;
    GLTexture *t=(GLTexture*)dst;
    if(t->format!=D3DFMT_A8R8G8B8 && t->format!=D3DFMT_X8R8G8B8 &&
       t->format!=D3DFMT_LIN_A8R8G8B8 && t->format!=D3DFMT_LIN_X8R8G8B8)
        return E_NOTIMPL;
    int64_t left=0,top=0,right=g.backbuf_w,bottom=g.backbuf_h;
    int64_t x=0,y=0,w=t->width,h=t->height;
    if(src) {
        left=src->left;top=src->top;right=src->right;bottom=src->bottom;
        x=at?at->x:0;y=at?at->y:0;
        /* Intersect both surfaces, advancing the paired source/destination. */
        if(x<0){left-=x;x=0;}if(y<0){top-=y;y=0;}
        if(left<0){x-=left;left=0;}if(top<0){y-=top;top=0;}
        if(right>g.backbuf_w)right=g.backbuf_w;if(bottom>g.backbuf_h)bottom=g.backbuf_h;
        w=right-left;h=bottom-top;
        if(x>=(int)t->width || y>=(int)t->height)return S_OK;
        if(w>(int)t->width-x)w=t->width-x;if(h>(int)t->height-y)h=t->height-y;
        right=left+w;bottom=top+h;
        if(w<=0 || h<=0)return S_OK;
    }
    GLint read,draw,binding,width=0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);
    glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding);
    GLboolean scissor=glIsEnabled(GL_SCISSOR_TEST);
    glBindTexture(GL_TEXTURE_2D,t->gl_tex);
    glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&width);
    if(!width)glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,t->width,t->height,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    GLuint fbo;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,fbo);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,t->gl_tex,0);
    HRESULT result=E_FAIL;
    if(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER,native_scene_fbo);glDisable(GL_SCISSOR_TEST);
        int flip=src?t->flipped:1;
        int dy0=flip?(int)t->height-y-h:y+h,dy1=flip?(int)t->height-y:y;
        double sx=(double)native_scene_w/g.backbuf_w,sy=(double)native_scene_h/g.backbuf_h;
        glBlitFramebuffer((int)(left*sx),(int)((g.backbuf_h-bottom)*sy),
                          (int)(right*sx),(int)((g.backbuf_h-top)*sy),
                          x,dy0,x+w,dy1,GL_COLOR_BUFFER_BIT,GL_NEAREST);
        t->flipped=flip;t->screen_copy=1;native_screen_copies++;result=S_OK;
        /* A screen image must never sample a replacement for old CPU texels. */
        native_pack_release(t);t->pack_checked=1;
    }
    if(scissor)glEnable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,read);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,draw);
    glBindTexture(GL_TEXTURE_2D,binding);glDeleteFramebuffers(1,&fbo);
    return result;
}
HRESULT xbox_D3D8CopyBackBufferToTexture(IDirect3DTexture8 *dst) {return native_copy_screen(dst,NULL,NULL);}
HRESULT xbox_D3D8CopyBackBufferRectToTexture(IDirect3DTexture8 *dst,const RECT *src,const POINT *at) {return native_copy_screen(dst,src,at);}
unsigned long xbox_D3D8ScreenCopyCount(void) {return native_screen_copies;}
void xbox_D3D8ScreenCopyShutdown(void) {native_screen_copies=0;}
