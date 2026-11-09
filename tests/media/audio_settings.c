#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#define PC_AUDIO_SETTINGS_IMPLEMENTATION
#include "interface/pc_audio_settings.h"
int main(int argc,char **argv)
{
    if(argc==2 && !strcmp(argv[1],"features")) {
        for(unsigned mask=0;mask<32;mask++) {
            PcAudioOptions o={(int)(mask&1),(int)((mask>>1)&1),(int)((mask>>2)&1),
                (int)((mask>>3)&1),(int)((mask>>4)&1)};
            assert(pc_audio_set_options(&o));
            /* Reinitialize from the actual file, rather than test only the
               live atomic value written by the setter. */
            pc_audio_live=-1;PcAudioOptions read=pc_audio_get_options();
            assert(pc_audio_pack(read)==(int)mask);
            for(int feature=0;feature<4;feature++)
                assert(pc_audio_feature_enabled((PcAudioFeature)feature)==((mask&1) && (mask&(2u<<feature))));
            assert(pc_audio_set_enhanced(0));assert(!pc_audio_feature_enabled(PC_AUDIO_SPATIAL));
            assert(pc_audio_set_enhanced(1));read=pc_audio_get_options();
            assert(pc_audio_pack(read)==((int)mask|1));
        }
        PcAudioOptions o=pc_audio_get_options();o.spatial=2;
        assert(!pc_audio_set_options(&o));
        assert(!pc_audio_feature_enabled((PcAudioFeature)99));
        puts("PASS: all feature combinations, disk round trips and master retention");return 0;
    }
    assert(argc==3 || argc==5);
    assert(pc_audio_enhanced_enabled()==atoi(argv[1]));
    if(argc==5) {
        int before=pc_audio_enhanced_enabled(),value=atoi(argv[3]),ok=atoi(argv[4]);
        assert(pc_audio_set_enhanced(value)==ok);
        assert(pc_audio_enhanced_enabled()==(ok?value:before));
    }
    assert(strstr(PC_AUDIO_BETA_LABEL,"Beta"));
    puts("PASS: beta audio default, persistence, live update and failed-save isolation");
}
