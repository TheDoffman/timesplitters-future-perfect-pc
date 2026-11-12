#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "network/pc_online_roster.h"
#define REQUIRE(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned char memory[0x800000];
static void word(unsigned address,unsigned value){memcpy(memory+address,&value,4);}
static int read_memory(void *ctx,uint32_t address,void *out,size_t bytes){
    (void)ctx;if(address>sizeof memory || bytes>sizeof memory-address)return 0;
    memcpy(out,memory+address,bytes);return 1;
}
static void portrait(void *ctx,int character,int skin,int32_t *id,char *name,size_t size){
    (void)ctx;REQUIRE(character==72 && skin==-1);*id=528;snprintf(name,size,"Character name");
}
int main(int argc,char **argv){
    REQUIRE(argc==2);word(0x76b108,1);word(0x76b140,1);word(0x76b1a4,15);word(0x7de30c,7);
    word(0x79094c,53);word(0x7de318,120);word(0x7356d8,0x10000);
    memory[0x7de400]=2;memory[0x7de401]=1;memory[0x7de402]=1;memory[0x7de499]=9;
    word(0x777244,1);word(0x77728c,1); /* Slot1 is a vacant human slot. */
    for(unsigned i=0;i<3;i++){
        unsigned actor=0x10000+i*0xea0;word(actor+0x48,72);word(actor+0x70,0x30000+i*64);
        memory[actor+0xc0d]=255;strcpy((char *)memory+0x30000+i*64,"Player\nname");
    }
    PcOnlineRoster out;REQUIRE(pc_online_roster_capture(&out,NULL,read_memory,portrait));
    REQUIRE(out.count==2 && out.people[0].slot==0 && out.people[1].slot==2);
    REQUIRE(!strcmp(out.people[0].name,"Playername") && out.people[0].portrait==528);
    REQUIRE(!strcmp(out.people[0].character_name,"Character name"));
    PcOnlineRoster original=out;word(0x76b140,2);
    REQUIRE(!pc_online_roster_capture(&out,NULL,read_memory,portrait) && !memcmp(&out,&original,sizeof out));
    word(0x76b140,1);word(0x7356d8,0xfffffff0u);
    REQUIRE(!pc_online_roster_capture(&out,NULL,read_memory,portrait));
    word(0x7356d8,0x10000);word(0x10070,0xfffffff0u);
    REQUIRE(pc_online_roster_capture(&out,NULL,read_memory,portrait) && !out.people[0].name[0]);
    out.serial=1;FILE *file=fopen(argv[1],"wb");REQUIRE(file);
    REQUIRE(fwrite(&out,1,sizeof out,file)==sizeof out && !fclose(file));
    puts("PASS: guarded read-only identity capture and bounded local roster ABI");return 0;
}
