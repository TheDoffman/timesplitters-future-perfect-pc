#include "recomp_config.h"
#include "pc_frame_rate_settings.h"
#include "pc_native_rate.h"
#include <assert.h>
static void env(const char *key,const char *value) {
#ifdef _WIN32
    assert(!_putenv_s(key,value));
#else
    assert(!setenv(key,value,1));
#endif
}
int main(int argc,char **argv)
{
    assert(argc==4);
    env("TSFP_DISPLAY_CONFIG",argv[1]);env("TSFP_NATIVE_FPS","144");
    env("RECOMP_DISPLAY_CONFIG",argv[1]);
    int before=atoi(argv[2]),next=atoi(argv[3]);
    assert(pc_native_rate()==before);
    assert(!pc_frame_rate_save(30) && !pc_frame_rate_save(240));
    assert(pc_frame_rate_save(next));assert(pc_frame_rate_read()==next);
    /* Persistence alone cannot change a partially executed frame. */
    assert(pc_native_rate()==before);
    assert(!recomp_config_request_native_rate(30));
    assert(!recomp_config_request_native_rate(60));
    assert(!recomp_config_request_native_rate(240));
    assert(recomp_config_request_native_rate(next));
    assert(pc_native_rate()==before);
    recomp_config_commit_native_rate();
    assert(pc_native_rate()==next);
    const char *retained=recomp_config_lookup("TSFP_NATIVE_FPS","native_fps");
    assert(recomp_config_request_native_rate(120));
    assert(recomp_config_request_native_rate(144)); /* Latest selection wins. */
    assert(pc_native_rate()==next);
    recomp_config_commit_native_rate();assert(pc_native_rate()==144);
    assert(atoi(retained)==next); /* Previous readers retain immutable storage. */
    assert(recomp_config_request_native_rate(0));
    recomp_config_commit_native_rate();assert(pc_native_rate()==0);
    recomp_config_commit_native_rate();assert(pc_native_rate()==0);
    assert(pc_frame_rate_read()==next); /* Live switching never rewrites settings. */
    assert(!strcmp(recomp_config_lookup("RECOMP_FRAME_CAP","frame_cap"),"60"));
    puts("PASS: persisted rate, deferred live apply, original clock, immutable strings and invalid values");
    return 0;
}
