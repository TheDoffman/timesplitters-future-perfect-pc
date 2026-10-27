/* Asset-free diagnostic: preserve the difference between generated/selected
 * contacts, reject unreadable data, and stop output at explicit bounds. */
#include <assert.h>
#include "../src/gameplay/pc_native_contact_trace.h"
static unsigned char low[0x800000],heap[0x10000];
static unsigned reads;
static uint32_t fail_address;
#define POOL 0x80001000u
#define BODY (POOL+2u*0x200u)
#define TABLE 0x80002000u
#define PROP 0x80003000u
#define ARENA 0x80004000u
#define LAST (ARENA+499u*0x38u)
#define PREVIOUS (LAST-0x38u)
#define ARRAY 0x8000c000u
#define STACK 0x10000u
static void *memory(uint32_t a,size_t n)
{
    if(a<sizeof low && n<=sizeof low-a)return low+a;
    if(a>=0x80000000u && a-0x80000000u<sizeof heap && n<=sizeof heap-(a-0x80000000u))return heap+a-0x80000000u;
    return NULL;
}
static int read_bytes(uint32_t a,void *out,size_t n)
{
    reads++;
    void *p=memory(a,n);
    if(a==fail_address || !p)return 0;
    memcpy(out,p,n);return 1;
}
static void word(uint32_t a,uint32_t v){void *p=memory(a,4);assert(p);memcpy(p,&v,4);}
static void number(uint32_t a,float v){void *p=memory(a,4);assert(p);memcpy(p,&v,4);}
static void setup(void)
{
    assert(sizeof(PcContactRecord)==0x38u);
    word(0x74c458,POOL);word(0x74c46c,TABLE);word(TABLE+8,PROP);
    word(BODY+0x1e4,0x10002);word(PROP+0x80,0x10002);word(PROP+0x20,0x400000);
    number(BODY+0x148,65);number(BODY+0x154,12);number(BODY+0x10c,1);
    word(0x7497b8,123);number(0x4e794c,.5f);
    word(0x74c468,ARENA);word(0x4e826c,498);
    word(STACK,0x172119);word(STACK+4,ARRAY);word(STACK+8,1);
    number(STACK+12,1.f/120.f);number(STACK+16,1.f/960.f);
    word(ARRAY,LAST);
    PcContactRecord record={0};record.owner=PROP;record.kind=2;record.normal[2]=-1;
    record.depth=.025f;record.point[2]=67;record.flags=4;
    memcpy(memory(LAST,sizeof record),&record,sizeof record);
    record.normal[2]=1;record.next=LAST;
    memcpy(memory(PREVIOUS,sizeof record),&record,sizeof record);
}
static void collector(void)
{
    PcContactTrace filter={0};PcContactSnapshot s;
    assert(pc_contact_options(&filter,"2","60,80"));
    assert(pc_contact_collect(read_bytes,STACK,&filter,&s));
    assert(s.frame==123 && s.prop==PROP && s.body==BODY && s.count==3);
    assert(s.raw_count==2 && s.selected_count==1 && s.position[2]==65 && s.velocity[2]==12);
    assert(s.contacts[0].address==PREVIOUS && !s.contacts[0].selected);
    assert(s.contacts[1].address==LAST && !s.contacts[1].selected);
    assert(s.contacts[2].address==LAST && s.contacts[2].selected);
    assert(s.contacts[0].record.normal[2]==1 && s.contacts[2].record.normal[2]==-1);
    /* Full memory remains unchanged by repeated observations. */
    unsigned char before[sizeof heap];memcpy(before,heap,sizeof heap);
    for(unsigned i=0;i<3;i++)assert(pc_contact_collect(read_bytes,STACK,&filter,&s));
    assert(!memcmp(before,heap,sizeof heap));
    number(BODY+0x148,90);assert(!pc_contact_collect(read_bytes,STACK,&filter,&s));number(BODY+0x148,65);
    number(BODY+0x148,NAN);assert(!pc_contact_collect(read_bytes,STACK,&filter,&s));number(BODY+0x148,65);
    word(PROP+0x80,0x20002);assert(!pc_contact_collect(read_bytes,STACK,&filter,&s));word(PROP+0x80,0x10002);
    word(0x74c458,0xfffffff0);assert(!pc_contact_collect(read_bytes,STACK,&filter,&s));word(0x74c458,POOL);
    word(STACK+8,501);assert(!pc_contact_collect(read_bytes,STACK,&filter,&s));word(STACK+8,1);
    word(0x4e826c,501);assert(!pc_contact_collect(read_bytes,STACK,&filter,&s));word(0x4e826c,498);
    word(ARRAY,LAST+4);assert(!pc_contact_collect(read_bytes,STACK,&filter,&s));word(ARRAY,LAST);
    fail_address=LAST;assert(!pc_contact_collect(read_bytes,STACK,&filter,&s));fail_address=0;
    number(LAST+0xc,NAN);assert(!pc_contact_collect(read_bytes,STACK,&filter,&s));number(LAST+0xc,0);
    number(STACK+16,0);assert(!pc_contact_collect(read_bytes,STACK,&filter,&s));number(STACK+16,1.f/960.f);
    word(STACK,0x12345678);assert(!pc_contact_collect(read_bytes,STACK,&filter,&s));word(STACK,0x172119);
    word(0x4e826c,500);word(STACK+8,0);
    assert(pc_contact_collect(read_bytes,STACK,&filter,&s) && s.count==0 && s.raw_count==0 && s.selected_count==0);
    assert(!pc_contact_options(&filter,"-1","60,80"));
    assert(!pc_contact_options(&filter,"1024","60,80"));
    assert(!pc_contact_options(&filter,"2tail","60,80"));
    assert(!pc_contact_options(&filter,"2","80,60"));
    assert(!pc_contact_options(&filter,"2","nan,80"));
    assert(!pc_contact_options(&filter,"2","60,inf"));
    assert(!pc_contact_options(&filter,"2","60,80tail"));
}
int main(int argc,char **argv)
{
    setup();collector();setup();
    PcContactTrace trace={0};PcContactSnapshot s;
    if(argc>1 && (!strcmp(argv[1],"disabled") || !strcmp(argv[1],"invalid"))) {
        unsigned previous=reads;
        for(unsigned i=0;i<10;i++)assert(!pc_contact_enabled(&trace));
        assert(reads==previous && trace.checked && !trace.file);return 0;
    }
    assert(pc_contact_enabled(&trace));
    assert(pc_contact_collect(read_bytes,STACK,&trace,&s));
    pc_contact_record(&trace,&s,1);assert(trace.rows==4);
    if(argc>1 && !strcmp(argv[1],"bytes"))trace.bytes=PC_CONTACT_BYTES-1;
    for(unsigned i=0;i<PC_CONTACT_ROWS+10;i++)pc_contact_record(&trace,&s,1);
    assert(!trace.file && !pc_contact_enabled(&trace));
    if(argc>1 && !strcmp(argv[1],"bytes"))assert(trace.rows==4);
    else assert(trace.rows==PC_CONTACT_ROWS && trace.bytes<=PC_CONTACT_BYTES);
    puts("PASS: synchronous contact lists, identity, unreadable/invalid data, zero contacts and bounded writer");return 0;
}
