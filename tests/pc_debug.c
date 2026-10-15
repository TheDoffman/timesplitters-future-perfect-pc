#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "pc_debug.h"
#include "pc_settings.h"
#include "pc_controls.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while (0)
static int save_dialog, driven, timed_out;
static ULONGLONG deadline;

static BOOL CALLBACK drive_dialog(HWND window, LPARAM unused)
{
    char title[128];
    (void)unused;
    GetWindowTextA(window,title,sizeof title);
    if (strcmp(title,"Debug - TimeSplitters Future Perfect")) return TRUE;
    if (GetTickCount64()>deadline) { timed_out=1; EndDialog(window,IDCANCEL); return TRUE; }
    CheckDlgButton(window,200,BST_UNCHECKED);
    CheckDlgButton(window,201,BST_UNCHECKED);
    CheckDlgButton(window,202,BST_UNCHECKED);
    CheckDlgButton(window,203,BST_UNCHECKED);
    driven=1;
    PostMessage(window,WM_COMMAND,save_dialog?IDOK:IDCANCEL,0);
    return TRUE;
}

static void CALLBACK drive(HWND window, UINT message, UINT_PTR timer, DWORD tick)
{
    (void)window; (void)message; (void)timer; (void)tick;
    EnumThreadWindows(GetCurrentThreadId(),drive_dialog,0);
}

static char *load(const char *path)
{
    FILE *file=fopen(path,"rb");
    long size; char *text;
    if (!file) return NULL;
    fseek(file,0,SEEK_END); size=ftell(file); rewind(file);
    text=calloc((size_t)size+1,1);
    if (text) fread(text,1,(size_t)size,file);
    fclose(file); return text;
}

