#include "pc_storage.h"
#include <assert.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#include "pc_storage.inc"
static char routed[4096];
static int route_ok=1;
static int route_profile(const char *path)
{
    if(!route_ok)return 0;
    snprintf(routed,sizeof routed,"%s",path);return 1;
}
#endif
int main(void)
{
    PcStorageInfo info={"Games (D:)",53ull*1024*1024*1024,2ull*1024*1024*1024*1024,1};
    char out[160];
    assert(pc_storage_format("Xbox Hard Disk",&info,out,sizeof out));
    assert(!strcmp(out,"Games (D:)"));
    assert(pc_storage_format("Unable to save to Xbox Hard Disk",&info,out,sizeof out));
    assert(!strcmp(out,"Unable to save to Games (D:)"));
    assert(pc_storage_format("50000+ Free 50000+ Total",&info,out,sizeof out));
    assert(!strcmp(out,"53.0 GiB free / 2.0 TiB"));
    info.available=0;
    assert(pc_storage_format("0 Free 123 Total",&info,out,sizeof out));
    assert(!strcmp(out,"0 B free / 2.0 TiB"));
    info.known=0;info.label[0]=0;
    assert(pc_storage_format("Xbox Hard Disk",&info,out,sizeof out));
    assert(!strcmp(out,"Save storage"));
    assert(pc_storage_format("50000+ Free 50000+ Total",&info,out,sizeof out));
    assert(!strcmp(out,"Space unavailable"));
    assert(!pc_storage_format("Xbox Hard Disk",&info,out,2));
    assert(!pc_storage_format("123 Free 456 Total extra",&info,out,sizeof out));
    assert(!pc_storage_format(" Free 456 Total",&info,out,sizeof out));
    assert(!pc_storage_format("Score: 123 Total",&info,out,sizeof out));
#ifdef _WIN32
    char path[MAX_PATH];
    assert(GetCurrentDirectoryA(MAX_PATH,path));
    pc_storage_set_path(path);
    pc_storage_set_profile_router(route_profile);
    assert(pc_storage_rewrite("Xbox Hard Disk",out,sizeof out));
    assert(strstr(out,":)"));
    assert(pc_storage_rewrite("50000+ Free 50000+ Total",out,sizeof out));
    assert(storage_info.known&&storage_info.total>0&&storage_info.available<=storage_info.total);
    PcStorageDrive drives[PC_STORAGE_DRIVES];
    int count=pc_storage_drives(drives,PC_STORAGE_DRIVES),found=0,games=0;
    assert(count>0&&count<=PC_STORAGE_DRIVES);
    for(int i=0;i<count;i++){if(drives[i].current)found++;if(drives[i].game)games++;}
    assert(found==1&&games==1);
    char config[4096],selected[4096],error[160],profile[4096];
    snprintf(config,sizeof config,"%s/controls.json",path);
    _putenv_s("RECOMP_INPUT_CONFIG",config);
    assert(pc_storage_resolve_root(path,selected,sizeof selected)==0);
    PcStorageDrive fixture={0};
    snprintf(fixture.path,sizeof fixture.path,"%s",path);
    snprintf(fixture.target,sizeof fixture.target,"%s/alternate/Saves",path);
    _putenv_s("TSFP_SAVE_LOCATION_LOCK","1");
    assert(!pc_storage_choose(&fixture,error,sizeof error));
    assert(strstr(error,"locked"));
    _putenv_s("TSFP_SAVE_LOCATION_LOCK","");
    assert(pc_storage_choose(&fixture,error,sizeof error));
    assert(pc_storage_resolve_root(path,selected,sizeof selected)==1);
    assert(!strcmp(selected,fixture.target));
    assert(!strcmp(storage_path,fixture.target)&&!strcmp(routed,fixture.target));
    snprintf(profile,sizeof profile,"%s/UserData/45410066",fixture.target);
    assert(storage_directory(profile,0));
    snprintf(profile,sizeof profile,"%s/profile.dat",selected);
    FILE *saved=storage_fopen(profile,"wb");assert(saved);fputs("existing profile",saved);fclose(saved);
    assert(pc_storage_choose(&fixture,error,sizeof error));
    saved=storage_fopen(profile,"rb");assert(saved);char data[32]={0};assert(fread(data,1,16,saved)==16);fclose(saved);
    assert(!strcmp(data,"existing profile"));
    /* A rejected adapter must neither change the live root nor its preference. */
    PcStorageDrive other=fixture;
    snprintf(other.target,sizeof other.target,"%s/second/Saves",path);
    route_ok=0;
    assert(!pc_storage_choose(&other,error,sizeof error));
    assert(!strcmp(storage_path,fixture.target)&&!strcmp(routed,fixture.target));
    assert(pc_storage_resolve_root(path,selected,sizeof selected)==1&&!strcmp(selected,fixture.target));
    route_ok=1;
    /* Preference failure rolls routing back, leaving existing data intact. */
    assert(storage_preference(config,sizeof config));
    HANDLE held=CreateFileA(config,GENERIC_READ,0,NULL,OPEN_EXISTING,0,NULL);
    assert(held!=INVALID_HANDLE_VALUE);
    assert(!pc_storage_choose(&other,error,sizeof error));
    assert(!strcmp(storage_path,fixture.target)&&!strcmp(routed,fixture.target));
    CloseHandle(held);
    _putenv_s("TSFP_SAVE_LOCATION_LOCK","1");
    assert(pc_storage_resolve_root(path,selected,sizeof selected)==0);
    _putenv_s("TSFP_SAVE_LOCATION_LOCK","");
    assert(storage_preference(config,sizeof config));
    assert(storage_write_preference(config,"Z:/missing-tsfp-test/Saves"));
    assert(pc_storage_resolve_root(path,selected,sizeof selected)==-1);
    assert(storage_write_preference(config,"relative/path"));
    assert(pc_storage_resolve_root(path,selected,sizeof selected)==-1);
    pc_storage_set_path("Z:\\nonexistent-storage-test\\missing");
    assert(pc_storage_rewrite("0 Free 0 Total",out,sizeof out));
    assert(!strcmp(out,"Space unavailable"));
#endif
    puts("Storage menu formatting and host probe passed");
    return 0;
}
