/* Retired engine must never suppress guest frames or replay GPU commands. */
#include <assert.h>
#include <stdio.h>
#include "hle/hle_d3d8_record.h"
int hle_d3d8_interp_rec;
#include "hle/hle_d3d8_interp.c"
static int calls;
static void call(const void *arg) { calls += *(const int *)arg; }
int main(void) {
    int n=1;
    hle_d3d8_interp_init();
    for(int i=0;i<240;i++) {
        hle_d3d8_interp_op(call,&n,sizeof n);
        hle_d3d8_interp_draw(call,&n,sizeof n,NULL);
        hle_d3d8_interp_frame_end();hle_d3d8_interp_finish();
        assert(!hle_d3d8_interp_rec && !hle_d3d8_interp_predicting);
        assert(!hle_d3d8_interp_skip_guest());
    }
    assert(!calls && !hle_d3d8_interp_presents() && !hle_d3d8_interp_suppressed());
    hle_d3d8_interp_retire(call,&n,sizeof n);assert(calls==1);
    puts("PASS: no interpolation, replay, prediction or suppressed guest frames");
    return 0;
}
