/* Keep the completed guest frame intact while the shared HLE replays an
 * intermediate frame. The default framebuffer remains the draw destination,
 * so offscreen target changes and readback use the same paths as guest draws. */
static GLuint native_saved_fbo,native_saved_color,native_saved_depth;
static unsigned native_saved_w,native_saved_h;
static GLint native_interp_read,native_interp_draw;
static unsigned native_interpolated_count;
static int native_interp_projection=60,native_interp_affine=64,native_interp_count=12;

static void native_interp_copy(GLuint from,GLuint to) {
    GLboolean scissor=glIsEnabled(GL_SCISSOR_TEST);
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,from);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,to);
    glBlitFramebuffer(0,0,native_scene_w,native_scene_h,0,0,native_scene_w,native_scene_h,
                      GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT,GL_NEAREST);
    if(scissor)glEnable(GL_SCISSOR_TEST);
}
BOOL xbox_D3D8InterpBegin(void) {
    NATIVE_GL_SCOPE;
    if(native_interp_on || !g.glctx)return FALSE;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&native_interp_read);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&native_interp_draw);
    if(!native_saved_fbo) {
        glGenFramebuffers(1,&native_saved_fbo);
        glGenRenderbuffers(1,&native_saved_color);
        glGenRenderbuffers(1,&native_saved_depth);
    }
    glBindFramebuffer(GL_FRAMEBUFFER,native_saved_fbo);
    if(native_saved_w!=native_scene_w || native_saved_h!=native_scene_h) {
        glBindRenderbuffer(GL_RENDERBUFFER,native_saved_color);
        glRenderbufferStorage(GL_RENDERBUFFER,GL_RGBA8,native_scene_w,native_scene_h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,native_saved_color);
        glBindRenderbuffer(GL_RENDERBUFFER,native_saved_depth);
        glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,native_scene_w,native_scene_h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,native_saved_depth);
        native_saved_w=native_scene_w;native_saved_h=native_scene_h;
    }
    int complete=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    if(complete) {
        native_interp_copy(native_scene_fbo,native_saved_fbo);
        /* A replay starts a new depth history. Retaining N's opaque depth
         * incorrectly rejects translucent draws at the same surface. */
        GLboolean scissor=glIsEnabled(GL_SCISSOR_TEST),depth_write;
        GLint stencil_write;GLdouble clear_depth;GLint clear_stencil;
        glGetBooleanv(GL_DEPTH_WRITEMASK,&depth_write);
        glGetIntegerv(GL_STENCIL_WRITEMASK,&stencil_write);
        glGetDoublev(GL_DEPTH_CLEAR_VALUE,&clear_depth);
        glGetIntegerv(GL_STENCIL_CLEAR_VALUE,&clear_stencil);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,native_scene_fbo);glDisable(GL_SCISSOR_TEST);
        glDepthMask(GL_TRUE);glStencilMask(~0u);glClearDepth(1);glClearStencil(0);
        glClear(GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
        glDepthMask(depth_write);glStencilMask(stencil_write);
        glClearDepth(clear_depth);glClearStencil(clear_stencil);
        if(scissor)glEnable(GL_SCISSOR_TEST);
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER,native_interp_read);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,native_interp_draw);
    native_interp_on=complete;return complete;
}
BOOL xbox_D3D8InterpPresent(void) {
    NATIVE_GL_SCOPE;
    if(!native_interp_on)return FALSE;
    native_scene_present();
    Uint64 start=SDL_GetTicksNS();
    if(native_swap_window()) {
        native_present_count++;native_interpolated_count++;
        native_frame_timing(start,SDL_GetTicksNS());
        return TRUE;
    }
    tsfp_present_record(&native_metrics,"gl",RHI_PRESENT_INTERPOLATED,-1,start,SDL_GetTicksNS());
    fprintf(stderr,"[native-gl] interpolated present failed: %s\n",SDL_GetError());
    return FALSE;
}
void xbox_D3D8InterpEnd(void) {
    NATIVE_GL_SCOPE;
    if(!native_interp_on)return;
    native_interp_copy(native_saved_fbo,native_scene_fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,native_interp_read);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,native_interp_draw);
    native_interp_on=0;
}
void xbox_D3D8SetInterpRegisters(int projection,int first,int count) {
    native_interp_projection=projection;native_interp_affine=first;native_interp_count=count;
}
