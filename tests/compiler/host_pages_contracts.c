#include "host_pages.h"
#include "host_blocks.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void *map(void *context,size_t bytes,uint32_t protection) {
    (void)context;return tsfp_host_pages_map(bytes,protection);
}
static int unmap(void *context,void *address,size_t bytes) {
    (void)context;return tsfp_host_pages_unmap(address,bytes);
}
int main(void) {
    size_t page=tsfp_host_page_size();
    tsfp_host_blocks owner={0};
    tsfp_host_block records[8],block;
    tsfp_host_block_backend backend={NULL,map,unmap};
    tsfp_host_page_region region,untouched;
    assert(page && !(page&(page-1)));
    assert(!tsfp_host_pages_map(0,4));
    assert(!tsfp_host_pages_map(page-1,4));
    assert(!tsfp_host_pages_map(page,3));
    assert(tsfp_host_blocks_init(&owner,&backend,page,records,8));
    for(size_t alignment=page;alignment<=page*64;alignment*=2) {
        unsigned char *p=tsfp_host_block_allocate(&owner,page*3,alignment,4,1);
        assert(p && !((uintptr_t)p&(alignment-1)));
        assert((uintptr_t)p>UINT32_MAX);
        assert(tsfp_host_block_find(&owner,p,&block));
        for(size_t i=0;i<page*3;++i)assert(p[i]==0);
        memset(p,0x5a,page*3);
        assert(tsfp_host_pages_query(p+7,&region) && region.protection==4);
        assert(region.base<=(uintptr_t)p && region.bytes>=(uintptr_t)p-region.base+page*3);
        /* Unaligned byte spans protect whole host pages, and query remains
         * usable while the target cannot be read. Neighboring pages stay RW. */
        assert(tsfp_host_pages_protect(p+page+7,19,1));
        assert(tsfp_host_pages_query(p+page+7,&region) && region.protection==1);
        assert(tsfp_host_pages_query(p+7,&region) && region.protection==4);
        assert(tsfp_host_pages_query(p+2*page+7,&region) && region.protection==4);
        assert(tsfp_host_pages_protect(p+page+7,19,2));
        assert(tsfp_host_pages_query(p+page+7,&region) && region.protection==2);
        assert(p[page+7]==0x5a);
        assert(tsfp_host_pages_protect(p+page+7,19,4));
        p[page+7]=0x19;
        assert(!tsfp_host_pages_protect(p,0,1));
        assert(!tsfp_host_pages_protect(p,SIZE_MAX,1));
        assert(!tsfp_host_pages_protect(p,1,3));
        assert(tsfp_host_pages_query(p,&region) && region.protection==4);
        assert(!tsfp_host_block_release(&owner,p+1,1));
        assert(tsfp_host_block_release(&owner,p,1));
        memset(&untouched,0x35,sizeof(untouched));region=untouched;
        assert(!tsfp_host_pages_query(block.mapping,&region));
        assert(!memcmp(&region,&untouched,sizeof(region)));
    }
    assert(!tsfp_host_pages_unmap(NULL,page));
    assert(!tsfp_host_pages_protect(NULL,page,4));
    assert(!tsfp_host_blocks_stop(&owner));
    puts("real host pages and native ownership contracts passed");return 0;
}
