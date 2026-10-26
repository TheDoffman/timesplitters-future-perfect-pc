/* Production cache lookup with immutable host shader copies. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
typedef uint32_t DWORD;
#define SLOT_PROGRAM_CACHE 96
static DWORD code[96][8];
static int lengths[96];
static unsigned hash_calls;
static int host_vsh_same_microcode(DWORD host, const DWORD *current, int count)
{
    return host < 96 && lengths[host] == count &&
        !memcmp(code[host], current, (size_t)count * 4 * sizeof(DWORD));
}
/* PRODUCTION_LOOKUP */
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL: %s at %d\n",#x,__LINE__); return 1; } } while(0)
int main(void)
{
    DWORD a[8]={1,2,3,4,5,6,7,8}, b[8]={9,10,11,12,13,14,15,16};
    uint32_t hash=0;
    memcpy(code[1],a,sizeof a); lengths[1]=2;
    memcpy(code[2],b,sizeof b); lengths[2]=2;
    g_slot_program_count=2;
    g_slot_programs[0]=(SlotProgram){0x1000,microcode_hash(a,2),2,1};
    g_slot_programs[1]=(SlotProgram){0x2000,microcode_hash(b,2),2,2};
    hash_calls=0;
    for (unsigned i=0;i<10000;i++) {
        CHECK(slot_program_lookup(0x1000,2,a,&hash)==0);
        CHECK(slot_program_lookup(0x2000,2,b,&hash)==1);
    }
    CHECK(hash_calls==0); /* Alternating meshes need no repeated byte hashing. */
    /* Reused address with different code resolves by current contents. */
    CHECK(slot_program_lookup(0x1000,2,b,&hash)==1);
    CHECK(hash_calls==1 && g_slot_programs[1].source==0x1000);
    a[7]^=0x80; /* Last word must invalidate, not just a prefix check. */
    CHECK(slot_program_lookup(0x1000,2,a,&hash)==-1);
    CHECK(hash==microcode_hash(a,2));
    a[7]^=0x80;
    CHECK(slot_program_lookup(0x1000,1,a,&hash)==-1); /* Same source, new length. */
    CHECK(slot_program_lookup(0x9000,2,a,&hash)==0); /* Identical content relocated. */
    CHECK(g_slot_programs[0].source==0x9000);
    /* A hash match is never sufficient: exercise a colliding cache entry. */
    g_slot_program_count=1;g_slot_programs[0].hash=microcode_hash(b,2);
    CHECK(slot_program_lookup(0x8000,2,b,&hash)==-1);
    g_slot_program_count=0;
    CHECK(slot_program_lookup(0x9000,2,a,&hash)==-1);
    puts("PASS: fresh full contents, reused addresses, lengths, relocation, collisions, empty cache and avoided hashing");
    return 0;
}
