/* Shared schedules for the asset-free gate and original US input integration.
 * Callbacks supply either modeled button edges or the real guest decoder.
 * Every press spans at least one 60 Hz input poll; sub-poll taps are not covered.
 */
static void reload_phase_sweep(void (*reset)(void),
    unsigned (*poll)(PcNativeStep, unsigned),
    unsigned (*consume)(PcNativeStep, unsigned))
{
    const unsigned rates[]={60,60,120,144,180,240};
    unsigned trials=0, presses=0, mismatches=0;
    for(unsigned mode=0;mode<6;mode++)
    for(unsigned pattern=0;pattern<6;pattern++)
    for(unsigned phase=0;phase<8;phase++)
    for(unsigned width=8;width<=32;width*=2) {
        PcNativeTiming clock={0};
        const int64_t frequency=720000;
        int64_t now=0;
        unsigned edges_seen=0, accepted=0;
        g_tsfp_native_timing_active=mode!=0;
        reset();
        pc_native_timing_step(&clock,now,frequency,rates[mode],0);
        for(unsigned frame=1;frame<=200;frame++) {
            unsigned fps=rates[mode];
            if(pattern==4 && mode) {
                fps=frame<100 ? 240 : frame<150 ? 60 : rates[mode];
                g_tsfp_native_timing_active=frame<100 || frame>=150;
            }
            int64_t elapsed=frequency/fps;
            if(pattern==1) elapsed=elapsed*(frame%2 ? 3 : 5)/4;
            if(pattern==2 && frame%17==0) elapsed=frequency/10;
            if(pattern==5 && frame%7==0) elapsed=0;
            /* Original60 uses the authored fixed interval, not native jitter. */
            if(!mode) elapsed=frequency/60;
            int paused=frame<=phase ||
                (pattern==3 && ((frame>=24 && frame<28) || (frame>=96 && frame<100)));
            now+=elapsed;
            PcNativeStep step=pc_native_timing_step(&clock,now,frequency,fps,paused);
            unsigned down=(frame>=32+phase && frame<32+phase+width) ||
                          (frame>=110+phase && frame<110+phase+width);
            unsigned edge=poll(step,down);
            unsigned expected=edge && step.game_delta>0;
            unsigned actual=consume(step,edge);
            if(actual!=expected) {
                fprintf(stderr,"reload phase mismatch mode=%u pattern=%u phase=%u width=%u frame=%u raw=%u game=%u edge=%u accepted=%u\n",
                    mode,pattern,phase,width,frame,step.raw_ticks,step.game_ticks,edge,actual);
                assert(actual==expected);
            }
            edges_seen+=edge!=0; accepted+=actual;
            mismatches+=edge && step.raw_ticks && !step.game_ticks && step.game_delta>0;
        }
        /* Long holds must never repeat; releasing and pressing must re-arm. */
        assert(edges_seen==2 && accepted==2);
        trials++; presses+=accepted;
    }
    assert(mismatches>0);
    printf("reload_sweep trials=%u presses=%u offset_edges=%u\n",trials,presses,mismatches);
}