int main(int argc, char **argv)
{
    PcDebugOptions options, readback;
    PcDebugSnapshot snapshot;
    PcSettings controls;
    char config[MAX_PATH], log[MAX_PATH], error[320], overlay[96], *text, *before;
    FILE *file;
    HWND owner;
    UINT_PTR timer;
    int i;
    if (argc==3 && !strcmp(argv[1],"--verify")) {
        _putenv_s("RECOMP_INPUT_CONFIG",argv[2]);
        pc_debug_config_read(&readback);
        CHECK(readback.show_fps && readback.show_frame_time && readback.show_issues && readback.log_performance);
        puts("PASS: debug settings reload in a fresh process"); return 0;
    }
    CHECK(argc==3);
    snprintf(config,sizeof config,"%s\\debug-settings.json",argv[2]);
    snprintf(log,sizeof log,"%s\\debug-runtime.err",argv[2]);
    text=load(argv[1]); CHECK(text && text[0]=='{');
    file=fopen(config,"wb"); CHECK(file);
    fputs("{\"debug\":{\"retained\":[1,{\"text\":\"} escaped \\\"\"}]},",file);
    fputs(text+1,file); fclose(file); free(text);
    file=fopen(log,"wb"); CHECK(file);
    fputs("[INFO] startup\n[WARN] unsupported format\n[ERROR] draw failed\n[PC-DEBUG] warning counters\n",file);
    fclose(file);
    _putenv_s("RECOMP_INPUT_CONFIG",config);
    _putenv_s("RECOMP_LOG_FILE",log);
    _putenv_s("RECOMP_FPS_OVERLAY","");
    pc_controls_attach(NULL);
    pc_debug_get(&options);
    CHECK(!options.show_fps && !options.show_frame_time && !options.show_issues && !options.log_performance);
    CHECK(!pc_debug_overlay(overlay,sizeof overlay,"auto"));
    pc_debug_snapshot(&snapshot);
    CHECK(snapshot.log_available && snapshot.errors==1 && snapshot.warnings==1);
    pc_debug_poll(); pc_debug_snapshot(&snapshot); CHECK(snapshot.errors==1 && snapshot.warnings==1);
    file=fopen(log,"ab"); CHECK(file); fputs("fatal",file); fclose(file);
    pc_debug_poll(); pc_debug_snapshot(&snapshot); CHECK(snapshot.errors==1);
    file=fopen(log,"ab"); CHECK(file); fputs(" exception\r\n",file); fclose(file);
    pc_debug_poll(); pc_debug_snapshot(&snapshot); CHECK(snapshot.errors==2);
    options.show_fps=options.show_frame_time=options.show_issues=options.log_performance=1;
    CHECK(pc_debug_config_save(&options,error,sizeof error));
    CHECK(pc_settings_read(&controls,error,sizeof error));
    CHECK(pc_settings_save(&controls,error,sizeof error)); /* controls must preserve debug settings */
    text=load(config); CHECK(text);
    {
        char *boolean=strstr(text,"\"show_fps\":true"); CHECK(boolean);
        boolean+=strlen("\"show_fps\":true");
        file=fopen(config,"wb"); CHECK(file);
        fwrite(text,1,(size_t)(boolean-text),file); fputs(" \r\n",file); fputs(boolean,file); fclose(file);
    }
    free(text); /* Hand-edited JSON may have whitespace after a boolean. */
    pc_debug_config_read(&readback); CHECK(!memcmp(&options,&readback,sizeof options));
    text=load(config); CHECK(text && strstr(text,"retained") && strstr(text,"escaped") && strstr(text,"\"port\": 4")); free(text);
    {
        char executable[MAX_PATH],command[2*MAX_PATH+32];
        STARTUPINFOA startup={sizeof startup}; PROCESS_INFORMATION child={0}; DWORD code;
        GetModuleFileNameA(NULL,executable,sizeof executable);
        snprintf(command,sizeof command,"\"%s\" --verify \"%s\"",executable,config);
        CHECK(CreateProcessA(NULL,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&startup,&child));
        CHECK(WaitForSingleObject(child.hProcess,10000)==WAIT_OBJECT_0);
        CHECK(GetExitCodeProcess(child.hProcess,&code) && code==0);
        CloseHandle(child.hProcess); CloseHandle(child.hThread);
    }
    CHECK(SetFileAttributesA(config,FILE_ATTRIBUTE_READONLY));
    memset(&readback,0,sizeof readback);
    CHECK(!pc_debug_config_save(&readback,error,sizeof error));
    pc_debug_get(&readback); CHECK(!memcmp(&options,&readback,sizeof options));
    CHECK(SetFileAttributesA(config,FILE_ATTRIBUTE_NORMAL));
    pc_debug_sample(60,120);
    CHECK(pc_debug_overlay(overlay,sizeof overlay,"auto"));
    CHECK(strstr(overlay,"60.0/120.0 fps") && strstr(overlay,"16.7 ms") && strstr(overlay,"E:2 W:1"));
    pc_debug_frame(); Sleep(65); pc_debug_frame();
    pc_debug_snapshot(&snapshot); CHECK(snapshot.slow_frames==1);
    pc_debug_toggle_fps(); pc_debug_get(&readback); CHECK(!readback.show_fps && readback.show_issues);
    pc_debug_set(&options);
    pc_debug_clear(); pc_debug_snapshot(&snapshot);
    CHECK(!snapshot.errors && !snapshot.warnings && !snapshot.slow_frames && !snapshot.recent[0]);
    file=fopen(log,"ab"); CHECK(file);
    for (i=0;i<100000;i++) fputc('x',file);
    fputs("\n[WARN] after bounded read\n",file); fclose(file);
    pc_debug_poll(); pc_debug_snapshot(&snapshot); CHECK(!snapshot.warnings);
    pc_debug_poll(); pc_debug_snapshot(&snapshot); CHECK(snapshot.warnings==1);
    file=fopen(log,"wb"); CHECK(file); fputs("[ERROR] after truncation\n",file); fclose(file);
    pc_debug_poll(); pc_debug_snapshot(&snapshot); CHECK(snapshot.errors==1);
    owner=CreateWindowExA(0,"STATIC","PC debug test",WS_OVERLAPPEDWINDOW,0,0,640,480,NULL,NULL,GetModuleHandle(NULL),NULL);
    CHECK(owner); pc_settings_attach(owner);
    CHECK(GetMenuItemID(GetSubMenu(GetMenu(owner),0),1)==PC_DEBUG_COMMAND);
    CHECK(GetMenuState(GetSubMenu(GetMenu(owner),0),PC_FULLSCREEN_COMMAND,MF_BYCOMMAND)!=(UINT)-1);
    before=load(config); CHECK(before);
    deadline=GetTickCount64()+5000; timer=SetTimer(NULL,0,70,drive); CHECK(timer);
    pc_debug_show(owner); KillTimer(NULL,timer);
    CHECK(driven && !timed_out);
    text=load(config); CHECK(text && !strcmp(before,text)); free(text); free(before);
    pc_debug_get(&readback); CHECK(!memcmp(&options,&readback,sizeof options));
    save_dialog=1; driven=0;
    deadline=GetTickCount64()+5000; timer=SetTimer(NULL,0,70,drive); CHECK(timer);
    pc_debug_show(owner); KillTimer(NULL,timer);
    CHECK(driven && !timed_out);
    pc_debug_get(&readback); CHECK(!readback.show_fps && !readback.show_frame_time && !readback.show_issues && !readback.log_performance);
    CHECK(!pc_debug_overlay(overlay,sizeof overlay,"auto"));
    pc_debug_shutdown(); DestroyWindow(owner);
    puts("PASS: debug persistence, unrelated settings retention, failed save, live counters, partial lines, bounded reads, truncation, FPS/frame time, menu, Cancel, and dialog Save");
    return 0;
}
