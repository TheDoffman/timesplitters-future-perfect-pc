/* Production persistence across independent processes, without game assets. */
#include "texture_mode.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv)
{
    assert(argc==2 || argc==4);
    assert(tsfp_texture_is_4k()==atoi(argv[1]));
    if(argc==4) {
        int before=tsfp_texture_is_4k(),value=atoi(argv[2]),ok=atoi(argv[3]);
        if(value==2)assert(tsfp_texture_toggle()==(ok?!before:before));
        else assert(tsfp_texture_save_4k(value)==ok);
        assert(tsfp_texture_is_4k()==(ok?(value==2?!before:value):before));
    }
    return 0;
}
