/* Visibility commands share the exact device-call queue as their geometry.
 * Do not flush early: guest textures may intentionally update after a draw. */
#if defined(TSFP_PC_CONTROLS) || defined(TSFP_NATIVE_DISPLAY)
static void queued_visibility_begin(const void *unused) {
    (void)unused;static int warned;
    if(!xbox_D3D8VisibilityBegin() && !warned) {
        warned=1;
        fprintf(stderr,"[visibility] host query unavailable; original result remains pending\n");
    }
}
void host_VisibilityBegin(void) {
    if(hle_d3d8_defer_recording())hle_d3d8_defer_op(queued_visibility_begin,NULL,0);
    else queued_visibility_begin(NULL);
}
typedef struct VisibilityEndPayload {uint32_t index;int accepted;} VisibilityEndPayload;
static void queued_visibility_end(const void *arg) {
    const VisibilityEndPayload *p=arg;xbox_D3D8VisibilityEnd(p->index,p->accepted);
}
void host_VisibilityEnd(uint32_t index,int accepted) {
    VisibilityEndPayload p={index,accepted};
    if(hle_d3d8_defer_recording())hle_d3d8_defer_op(queued_visibility_end,&p,sizeof(p));
    else queued_visibility_end(&p);
}
#endif
