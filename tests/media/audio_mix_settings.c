/* Exercise the production serializer and gain policy in an isolated directory. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#define PC_AUDIO_MIX_SETTINGS_IMPLEMENTATION
#include "interface/pc_audio_mix_settings.h"
#undef PC_AUDIO_MIX_SETTINGS_IMPLEMENTATION
int main(int argc,char **argv)
{
    assert(argc==3 || argc==4);
    PcAudioMixOptions current=pc_audio_mix_get();
    assert(current.master==atoi(argv[1]));
    if(!strcmp(argv[2],"listening")) {
        for(int duck=0;duck<=1;duck++)for(int mode=0;mode<=2;mode++) {
            PcAudioMixOptions wanted=current;wanted.ducking=duck;wanted.dynamics=mode;
            assert(pc_audio_mix_set(&wanted));pc_audio_mix_live=-1;
            PcAudioMixOptions loaded=pc_audio_mix_get();
            assert(loaded.master==wanted.master && loaded.ducking==duck && loaded.dynamics==mode);
            for(unsigned i=0;i<PC_AUDIO_BUS_COUNT;i++)assert(loaded.volume[i]==wanted.volume[i]);
        }
        return 0;
    }
    if(argc==4) {
        PcAudioMixOptions wanted=PC_AUDIO_MIX_DEFAULTS;wanted.master=atoi(argv[2]);
        int ok=pc_audio_mix_set(&wanted);assert(ok==atoi(argv[3]));
        assert(pc_audio_mix_get().master==(ok?wanted.master:current.master));
        return 0;
    }
    assert(!strcmp(argv[2],"policy"));
    PcAudioMixOptions o=PC_AUDIO_MIX_DEFAULTS;
    for(int bus=0;bus<PC_AUDIO_BUS_COUNT;bus++) {
        assert(pc_audio_mix_apply(o,-123,bus)==-123);
        assert(pc_audio_mix_apply(o,123,bus)==123); /* exact neutral pass-through */
        o.volume[bus]=50;assert(pc_audio_mix_apply(o,-123,bus)==-725);
        for(int other=0;other<PC_AUDIO_BUS_COUNT;other++)
            if(other!=bus)assert(pc_audio_mix_apply(o,-123,other)==-123);
        o.master=50;assert(pc_audio_mix_apply(o,-123,bus)==-1327);
        o.volume[bus]=0;assert(pc_audio_mix_apply(o,0,bus)==-10000);
        o.master=100;o.volume[bus]=100;
    }
    o.master=0;assert(pc_audio_mix_apply(o,123,PC_AUDIO_MOVIES)==-10000);
    assert(!pc_audio_mix_set(NULL));
    pc_audio_mix_writer=1;
    assert(!pc_audio_mix_set(&current) && pc_audio_mix_get().master==current.master);
    pc_audio_mix_writer=0; /* Competing save does not spin, write or publish. */
    o.master=101;assert(!pc_audio_mix_set(&o));
    o.master=100;o.volume[PC_AUDIO_MOVIES]=-1;assert(!pc_audio_mix_set(&o));
    const char *bad[]={"TSFP_AUDIO_MIX_V1 100 100 100 100 100", "TSFP_AUDIO_MIX_V2 1 2 3 4 5 6",
        "TSFP_AUDIO_MIX_V1 100 100 100 100 100 100 extra", "TSFP_AUDIO_MIX_V1 01 1 1 1 1 1",
        "TSFP_AUDIO_MIX_V1 101 1 1 1 1 1", "TSFP_AUDIO_MIX_V1 1 1 1 1 1 -1",
        "TSFP_AUDIO_MIX_V1 99999999999999999999999 1 1 1 1 1"};
    for(unsigned i=0;i<sizeof bad/sizeof *bad;i++)assert(!pc_audio_mix_parse(bad[i],&o));
    assert(pc_audio_mix_parse("\tTSFP_AUDIO_MIX_V1 100 90 80 70 60 50\r\n",&o));
    assert(o.master==100 && o.volume[PC_AUDIO_EFFECTS]==90 && o.volume[PC_AUDIO_MUSIC]==80 &&
        o.volume[PC_AUDIO_DIALOGUE]==70 && o.volume[PC_AUDIO_BACKGROUND]==60 && o.volume[PC_AUDIO_MOVIES]==50);
    PcAudioMixOptions roundtrip=pc_audio_mix_unpack(pc_audio_mix_pack(o));
    assert(roundtrip.master==o.master);
    for(int i=0;i<PC_AUDIO_BUS_COUNT;i++)assert(roundtrip.volume[i]==o.volume[i]);
    assert(!o.ducking && !o.dynamics);
    assert(pc_audio_mix_parse("TSFP_AUDIO_MIX_V2 100 90 80 70 60 50 1 2",&o));
    roundtrip=pc_audio_mix_unpack(pc_audio_mix_pack(o));assert(roundtrip.ducking==1 && roundtrip.dynamics==2);
    assert(!pc_audio_mix_parse("TSFP_AUDIO_MIX_V2 100 100 100 100 100 100 2 0",&o));
    assert(!pc_audio_mix_parse("TSFP_AUDIO_MIX_V2 100 100 100 100 100 100 1 3",&o));
    o=(PcAudioMixOptions)PC_AUDIO_MIX_DEFAULTS;o.ducking=1;pc_audio_mix_live=pc_audio_mix_pack(o);
    pc_audio_duck_guard=1;assert(!pc_audio_mix_duck_begin(1000));pc_audio_duck_guard=0;
    assert(pc_audio_mix_duck_begin(1000));pc_audio_mix_duck_end(1,1000);
    assert(!pc_audio_mix_duck_begin(1004));
    assert(pc_audio_mix_level(0,PC_AUDIO_MUSIC)<0 && pc_audio_mix_level(0,PC_AUDIO_DIALOGUE)==0);
    for(uint64_t t=1005;t<=1400;t+=5){assert(pc_audio_mix_duck_begin(t));pc_audio_mix_duck_end(1,t);}
    assert(pc_audio_mix_level(0,PC_AUDIO_MUSIC)==-600);
    assert(pc_audio_mix_duck_begin(1300));pc_audio_mix_duck_end(0,1300); /* host clock reset */
    assert(pc_audio_mix_level(0,PC_AUDIO_MUSIC)==0);
    o.ducking=0;pc_audio_mix_live=pc_audio_mix_pack(o);
    assert(!pc_audio_mix_duck_begin(1500));
    assert(pc_audio_mix_level(-12345,PC_AUDIO_EFFECTS)==-12345); /* exact Original */
    o.dynamics=2;pc_audio_mix_live=pc_audio_mix_pack(o);
    assert(!pc_audio_mix_dynamics(PC_AUDIO_DIALOGUE) && pc_audio_mix_dynamics(PC_AUDIO_MUSIC)==2);
    return 0;
}
