#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/gameplay/pc_native_character_trace.h"
static unsigned char memory[0x800000];
static uint32_t denied;
#define WORD(a) (*(uint32_t *)(memory+(a)))
#define FLOAT(a) (*(float *)(memory+(a)))
static int read_guest(uint32_t a,void *out,size_t n)
{
    if(a==denied || a>=sizeof memory || n>sizeof memory-a)return 0;
    memcpy(out,memory+a,n);return 1;
}
int main(void)
{
    enum { STATE=0x10000,MODEL=0x11000,MATRIX=0x12000 };
    WORD(STATE+4)=1;WORD(STATE+0x54)=3;WORD(STATE+0x60)=MODEL;WORD(MODEL+0x18)=MATRIX;
    FLOAT(STATE+0x5c)=.5f;FLOAT(0x4e7948)=.25f;FLOAT(0x4e794c)=.125f;
    FLOAT(0x478860)=3;FLOAT(0x47d6d4)=-.1f;WORD(0x7497b8)=9;WORD(0x79094c)=56;
    for(unsigned i=0;i<16;i++)FLOAT(MATRIX+4*i)=i%5==0?1:0;
    PcCharacterSnapshot before,after;
    assert(pc_character_collect(read_guest,STATE,&before)==1 && before.frame==9 && before.scene==56);
    assert(before.age==.5f && before.raw==.25f && before.game==.125f && before.values[0]==1);
    FLOAT(MATRIX)=2;assert(pc_character_collect(read_guest,STATE,&after)==1);
    assert(before.values[0]==1 && after.values[0]==2); /* copied, not aliases */
    PcCharacterTrace trace={0};trace.checked=1;trace.file=tmpfile();assert(trace.file);
    pc_character_record(&trace,&before,&after,1,1);assert(trace.rows==1 && trace.bytes>0);
    unsigned bytes=trace.bytes;
    after.frame++;pc_character_record(&trace,&before,&after,1,1);assert(trace.rows==1);after.frame--;
    after.scene++;pc_character_record(&trace,&before,&after,1,1);assert(trace.rows==1);after.scene--;
    after.model++;pc_character_record(&trace,&before,&after,1,1);assert(trace.rows==1);after.model--;
    after.matrix++;pc_character_record(&trace,&before,&after,1,1);assert(trace.rows==1);after.matrix--;
    after.state++;pc_character_record(&trace,&before,&after,1,1);assert(trace.rows==1);after.state--;
    pc_character_record(&trace,&before,&after,0,1);assert(trace.bytes==bytes);
    WORD(STATE+4)=0;WORD(STATE+0x60)=0;
    assert(pc_character_collect(read_guest,STATE,&after)==2);
    pc_character_record(&trace,&before,&after,2,0);assert(trace.rows==2);
    fclose(trace.file);trace.file=tmpfile();assert(trace.file);trace.rows=PC_CHARACTER_ROWS;
    pc_character_record(&trace,&before,&after,2,0);assert(!trace.file && trace.rows==PC_CHARACTER_ROWS);
    trace.rows=0;trace.bytes=PC_CHARACTER_BYTES-1;trace.file=tmpfile();assert(trace.file);
    pc_character_record(&trace,&before,&after,2,0);assert(!trace.file && !trace.rows);
    WORD(STATE+4)=1;WORD(STATE+0x60)=MODEL;WORD(STATE+0x54)=4;
    assert(!pc_character_collect(read_guest,STATE,&after));WORD(STATE+0x54)=3;
    denied=MATRIX;assert(!pc_character_collect(read_guest,STATE,&after));denied=0;
    FLOAT(MATRIX)=NAN;assert(!pc_character_collect(read_guest,STATE,&after));FLOAT(MATRIX)=1;
    WORD(MODEL+0x18)=0x83fffff0;assert(!pc_character_collect(read_guest,STATE,&after));
    assert(!pc_character_collect(read_guest,0xfffffff0,&after));
    PcCharacterTrace disabled={0};disabled.checked=1;assert(!pc_character_enabled(&disabled));
    puts("PASS: coherent copied character pairs, removal, identity/frame guards, unreadable/invalid values and bounded CSV");
}
