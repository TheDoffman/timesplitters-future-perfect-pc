#include "kernel.h"
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>

/* The linked runtime has guest dispatch references; this test runs no guest. */
void *recomp_lookup(ULONG address) { (void)address; abort(); }
void *recomp_lookup_manual(ULONG address) { (void)address; abort(); }
extern int xbox_set_profile_root_utf8(const char *);

static int expect(const char *guest, const char *base, const char *relative)
{
    WCHAR actual[MAX_PATH], wanted[MAX_PATH];
    char joined[MAX_PATH];
    if(relative[0])snprintf(joined, sizeof joined, "%s\\%s", base, relative);
    else snprintf(joined,sizeof joined,"%s",base);
    MultiByteToWideChar(CP_UTF8, 0, joined, -1, wanted, MAX_PATH);
    if (!xbox_translate_path(guest, actual, MAX_PATH) || _wcsicmp(actual, wanted)) {
        fprintf(stderr, "FAIL %s\n  got: %ls\n  wanted: %ls\n", guest, actual, wanted);
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    char game[MAX_PATH], saveA[MAX_PATH], saveB[MAX_PATH];
    int failures = 0;
    if (argc != 2) return 2;
    snprintf(game, sizeof game, "%s\\game", argv[1]);
    snprintf(saveA, sizeof saveA, "%s\\save-a", argv[1]);
    snprintf(saveB, sizeof saveB, "%s\\save-b", argv[1]);
    xbox_path_init(game, saveA);
    {
        const char link[]="\\??\\Z:",target[]="\\Device\\Harddisk0\\Partition1\\UDATA";
        if(tsfp_kernel_symlink_create(link,sizeof(link)-1,target,sizeof(target)-1)!=TSFP_SYMLINK_OK)return 1;
        failures += expect("z:\\45410066\\profile",saveA,"UserData\\45410066\\profile");
        failures += expect("\\??\\Z:\\45410066\\profile",saveA,"UserData\\45410066\\profile");
        if(tsfp_kernel_symlink_remove(link,sizeof(link)-1)!=TSFP_SYMLINK_OK)return 1;
        if(tsfp_kernel_symlink_remove(link,sizeof(link)-1)!=TSFP_SYMLINK_MISSING)return 1;
    }
    failures += expect("\\Device\\Harddisk0\\partition1\\UDATA", saveA, "UserData");
    failures += expect("\\Device\\Harddisk0\\partition1\\UDATA\\45410066\\profile", saveA, "UserData\\45410066\\profile");
    failures += expect("\\Device\\Harddisk0\\partition1\\TDATA\\45410066\\cache", saveA, "TitleData\\45410066\\cache");
    failures += expect("\\Device\\Harddisk0\\partition1\\TDATA", saveA, "TitleData");
    failures += expect("U:\\45410066\\profile", saveA, "UserData\\45410066\\profile");
    failures += expect("T:\\45410066\\cache", saveA, "TitleData\\45410066\\cache");
    failures += expect("D:\\pak\\test.pak", game, "pak\\test.pak");
    failures += expect("\\Device\\CdRom0", game, "");
    failures += expect("\\Device\\Harddisk0\\Partition2\\dashboard.xip",game,"dashboard.xip");
    failures += expect("\\Device\\Harddisk0\\partition1\\default.xbe", game, "default.xbe");
    failures += expect("\\Device\\Harddisk0\\partition1\\UDATA_extra\\asset", game, "UDATA_extra\\asset");
    xbox_path_init(game, saveB);
    failures += expect("\\Device\\Harddisk0\\partition1\\udata\\45410066\\profile", saveB, "UserData\\45410066\\profile");
    if(!xbox_set_profile_root_utf8(saveA))return 1;
    failures += expect("U:\\45410066\\profile",saveA,"UserData\\45410066\\profile");
    failures += expect("\\Device\\Harddisk0\\partition1\\UDATA\\45410066\\profile",saveA,"UserData\\45410066\\profile");
    failures += expect("T:\\45410066\\cache",saveB,"TitleData\\45410066\\cache");
    failures += expect("\\Device\\Harddisk0\\partition0",saveB,"Partition0.img");
    failures += expect("D:\\pak\\test.pak",game,"pak\\test.pak");
    if(xbox_set_profile_root_utf8("missing-relative-folder"))return 1;
    failures += expect("U:\\45410066\\profile",saveA,"UserData\\45410066\\profile");
    if(!xbox_set_profile_root_utf8(""))return 1;
    failures += expect("U:\\45410066\\profile",saveB,"UserData\\45410066\\profile");
    {
        WCHAR small[6],before[6];
        for(unsigned i=0;i<6;++i)small[i]=(WCHAR)0x7777;
        memcpy(before,small,sizeof small);
        if(xbox_translate_path("D:\\pak\\test.pak",small,6) || memcmp(before,small,sizeof small))return 1;
    }
    if (failures) return 1;
    puts("PASS: save/asset routes, copied symbolic-link drive remapping, component boundaries, case folding, and save-directory switching");
    return 0;
}
