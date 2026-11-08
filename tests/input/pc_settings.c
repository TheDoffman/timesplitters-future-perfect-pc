#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "pc_settings.h"
#include "pc_developer.h"
#include "pc_prompts.h"
#include "pc_controls.h"
#include "input_bindings.h"
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int phase, mode, failed;
static HWND main_dialog, capture_dialog, conflict_dialog;
static char screenshot[MAX_PATH];
static DWORD deadline;

static BOOL CALLBACK find_dialog(HWND window,LPARAM unused)
{
    char title[160]; (void)unused;
    GetWindowTextA(window,title,sizeof title);
    if(!strcmp(title,"Keyboard & Mouse - TimeSplitters Future Perfect")) main_dialog=window;
    else if(!strcmp(title,"Choose a keyboard or mouse binding")) capture_dialog=window;
    else if(!strcmp(title,"Binding already in use")) conflict_dialog=window;
    return TRUE;
}

static void snapshot(HWND window)
{
    RECT r;
    HDC screen=GetDC(window),memory=CreateCompatibleDC(screen);
    BITMAPINFO info={0}; BITMAPFILEHEADER header={0}; void *pixels;
    HBITMAP bitmap,old; FILE *file;
    GetWindowRect(window,&r);
    info.bmiHeader.biSize=sizeof info.bmiHeader;
    info.bmiHeader.biWidth=r.right-r.left; info.bmiHeader.biHeight=-(r.bottom-r.top);
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels,NULL,0);
    old=SelectObject(memory,bitmap);
    PrintWindow(window,memory,0);
    header.bfType=0x4d42; header.bfOffBits=sizeof header+sizeof info.bmiHeader;
    header.bfSize=header.bfOffBits+info.bmiHeader.biWidth*(r.bottom-r.top)*4;
    file=fopen(screenshot,"wb");
    if(file) { fwrite(&header,sizeof header,1,file); fwrite(&info.bmiHeader,sizeof info.bmiHeader,1,file); fwrite(pixels,1,header.bfSize-header.bfOffBits,file); fclose(file); }
    SelectObject(memory,old); DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(window,screen);
}

static void choose_row(int row)
{
    HWND list=GetDlgItem(main_dialog,100);
    ListView_SetItemState(list,-1,0,LVIS_SELECTED|LVIS_FOCUSED);
    ListView_SetItemState(list,row,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
    ListView_EnsureVisible(list,row,FALSE);
}

static void CALLBACK drive(HWND window,UINT message,UINT_PTR timer,DWORD time)
{
    char text[80]; (void)window; (void)message; (void)timer;
    main_dialog=capture_dialog=conflict_dialog=NULL;
    EnumThreadWindows(GetCurrentThreadId(),find_dialog,0);
    if(time-deadline<0x80000000u) { failed=1; if(capture_dialog) EndDialog(capture_dialog,IDCANCEL); if(conflict_dialog) EndDialog(conflict_dialog,IDNO); if(main_dialog) EndDialog(main_dialog,IDCANCEL); return; }
    if(!mode) {
        if(main_dialog) { SetDlgItemTextA(main_dialog,105,"0.9"); PostMessage(main_dialog,WM_COMMAND,IDCANCEL,0); phase=1; }
        return;
    }
    switch(phase) {
    case 0:
        if(main_dialog) { choose_row(14); phase=1; PostMessage(main_dialog,WM_COMMAND,102,0); }
        break;
    case 1:
        if(capture_dialog) { phase=2; PostMessage(GetDlgItem(capture_dialog,100),WM_KEYDOWN,'J',0); }
        break;
    case 2:
        if(main_dialog && !capture_dialog) {
            ListView_GetItemText(GetDlgItem(main_dialog,100),14,1,text,sizeof text);
            if(strcmp(text,"J")) failed=1;
            choose_row(9); phase=3; PostMessage(main_dialog,WM_COMMAND,102,0);
        }
        break;
    case 3:
        if(capture_dialog) { phase=4; PostMessage(GetDlgItem(capture_dialog,100),WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA),0); }
        break;
    case 4:
        if(conflict_dialog) { phase=5; PostMessage(conflict_dialog,WM_COMMAND,IDYES,0); }
        break;
    case 5:
        if(main_dialog && !capture_dialog && !conflict_dialog) {
            ListView_GetItemText(GetDlgItem(main_dialog,100),9,1,text,sizeof text);
            if(strcmp(text,"Wheel up")) failed=1;
            SetDlgItemTextA(main_dialog,105,"0.35");
            CheckDlgButton(main_dialog,106,BST_UNCHECKED);
            snapshot(main_dialog); phase=6;
            PostMessage(main_dialog,WM_COMMAND,IDOK,0);
        }
        break;
    }
}

