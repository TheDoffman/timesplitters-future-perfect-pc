/* Actual shared worker reader, with tiny generated DDS and no game/GPU assets. */
#include "texture_pack_io.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv) {
    if(argc!=5)return 2;
    TsfpDDS dds={0};
    unsigned char *data=tsfp_pack_read(argv[1],(uint32_t)atoi(argv[2]),
        (uint32_t)atoi(argv[3]),1024,&dds);
    if(!data)return atoi(argv[4])<0?0:3;
    int ok=atoi(argv[4])>=0 && data[128]==(unsigned char)atoi(argv[4]);
    free(data);return ok?0:4;
}
