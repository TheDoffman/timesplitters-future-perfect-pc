#include "pc_breadcrumbs.h"
#include <assert.h>
#include <string.h>
int main(void){
    PcBreadcrumbs b={0};int32_t game[6]={4,4,0,0,0,0};
    pc_crumb_game(&b,10,game);assert(pc_crumb_load(&b.next)==6);
    pc_crumb_game(&b,20,game);assert(pc_crumb_load(&b.next)==6);
    game[3]=1;pc_crumb_game(&b,30,game);assert(pc_crumb_load(&b.next)==7);
    for(int i=0;i<100;i++)pc_crumb_add(&b,40+i,PC_CRUMB_MODAL,i);
    assert(pc_crumb_load(&b.next)==107);
    /* A producer stopped during a write must not prevent reading older entries. */
    pc_crumb_store(&b.writer,1);pc_crumb_add(&b,999,PC_CRUMB_MODAL,999);
    assert(pc_crumb_load(&b.next)==107);
    pc_crumb_store(&b.slots[106%64].sequence,0);
    FILE *out=tmpfile();assert(out);pc_crumb_report(out,&b);fflush(out);
    long size=ftell(out);assert(size>0 && size<8192);rewind(out);
    char text[8192]={0};assert(fread(text,1,sizeof text-1,out)==(size_t)size);fclose(out);
    assert(strstr(text,"44 76 modal-depth 36\n"));
    assert(strstr(text,"106 138 modal-depth 98\n"));
    assert(!strstr(text,"107 139") && !strstr(text,"999"));
    unsigned lines=0;for(char *p=text;*p;p++)if(*p=='\n')lines++;
    assert(lines==65);puts("PASS bounded breadcrumb ordering, deduplication and interrupted-producer snapshot");
    PcBreadcrumbs models={0};pc_crumb_model(&models,50,0x811ee970u,15,123);
    assert(pc_crumb_load(&models.next)==3);
    assert(pc_crumb_load(&models.slots[0].value)==(int32_t)0x811ee970u);
    assert(pc_crumb_load(&models.slots[1].value)==15);
    assert(pc_crumb_load(&models.slots[2].value)==123);
    pc_crumb_store(&models.writer,1);pc_crumb_model(&models,60,0,0,0);
    assert(pc_crumb_load(&models.next)==3);
    PcBreadcrumbs match={0};int32_t values[4]={1,10,3,16};
    pc_crumb_match(&match,100,values);assert(pc_crumb_load(&match.next)==4);
    pc_crumb_match(&match,200,values);assert(pc_crumb_load(&match.next)==4);
    values[1]=9;pc_crumb_match(&match,300,values);assert(pc_crumb_load(&match.next)==5);
    assert(pc_crumb_load(&match.slots[4].event)==31 && pc_crumb_load(&match.slots[4].value)==9);
    return 0;
}
