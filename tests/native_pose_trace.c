/* Exercise the production boundary filter, address guards and bounded writer. */
#include <assert.h>
#include "../src/pc_native_pose_trace.h"
static unsigned reads;
static uint32_t network=15,match=7,humans=2,pool=0x80001000,entity=0x80004000;
static float position=1.25f;
static uint32_t read_word(uint32_t address)
{
    reads++;
    switch(address) {
    case 0x76b1a4:return network;
    case 0x7de30c:return match;
    case 0x7de400:return humans;
    case 0x7356d8:return pool;
    case 0x7497b8:return 123;
    case 0x79094c:return 48;
    case 0x7de318:return 456;
    case 0x80001000:case 0x80001ea0:return entity;
    default: {
        assert(address==0x4e794c || (address>=0x80004064 && address<=0x8000406c));
        uint32_t bits;memcpy(&bits,&position,4);return bits;
    }
    }
}
int main(void)
{
    PcNativePose pose;
    assert(!pc_native_pose_collect(read_word,0x3d455,&pose) && reads==0);
    assert(pc_native_pose_collect(read_word,0x3d3eb,&pose));
    assert(pose.frame==123 && pose.scene==48 && pose.tick==456);
    assert(pose.delta==1.25f && pose.position[1][2]==1.25f);
    network=11;assert(!pc_native_pose_collect(read_word,0x3d3eb,&pose));network=15;
    match=1;assert(!pc_native_pose_collect(read_word,0x3d3eb,&pose));match=7;
    humans=1;assert(!pc_native_pose_collect(read_word,0x3d3eb,&pose));humans=2;
    pool=0;assert(!pc_native_pose_collect(read_word,0x3d3eb,&pose));pool=0x80001000;
    entity=0x83ffffff;assert(!pc_native_pose_collect(read_word,0x3d3eb,&pose));entity=0x80004000;
    position=NAN;assert(!pc_native_pose_collect(read_word,0x3d3eb,&pose));position=2.5f;
    assert(pc_native_pose_collect(read_word,0x3d3eb,&pose));
    PcNativePoseTrace trace={0};
    assert(pc_native_pose_enabled(&trace));
    for(unsigned i=0;i<PC_NATIVE_POSE_LIMIT+10;i++)pc_native_pose_record(&trace,&pose,i/144.0);
    assert(trace.rows==PC_NATIVE_POSE_LIMIT && !trace.file && !pc_native_pose_enabled(&trace));
    FILE *file=fopen(getenv("TSFP_NATIVE_POSE_TRACE"),"r");assert(file);
    char line[512];unsigned lines=0;while(fgets(line,sizeof line,file))lines++;
    fclose(file);assert(lines==PC_NATIVE_POSE_LIMIT+1);
    puts("PASS: pose scope, completed-position reads, invalid addresses and bounded output");return 0;
}
