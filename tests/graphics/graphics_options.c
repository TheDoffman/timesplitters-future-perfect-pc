#include <assert.h>
#include "graphics/graphics_options.h"
#include "graphics/supersampling.h"

static void rejected(const char *text)
{
    FILE *file = tmpfile();
    TsfpGraphicsOptions before = {8,1,16,1,0,3}, after = before;
    assert(file); fputs(text,file); rewind(file);
    assert(!tsfp_graphics_read(file,&after));
    assert(after.msaa == before.msaa && after.edge_smoothing == before.edge_smoothing &&
        after.anisotropy == before.anisotropy && after.ambient_occlusion == before.ambient_occlusion &&
        after.shader_cache == before.shader_cache && after.supersampling==before.supersampling);
    fclose(file);
}
int main(void)
{
    int samples[] = {1,2,4,8}, filters[] = {1,2,4,8,16};
    TsfpGraphicsOptions defaults = tsfp_graphics_defaults();
    assert(defaults.msaa == 1 && !defaults.edge_smoothing && defaults.anisotropy == 1 &&
           !defaults.ambient_occlusion && defaults.shader_cache && !defaults.supersampling);
    for(int scale=0;scale<4;scale++)
    for (int s=0;s<4;s++) for (int a=0;a<5;a++) for (int bits=0;bits<8;bits++) {
        TsfpGraphicsOptions input = {samples[s],bits&1,filters[a],(bits>>1)&1,(bits>>2)&1,scale};
        TsfpGraphicsOptions output = defaults;
        FILE *file=tmpfile();assert(file);
        assert(tsfp_graphics_write(file,&input));rewind(file);
        assert(tsfp_graphics_read(file,&output));fclose(file);
        assert(output.msaa==input.msaa && output.edge_smoothing==input.edge_smoothing &&
               output.anisotropy==input.anisotropy && output.ambient_occlusion==input.ambient_occlusion &&
               output.shader_cache==input.shader_cache && output.supersampling==input.supersampling);
    }
    rejected("TSFP_GRAPHICS_V1 8 1 16 1");
    rejected("TSFP_GRAPHICS_V2 8 1 16 1 1");
    rejected("TSFP_GRAPHICS_V2 8 1 16 1 1 4");
    rejected("TSFP_GRAPHICS_V2 8 1 16 1 1 -1");
    rejected("TSFP_GRAPHICS_V2 8 1 16 1 1 2 extra");
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
    {
        unsigned w,h;
        assert(tsfp_supersampling_size(1920,1080,0,&w,&h)==0 && w==1920 && h==1080);
        assert(tsfp_supersampling_size(1920,1080,1,&w,&h)==1 && w==2400 && h==1350);
        assert(tsfp_supersampling_size(1920,1080,2,&w,&h)==2 && w==2880 && h==1620);
        assert(tsfp_supersampling_size(1920,1080,3,&w,&h)==3 && w==3840 && h==2160);
        assert(tsfp_supersampling_size(3840,2160,3,&w,&h)==3 && w==7680 && h==4320);
        assert(tsfp_supersampling_size(5120,2880,3,&w,&h)==2 && w==7680 && h==4320);
        assert(tsfp_supersampling_size(7680,4320,3,&w,&h)==0 && w==7680 && h==4320);
        assert(tsfp_supersampling_size(853,481,1,&w,&h)==1 && w==1067 && h==602);
        assert(tsfp_supersampling_size(UINT32_MAX,UINT32_MAX,3,&w,&h)==0 && w==UINT32_MAX && h==UINT32_MAX);
        assert(tsfp_supersampling_size(0,0,3,&w,&h)==0 && !w && !h);
    }
    puts("PASS: graphics preferences and attachment-compatible sample fallback");
    return 0;
}
