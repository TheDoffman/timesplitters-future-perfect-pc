#include "diagnostics/pc_native_modules.h"
#include <string.h>
int main(void){
    static PcNativeModules images;PcNativeImage image;uint64_t address;
#define CHECK(v) do {if(!(v))return __LINE__;} while(0)
    CHECK(!pc_native_module_add(&images,0,2,1,0,"/bad"));
    CHECK(pc_native_module_add(&images,0x1000,0x1100,0x1200,0x1000,"/first"));
    CHECK(pc_native_module_find(&images,0x1100,&image) && !strcmp(image.path,"/first"));
    CHECK(pc_native_module_address(&image,0x11ff,&address) && address==0x1ff);
    CHECK(!pc_native_module_find(&images,0x1200,&image));
    pc_native_module_remove(&images,0x1000);CHECK(!pc_native_module_find(&images,0x1100,&image));
    CHECK(pc_native_module_add(&images,0x1000,0x1100,0x1200,0x1000,"/replacement"));
    CHECK(pc_native_module_find(&images,0x1100,&image) && !strcmp(image.path,"/replacement"));
    pc_crumb_store(&images.reserved,3);images.slots[2].image.begin=0x2000;images.slots[2].image.end=0x3000;
    pc_crumb_store(&images.slots[2].live,1);CHECK(!pc_native_module_find(&images,0x2000,&image));
    pc_crumb_store(&images.writer,1);CHECK(!pc_native_module_add(&images,1,2,3,0,"/busy"));CHECK(pc_crumb_load(&images.truncated));pc_crumb_store(&images.writer,0);
    char path[PC_NATIVE_MODULE_PATH+2];memset(path,'a',sizeof path);path[sizeof path-1]=0;
    CHECK(pc_native_module_add(&images,0x4000,0x4000,0x5000,0,path));CHECK(pc_native_module_find(&images,0x4000,&image) && !image.path_complete && image.path[PC_NATIVE_MODULE_PATH-1]==0);
    memset(&images,0,sizeof images);
    for(unsigned i=0;i<PC_NATIVE_MODULE_COUNT;i++)CHECK(pc_native_module_add(&images,i*16,i*16,i*16+8,0,"/bounded"));
    CHECK(!pc_native_module_add(&images,0,1,2,0,"/overflow"));CHECK(pc_native_module_count(&images)==PC_NATIVE_MODULE_COUNT && pc_crumb_load(&images.truncated));
#undef CHECK
    return 0;
}
