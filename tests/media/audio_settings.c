#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#define PC_AUDIO_SETTINGS_IMPLEMENTATION
#include "interface/pc_audio_settings.h"
int main(int argc,char **argv)
{
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
