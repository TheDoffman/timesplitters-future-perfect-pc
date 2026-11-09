#define PC_FEED_SETTINGS_IMPLEMENTATION
#include "interface/pc_feed_settings.h"
int main(int argc,char **argv) {
    if(argc!=3)return 2;
    int expected=atoi(argv[2]);
    if(argv[1][0]=='r')return pc_feed_enabled()!=expected;
    if(argv[1][0]=='f') {
        int before=pc_feed_enabled();
        return pc_feed_set_enabled(!before)!=0 || pc_feed_enabled()!=before;
    }
    return !pc_feed_set_enabled(expected) || pc_feed_enabled()!=expected;
}
