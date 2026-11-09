#include "graphics/msaa_color_surface.h"
static void test_msaa_color_surface(void)
{
    unsigned samples=4;
    while(samples>1&&!rhi_sample_count_supported(RHI_FORMAT_R8G8B8A8_UNORM,samples))samples/=2;
    assert(samples>1);
    for(unsigned cube=0;cube<=1;cube++){
        RhiImageDesc d={0};d.type=RHI_IMAGE_2D;d.width=d.height=32;d.depth=cube?6:1;
        d.mip_levels=2;d.format=RHI_FORMAT_R8G8B8A8_UNORM;d.samples=1;d.cube=cube;
        d.bind=RHI_BIND_RENDER_TARGET|RHI_BIND_SAMPLED;
        RhiImage *original=rhi_image_create(&d,NULL);assert(original);
        unsigned char input[32*32*4],output[32*32*4];
        for(unsigned i=0;i<32*32;i++){
            input[i*4]=(unsigned char)i;input[i*4+1]=(unsigned char)(i/32);
            input[i*4+2]=73;input[i*4+3]=127;
        }
        for(unsigned sub=0;sub<d.depth*2;sub++)
            rhi_image_update(original,sub,NULL,input,(32>>(sub%2))*4,0);
        TsfpMsaaColorSurface p={0};unsigned selected=(d.depth-1)*2+1;
        assert(tsfp_msaa_color_create(&p,original,selected,samples));
        assert(p.width==16&&p.height==16&&p.samples==samples);
        assert(tsfp_msaa_color_resolve(&p));
        assert(rhi_image_readback(original,selected,output,16*4)==0);
        assert(!memcmp(input,output,16*16*4)); /* seed preserves every color and alpha */
        TsfpMsaaColorSurface previous=p;
        assert(!tsfp_msaa_color_create(&p,original,d.depth*2,samples));
        assert(!memcmp(&p,&previous,sizeof p));
        /* CPU writes replace the old coverage contents on explicit reseed. */
        memset(input,91,sizeof input);rhi_image_update(original,selected,NULL,input,16*4,0);
        assert(tsfp_msaa_color_seed(&p)&&tsfp_msaa_color_resolve(&p));
        assert(rhi_image_readback(original,selected,output,16*4)==0);
        assert(!memcmp(input,output,16*16*4));
        /* A later render wins over the old sampled texture. */
        const float green[4]={0,1,0,1};rhi_clear_color(p.rtv,green);
        assert(tsfp_msaa_color_resolve(&p));
        assert(rhi_image_readback(original,selected,output,16*4)==0);
        for(unsigned i=0;i<16*16;i++)assert(output[i*4]==0&&output[i*4+1]==255&&output[i*4+2]==0&&output[i*4+3]==255);
        /* Adjacent level is not touched by seeding or resolving. */
        assert(rhi_image_readback(original,selected-1,output,32*4)==0);
        for(unsigned i=0;i<32*32;i++)assert(output[i*4]==(unsigned char)i&&output[i*4+2]==73&&output[i*4+3]==127);
        rhi_image_destroy(original); /* companion retains its original until destruction */
        assert(tsfp_msaa_color_resolve(&p));
        rhi_set_render_target(NULL,NULL);tsfp_msaa_color_destroy(&p);tsfp_msaa_color_destroy(&p);
    }
    puts("PASS: MSAA texture companions preserve seed, uploads, alpha and adjacent faces/mips");
}
