#include "pc_debug.h"
#include "../../src/interface/pc_counter_layout.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) {fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;} } while(0)
int main(void)
{
    PcDebugOptions values={0},before;
    float hd=pc_counter_scale(1920,1080,240,28);
    CHECK(pc_counter_scale(640,480,240,28)==1.f);
    CHECK(hd>0 && pc_counter_scale(3840,2160,240,28)>hd);
    CHECK(pc_counter_scale(3440,1440,240,28)==pc_counter_scale(2560,1440,240,28));
    CHECK(pc_counter_scale(120,1080,240,28)*256<=120);
    CHECK(pc_counter_scale(0,0,0,0)==1.f);
    CHECK(!values.show_counter);
    CHECK(pc_debug_option_count(PC_DEBUG_CAP_NETWORK)==PC_DEBUG_OPTION_COUNT);
    CHECK(pc_debug_option_count(0)==PC_DEBUG_OPTION_COUNT-1);
    for(unsigned i=0;i<PC_DEBUG_OPTION_COUNT;i++) {
        const PcDebugOption *option=pc_debug_option(i);
        CHECK(option && option->key[0] && option->label[0]);
        CHECK(option->offset+sizeof(int)<=sizeof values);
        for(unsigned j=0;j<i;j++) {
            CHECK(strcmp(option->key,pc_debug_option(j)->key));
            CHECK(option->offset!=pc_debug_option(j)->offset);
        }
        memset(&values,0,sizeof values);
        pc_debug_option_set(&values,i,-7);
        for(unsigned j=0;j<PC_DEBUG_OPTION_COUNT;j++)
            CHECK(pc_debug_option_value(&values,j)==(i==j));
    }
    CHECK(!pc_debug_option_available(PC_DEBUG_OPTION_LATENCY,0));
    pc_debug_option_set(&values,PC_DEBUG_OPTION_LATENCY,1);
    for(unsigned i=0;i<PC_DEBUG_OPTION_COUNT;i++)
        if(pc_debug_option_available(i,0))pc_debug_option_set(&values,i,0);
    CHECK(values.show_latency); /* Native edits preserve unavailable settings. */
    before=values;pc_debug_option_set(&values,PC_DEBUG_OPTION_COUNT,1);
    CHECK(!memcmp(&values,&before,sizeof values));
    CHECK(!pc_debug_option(PC_DEBUG_OPTION_COUNT));
    CHECK(!pc_debug_option_value(&values,PC_DEBUG_OPTION_COUNT));
#ifdef TEST_NEW_OPTION
    CHECK(!values.new_shared_option);
    CHECK(pc_debug_option_available(PC_DEBUG_OPTION_NEW,0));
    pc_debug_option_set(&values,PC_DEBUG_OPTION_NEW,1);
    CHECK(values.new_shared_option);
    CHECK(!strcmp(pc_debug_option(PC_DEBUG_OPTION_NEW)->key,"new_shared_option"));
#endif
    puts("PASS: shared setting definitions, capabilities, updates and preservation");
    return 0;
}
