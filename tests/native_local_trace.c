#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include "../src/pc_native_local_trace.h"
static struct {uint32_t address,value;} cells[128];
static unsigned count,reads;
static void word(uint32_t address,uint32_t value) {
    for(unsigned i=0;i<count;i++)if(cells[i].address==address){cells[i].value=value;return;}
    assert(count<128);cells[count].address=address;cells[count++].value=value;
}
static void real(uint32_t address,float value) {uint32_t bits;memcpy(&bits,&value,4);word(address,bits);}
static uint32_t read_word(uint32_t address) {
    reads++;for(unsigned i=0;i<count;i++)if(cells[i].address==address)return cells[i].value;
    fprintf(stderr,"Unexpected read: %x\n",address);abort();
}
static void setup(void) {
    count=reads=0;
    word(0x79094c,39);word(0x7de30c,7);word(0x7de400,0xa0101);
    word(0x7b0c7c,0x80001000);word(0x80001014,0x80004000);
    word(0x7497b8,123);word(0x7de318,456);word(0x4e7940,0);word(0x4e7944,0);
    real(0x4e794c,.25f);word(0x6b9408,0);
    for(unsigned i=0;i<3;i++)real(0x800010b4+i*4,10.f+i);
    real(0x80001100,25);real(0x80001104,-12);
    word(0x800015b0,0x80005000);word(0x80001e84,0x80006000);
    for(unsigned i=0;i<3;i++) {
        uint32_t entity=0x80004000+i*0x1000,model=0x80010000+i*0x1000;
        word(entity+4,model);word(model+0xd4,17+i);word(model+0x124,2);
        real(model+0xdc,1.25f+i);real(model+0xe0,1.2f+i);real(model+0x108,.5f);
        word(model+0x12c,27+i);real(model+0x134,.1f+i);
    }
    word(0x7b0cc0,0x80008000);
    for(unsigned i=0;i<3;i++){real(0x8000827c+i*4,20.f+i);real(0x80008334+i*4,.25f+i);}
}
static void checks(void) {
    PcNativeLocalPose p;
    setup();assert(!pc_native_local_collect(read_word,0x3d455,&p)&&reads==0);
    assert(pc_native_local_collect(read_word,0x3d3eb,&p));
    assert(p.frame==123&&p.scene==39&&p.tick==456&&p.delta==.25f&&p.game_ticks==0);
    assert(p.valid==15&&p.position[2]==12&&p.look[0]==25&&p.camera_direction[2]==2.25f);
    assert(p.animation[1].entity==0x80005000&&p.animation[1].model==0x80011000);
    assert(p.animation[1].clip==18&&p.animation[1].time==2.25f&&p.animation[1].next_clip==28);
    word(0x7497b8,124);real(0x800110dc,2.5f);real(0x80001100,26);
    assert(pc_native_local_collect(read_word,0x3d3eb,&p));
    assert(p.frame==124&&p.look[0]==26&&p.animation[1].time==2.5f);
    word(0x79094c,101);assert(!pc_native_local_collect(read_word,0x3d3eb,&p));word(0x79094c,39);
    word(0x7de30c,3);assert(!pc_native_local_collect(read_word,0x3d3eb,&p));word(0x7de30c,1);
    assert(pc_native_local_collect(read_word,0x3d3eb,&p));word(0x7de30c,7);
    word(0x7de400,2);assert(!pc_native_local_collect(read_word,0x3d3eb,&p));word(0x7de400,1);
    word(0x7b0c7c,0xfffffffc);assert(!pc_native_local_collect(read_word,0x3d3eb,&p));word(0x7b0c7c,0x80001000);
    word(0x80001014,0xfffffffc);assert(!pc_native_local_collect(read_word,0x3d3eb,&p));word(0x80001014,0x80004000);
    real(0x800010b4,NAN);assert(!pc_native_local_collect(read_word,0x3d3eb,&p));real(0x800010b4,10);
    real(0x4e794c,-1);assert(!pc_native_local_collect(read_word,0x3d3eb,&p));real(0x4e794c,0);
    assert(pc_native_local_collect(read_word,0x3d3eb,&p)&&p.delta==0);
    word(0x80005004,0xfffffffc);assert(pc_native_local_collect(read_word,0x3d3eb,&p)&&p.valid==13);
    assert(p.animation[1].model==0&&p.animation[1].time==0);word(0x80005004,0x80011000);
    real(0x800110dc,NAN);assert(pc_native_local_collect(read_word,0x3d3eb,&p)&&p.valid==13);
    assert(p.animation[1].time==0);real(0x800110dc,2.25f);
    real(0x80008334,NAN);assert(pc_native_local_collect(read_word,0x3d3eb,&p)&&p.valid==7);
    assert(p.camera_direction[0]==0&&p.camera_position[0]==0);
    assert(pc_native_local_address(0x83fff178,0xe88)&&!pc_native_local_address(0x83fff179,0xe88));
    assert(!pc_native_local_address(0x7fffffff,4)&&!pc_native_local_address(0x80000000,0x4000001));
}
int main(int argc,char **argv) {
    assert(argc==2);PcNativeLocalTrace trace={0};PcNativeLocalPose p;
    if(!strcmp(argv[1],"disabled")||!strcmp(argv[1],"failure")) {
        setup();if(pc_native_local_enabled(&trace))pc_native_local_collect(read_word,0x3d3eb,&p);
        assert(trace.checked&&!trace.file&&reads==0);assert(!pc_native_local_enabled(&trace));return 0;
    }
    checks();setup();assert(pc_native_local_enabled(&trace));
    if(!strcmp(argv[1],"bytes")) {
        trace.bytes=PC_NATIVE_LOCAL_BYTES-1;assert(pc_native_local_collect(read_word,0x3d3eb,&p));
        pc_native_local_record(&trace,&p,1);assert(!trace.file&&trace.rows==0);return 0;
    }
    unsigned limit=!strcmp(argv[1],"limit")?PC_NATIVE_LOCAL_LIMIT+5:2;
    for(unsigned i=0;i<limit;i++) {
        word(0x7497b8,123+i);real(0x800110dc,2.25f+i*.25f);
        assert(pc_native_local_collect(read_word,0x3d3eb,&p));pc_native_local_record(&trace,&p,i/240.);
    }
    assert(trace.rows==(limit==2?2:PC_NATIVE_LOCAL_LIMIT));
    if(limit==2){assert(trace.file);fclose(trace.file);trace.file=NULL;}
    else assert(!trace.file&&!pc_native_local_enabled(&trace));
    puts("PASS: completed local state, fractional/zero delta, fresh fields, optional validity and bounds");return 0;
}
