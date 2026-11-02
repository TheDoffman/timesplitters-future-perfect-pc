#include <assert.h>
#include "graphics/graphics_options.h"

static void rejected(const char *text)
{
    FILE *file = tmpfile();
    TsfpGraphicsOptions before = {8,1,16,1,0}, after = before;
    assert(file); fputs(text,file); rewind(file);
    assert(!tsfp_graphics_read(file,&after));
    assert(after.msaa == before.msaa && after.edge_smoothing == before.edge_smoothing &&
        after.anisotropy == before.anisotropy && after.ambient_occlusion == before.ambient_occlusion &&
        after.shader_cache == before.shader_cache);
    fclose(file);
}
int main(void)
{
    int samples[] = {1,2,4,8}, filters[] = {1,2,4,8,16};
    TsfpGraphicsOptions defaults = tsfp_graphics_defaults();
    assert(defaults.msaa == 1 && !defaults.edge_smoothing && defaults.anisotropy == 1 &&
           !defaults.ambient_occlusion && defaults.shader_cache);
    for (int s=0;s<4;s++) for (int a=0;a<5;a++) for (int bits=0;bits<8;bits++) {
        TsfpGraphicsOptions input = {samples[s],bits&1,filters[a],(bits>>1)&1,(bits>>2)&1};
        TsfpGraphicsOptions output = defaults;
        FILE *file=tmpfile();assert(file);
        assert(tsfp_graphics_write(file,&input));rewind(file);
        assert(tsfp_graphics_read(file,&output));fclose(file);
        assert(output.msaa==input.msaa && output.edge_smoothing==input.edge_smoothing &&
               output.anisotropy==input.anisotropy && output.ambient_occlusion==input.ambient_occlusion &&
               output.shader_cache==input.shader_cache);
    }
    rejected("TSFP_GRAPHICS_V1 8 1 16 1");
    rejected("TSFP_GRAPHICS_V2 8 1 16 1 1");
    rejected("TSFP_GRAPHICS_V1 3 1 16 1 1");
    rejected("TSFP_GRAPHICS_V1 8 2 16 1 1");
    rejected("TSFP_GRAPHICS_V1 8 1 17 1 1");
    rejected("TSFP_GRAPHICS_V1 8 1 16 -1 1");
    rejected("TSFP_GRAPHICS_V1 8 1 16 1 2");
    rejected("TSFP_GRAPHICS_V1 99999999999999999999999 1 16 1 1");
    rejected("TSFP_GRAPHICS_V1 8 1 16 1 1\nextra");
    assert(tsfp_graphics_samples(8,15,7)==4);
    assert(tsfp_graphics_samples(8,9,3)==1);
    assert(tsfp_graphics_samples(4,15,15)==4);
    assert(tsfp_graphics_samples(1,15,15)==1);
    assert(tsfp_graphics_samples(3,15,15)==1);
    puts("PASS: graphics preferences and attachment-compatible sample fallback");
    return 0;
}
