#include "pc_terminal.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if(!(c)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c); return 1; } } while(0)
#define U(a) (*(uint32_t*)(guest+(a)))
#define F(a) (*(float*)(guest+(a)))
#define MOVE(x,y) pc_input_motion(&input,(x),(y),1000)
#define APPLY() pc_terminal_mouse(&input,1001,.1f,0x10000,offset,0)

int main(void)
{
    unsigned char *guest=calloc(1,0x800000);
    ptrdiff_t offset=(ptrdiff_t)guest;
    PcInput input={0};
    CHECK(guest);
    pc_input_focus(&input,1);pc_input_capture(&input,1);
    U(0x10000)=15;U(0x10004)=2;U(0x10114)=0x2000;
    U(0x76c27c+2*4)=0x20000;U(0x2007c)=0x21000;
    U(0x21014)=0x10000;U(0x21030)=0x22000;U(0x4e7944)=1;
    F(0x21038)=100;F(0x2103c)=100;F(0x10100)=34;F(0x10104)=-12;
    U(0x21020)=2000;
    MOVE(100,20);CHECK(APPLY()==1);
    CHECK(fabsf(F(0x21038)-110)<.001f && fabsf(F(0x2103c)-102)<.001f);
    CHECK(F(0x10100)==34 && F(0x10104)==-12 && U(0x21020)==0);
    CHECK(APPLY()==0 && F(0x21038)==110); /* no repeated motion */
    MOVE(1000000,-1000000);CHECK(APPLY()==1);
    CHECK(F(0x21038)==251 && F(0x2103c)==4);
    MOVE(-1000000,1000000);CHECK(APPLY()==1);
    CHECK(F(0x21038)==4 && F(0x2103c)==251);
    MOVE(10,10);CHECK(pc_terminal_mouse(&input,1200,.1f,0x10000,offset,0)==0);
    CHECK(!input.dx && !input.dy && F(0x21038)==4);
    /* Pause, scripted pointer and disabled-pointer descriptors consume motion
     * without moving either the pointer or the hidden gameplay view. */
    U(0x4e7944)=0;MOVE(10,10);CHECK(APPLY()==0 && !input.dx);U(0x4e7944)=1;
    U(0x21058)=1;MOVE(10,10);CHECK(APPLY()==0 && !input.dx);U(0x21058)=0;
    U(0x22014)=4;MOVE(10,10);CHECK(APPLY()==0 && !input.dx);U(0x22014)=0;
    input.focused=0;MOVE(10,10);CHECK(APPLY()==0);input.focused=1;
    input.captured=0;MOVE(10,10);CHECK(APPLY()==0);input.captured=1;
    /* Stale ownership and missing pointers must fail closed, not fall through
     * to player angles. A nonzero player ID is not the local input index. */
    U(0x21014)=0x10010;MOVE(10,10);CHECK(APPLY()==0 && !input.dx);U(0x21014)=0x10000;
    U(0x10004)=4;MOVE(10,10);CHECK(APPLY()==0);U(0x10004)=2;
    U(0x2007c)=0;MOVE(10,10);CHECK(APPLY()==0);U(0x2007c)=0x21000;
    F(0x21038)=NAN;MOVE(10,10);CHECK(APPLY()==0);F(0x21038)=4;
    CHECK(F(0x10100)==34 && F(0x10104)==-12);
    /* Exiting the terminal restores normal look without an accumulated jump. */
    U(0x10114)=2;CHECK(APPLY()==-1 && !input.dx && !input.dy);
    MOVE(10,20);CHECK(APPLY()==-1 && input.dx==10 && input.dy==20);
    CHECK(pc_input_look(&input,1001,.1f,0,1,(float*)(guest+0x10100),(float*)(guest+0x10104)));
    CHECK(F(0x10100)==33 && F(0x10104)==-14 && F(0x21038)==4);
    /* Left click confirms only inside a terminal. Held buttons cannot cross
     * either boundary, including when game input is polled repeatedly. */
    pc_input_terminal_context(&input,1,1);
    CHECK(!pc_input_terminal_click(&input,1));
    CHECK(pc_input_terminal_suppresses_left(&input,1));
    CHECK(!pc_input_terminal_click(&input,0));
    CHECK(pc_input_terminal_click(&input,1));
    CHECK(pc_input_terminal_click(&input,1));
    pc_input_terminal_context(&input,0,1);
    CHECK(!pc_input_terminal_click(&input,1));
    CHECK(pc_input_terminal_suppresses_left(&input,1));
    CHECK(!pc_input_terminal_suppresses_left(&input,0));
    CHECK(!pc_input_terminal_suppresses_left(&input,1));
    pc_input_terminal_context(&input,1,0);
    input.focused=0;CHECK(!pc_input_terminal_click(&input,1));input.focused=1;
    input.captured=0;CHECK(!pc_input_terminal_click(&input,1));input.captured=1;
    free(guest);
    puts("PASS: terminal mouse routing, bounds, ownership, pause, stale input and gameplay return");
    return 0;
}
