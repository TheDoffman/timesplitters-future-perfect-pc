#include <initializer_list>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "rhi.h"
#include "graphics/ssao_shader.h"
extern "C" int rhi_vk_dxc_compile(uint32_t,const RhiShaderSource *,uint32_t **,size_t *,char *,size_t);
int main(){
    for(const char *samples:{"1","2","4","8"}) {
        RhiMacro macros[]={{"DEPTH_SAMPLES",samples},{nullptr,nullptr}};
        for(const char *entry:{"vs_main","ps_main","composite_main","multiply_main"}) {
            bool vertex=!strcmp(entry,"vs_main");
            RhiShaderSource source={tsfp_ssao_shader,strlen(tsfp_ssao_shader),"ssao",macros,entry,vertex?"vs_5_0":"ps_5_0",1};
            uint32_t *code=nullptr;size_t bytes=0;char error[8192]={0};
            if(!rhi_vk_dxc_compile(vertex?RHI_STAGE_VERTEX:RHI_STAGE_PIXEL,&source,&code,&bytes,error,sizeof error)){
                fprintf(stderr,"%s/%s: %s\n",samples,entry,error);return 1;
            }
            if(bytes<20 || code[0]!=0x07230203){free(code);return 2;}
            free(code);printf("PASS SPIR-V AO depth%s %s\n",samples,entry);
        }
    }
    return 0;
}
