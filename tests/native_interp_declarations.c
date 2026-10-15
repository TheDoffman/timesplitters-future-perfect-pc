/* Compile the actual shared recorder with a mock renderer/operation queue.
 * Verify layouts at draw boundaries, including frame-start and immediate-mode
 * changes, copied payloads, rejected updates and final state restoration. */
#include "hle/hle_d3d8_record.c"
#include <assert.h>
static D3D8VshInput layouts[2][16];
static int counts[2];
static struct { void (*fn)(const void *); unsigned char data[512]; } calls[32];
static int ncalls;
int hle_d3d8_interp_rec;
void hle_d3d8_interp_op(void (*fn)(const void *),const void *arg,size_t size) {
 assert(ncalls<32 && size<=512);calls[ncalls].fn=fn;memcpy(calls[ncalls++].data,arg,size);
}
HRESULT d3d8_vsh_set_declaration(DWORD h,const D3D8VshInput *in,int n) {
 if(h<0x10000 || h>=0x10002 || n<0 || n>16 || (n&&!in))return E_INVALIDARG;
 counts[h-0x10000]=n;if(n)memcpy(layouts[h-0x10000],in,n*sizeof *in);return S_OK;
}
BOOL d3d8_vsh_get_slot(int slot,DWORD *h,const DWORD **m,int *len,const D3D8VshInput **in,int *n) {
 if(slot<0||slot>1)return FALSE;
 if(h)*h=0x10000+slot;if(in)*in=layouts[slot];if(n)*n=counts[slot];return TRUE;
}
int d3d8cap_chunk(D3D8CapWriter *w,uint32_t t,const void *p0,size_t n0,const void *p1,size_t n1,const void *p2,size_t n2){return 1;}
static void verify(const void *arg) {
 const D3D8VshInput *expected=arg;assert(counts[0]==1);assert(!memcmp(layouts[0],expected,sizeof *expected));
}
int main(void) {
 D3D8VshInput first={0,RHI_FORMAT_R32G32B32_FLOAT,0},second={0,RHI_FORMAT_R32G32B32A32_FLOAT,32};
 host_vsh_set_declaration(0x10000,&first,1);
 hle_d3d8_interp_rec=1;snapshot_declarations();
 hle_d3d8_interp_op(verify,&first,sizeof first);
 host_vsh_set_declaration(0x10000,&second,1);
 hle_d3d8_interp_op(verify,&second,sizeof second);
 host_vsh_set_declaration(0x10000,&first,1);
 int saved=ncalls;
 assert(FAILED(host_vsh_set_declaration(0x10000,&first,17)) && ncalls==saved);
 /* Replay starts with a wrong final layout; snapshot and per-draw changes
    must restore the correct layout before each check, without recording. */
 d3d8_vsh_set_declaration(0x10000,&second,1);
 first.offset=99;second.offset=99;hle_d3d8_interp_rec=0;
 for(int i=0;i<saved;i++)calls[i].fn(calls[i].data);
 assert(ncalls==saved && layouts[0][0].offset==0);
 puts("interpolation declaration snapshot/change/copy/restore: PASS");
}
