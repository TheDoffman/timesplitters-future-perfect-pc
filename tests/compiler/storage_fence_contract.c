#include "guest_storage.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"fence line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void) {
    tsfp_storage_state guarded={0},failed={0},late={0};tsfp_storage_snapshot paths,out,before;
    char required[TSFP_PATH_LIMIT],old[TSFP_PATH_LIMIT],oversized[TSFP_PATH_LIMIT+1];unsigned source;
    memset(required,0x55,sizeof(required));memcpy(old,required,sizeof(old));
    CHECK(tsfp_storage_required(&guarded,required,sizeof(required))==0 && !memcmp(required,old,sizeof(old)));
    CHECK(tsfp_storage_profile(&guarded,"/personal-profile"));
    CHECK(tsfp_storage_require(&guarded,"/isolated"));
    CHECK(tsfp_storage_required(&guarded,required,2)==-1 && !memcmp(required,old,sizeof(old)));
    CHECK(tsfp_storage_required(&guarded,required,sizeof(required))==1 && !strcmp(required,"/isolated"));
    memset(&out,0x77,sizeof(out));before=out;CHECK(!tsfp_storage_read(&guarded,&out) && !memcmp(&out,&before,sizeof(out)));
    CHECK(tsfp_storage_select("/game","/personal",NULL,NULL,NULL,&paths,&source));
    CHECK(!tsfp_storage_commit(&guarded,&paths));
    CHECK(tsfp_storage_select("/game","/isolated",NULL,NULL,NULL,&paths,&source));
    CHECK(tsfp_storage_commit(&guarded,&paths) && tsfp_storage_read(&guarded,&out));
    CHECK(!strcmp(out.save,"/isolated") && !out.profile[0]);
    CHECK(!tsfp_storage_profile(&guarded,"/personal-profile") && !tsfp_storage_profile(&guarded,"/isolated/profile"));
    CHECK(tsfp_storage_profile(&guarded,""));
    CHECK(!tsfp_storage_require(&guarded,"/personal") && !tsfp_storage_require(&guarded,NULL));
    CHECK(tsfp_storage_read(&guarded,&out) && !strcmp(out.save,"/isolated") && !out.profile[0]);
    CHECK(!tsfp_storage_require(&failed,NULL));
    CHECK(tsfp_storage_required(&failed,required,sizeof(required))==-1);
    CHECK(!tsfp_storage_require(&failed,"/personal") && !tsfp_storage_commit(&failed,&paths));
    CHECK(!tsfp_storage_profile(&failed,"/profile") && tsfp_storage_profile(&failed,""));
    CHECK(tsfp_storage_commit(&late,&paths));
    CHECK(!tsfp_storage_require(&late,"/isolated"));
    out=before;CHECK(!tsfp_storage_read(&late,&out) && !memcmp(&out,&before,sizeof(out)));
    CHECK(tsfp_storage_required(&late,required,sizeof(required))==-1 && !tsfp_storage_commit(&late,&paths));
    memset(oversized,'x',TSFP_PATH_LIMIT);oversized[TSFP_PATH_LIMIT]=0;
    {tsfp_storage_state a={0},b={0};
     CHECK(!tsfp_storage_require(&a,"") && !tsfp_storage_require(&b,oversized));
     CHECK(tsfp_storage_required(&a,required,sizeof(required))==-1 && tsfp_storage_required(&b,required,sizeof(required))==-1);}
    puts("PASS: one-way root fence, no profile escape, closed invalid/late requests and preserved outputs");return 0;
}
