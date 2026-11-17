/* US SDK Begin/End wrappers retain every original packet, allocation and
 * return value. Only a completed native GPU result can answer the poller. */
extern void host_VisibilityBegin(void);
extern void host_VisibilityEnd(uint32_t,int);
extern int xbox_D3D8VisibilityResult(uint32_t,uint32_t *);
void pc_visibility_begin(void (*original)(void)) {
    original();host_VisibilityBegin();
}
void pc_visibility_end(void (*original)(void)) {
    uint32_t index=WORD_AT(g_esp+4);
    original();host_VisibilityEnd(index,g_eax==0);
}
int pc_visibility_result(uint32_t index,uint32_t result_va,uint32_t stamp_va) {
    /* The title's two flare polls pass no timestamp. Do not invent GPU time
     * for callers requiring a timestamp; leave their original report path. */
    if(stamp_va || (result_va && !guest_writable(result_va,4)))return 0;
    uint32_t pixels=0;
    if(xbox_D3D8VisibilityResult(index,&pixels)!=1)return 0;
    if(result_va)WORD_AT(result_va)=pixels;
    static unsigned nonzero,zero,zero_after_visible,calls,periodic;
    int report=pixels?nonzero<4:(!zero || (nonzero && !zero_after_visible));
    if(periodic<32 && ++calls%300u==0){periodic++;report=1;}
    if(report) {
        if(pixels)nonzero++;else {zero=1;if(nonzero)zero_after_visible=1;}
        fprintf(stderr,"[visibility] SDK index=%u completed with %u depth-tested pixel(s)\n",index,pixels);
    }
    return 1;
}
