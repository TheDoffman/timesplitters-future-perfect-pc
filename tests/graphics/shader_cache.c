#include "rhi.h"
#include "rhi_shader_cache.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const char *root;
int recomp_config_user_dir(char *out,size_t n)
{ if(strlen(root)>=n)return 0;strcpy(out,root);return 1; }
uint32_t recomp_config_title_id(void) { return 0x45410066; }
int main(int argc,char **argv)
{
    const unsigned char payload[]={0,1,2,3,127,128,254,255};
    RhiShaderSource source={"shader source",13,"fixture",NULL,"main","ps_4_0",1};
    void *blob=NULL;size_t bytes=0;char path[1024];
    assert(argc==5);root=argv[1];
    if(!strcmp(argv[2],"put"))rhi_shader_cache_put(&source,argv[3],payload,sizeof payload);
    else if(!strcmp(argv[2],"get")) {
        int found=rhi_shader_cache_get(&source,argv[3],&blob,&bytes);
        assert(found==atoi(argv[4]));
        if(found)assert(bytes==sizeof payload && !memcmp(blob,payload,bytes));
        else assert(blob==NULL && bytes==0);
        free(blob);
    } else if(!strcmp(argv[2],"clear")) {
        assert(rhi_shader_cache_clear()==atoi(argv[4]));
        assert(!rhi_cache_file_path("vulkan-pipelines","pipelines.bin",path,sizeof path));
    } else {
        assert(!rhi_cache_file_path("../escape","file.bin",path,sizeof path));
        assert(!rhi_cache_file_path("test","../escape",path,sizeof path));
    }
    return 0;
}