static char *load(const char *path)
{
    FILE *f=fopen(path,"rb"); long size; char *text;
    if(!f) return NULL;
    fseek(f,0,SEEK_END); size=ftell(f); rewind(f);
    text=calloc((size_t)size+1,1); if(text) fread(text,1,(size_t)size,f);
    fclose(f); return text;
}

int main(int argc,char **argv)
{
    char path[MAX_PATH],replay[MAX_PATH],backup[MAX_PATH],error[320],*original,*saved;
    PcSettings settings,current;
    XBOX_GAMEPAD pad;
    FILE *f;
    HWND owner;
    UINT_PTR timer;
    LRESULT result;
    if(argc==4 && !strcmp(argv[1],"--verify-presets")) {
        int expected=atoi(argv[3]);original=load(argv[2]);CHECK(original);
        _putenv_s("RECOMP_INPUT_CONFIG",argv[2]);pc_controls_attach(NULL);
        CHECK(pc_settings_read(&current,error,sizeof error) && current.preset==expected);
        for(unsigned port=0;port<4;port++)CHECK(pc_settings_controller_preset(port)==expected);
        if(expected) {
            PcSettings custom;CHECK(pc_settings_custom_read(&custom,error,sizeof error));
            CHECK(pc_settings_custom_semantics() && custom.preset==0);
            for(int action=0;action<PC_ACTION_COUNT;action++)for(int slot=0;slot<PC_BINDING_SLOTS;slot++)CHECK(custom.keys[action][slot]==current.keys[action][slot]);
        } else CHECK(!pc_settings_custom_semantics());
        saved=load(argv[2]);CHECK(saved && !strcmp(saved,original));free(saved);free(original);
        puts("PASS: fresh/legacy preset loading preserves file bytes");return 0;
    }
    if(argc==3 && !strcmp(argv[1],"--verify")) {
        _putenv_s("RECOMP_INPUT_CONFIG",argv[2]);
        pc_controls_attach(NULL);
        CHECK(pc_settings_read(&current,error,sizeof error));
        CHECK(current.keys[14][0]=='J' && current.keys[9][0]==VK_F23 && !current.keys[13][1]);
        CHECK(!current.keys[19][0] && current.keys[19][1]=='K');
        CHECK(pc_settings_menu_visible());
        owner=CreateWindowExA(0,"STATIC","Restored menu",WS_OVERLAPPEDWINDOW,0,0,640,480,NULL,NULL,GetModuleHandle(NULL),NULL);
        CHECK(owner); pc_settings_attach(owner); CHECK(!GetMenu(owner));
        pc_settings_detach(owner); DestroyWindow(owner);
        puts("PASS: bindings and menu visibility reload in a fresh process"); return 0;
    }
    CHECK(argc==3);
    snprintf(path,sizeof path,"%s\\settings-test.json",argv[2]);
    snprintf(replay,sizeof replay,"%s\\settings-replay.txt",argv[2]);
    snprintf(backup,sizeof backup,"%s.bak",path);
    if(GetFileAttributesA(backup)!=INVALID_FILE_ATTRIBUTES) SetFileAttributesA(backup,FILE_ATTRIBUTE_NORMAL);
    snprintf(screenshot,sizeof screenshot,"%s\\settings-dialog.bmp",argv[2]);
    original=load(argv[1]); CHECK(original && original[0]=='{');
    f=fopen(path,"wb"); CHECK(f);
    fputs("{\"retained\":{\"text\":\"escaped \\\" brace } [\",\"values\":[1,2]},",f);
    {
        char *mouse=strstr(original,"\"mouse\"");
        char *brace=mouse?strchr(mouse,'{'):NULL;
        CHECK(brace);
        fwrite(original+1,1,(size_t)(brace-original),f);
        fputs("\"retained_mouse\":[1,{\"keep\":true}],",f);
        fputs(brace+1,f);
    }
    fclose(f); free(original);
    f=fopen(replay,"w"); CHECK(f); fputs("0 key 73 1\n0 key 84 1\n0 key 1 1\n500 key 73 1\n500 key 84 1\n500 key 1 1\n",f); fclose(f);
    _putenv_s("RECOMP_INPUT_CONFIG",path); _putenv_s("TSFP_PC_INPUT_REPLAY",replay);
    pc_controls_attach(NULL);
    CHECK(pc_settings_read(&settings,error,sizeof error));
    CHECK(settings.keys[24][0]=='1' && settings.keys[25][0]=='2' && settings.keys[33][0]=='0');
    CHECK(settings.keys[4][0]=='X' && settings.keys[5][0]=='Z');
    pc_settings_defaults(&current);
    CHECK(!memcmp(settings.keys,current.keys,sizeof current.keys));
    CHECK(PC_BINDING_SLOTS==2);
    CHECK(!pc_settings_assign(&settings,14,2,'K',1));
    CHECK(settings.keys[14][0]=='W');
    CHECK(recomp_bindings_sample(0,&pad) && pad.sThumbLY==0);
    CHECK(pc_settings_conflict(&settings,9,VK_XBUTTON2)==8);
    settings.keys[14][0]='I'; settings.keys[2][0]='T'; settings.sensitivity=.25; settings.invert_y=1;
    CHECK(pc_settings_save(&settings,error,sizeof error));
    {
        char prompt[128];
        CHECK(pc_prompts_rewrite("\v4 Reload / \vD Move",prompt,sizeof prompt));
        CHECK(!strcmp(prompt,"[T] Reload / [I/S/A/D] Move"));
    }
    Sleep(520);
    CHECK(recomp_bindings_sample(0,&pad) && pad.sThumbLY==32767 && pad.bAnalogButtons[XBOX_BUTTON_X]==255);
    CHECK(pc_settings_read(&current,error,sizeof error) && current.keys[14][0]=='I' && fabs(current.sensitivity-.25)<.0001 && current.invert_y);
    saved=load(path); CHECK(saved && strstr(saved,"escaped \\\" brace } [") && strstr(saved,"\"port\": 4") && strstr(saved,"retained_mouse")); free(saved);
    saved=load(backup); CHECK(saved && strstr(saved,"key:W")); free(saved);
    /* Presets are overlays: select/apply/restore never replace custom keys. */
    {
        PcSettings custom,modern;CHECK(pc_settings_custom_read(&custom,error,sizeof error));
        pc_settings_modern(&modern);modern.sensitivity=custom.sensitivity;modern.invert_y=custom.invert_y;
        CHECK(pc_settings_save(&modern,error,sizeof error));
        CHECK(pc_settings_read(&current,error,sizeof error) && current.preset==1 && current.keys[1][1]==5 && current.keys[10][0]=='B');
        CHECK(pc_settings_custom_read(&current,error,sizeof error) && !memcmp(current.keys,custom.keys,sizeof custom.keys));
        /* Non-preset shortcut edits must persist while Modern remains selected. */
        CHECK(pc_settings_read(&current,error,sizeof error) && current.preset==1);
        CHECK(pc_settings_assign(&current,19,1,'J',1)==1);
        CHECK(pc_settings_assign(&current,PC_DEVELOPER_ACTION,0,VK_F3,1)==1);
        CHECK(pc_settings_save(&current,error,sizeof error));
        CHECK(pc_settings_read(&current,error,sizeof error) && current.preset==1 && current.keys[19][1]=='J' && current.keys[PC_DEVELOPER_ACTION][0]==VK_F3);
        int shortcut[PC_BINDING_SLOTS];pc_settings_action_keys(PC_DEVELOPER_ACTION,shortcut);CHECK(shortcut[0]==VK_F3);
        CHECK(pc_settings_custom_read(&current,error,sizeof error) && current.keys[14][0]=='I' && current.keys[2][0]=='T' && current.keys[19][1]=='J');
        saved=load(path);CHECK(saved && strstr(saved,"key:BACK") && strstr(saved,"key:RETURN"));free(saved);
        CHECK(pc_settings_read(&current,error,sizeof error));PcSettings unchanged=current;
        CHECK(!pc_settings_assign(&current,0,0,'H',1));
        CHECK(!pc_settings_assign(&current,PC_DEVELOPER_ACTION,0,'B',1));
        CHECK(!memcmp(&current,&unchanged,sizeof current));

        CHECK(pc_settings_controller_preset_save(0,1,error,sizeof error));
        CHECK(pc_settings_controller_preset(0)==1 && pc_settings_controller_preset(1)==0);
        CHECK(SetFileAttributesA(path,FILE_ATTRIBUTE_READONLY));
        CHECK(!pc_settings_controller_preset_save(0,0,error,sizeof error) && pc_settings_controller_preset(0)==1);
        CHECK(!pc_settings_save(&custom,error,sizeof error));
        CHECK(pc_settings_read(&current,error,sizeof error) && current.preset==1);
        CHECK(SetFileAttributesA(path,FILE_ATTRIBUTE_NORMAL));
        CHECK(pc_settings_save(&custom,error,sizeof error));
        CHECK(pc_settings_controller_preset_save(0,0,error,sizeof error));
        CHECK(pc_settings_read(&current,error,sizeof error) && !current.preset && !memcmp(current.keys,custom.keys,sizeof custom.keys));
    }
    current=settings; current.sensitivity=NAN; CHECK(!pc_settings_save(&current,error,sizeof error));
    current=settings; current.keys[0][0]=VK_F2; CHECK(!pc_settings_save(&current,error,sizeof error));
    current=settings; current.keys[0][0]=VK_F12; CHECK(!pc_settings_save(&current,error,sizeof error));
    current=settings; current.keys[0][0]=VK_F11; CHECK(!pc_settings_save(&current,error,sizeof error));
    current=settings; current.keys[0][0]=VK_F3; CHECK(pc_settings_save(&current,error,sizeof error));
    CHECK(pc_settings_save(&settings,error,sizeof error));
    /* Deadzone persistence is independent of native profile Accept/Cancel,
     * and failed saves must never publish a new live threshold. */
    int original_move=pc_settings_deadzone(0,0);
    CHECK(pc_settings_deadzone_save(0,1,1638,error,sizeof error));
    CHECK(pc_settings_deadzone(0,1)==1638 && pc_settings_deadzone(0,0)==original_move);
    CHECK(!pc_settings_deadzone_save(0,1,-1,error,sizeof error));
    CHECK(!pc_settings_deadzone_save(4,1,100,error,sizeof error));
    CHECK(SetFileAttributesA(path,FILE_ATTRIBUTE_READONLY));
    CHECK(!pc_settings_deadzone_save(0,1,3277,error,sizeof error));
    CHECK(pc_settings_deadzone(0,1)==1638);
    CHECK(SetFileAttributesA(path,FILE_ATTRIBUTE_NORMAL));
    CHECK(pc_settings_save(&settings,error,sizeof error));
    saved=load(path);CHECK(saved && strstr(saved,"\"aim_deadzone\":1638"));free(saved);
    CHECK(SetFileAttributesA(path,FILE_ATTRIBUTE_READONLY));
    current=settings; current.keys[14][0]='U'; CHECK(!pc_settings_save(&current,error,sizeof error));
    CHECK(SetFileAttributesA(path,FILE_ATTRIBUTE_NORMAL));
    CHECK(pc_settings_read(&current,error,sizeof error) && current.keys[14][0]=='I');

    owner=CreateWindowExA(0,"STATIC","PC settings integration test",WS_OVERLAPPEDWINDOW,0,0,640,480,NULL,NULL,GetModuleHandle(NULL),NULL);
    CHECK(owner);
    CHECK(!pc_settings_menu_visible());
    CHECK(pc_settings_developer_save(1,error,sizeof error));
    pc_settings_attach(owner); CHECK(!GetMenu(owner));
    pc_settings_toggle_menu(owner);
    CHECK(pc_developer_open() && !GetMenu(owner));
    CHECK(!pc_settings_menu_visible());
    pc_settings_toggle_menu(owner); CHECK(!GetMenu(owner));
    CHECK(!pc_developer_open());
    CHECK(!pc_settings_menu_visible());
    pc_settings_detach(owner); DestroyWindow(owner);
    owner=CreateWindowExA(0,"STATIC","PC settings restart test",WS_OVERLAPPEDWINDOW,0,0,640,480,NULL,NULL,GetModuleHandle(NULL),NULL);
    CHECK(owner); pc_settings_attach(owner); CHECK(!GetMenu(owner));
    pc_settings_toggle_menu(owner); CHECK(pc_developer_open() && !GetMenu(owner));
    pc_developer_close();
    CHECK(pc_settings_save_menu_visible(1)); /* Preserve legacy JSON, never attach a bar. */
    CHECK(SetFileAttributesA(path,FILE_ATTRIBUTE_READONLY));
    CHECK(!pc_settings_save_menu_visible(0));
    CHECK(SetFileAttributesA(path,FILE_ATTRIBUTE_NORMAL));
    CHECK(pc_settings_menu_visible());
    deadline=GetTickCount()+10000; timer=SetTimer(NULL,0,70,drive); CHECK(timer);
    pc_settings_show(owner); KillTimer(NULL,timer);
    CHECK(!failed && phase==1);
    CHECK(pc_settings_read(&current,error,sizeof error) && fabs(current.sensitivity-.25)<.0001);
    mode=1; phase=0; deadline=GetTickCount()+10000;
    timer=SetTimer(NULL,0,70,drive); CHECK(timer);
    pc_settings_show(owner); KillTimer(NULL,timer);
    if(failed || phase!=6) fprintf(stderr,"Dialog test phase=%d failed=%d\n",phase,failed);
    CHECK(!failed && phase==6);
    CHECK(pc_settings_read(&current,error,sizeof error));
    CHECK(current.keys[14][0]=='J' && current.keys[9][0]==VK_F23 && !current.keys[13][1]);
    CHECK(fabs(current.sensitivity-.35)<.0001 && !current.invert_y);
    current.keys[19][0]=0;current.keys[19][1]='K';
    CHECK(pc_settings_save(&current,error,sizeof error));
    CHECK(pc_settings_read(&current,error,sizeof error) && !current.keys[19][0] && current.keys[19][1]=='K');
    saved=load(path); CHECK(saved && strstr(saved,"key:#74") && strstr(saved,"key:#134")); free(saved);
    {
        char executable[MAX_PATH],command[2*MAX_PATH+32];
        STARTUPINFOA startup={sizeof startup}; PROCESS_INFORMATION child={0}; DWORD code;
        GetModuleFileNameA(NULL,executable,sizeof executable);
        snprintf(command,sizeof command,"\"%s\" --verify \"%s\"",executable,path);
        CHECK(CreateProcessA(NULL,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&startup,&child));
        CHECK(WaitForSingleObject(child.hProcess,10000)==WAIT_OBJECT_0);
        CHECK(GetExitCodeProcess(child.hProcess,&code) && code==0);
        CloseHandle(child.hProcess); CloseHandle(child.hThread);
    }
    pc_settings_defaults(&current); CHECK(current.keys[14][0]=='W' && fabs(current.sensitivity-.1)<.0001);
    {
        HMENU bar=(HMENU)GetPropA(owner,"TSFP.PcSettingsMenu");
        pc_settings_toggle_menu(owner); CHECK(pc_developer_open() && !GetMenu(owner) && IsMenu(bar));
        pc_settings_detach(owner); CHECK(!IsMenu(bar));
        pc_developer_close();
    }
    CHECK(pc_settings_read(&settings,error,sizeof error));
    CHECK(pc_settings_assign(&settings,33,0,'1',1)==1 && !settings.keys[24][0]);
    { int ok=pc_settings_save(&settings,error,sizeof error); if(!ok)fprintf(stderr,"Final settings save: %s\n",error); CHECK(ok); }
    CHECK(pc_settings_read(&current,error,sizeof error) && current.keys[33][0]=='1' && !current.keys[24][0]);
    DestroyWindow(owner);
    {
        int devices[4]={-2,0,1,-1},actual[4];
        CHECK(pc_settings_players_save(devices,error,sizeof error));
        pc_settings_players_read(actual);CHECK(!memcmp(devices,actual,sizeof devices));
        CHECK(pc_settings_keyboard_player());
        saved=load(path);CHECK(saved && strstr(saved,"retained_mouse") && strstr(saved,"key:#74"));free(saved);
        devices[2]=0;CHECK(!pc_settings_players_save(devices,error,sizeof error));
        pc_settings_players_read(actual);CHECK(actual[2]==1);
        devices[0]=2;devices[2]=1;
        CHECK(SetFileAttributesA(path,FILE_ATTRIBUTE_READONLY));
        CHECK(!pc_settings_players_save(devices,error,sizeof error));
        CHECK(pc_settings_keyboard_player());
        CHECK(SetFileAttributesA(path,FILE_ATTRIBUTE_NORMAL));
        CHECK(pc_settings_players_save(devices,error,sizeof error));
        CHECK(!pc_settings_keyboard_player());
        devices[0]=-2;CHECK(pc_settings_players_save(devices,error,sizeof error));
        CHECK(pc_settings_keyboard_player());
    }
    /* Replay mode leaves desktop messages alone. */
    CHECK(pc_controls_message(NULL,WM_MOUSEACTIVATE,0,0,&result)==0); /* replay does not consume native messages */
    puts("PASS: live rebind sampling, atomic persistence/backup, unrelated JSON retention, validation, defaults, menu creation, Cancel, keyboard/wheel capture, conflict reassignment, and dialog save");
    return 0;
}
