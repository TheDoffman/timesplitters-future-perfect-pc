#include <stdint.h>
#include <string.h>
#include <math.h>
#include <assert.h>
static uint32_t memory[0x10000/4];
#define WORD_AT(a) memory[(a)/4]
#define FLOAT_AT(a) (*(float *)&WORD_AT(a))
static uint32_t bits(float f){uint32_t u;memcpy(&u,&f,4);return u;}
static int guest_writable(uint32_t a,unsigned n){return a>=0x1000&&a<sizeof memory&&n<=sizeof memory-a;}
#include "../../src/interface/pc_settings_spacing.inc"
int main(void)
{
    const unsigned menu=0x1000,header=0x2000,divider=0x2200,body=0x2400,footer=0x2600;
    const float heights[]={47.5f,53.32f,56.61f,64.61f};
    for(unsigned i=0;i<4;i++) {
        memset(memory,0,sizeof memory);
        WORD_AT(menu+0x68)=bits(heights[i]);WORD_AT(menu+0x178)=header;
        WORD_AT(header+4)=divider;WORD_AT(header+0x68)=bits(2.f);WORD_AT(header+0x70)=bits(6.f);
        WORD_AT(divider)=9;WORD_AT(divider+4)=body;WORD_AT(divider+0x68)=bits(8.f);WORD_AT(divider+0x70)=bits(3.f);
        WORD_AT(body+4)=footer;WORD_AT(body+0x68)=bits(11.f);WORD_AT(body+0x70)=bits(6.f);WORD_AT(body+0xb0)=bits(6.f);
        WORD_AT(footer+0x68)=bits(heights[i]-8.f);WORD_AT(footer+0x70)=bits(6.f);WORD_AT(footer+0xb0)=bits(6.f);
        WORD_AT(body+0x14)=123;WORD_AT(body+0x6c)=bits(78.f);WORD_AT(body+0x54)=0xcafe;
        settings_panel_anchor(menu);
        assert(fabsf(FLOAT_AT(menu+0x64)-(heights[i]<=60.f?24.f:84.f-heights[i]))<.0001f);
        assert(FLOAT_AT(menu+0x68)==heights[i]);
        assert(FLOAT_AT(menu+0x64)+FLOAT_AT(menu+0x68)<=84.f);
        assert(FLOAT_AT(header+0x68)==2.f&&FLOAT_AT(header+0x70)==6.f);
        assert(FLOAT_AT(divider+0x70)==3.f); /* Preserve tab/divider spacing. */
        assert(FLOAT_AT(footer+0x68)+FLOAT_AT(footer+0x70)<=FLOAT_AT(menu+0x68));
        assert(WORD_AT(body+0x14)==123&&WORD_AT(body+0x54)==0xcafe&&FLOAT_AT(body+0x6c)==78.f);
        uint32_t saved[sizeof memory/4];memcpy(saved,memory,sizeof memory);
        settings_panel_anchor(menu);assert(!memcmp(saved,memory,sizeof memory));
    }
    return 0;
}
