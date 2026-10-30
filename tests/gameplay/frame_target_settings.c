#include "recomp_config.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void env(const char *key,const char *value) {
#ifdef _WIN32
    _putenv_s(key,value);
#else
    setenv(key,value,1);
#endif
}
static void fixed(const RecompSettings *s) {
    assert(s->frame_target==0 && s->frame_interp==1 && s->refresh_rate==0);
    assert(!strcmp(s->frame_cap,"60"));
}
int main(int argc,char **argv) {
    RecompSettings s; FILE *f;
    assert(argc==2);
    recomp_settings_defaults(&s);fixed(&s);
    for(int target=120;target<=144;target+=24) {
        f=fopen(argv[1],"w");assert(f);
        fprintf(f,"frame_target = %d\nframe_interp = 4\nrefresh_rate = 144\nframe_cap = 0\nanisotropy = 8\n",target);
        fclose(f);
        assert(recomp_settings_read(argv[1],&s));fixed(&s);assert(s.anisotropy==8);
        /* Even an old caller passing retired values cannot persist them. */
        s.frame_target=target;s.frame_interp=4;s.refresh_rate=144;strcpy(s.frame_cap,"0");
        assert(recomp_settings_write(argv[1],&s));
        assert(recomp_settings_read(argv[1],&s));fixed(&s);
    }
    env("RECOMP_DISPLAY_CONFIG",argv[1]);
    env("RECOMP_FRAME_INTERP","4");env("RECOMP_FRAME_TARGET","144");
    env("RECOMP_FPS_CAP","0");env("RECOMP_REFRESH_RATE","144");
    assert(recomp_config_int("RECOMP_FRAME_INTERP","frame_interp",4)==1);
    assert(recomp_config_int("RECOMP_FRAME_TARGET","frame_target",144)==0);
    assert(recomp_config_int("RECOMP_FPS_CAP","frame_cap",0)==60);
    assert(recomp_config_int("RECOMP_REFRESH_RATE","refresh_rate",144)==0);
    puts("PASS: fixed 60 FPS defaults, legacy migration, writes and overrides");
    return 0;
}
