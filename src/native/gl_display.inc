/* Keep guest coordinates/resources at their original dimensions. Render the
 * screen into a separate high-resolution target, then fit its display shape
 * into the drawable. Offscreen guest targets retain their own coordinates. */
static GLuint native_scene_fbo,native_scene_color,native_scene_depth;
static unsigned native_scene_w,native_scene_h;
static int native_scene_active=1;
static float native_hud_k=1,native_hud_ky=1,native_hud_anchor=.5f;
static int native_placement;
static uint32_t native_placement_tag;
static PcUiWorldAnchor native_world_anchor;
static void native_place_draw(const void *,UINT,UINT);
static void native_viewport(void) {
    double sx=native_scene_active?(double)native_scene_w/g.backbuf_w:1;
    double sy=native_scene_active?(double)native_scene_h/g.backbuf_h:1;
    D3DVIEWPORT8 *v=&g.viewport;
    double x=v->X*sx,width=v->Width*sx;
    if(native_scene_active){double anchor=native_scene_w*native_hud_anchor;x=anchor+(x-anchor)*native_hud_k;width*=native_hud_k;}
    double y=v->Y*sy,height=v->Height*sy;
    if(native_scene_active){double cy=native_scene_h*.5;y=cy+(y-cy)*native_hud_ky;height*=native_hud_ky;}
    glViewport((GLint)x,(GLint)(native_target_height*sy-y-height),(GLsizei)width,(GLsizei)height);
}
static int native_scene_resize(unsigned w,unsigned h) {
    if(!w)w=g.backbuf_w;if(!h)h=g.backbuf_h;
    if(w==native_scene_w&&h==native_scene_h)return 1;
    GLint limit,read,draw;glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE,&limit);
    if(w>(unsigned)limit||h>(unsigned)limit)return 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);
    GLuint fbo,color,depth;glGenFramebuffers(1,&fbo);glGenRenderbuffers(1,&color);glGenRenderbuffers(1,&depth);
    glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glBindRenderbuffer(GL_RENDERBUFFER,color);glRenderbufferStorage(GL_RENDERBUFFER,GL_RGBA8,w,h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,color);
    glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,w,h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,depth);
    int ok=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    if(!ok){glDeleteFramebuffers(1,&fbo);glDeleteRenderbuffers(1,&color);glDeleteRenderbuffers(1,&depth);}
    else {
        GLuint old=native_scene_fbo;
        if(!old || (GLuint)read==old)read=fbo;
        if(!old || (GLuint)draw==old)draw=fbo;
        if(old)glDeleteFramebuffers(1,&old);
        if(native_scene_color)glDeleteRenderbuffers(1,&native_scene_color);
        if(native_scene_depth)glDeleteRenderbuffers(1,&native_scene_depth);
        native_scene_fbo=fbo;native_scene_color=color;native_scene_depth=depth;
        native_scene_w=w;native_scene_h=h;
        fprintf(stderr,"[native-gl] scene resolution %ux%u; guest %dx%d\n",w,h,g.backbuf_w,g.backbuf_h);
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER,read);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,draw);
    if(ok)native_viewport();return ok;
}
HWND xbox_D3D8Window(void) {return (HWND)g.window;}
int xbox_D3D8ChangeResolution(unsigned w,unsigned h) {
    NATIVE_GL_SCOPE;
    if(!g.glctx||!native_scene_active||native_interp_on)return 0;
    return native_scene_resize(w,h);
}
static void native_scene_present(void) {
    GLint read,draw;glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);
    GLboolean scissor=glIsEnabled(GL_SCISSOR_TEST),mask[4];GLfloat clear[4];
    glGetBooleanv(GL_COLOR_WRITEMASK,mask);glGetFloatv(GL_COLOR_CLEAR_VALUE,clear);
    uint32_t drawable_w,drawable_h;
    if(!rhi_drawable_size(g.window,&drawable_w,&drawable_h))return;
    int w=(int)drawable_w,h=(int)drawable_h;
    unsigned shape_w,shape_h;pc_display_shape(&shape_w,&shape_h);
    int width=w,height=(int)((double)w*shape_h/shape_w);
    if(height>h){height=h;width=(int)((double)h*shape_w/shape_h);}
    glDisable(GL_SCISSOR_TEST);glColorMask(1,1,1,1);glClearColor(0,0,0,1);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,native_scene_fbo);
    glBlitFramebuffer(0,0,native_scene_w,native_scene_h,(w-width)/2,(h-height)/2,
                      (w+width)/2,(h+height)/2,GL_COLOR_BUFFER_BIT,GL_LINEAR);
    native_debug_draw(0,w,h);
    glColorMask(mask[0],mask[1],mask[2],mask[3]);glClearColor(clear[0],clear[1],clear[2],clear[3]);
    if(scissor)glEnable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,read);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,draw);
}
/* Readback stays in guest dimensions for the HLE capture/screen-copy contract. */
static GLuint native_scene_readback(unsigned w,unsigned h,GLuint *color) {
    GLuint fbo;glGenFramebuffers(1,&fbo);glGenRenderbuffers(1,color);
    glBindRenderbuffer(GL_RENDERBUFFER,*color);glRenderbufferStorage(GL_RENDERBUFFER,GL_RGBA8,w,h);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,fbo);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,*color);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,native_scene_fbo);
    GLboolean scissor=glIsEnabled(GL_SCISSOR_TEST);glDisable(GL_SCISSOR_TEST);
    glBlitFramebuffer(0,0,native_scene_w,native_scene_h,0,0,w,h,GL_COLOR_BUFFER_BIT,GL_LINEAR);
    if(scissor)glEnable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);glReadBuffer(GL_COLOR_ATTACHMENT0);return fbo;
}

/* Cocoa flushBuffer must target the window drawable, not the scene FBO. */
static bool native_swap_window(void) {
    GLint draw;glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);
    bool ok=SDL_GL_SwapWindow(g.window);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,draw);
    return ok;
}
