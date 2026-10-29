#include "guest_io_share.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"sharing line%d %s\n",__LINE__,#x);exit(1);}}while(0)
static uint32_t access(unsigned v) {return (v&1?1:0)|(v&2?2:0)|(v&4?0x10000:0);}
int main(void) {
    unsigned a,b,sa,sb,i;unsigned char f[5],g[5],s[9],before[9],old[5];uint32_t status;
    for(a=0;a<8;++a)for(b=0;b<8;++b)for(sa=0;sa<8;++sa)for(sb=0;sb<8;++sb) {
        int conflict=a && b && ((a&~sb) || (b&~sa));
        memset(f,0x81,sizeof(f));memset(g,0x81,sizeof(g));memset(s,0xa7,sizeof(s));
        CHECK(tsfp_io_share_set(f+1,3,s+1,7,access(a),sa)==0);memcpy(before,s,9);memcpy(old,g,5);
        status=tsfp_io_share_check(g+1,3,s+1,7,access(b),sb,0);
        CHECK(status==(conflict?TSFP_SHARE_CONFLICT:TSFP_SHARE_OK));CHECK(!memcmp(s,before,9));
        if(conflict)CHECK(!memcmp(g,old,5));
        status=tsfp_io_share_check(g+1,3,s+1,7,access(b),sb,1);
        CHECK(status==(conflict?TSFP_SHARE_CONFLICT:TSFP_SHARE_OK));
        if(!conflict) {CHECK(s[1]==(a!=0)+(b!=0));CHECK(tsfp_io_share_remove(g+1,3,s+1,7)==0);CHECK(!memcmp(s,before,9));}
        CHECK(tsfp_io_share_remove(f+1,3,s+1,7)==0);for(i=1;i<8;++i)CHECK(s[i]==0);
        CHECK(f[0]==0x81 && f[4]==0x81 && (f[3]&0x81)==0x81 && s[0]==0xa7 && s[8]==0xa7);
    }
    memset(s,255,sizeof(s));memset(f,0x81,sizeof(f));memcpy(before,s,9);memcpy(old,f,5);
    CHECK(tsfp_io_share_check(f+1,3,s+1,7,1,7,1)==TSFP_SHARE_BAD_ARGUMENT);CHECK(!memcmp(s,before,9) && !memcmp(f,old,5));
    CHECK(tsfp_io_share_check(f+1,3,s+1,7,1,7,0)==0);
    CHECK(tsfp_io_share_set(f+1,2,s+1,7,1,7)==TSFP_SHARE_BAD_ARGUMENT);
    CHECK(tsfp_io_share_set(f+1,3,s+1,6,1,7)==TSFP_SHARE_BAD_ARGUMENT);
    CHECK(tsfp_io_share_set(f+1,3,s+1,7,1,8)==TSFP_SHARE_BAD_ARGUMENT);
    CHECK(tsfp_io_share_set(s,3,s+1,7,1,7)==TSFP_SHARE_BAD_ARGUMENT);
    s[1]=0;s[2]=1;CHECK(tsfp_io_share_remove(f+1,3,s+1,7)==TSFP_SHARE_BAD_ARGUMENT);
    CHECK(tsfp_io_share_set(f+1,3,s+1,7,0x20,7)==0 && s[2]==1 && s[3]==0);
    CHECK(tsfp_io_share_set(f+1,3,s+1,7,4,7)==0 && s[2]==0 && s[3]==1);
    puts("PASS: all4096 sharing pairs, exact byte counters, check-only, removal, guards and saturation");return 0;
}
