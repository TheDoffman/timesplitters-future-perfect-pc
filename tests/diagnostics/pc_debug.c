#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "pc_debug.h"
#include "pc_settings.h"
#include "pc_controls.h"
#include "interface/pc_feature_flags.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while (0)
static int save_dialog, driven, timed_out, expected_save_error;
static ULONGLONG deadline;

static BOOL CALLBACK drive_dialog(HWND window, LPARAM unused)
{
    char title[128];
    (void)unused;
    GetWindowTextA(window,title,sizeof title);
    if (!strcmp(title,"Debug settings were not saved")) {
        if (!timed_out) fprintf(stderr,"%s: debug Save opened an error dialog\n",
                              expected_save_error ? "Expected failure" : "FAIL");
        timed_out=1;
        PostMessage(window,WM_CLOSE,0,0);
        return TRUE;
    }
    if (strcmp(title,"Debug - TimeSplitters Future Perfect")) return TRUE;
    if (timed_out || GetTickCount64()>deadline) { timed_out=1; EndDialog(window,IDCANCEL); return TRUE; }
    if (driven) return TRUE; /* Do not queue Save again inside a nested dialog. */
    CheckDlgButton(window,200,BST_UNCHECKED);
    CheckDlgButton(window,201,BST_UNCHECKED);
    CheckDlgButton(window,202,BST_UNCHECKED);
    CheckDlgButton(window,203,BST_UNCHECKED);
    CheckDlgButton(window,209,BST_UNCHECKED);
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
    char config[MAX_PATH], runtime_config[MAX_PATH], log[MAX_PATH], error[320], overlay[96], *text, *before;
    FILE *file;
    HWND owner;
    UINT_PTR timer;
    int i;
    if (argc==3 && !strcmp(argv[1],"--verify")) {
        _putenv_s("RECOMP_INPUT_CONFIG",argv[2]);
        pc_debug_config_read(&readback);
        CHECK(readback.show_fps && readback.show_frame_time && readback.show_issues && readback.log_performance && readback.show_latency);
        puts("PASS: debug settings reload in a fresh process"); return 0;
    }
    CHECK(argc==3);
    snprintf(config,sizeof config,"%s\\debug-settings.json",argv[2]);
    snprintf(runtime_config,sizeof runtime_config,"%s\\debug-runtime.ini",argv[2]);
    _putenv_s("RECOMP_DISPLAY_CONFIG",runtime_config);
    _putenv_s("TSFP_NATIVE_FPS","0");
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
    CHECK(strstr(pc_debug_watermark(),"TSFP PC v") && strlen(pc_debug_watermark())<96);
    CHECK(strstr(pc_debug_watermark(),pc_build_version()));
    CHECK(pc_story_coop_enabled()==(strstr(pc_debug_watermark(),"story-coop")!=NULL));
    printf("PASS: linked build version %s; Story chooser %s\n",pc_build_version(),pc_story_coop_enabled()?"enabled":"disabled");
    pc_debug_snapshot(&snapshot);
    CHECK(snapshot.log_available && snapshot.errors==1 && snapshot.warnings==1);
    pc_debug_poll(); pc_debug_snapshot(&snapshot); CHECK(snapshot.errors==1 && snapshot.warnings==1);
    file=fopen(log,"ab"); CHECK(file); fputs("fatal",file); fclose(file);
    pc_debug_poll(); pc_debug_snapshot(&snapshot); CHECK(snapshot.errors==1);
    file=fopen(log,"ab"); CHECK(file); fputs(" exception\r\n",file); fclose(file);
    pc_debug_poll(); pc_debug_snapshot(&snapshot); CHECK(snapshot.errors==2);
    options.show_fps=options.show_frame_time=options.show_issues=options.log_performance=options.show_latency=1;
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
    CHECK(!strstr(overlay,"RTT:"));
    _putenv_s("TSFP_NATIVE_FPS","144");
    CHECK(pc_debug_overlay(overlay,sizeof overlay,"60") && strstr(overlay,"target 144 (Beta)") && !strstr(overlay,"cap 60"));
    _putenv_s("TSFP_NATIVE_FPS","120");
    CHECK(pc_debug_overlay(overlay,sizeof overlay,"60") && strstr(overlay,"target 120 (Beta)"));
    _putenv_s("TSFP_NATIVE_FPS","60");
    CHECK(pc_debug_overlay(overlay,sizeof overlay,"60") && strstr(overlay,"target 60") && !strstr(overlay,"Beta"));
    _putenv_s("TSFP_NATIVE_FPS","0");
    CHECK(pc_debug_overlay(overlay,sizeof overlay,"60") && strstr(overlay,"cap 60") && !strstr(overlay,"target") && !strstr(overlay,"Beta"));
    _putenv_s("TSFP_NATIVE_FPS","");
    memset(&readback,0,sizeof readback); readback.show_latency=1;
    pc_debug_set(&readback);
    CHECK(!pc_debug_overlay(overlay,sizeof overlay,"60") && !overlay[0]);
    pc_debug_set(&options);
    pc_debug_latency(2,42.5,1);
    CHECK(pc_debug_overlay(overlay,sizeof overlay,"auto") && strstr(overlay,"RTT: 42.5 ms"));
    pc_debug_snapshot(&snapshot); CHECK(snapshot.network_mode==2 && snapshot.latency_peers==1 && snapshot.latency_ms==42.5);
    pc_debug_latency(1,-1,0);
    CHECK(pc_debug_overlay(overlay,sizeof overlay,"auto") && strstr(overlay,"RTT: waiting"));
    pc_debug_latency(1,NAN,1);
    CHECK(pc_debug_overlay(overlay,sizeof overlay,"auto") && !strstr(overlay,"nan"));
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
    /* Quiet mode suppresses log processing without persisting debug changes
       or stopping the FPS measurement needed for the A/B experiment. */
    before=load(config); CHECK(before);
    CHECK(!_putenv_s("TSFP_DEBUG_QUIET","1"));
    file=fopen(log,"ab"); CHECK(file); fputs("[ERROR] quiet-mode probe\n",file); fclose(file);
    pc_debug_poll(); pc_debug_snapshot(&snapshot); CHECK(snapshot.errors==1);
    pc_debug_sample(55,55); pc_debug_snapshot(&snapshot); CHECK(snapshot.fps==55);
    pc_debug_get(&readback); CHECK(!memcmp(&options,&readback,sizeof options));
    text=load(config); CHECK(text && !strcmp(before,text)); free(text); free(before);
    CHECK(!_putenv_s("TSFP_DEBUG_QUIET","0"));
    pc_debug_poll(); pc_debug_snapshot(&snapshot); CHECK(snapshot.errors==2);
    owner=CreateWindowExA(0,"STATIC","PC debug test",WS_OVERLAPPEDWINDOW,0,0,640,480,NULL,NULL,GetModuleHandle(NULL),NULL);
    CHECK(owner); pc_settings_attach(owner);
    if(!GetMenu(owner)) pc_settings_toggle_menu(owner);
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
    CHECK(!readback.show_latency && !pc_debug_overlay(overlay,sizeof overlay,"auto"));
    /* A nested save-error dialog must fail or finish the test, not keep the
       build waiting indefinitely or queue more Save commands behind it. */
    before=load(config); CHECK(before);
    CHECK(SetFileAttributesA(config,FILE_ATTRIBUTE_READONLY));
    expected_save_error=1; driven=timed_out=0;
    deadline=GetTickCount64()+5000; timer=SetTimer(NULL,0,70,drive); CHECK(timer);
    pc_debug_show(owner); KillTimer(NULL,timer);
    CHECK(SetFileAttributesA(config,FILE_ATTRIBUTE_NORMAL));
    CHECK(driven && timed_out);
    text=load(config); CHECK(text && !strcmp(before,text)); free(text); free(before);
    pc_debug_get(&readback); CHECK(!readback.show_fps && !readback.show_frame_time && !readback.show_issues && !readback.log_performance);
    pc_debug_shutdown(); DestroyWindow(owner);
    puts("PASS: debug persistence, unrelated settings retention, failed save, live counters, partial lines, bounded reads, truncation, FPS/frame time, menu, Cancel, dialog Save and bounded save-error dismissal");
    return 0;
}
