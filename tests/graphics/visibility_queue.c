#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define TSFP_PC_CONTROLS 1
static int deferred,events[16],count,fake_accepted;static uint32_t fake_index;
static struct {void (*fn)(const void *);unsigned char bytes[32];size_t size;} queue[8];
static unsigned queued;
static int hle_d3d8_defer_recording(void){return deferred;}
static void hle_d3d8_defer_op(void (*fn)(const void *),const void *arg,size_t size) {
    assert(queued<8 && size<=32);queue[queued].fn=fn;queue[queued].size=size;
    if(size)memcpy(queue[queued].bytes,arg,size);
    queued++;
}
int xbox_D3D8VisibilityBegin(void){events[count++]=1;return 1;}
void xbox_D3D8VisibilityEnd(uint32_t i,int a){events[count++]=3;fake_index=i;fake_accepted=a;}
#include "graphics/visibility_queue.inc"
static void draw(const void *arg){(void)arg;events[count++]=2;}
int main(void) {
    host_VisibilityBegin();draw(NULL);host_VisibilityEnd(7,1);
    assert(count==3 && events[0]==1 && events[1]==2 && events[2]==3 && fake_index==7 && fake_accepted);
    deferred=1;count=0;
    host_VisibilityBegin();hle_d3d8_defer_op(draw,NULL,0);host_VisibilityEnd(123,0);
    assert(!count && queued==3 && fake_index==7);
    /* End payload survives its original stack frame, exactly like draw data. */
    for(unsigned i=0;i<queued;i++)queue[i].fn(queue[i].size?queue[i].bytes:NULL);
    assert(count==3 && events[0]==1 && events[1]==2 && events[2]==3 && fake_index==123 && !fake_accepted);
    puts("immediate/deferred visibility preserves draw order and copied End payload");return 0;
}
