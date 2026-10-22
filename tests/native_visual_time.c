#include <assert.h>
#include <math.h>
#include <stdint.h>
#include "../src/pc_native_visual_time.h"
int main(void)
{
    const unsigned rates[]={60,120,144};
    for(unsigned r=0;r<3;r++)for(unsigned slow=1;slow<=4;slow*=4) {
        PcNativeVisualTime clock={0};
        unsigned previous=0;
        pc_native_visual_time_store(&clock,1,0,99,0,0,0);
        for(unsigned f=1;f<=rates[r]*slow*8;f++) {
            unsigned value=f*60/(rates[r]*slow),whole=value-previous;
            float delta=60.f/(rates[r]*slow);
            pc_native_visual_time_store(&clock,1,1,previous,value,whole,delta);
            double expected=(double)f*60/(rates[r]*slow);
            double sample=pc_native_visual_time_sample(&clock,value);
            assert(fabs(sample-expected)<.00002);
            /* Skipped level-update branches and additional player renders
             * sample the same clock even while wall time keeps advancing. */
            for(unsigned view=0;view<4;view++)assert(pc_native_visual_time_sample(&clock,value)==sample);
            pc_native_visual_time_store(&clock,1,1,value,value,0,0);
            assert(pc_native_visual_time_sample(&clock,value)==sample);
            previous=value;
        }
    }
    PcNativeVisualTime clock={0};
    pc_native_visual_time_store(&clock,1,0,0,100,0,0);
    pc_native_visual_time_store(&clock,1,1,100,100,0,.4f);
    assert(pc_native_visual_time_sample(&clock,100)>100.39);
    /* Same-value overrides must discard the phase, not look like a pause. */
    pc_native_visual_time_store(&clock,1,0,100,100,0,0);
    assert(pc_native_visual_time_sample(&clock,100)==100);
    pc_native_visual_time_store(&clock,1,0,100,0,0,0);
    /* A level reset may cut across the global whole-tick carry. */
    pc_native_visual_time_store(&clock,1,1,0,1,1,.25f);
    assert(pc_native_visual_time_sample(&clock,1)==.25);
    pc_native_visual_time_store(&clock,1,1,1,4,3,2.75f);
    assert(pc_native_visual_time_sample(&clock,4)==3);
    /* Temporary native pause with a stale whole count still freezes. */
    pc_native_visual_time_store(&clock,1,1,4,5,1,0);
    assert(pc_native_visual_time_sample(&clock,5)==3);
    /* Unobserved writes, including mismatched increments, do not reuse an
     * old level's phase. Explicit rebases also cover rewind and forward seek. */
    assert(pc_native_visual_time_sample(&clock,700)==700);
    pc_native_visual_time_store(&clock,1,1,700,700,0,.5f);
    assert(pc_native_visual_time_sample(&clock,700)==700.5);
    pc_native_visual_time_store(&clock,1,1,700,12,0,.5f);
    assert(pc_native_visual_time_sample(&clock,12)==12);
    pc_native_visual_time_store(&clock,0,1,12,12,0,.5f);
    assert(pc_native_visual_time_sample(&clock,12)==12);
    /* Unsigned whole-count wrap keeps the original signed numeric read. */
    pc_native_visual_time_store(&clock,1,0,0,UINT32_MAX,0,0);
    pc_native_visual_time_store(&clock,1,1,UINT32_MAX,0,1,.5f);
    assert(pc_native_visual_time_sample(&clock,0)==-.5);
    pc_native_visual_time_store(&clock,1,0,0,0x7fffffff,0,0);
    pc_native_visual_time_store(&clock,1,1,0x7fffffff,0x80000000,1,1);
    assert(pc_native_visual_time_sample(&clock,0x80000000)==-2147483648.0);
    pc_native_visual_time_store(&clock,1,1,0x80000000,0x80000000,0,NAN);
    assert(pc_native_visual_time_sample(&clock,0x80000000)==-2147483648.0);
    /* Quantified float-delta drift over an hour; double offset avoids adding
     * small intervals to a large single-precision accumulated absolute time. */
    pc_native_visual_time_store(&clock,1,0,0,0,0,0);
    unsigned previous=0;
    for(unsigned f=1;f<=144*3600;f++) {
        unsigned value=f*5/12;
        pc_native_visual_time_store(&clock,1,1,previous,value,value-previous,60.f/144);
        previous=value;
    }
    assert(fabs(pc_native_visual_time_sample(&clock,216000)-216000)<.006);
    return 0;
}
