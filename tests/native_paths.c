/* Read/write routing under one isolated root, never personal settings or saves. */
#include "kernel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

void *recomp_lookup(ULONG address) { (void)address; abort(); }
void *recomp_lookup_manual(ULONG address) { (void)address; abort(); }
extern int xbox_set_profile_root_utf8(const char *);
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)
static int expect(const char *guest, const char *base, const char *relative)
{
    char actual[MAX_PATH], wanted[MAX_PATH];
    snprintf(wanted, sizeof wanted, "%s/%s", base, relative);
    if (!xbox_translate_path(guest, actual, MAX_PATH) || strcmp(actual, wanted)) {
        fprintf(stderr, "FAIL %s -> %s; wanted %s\n", guest, actual, wanted); return 1;
    }
    return 0;
}
int main(int argc, char **argv)
{
    CHECK(argc == 2);
    char game[MAX_PATH], save[MAX_PATH], other[MAX_PATH], path[MAX_PATH];
    snprintf(game, sizeof game, "%s/game", argv[1]);
    snprintf(save, sizeof save, "%s/save-a", argv[1]);
    snprintf(other, sizeof other, "%s/save-b", argv[1]);
    xbox_path_init(game, save);
    CHECK(!expect("\\Device\\Harddisk0\\partition1\\UDATA\\45410066\\profile", save, "UserData/45410066/profile"));
    CHECK(!expect("\\Device\\Harddisk0\\partition1\\TDATA", save, "TitleData"));
    CHECK(!expect("U:\\45410066\\profile", save, "UserData/45410066/profile"));
    CHECK(!expect("D:\\pak\\test.pak", game, "pak/test.pak"));
    CHECK(!expect("\\Device\\Harddisk0\\partition1\\UDATA_extra\\asset", game, "UDATA_extra/asset"));
    CHECK(!expect("\\Device\\Harddisk0\\partition0", save, "Partition0.img"));
    snprintf(path, sizeof path, "%s/Partition0.img", save);
    int fd = open(path, O_RDWR); CHECK(fd >= 0);
    char magic[16]; CHECK(pread(fd, magic, 16, 0x800) == 16);
    CHECK(memcmp(magic, "****PARTINFO****", 16) == 0);
    char marker = 0x5a; CHECK(pwrite(fd, &marker, 1, 16) == 1); close(fd);
    xbox_path_init(game, save); /* Existing image contents must survive initialization. */
    fd = open(path, O_RDONLY); CHECK(fd >= 0);
    marker = 0; CHECK(pread(fd, &marker, 1, 16) == 1 && marker == 0x5a); close(fd);
    snprintf(path, sizeof path, "%s/Partition1.img", save);
    struct stat st; CHECK(stat(path, &st) == 0);
    CHECK(st.st_size == (off_t)0x465400 * 512 && (uint64_t)st.st_blocks * 512 < 1024 * 1024);
    xbox_path_init(game, other);
    CHECK(!expect("\\Device\\Harddisk0\\partition1\\udata\\45410066\\profile", other, "UserData/45410066/profile"));
    CHECK(xbox_set_profile_root_utf8(save));
    CHECK(!expect("U:\\45410066\\profile",save,"UserData/45410066/profile"));
    CHECK(!expect("\\Device\\Harddisk0\\partition1\\UDATA\\45410066\\profile",save,"UserData/45410066/profile"));
    CHECK(!expect("T:\\45410066\\cache",other,"TitleData/45410066/cache"));
    CHECK(!expect("\\Device\\Harddisk0\\partition0",other,"Partition0.img"));
    CHECK(!expect("D:\\pak\\test.pak",game,"pak/test.pak"));
    CHECK(!xbox_set_profile_root_utf8("missing-relative-folder"));
    CHECK(!expect("U:\\45410066\\profile",save,"UserData/45410066/profile"));
    CHECK(xbox_set_profile_root_utf8(""));
    CHECK(!expect("U:\\45410066\\profile",other,"UserData/45410066/profile"));
    puts("PASS: native save/asset paths, partition table, sparse sizing and preservation, save-root switching");
    return 0;
}
