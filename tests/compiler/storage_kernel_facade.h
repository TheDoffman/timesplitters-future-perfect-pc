#ifndef TSFP_STORAGE_FIXTURE_KERNEL_H
#define TSFP_STORAGE_FIXTURE_KERNEL_H
#include <stdint.h>
#include <stddef.h>
#include <wchar.h>
#ifdef _WIN32
#include <windows.h>
typedef WCHAR xbox_host_char;
#else
typedef int BOOL;
typedef uint32_t DWORD;
typedef char xbox_host_char;
#define MAX_PATH 4096
#define TRUE 1
#define FALSE 0
#endif
enum {XBOX_LOG_ERROR,XBOX_LOG_PATH};
void xbox_log(int,int,const char *,...);
void xbox_path_init(const char *,const char *);
BOOL xbox_translate_path(const char *,xbox_host_char *,DWORD);
int xbox_save_root_utf8(char *,unsigned);
int xbox_set_profile_root_utf8(const char *);
#endif
