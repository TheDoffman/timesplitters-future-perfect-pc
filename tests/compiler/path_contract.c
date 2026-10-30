#include "guest_paths.h"
#include "guest_kernel_symlinks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"paths line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static int link_copy(void *p,const char *key,size_t n,char *out,size_t capacity,size_t *bytes) {
    CHECK(!p && n==6);return tsfp_kernel_symlink_copy(key,n,out,capacity,bytes)==TSFP_SYMLINK_OK;
}
static const tsfp_path_ops ops={NULL,link_copy};
static void expect(const char *input,unsigned root,unsigned category,const char *wanted,size_t directory) {
    char out[4096],windows[4096],check[4096];tsfp_path_plan plan,win;size_t i;
    CHECK(tsfp_path_prepare(input,strlen(input),0x454100af,'/',&ops,out,sizeof(out),&plan));
    CHECK(plan.root==root && plan.category==category && !strcmp(out,wanted) && plan.relative_bytes==strlen(wanted) && plan.directory_bytes==directory);
    CHECK(tsfp_path_prepare(input,strlen(input),0x454100af,'\\',&ops,windows,sizeof(windows),&win));
    memcpy(check,wanted,strlen(wanted)+1);for(i=0;check[i];++i)if(check[i]=='/')check[i]='\\';
    CHECK(!strcmp(windows,check) && win.root==root && win.category==category && win.directory_bytes==directory);
}
static void routes(void) {
    static const char *game[]={"D:\\pak\\x","d:/pak/x","Y:\\pak\\x","C:\\pak\\x","\\??\\D:\\pak\\x","\\Device\\CdRom0\\pak\\x","/device/cdrom0/pak/x","\\Device\\Harddisk0\\Partition1\\pak\\x","\\Device\\Harddisk0\\Partition2\\pak\\x"};
    unsigned i;char partition[80],relative[80];
    for(i=0;i<sizeof(game)/sizeof(game[0]);++i)expect(game[i],TSFP_PATH_GAME,TSFP_PATH_PLAIN,"pak/x",0);
    expect("\\Device\\CdRom0",TSFP_PATH_GAME,TSFP_PATH_PLAIN,"",0);
    expect("\\Device\\CdRom0\\",TSFP_PATH_GAME,TSFP_PATH_PLAIN,"",0);
    expect("T:\\a",TSFP_PATH_SAVE,TSFP_PATH_TITLE,"TitleData/a",9);
    expect("\\??\\t:\\a",TSFP_PATH_SAVE,TSFP_PATH_TITLE,"TitleData/a",9);
    expect("U:\\a",TSFP_PATH_PROFILE,TSFP_PATH_USER,"UserData/a",8);
    expect("\\??\\u:\\a",TSFP_PATH_PROFILE,TSFP_PATH_USER,"UserData/a",8);
    expect("\\Device\\Harddisk0\\Partition1\\udata\\a",TSFP_PATH_PROFILE,TSFP_PATH_USER,"UserData/a",8);
    expect("\\Device\\Harddisk0\\Partition1\\TDATA",TSFP_PATH_SAVE,TSFP_PATH_TITLE,"TitleData",9);
    expect("\\Device\\Harddisk0\\Partition1\\UDATA_extra\\a",TSFP_PATH_GAME,TSFP_PATH_PLAIN,"UDATA_extra/a",0);
    expect("Z:\\a",TSFP_PATH_SAVE,TSFP_PATH_CACHE,"Cache/454100AF/a",14);
    for(i=0;i<6;++i) {
        snprintf(partition,sizeof(partition),"\\Device\\Harddisk0\\Partition%u\\",i);snprintf(relative,sizeof(relative),"Partition%u.img",i);
        expect(partition,TSFP_PATH_SAVE,TSFP_PATH_PARTITION,relative,0);
        if(i>=3) {snprintf(partition,sizeof(partition),"\\Device\\Harddisk0\\Partition%u\\a",i);expect(partition,TSFP_PATH_SAVE,TSFP_PATH_CACHE,"Cache/454100AF/a",14);}
    }
    expect("\\Device\\Harddisk0\\Partition10\\a",TSFP_PATH_PASSTHROUGH,TSFP_PATH_PLAIN,"/Device/Harddisk0/Partition10/a",0);
    expect("\\Device\\CdRom0_extra\\a",TSFP_PATH_PASSTHROUGH,TSFP_PATH_PLAIN,"/Device/CdRom0_extra/a",0);
    expect("\\Dev\\CdRom0\\a",TSFP_PATH_PASSTHROUGH,TSFP_PATH_PLAIN,"/Dev/CdRom0/a",0);
    expect("relative\\a",TSFP_PATH_PASSTHROUGH,TSFP_PATH_PLAIN,"relative/a",0);
    expect("D:relative",TSFP_PATH_PASSTHROUGH,TSFP_PATH_PLAIN,"D:relative",0);
    expect("D:\\..\\a",TSFP_PATH_GAME,TSFP_PATH_PLAIN,"../a",0);
    expect("",TSFP_PATH_PASSTHROUGH,TSFP_PATH_PLAIN,"",0);
}
static void linked(void) {
    const char key[]="\\??\\P:",drive_key[]="\\??\\D:";const char target[]="\\Device\\Harddisk0\\Partition1\\UDATA";
    char huge[4096];memset(huge,'x',sizeof(huge));
    CHECK(tsfp_kernel_symlink_create(key,6,target,sizeof(target)-1)==TSFP_SYMLINK_OK);
    expect("p:\\45410066\\profile",TSFP_PATH_PROFILE,TSFP_PATH_USER,"UserData/45410066/profile",8);
    CHECK(tsfp_kernel_symlink_remove(key,6)==TSFP_SYMLINK_OK);
    CHECK(tsfp_kernel_symlink_create(key,6,"\\Unknown",8)==TSFP_SYMLINK_OK);
    expect("P:\\a",TSFP_PATH_PASSTHROUGH,TSFP_PATH_PLAIN,"P:/a",0);CHECK(tsfp_kernel_symlink_remove(key,6)==TSFP_SYMLINK_OK);
    CHECK(tsfp_kernel_symlink_create(key,6,"D:\\",3)==TSFP_SYMLINK_OK);
    CHECK(tsfp_kernel_symlink_create(drive_key,6,target,sizeof(target)-1)==TSFP_SYMLINK_OK);
    expect("P:\\a",TSFP_PATH_GAME,TSFP_PATH_PLAIN,"a",0);
    CHECK(tsfp_kernel_symlink_remove(drive_key,6)==TSFP_SYMLINK_OK && tsfp_kernel_symlink_remove(key,6)==TSFP_SYMLINK_OK);
    CHECK(tsfp_kernel_symlink_create(key,6,"\\Device\0evil",12)==TSFP_SYMLINK_OK);
    expect("P:\\a",TSFP_PATH_PASSTHROUGH,TSFP_PATH_PLAIN,"P:/a",0);CHECK(tsfp_kernel_symlink_remove(key,6)==TSFP_SYMLINK_OK);
    CHECK(tsfp_kernel_symlink_create(key,6,huge,sizeof(huge))==TSFP_SYMLINK_OK);
    expect("P:\\a",TSFP_PATH_PASSTHROUGH,TSFP_PATH_PLAIN,"P:/a",0);CHECK(tsfp_kernel_symlink_remove(key,6)==TSFP_SYMLINK_OK);
}
static void bounds(void) {
    char out[4096],before[4096],long_name[4096];tsfp_path_plan plan,saved;size_t length=99;
    memset(out,0x77,sizeof(out));memcpy(before,out,sizeof(out));memset(&plan,0x66,sizeof(plan));memcpy(&saved,&plan,sizeof(plan));
    CHECK(!tsfp_path_prepare("U:\\a",4,0,'/',NULL,out,10,&plan));
    CHECK(!memcmp(out,before,sizeof(out)) && !memcmp(&plan,&saved,sizeof(plan)));
    memset(long_name,'a',sizeof(long_name));CHECK(!tsfp_path_prepare(long_name,sizeof(long_name),0,'/',NULL,out,sizeof(out),&plan));
    CHECK(!tsfp_path_prepare("a\0b",3,0,'/',NULL,out,sizeof(out),&plan));
    CHECK(!tsfp_path_prepare("a",1,0,'?',NULL,out,sizeof(out),&plan));
    CHECK(tsfp_path_prepare("Z:\\x",4,0,'/',NULL,out,sizeof(out),&plan) && !strcmp(out,"Cache/x") && plan.directory_bytes==5);
    CHECK(tsfp_path_join("/",1,"a",1,'/',out,sizeof(out),&length) && !strcmp(out,"/a"));
    CHECK(tsfp_path_join("C:\\",3,"",0,'\\',out,sizeof(out),&length) && !strcmp(out,"C:\\"));
    CHECK(tsfp_path_join("C:\\",3,"a",1,'\\',out,sizeof(out),&length) && !strcmp(out,"C:\\a"));
    CHECK(tsfp_path_join("/base///",8,"file",4,'/',out,sizeof(out),&length) && !strcmp(out,"/base/file"));
    memcpy(before,out,sizeof(out));length=99;CHECK(!tsfp_path_join("/base",5,"file",4,'/',out,10,&length));
    CHECK(length==99 && !memcmp(before,out,sizeof(out)));
}
int main(void) {routes();linked();bounds();puts("PASS: shared game/save/profile/cache/partition routes, exact components, bounded link snapshots, host separators and unchanged output on failure");return 0;}
