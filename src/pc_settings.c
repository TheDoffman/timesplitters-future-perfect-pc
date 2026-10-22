#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define _WIN32_WINNT 0x0601
#include "pc_settings.h"
#include "texture_mode.h"
#include "pc_debug.h"
#include "pc_menu_theme.h"
#include "pc_dialog_theme.h"
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifdef _MSC_VER
/* Opt every executable using these dialogs into Windows visual styles. */
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

enum { ID_ACTIONS=100, ID_SLOT, ID_CHANGE, ID_CLEAR, ID_DEFAULTS, ID_SENSITIVITY,
       ID_INVERT, ID_ENABLED, ID_STATUS, ID_HELP, ID_SENS_LABEL };
typedef struct Editor { PcSettings settings; HWND list; int slot; } Editor;
typedef struct Capture { int key, accepted; } Capture;
typedef struct DialogContext { DLGPROC procedure; LPARAM data; HFONT font; PcDialogTheme theme; } DialogContext;
static const WCHAR dialog_context_property[]=L"TSFP.Settings.DialogContext";

static INT_PTR CALLBACK themed_dialog(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    DialogContext *context=(DialogContext *)GetPropW(window,dialog_context_property);
    INT_PTR result;
    if(message==WM_INITDIALOG) {
        context=(DialogContext *)lp;
        if(!SetPropW(window,dialog_context_property,(HANDLE)context)) { EndDialog(window,-1); return TRUE; }
        SendMessage(window,WM_SETFONT,(WPARAM)(context->font?context->font:GetStockObject(DEFAULT_GUI_FONT)),FALSE);
        lp=context->data;
    }
    if(!context) return FALSE;
    if(message==WM_SETTINGCHANGE || message==WM_THEMECHANGED) pc_dialog_theme_refresh(window,&context->theme);
    if(pc_dialog_theme_message(window,&context->theme,message,wp,lp,&result)) return result;
    result=context->procedure(window,message,wp,lp);
    if(message==WM_INITDIALOG) pc_dialog_theme_refresh(window,&context->theme);
    if(message==WM_NCDESTROY) {
        RemovePropW(window,dialog_context_property);
        if(context->theme.brush) {DeleteObject(context->theme.brush);context->theme.brush=NULL;}
    }
    return result;
}

/* DialogBox owns the message loop and keyboard navigation. Controls are
   created in WM_INITDIALOG, so no external resource files are required. */
static INT_PTR dialog(HWND owner, const WCHAR *title, int width, int height,
                      DLGPROC procedure, LPARAM data)
{
    union { DWORD align; BYTE bytes[256]; } buffer = {0};
    DLGTEMPLATE *t=(DLGTEMPLATE *)buffer.bytes;
    WORD *p=(WORD *)(t+1);
    const WCHAR *c=title;
    DWORD units=GetDialogBaseUnits();
    NONCLIENTMETRICSW metrics={sizeof metrics};
    DialogContext context={procedure,data,NULL};
    INT_PTR result;
    RECT work;
    SystemParametersInfo(SPI_GETWORKAREA,0,&work,0);
    if(width>work.right-work.left-40) width=work.right-work.left-40;
    if(height>work.bottom-work.top-70) height=work.bottom-work.top-70;
    t->style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_CENTER;
    t->cdit=0; t->cx=(short)MulDiv(width,4,LOWORD(units)); t->cy=(short)MulDiv(height,8,HIWORD(units));
    *p++=0; *p++=0;
    while (*c) *p++=(WORD)*c++;
    *p=0;
    if(SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof metrics,&metrics,0))
        context.font=CreateFontIndirectW(&metrics.lfMessageFont);
    result=DialogBoxIndirectParamW(GetModuleHandleW(NULL),t,owner,themed_dialog,(LPARAM)&context);
    /* Each modal dialog owns its font, including nested binding dialogs. */
    if(context.font) DeleteObject(context.font);
    return result;
}

static HWND control(HWND parent, const char *kind, const char *text, DWORD style, int id)
{
    HWND h=CreateWindowExA(!strcmp(kind,"EDIT")?WS_EX_CLIENTEDGE:0,kind,text,
        WS_CHILD|WS_VISIBLE|style,0,0,1,1,parent,(HMENU)(INT_PTR)id,GetModuleHandle(NULL),NULL);
    HFONT font=(HFONT)SendMessage(parent,WM_GETFONT,0,0);
    SendMessage(h,WM_SETFONT,(WPARAM)(font?font:GetStockObject(DEFAULT_GUI_FONT)),TRUE);
    return h;
}

static void place(HWND d,int id,int x,int y,int w,int h)
{ MoveWindow(GetDlgItem(d,id),x,y,w,h,TRUE); }

static int selected(Editor *e)
{ return ListView_GetNextItem(e->list,-1,LVNI_SELECTED); }

static void fill(HWND d, Editor *e)
{
    int i,j;
    char name[80],number[40];
    for(i=0;i<PC_ACTION_COUNT;i++) for(j=0;j<PC_BINDING_SLOTS;j++) {
        pc_settings_key_name(e->settings.keys[i][j],name,sizeof name);
        ListView_SetItemText(e->list,i,j+1,name);
    }
    snprintf(number,sizeof number,"%.4g",e->settings.sensitivity);
    SetDlgItemTextA(d,ID_SENSITIVITY,number);
    CheckDlgButton(d,ID_INVERT,e->settings.invert_y?BST_CHECKED:BST_UNCHECKED);
    CheckDlgButton(d,ID_ENABLED,e->settings.enabled?BST_CHECKED:BST_UNCHECKED);
}

static void capture_key(HWND d,int key)
{
    Capture *c=(Capture *)GetWindowLongPtr(d,DWLP_USER);
    if(!pc_settings_key_allowed(key)) { SetDlgItemTextA(d,ID_STATUS,"That key is reserved by the game window. Choose another key."); return; }
    c->key=key; c->accepted=1;
    EndDialog(d,IDOK);
}

static LRESULT CALLBACK capture_input(HWND window,UINT message,WPARAM wp,LPARAM lp,
                                     UINT_PTR id,DWORD_PTR data)
{
    HWND d=(HWND)data;
    (void)id;
    switch(message) {
    case WM_GETDLGCODE: return DLGC_WANTALLKEYS|DLGC_WANTCHARS;
    case WM_KEYDOWN: case WM_SYSKEYDOWN:
        if(wp==VK_ESCAPE) EndDialog(d,IDCANCEL);
        else capture_key(d,(int)wp);
        return 0;
    case WM_LBUTTONDOWN: capture_key(d,VK_LBUTTON); return 0;
    case WM_RBUTTONDOWN: capture_key(d,VK_RBUTTON); return 0;
    case WM_MBUTTONDOWN: capture_key(d,VK_MBUTTON); return 0;
    case WM_XBUTTONDOWN: capture_key(d,HIWORD(wp)==XBUTTON1?VK_XBUTTON1:VK_XBUTTON2); return TRUE;
    case WM_MOUSEWHEEL: capture_key(d,(short)HIWORD(wp)>0?VK_F23:VK_F24); return 0;
    }
    return DefSubclassProc(window,message,wp,lp);
}

static INT_PTR CALLBACK capture_dialog(HWND d,UINT message,WPARAM wp,LPARAM lp)
{
    switch(message) {
    case WM_INITDIALOG: {
        HWND input;
        SetWindowLongPtr(d,DWLP_USER,lp);
        control(d,"STATIC","Press a key, or click a mouse button / scroll inside the box.",0,ID_HELP);
        place(d,ID_HELP,15,14,430,30);
        input=control(d,"STATIC","Press your new binding here",WS_TABSTOP|SS_NOTIFY|SS_CENTER|SS_CENTERIMAGE|WS_BORDER,ID_ACTIONS);
        place(d,ID_ACTIONS,15,48,430,80);
        SetWindowSubclass(input,capture_input,1,(DWORD_PTR)d);
        control(d,"STATIC","Escape cancels. Use the button below to bind Escape itself.",0,ID_STATUS);
        place(d,ID_STATUS,15,140,430,36);
        control(d,"BUTTON","Use Escape",WS_TABSTOP|BS_PUSHBUTTON,ID_CHANGE); place(d,ID_CHANGE,15,183,105,28);
        control(d,"BUTTON","Unbind",WS_TABSTOP|BS_PUSHBUTTON,ID_CLEAR); place(d,ID_CLEAR,130,183,100,28);
        control(d,"BUTTON","Cancel",WS_TABSTOP|BS_PUSHBUTTON,IDCANCEL); place(d,IDCANCEL,345,183,100,28);
        SetFocus(input);
        return FALSE;
    }
    case WM_COMMAND:
        if(LOWORD(wp)==IDCANCEL) { EndDialog(d,IDCANCEL); return TRUE; }
        if(LOWORD(wp)==ID_CHANGE) { capture_key(d,VK_ESCAPE); return TRUE; }
        if(LOWORD(wp)==ID_CLEAR) { capture_key(d,0); return TRUE; }
        break;
    case WM_CLOSE: EndDialog(d,IDCANCEL); return TRUE;
    }
    return FALSE;
}

static void change_binding(HWND d,Editor *e)
{
    int row=selected(e), conflict,i,j;
    Capture capture={0};
    char title[256];
    if(row<0) { SetDlgItemTextA(d,ID_STATUS,"Select an action first."); return; }
    if(dialog(d,L"Choose a keyboard or mouse binding",465,230,capture_dialog,(LPARAM)&capture)!=IDOK || !capture.accepted) return;
    conflict=pc_settings_conflict(&e->settings,row,capture.key);
    if(conflict>=0) {
        char key_name[80];
        pc_settings_key_name(capture.key,key_name,sizeof key_name);
        snprintf(title,sizeof title,"%s is already assigned to %s.\n\nMove it to %s?",key_name,pc_action_labels[conflict],pc_action_labels[row]);
        if(MessageBoxA(d,title,"Binding already in use",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES) return;
    }
    if(pc_settings_assign(&e->settings,row,e->slot,capture.key,1)!=1)return;
    /* Only redraw bindings: do not overwrite mouse edits still in the form. */
    for(i=0;i<PC_ACTION_COUNT;i++) for(j=0;j<PC_BINDING_SLOTS;j++) {
        char name[80]; pc_settings_key_name(e->settings.keys[i][j],name,sizeof name);
        ListView_SetItemText(e->list,i,j+1,name);
    }
    SetDlgItemTextA(d,ID_STATUS,"Binding changed. Save to apply and remember your changes.");
}

static void layout(HWND d)
{
    RECT r;
    int w,h;
    GetClientRect(d,&r); w=r.right; h=r.bottom;
    place(d,ID_HELP,16,12,w-32,34);
    place(d,ID_ACTIONS,16,49,w-32,h-210);
    place(d,ID_SLOT,16,h-151,130,150);
    place(d,ID_CHANGE,158,h-153,135,28);
    place(d,ID_CLEAR,305,h-153,110,28);
    place(d,ID_SENS_LABEL,16,h-111,130,22);
    place(d,ID_SENSITIVITY,150,h-115,85,25);
    place(d,ID_INVERT,255,h-114,125,25);
    place(d,ID_ENABLED,397,h-114,180,25);
    place(d,ID_STATUS,16,h-78,w-32,28);
    place(d,ID_DEFAULTS,16,h-40,135,28);
    place(d,IDOK,w-260,h-40,125,28);
    place(d,IDCANCEL,w-123,h-40,107,28);
}

static INT_PTR CALLBACK settings_dialog(HWND d,UINT message,WPARAM wp,LPARAM lp)
{
    Editor *e=(Editor *)GetWindowLongPtr(d,DWLP_USER);
    switch(message) {
    case WM_INITDIALOG: {
        int i;
        LVCOLUMNA column={0};
        const char *headers[]={"Action","Primary","Alternate"};
        e=(Editor *)lp; SetWindowLongPtr(d,DWLP_USER,lp);
        control(d,"STATIC","Double-click a binding to change it. F2 opens settings; F1 shows/hides the menu bar.\nEscape releases the mouse. Pause a mission before opening PC settings.",0,ID_HELP);
        e->list=control(d,WC_LISTVIEWA,"",WS_TABSTOP|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS|WS_BORDER,ID_ACTIONS);
        ListView_SetExtendedListViewStyle(e->list,LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_DOUBLEBUFFER);
        for(i=0;i<=PC_BINDING_SLOTS;i++) { column.mask=LVCF_TEXT|LVCF_WIDTH; column.pszText=(char *)headers[i]; column.cx=i?105:225; ListView_InsertColumn(e->list,i,&column); }
        for(i=0;i<PC_ACTION_COUNT;i++) {
            LVITEMA item={0}; item.mask=LVIF_TEXT; item.iItem=i; item.pszText=(char *)pc_action_labels[i]; ListView_InsertItem(e->list,&item);
        }
        control(d,"COMBOBOX","",WS_TABSTOP|CBS_DROPDOWNLIST,ID_SLOT);
        for(i=1;i<=PC_BINDING_SLOTS;i++) SendDlgItemMessageA(d,ID_SLOT,CB_ADDSTRING,0,(LPARAM)headers[i]);
        SendDlgItemMessage(d,ID_SLOT,CB_SETCURSEL,0,0);
        control(d,"BUTTON","Change binding...",WS_TABSTOP|BS_PUSHBUTTON,ID_CHANGE);
        control(d,"BUTTON","Clear binding",WS_TABSTOP|BS_PUSHBUTTON,ID_CLEAR);
        control(d,"STATIC","Mouse sensitivity",0,ID_SENS_LABEL);
        control(d,"EDIT","",WS_TABSTOP|ES_AUTOHSCROLL,ID_SENSITIVITY);
        SendDlgItemMessage(d,ID_SENSITIVITY,EM_SETLIMITTEXT,24,0);
        control(d,"BUTTON","Invert mouse Y",WS_TABSTOP|BS_AUTOCHECKBOX,ID_INVERT);
        control(d,"BUTTON","Enable mouse controls",WS_TABSTOP|BS_AUTOCHECKBOX,ID_ENABLED);
        control(d,"STATIC","Changes apply immediately when saved; Cancel keeps your current settings.",0,ID_STATUS);
        control(d,"BUTTON","Restore defaults",WS_TABSTOP|BS_PUSHBUTTON,ID_DEFAULTS);
        control(d,"BUTTON","Save && Close",WS_TABSTOP|BS_DEFPUSHBUTTON,IDOK);
        control(d,"BUTTON","Cancel",WS_TABSTOP|BS_PUSHBUTTON,IDCANCEL);
        fill(d,e); layout(d);
        ListView_SetItemState(e->list,0,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
        return TRUE;
    }
    case WM_NOTIFY:
        if(((NMHDR *)lp)->idFrom==ID_ACTIONS && ((NMHDR *)lp)->code==NM_DBLCLK) {
            NMITEMACTIVATE *click=(NMITEMACTIVATE *)lp;
            if(click->iItem>=0) {
                e->slot=click->iSubItem>0?click->iSubItem-1:0;
                SendDlgItemMessage(d,ID_SLOT,CB_SETCURSEL,e->slot,0);
                change_binding(d,e);
            }
            return TRUE;
        }
        break;
    case WM_COMMAND:
        switch(LOWORD(wp)) {
        case ID_SLOT:
            if(HIWORD(wp)==CBN_SELCHANGE) e->slot=(int)SendDlgItemMessage(d,ID_SLOT,CB_GETCURSEL,0,0);
            return TRUE;
        case ID_CHANGE: change_binding(d,e); return TRUE;
        case ID_CLEAR: {
            int row=selected(e); char name[32];
            if(row>=0) { e->settings.keys[row][e->slot]=0; pc_settings_key_name(0,name,sizeof name); ListView_SetItemText(e->list,row,e->slot+1,name); }
            return TRUE;
        }
        case ID_DEFAULTS:
            if(MessageBoxA(d,"Restore the default PC bindings and mouse settings?\n\nThey are applied only when you save.","Restore defaults",MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)==IDYES) { pc_settings_defaults(&e->settings); fill(d,e); }
            return TRUE;
        case IDOK: {
            char number[40],error[320],*end;
            double sensitivity;
            GetDlgItemTextA(d,ID_SENSITIVITY,number,sizeof number);
            sensitivity=strtod(number,&end);
            while(*end==' ') end++;
            if(end==number || *end || !isfinite(sensitivity) || sensitivity<.001 || sensitivity>10) {
                MessageBoxA(d,"Enter a mouse sensitivity between 0.001 and 10.","Invalid sensitivity",MB_OK|MB_ICONWARNING);
                SetFocus(GetDlgItem(d,ID_SENSITIVITY)); return TRUE;
            }
            e->settings.sensitivity=sensitivity;
            e->settings.invert_y=IsDlgButtonChecked(d,ID_INVERT)==BST_CHECKED;
            e->settings.enabled=IsDlgButtonChecked(d,ID_ENABLED)==BST_CHECKED;
            if(!pc_settings_save(&e->settings,error,sizeof error)) { MessageBoxA(d,error,"Settings were not saved",MB_OK|MB_ICONERROR); return TRUE; }
            EndDialog(d,IDOK); return TRUE;
        }
        case IDCANCEL: EndDialog(d,IDCANCEL); return TRUE;
        }
        break;
    case WM_CLOSE: EndDialog(d,IDCANCEL); return TRUE;
    }
    return FALSE;
}

static const char menu_property[]="TSFP.PcSettingsMenu";

void pc_settings_toggle_menu(HWND game)
{
    HMENU bar=(HMENU)GetPropA(game,menu_property);
    if(!bar || !SetMenu(game,GetMenu(game) ? NULL : bar)) return;
    if(GetMenu(game)) pc_menu_theme_refresh(game,pc_window_theme_dark());
    /* Keep the outer window bounds, including borderless fullscreen. The
       renderer follows the new client size and WM_SIZE updates mouse clipping. */
    SetWindowPos(game,NULL,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
    DrawMenuBar(game);
}

void pc_settings_detach(HWND game)
{
    HMENU bar=(HMENU)RemovePropA(game,menu_property);
    pc_menu_theme_detach(game);
    /* Windows destroys an attached menu with its window; hidden menus are ours. */
    if(bar && GetMenu(game)!=bar) DestroyMenu(bar);
}

void pc_settings_attach(HWND game)
{
    HMENU bar=CreateMenu(),settings=CreatePopupMenu();
    RECT client,rect;
    INITCOMMONCONTROLSEX controls={sizeof controls,ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&controls);
    GetClientRect(game,&client);
    AppendMenuA(settings,MF_STRING,PC_SETTINGS_COMMAND,"&Keyboard && Mouse...\tF2");
    AppendMenuA(settings,MF_STRING,PC_DEBUG_COMMAND,"&Debug...\tF12");
    AppendMenuA(settings,MF_STRING | (tsfp_texture_is_4k() ? MF_CHECKED : 0),
                PC_TEXTURE_MODE_COMMAND,"&4K Textures (off = Classic)\tF10");
    AppendMenuA(settings,MF_SEPARATOR,0,NULL);
    AppendMenuA(settings,MF_STRING,PC_FULLSCREEN_COMMAND,"&Fullscreen\tAlt+Enter");
    AppendMenuA(settings,MF_STRING,PC_MENU_BAR_COMMAND,"&Hide Menu Bar\tF1");
    AppendMenuA(bar,MF_POPUP,(UINT_PTR)settings,"&PC Settings");
    if(!SetPropA(game,menu_property,(HANDLE)bar)) { DestroyMenu(bar); return; }
    SetMenu(game,bar);
    pc_menu_theme_attach(game);
    rect=client;
    AdjustWindowRectEx(&rect,(DWORD)GetWindowLongPtr(game,GWL_STYLE),TRUE,(DWORD)GetWindowLongPtr(game,GWL_EXSTYLE));
    SetWindowPos(game,NULL,0,0,rect.right-rect.left,rect.bottom-rect.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    DrawMenuBar(game);
}

void pc_settings_show(HWND game)
{
    Editor editor={0};
    char error[320];
    if(!pc_settings_read(&editor.settings,error,sizeof error)) { MessageBoxA(game,error,"PC Controls",MB_OK|MB_ICONERROR); return; }
    dialog(game,L"Keyboard & Mouse - TimeSplitters Future Perfect",780,720,settings_dialog,(LPARAM)&editor);
}

enum { ID_DEBUG_FPS=200, ID_DEBUG_TIME, ID_DEBUG_ISSUES, ID_DEBUG_LOG,
       ID_DEBUG_STATS, ID_DEBUG_RECENT, ID_DEBUG_FOLDER, ID_DEBUG_CLEAR, ID_DEBUG_HELP, ID_DEBUG_LATENCY };

/* Preserve existing automation/control IDs; new shared options get a private
 * ID range automatically. OS widget IDs stay out of the common definitions. */
static int debug_control_id(unsigned option)
{
    switch(option) {
    case PC_DEBUG_OPTION_FPS:return ID_DEBUG_FPS;
    case PC_DEBUG_OPTION_FRAME_TIME:return ID_DEBUG_TIME;
    case PC_DEBUG_OPTION_ISSUES:return ID_DEBUG_ISSUES;
    case PC_DEBUG_OPTION_LOG:return ID_DEBUG_LOG;
    case PC_DEBUG_OPTION_LATENCY:return ID_DEBUG_LATENCY;
    default:return 400+(int)option;
    }
}

static void debug_refresh(HWND d)
{
    PcDebugSnapshot stats;
    char text[512], old[6144], latency[128];
    pc_debug_poll();
    pc_debug_snapshot(&stats);
    if(!stats.network_mode) snprintf(latency,sizeof latency,"Network latency: offline");
    else if(stats.latency_ms<0 || !stats.latency_peers)
        snprintf(latency,sizeof latency,"%s latency: waiting for a peer / reply",stats.network_mode==2?"Online":"LAN");
    else snprintf(latency,sizeof latency,"%s RTT: %.1f ms (highest of %u peer%s)",
        stats.network_mode==2?"Online":"LAN",stats.latency_ms,stats.latency_peers,stats.latency_peers==1?"":"s");
    snprintf(text,sizeof text,"Game: %.1f FPS   Shown: %.1f FPS   Average frame interval: %.2f ms\r\n"
        "Log matches: %lu errors, %lu warnings   Frames over 50 ms: %lu (includes loading)\r\n%s",
        stats.fps,stats.shown_fps,stats.frame_ms,stats.errors,stats.warnings,stats.slow_frames,latency);
    SetDlgItemTextA(d,ID_DEBUG_STATS,text);
    if (!stats.log_available) snprintf(stats.recent,sizeof stats.recent,"Live log unavailable. Launch with Play.cmd to enable log diagnostics.");
    else if (!stats.recent[0]) snprintf(stats.recent,sizeof stats.recent,"No matching errors or warnings since startup / last clear.");
    GetDlgItemTextA(d,ID_DEBUG_RECENT,old,sizeof old);
    if (strcmp(old,stats.recent)) {
        SetDlgItemTextA(d,ID_DEBUG_RECENT,stats.recent);
        SendDlgItemMessage(d,ID_DEBUG_RECENT,EM_SETSEL,(WPARAM)-1,(LPARAM)-1);
        SendDlgItemMessage(d,ID_DEBUG_RECENT,EM_SCROLLCARET,0,0);
    }
}

static INT_PTR CALLBACK debug_dialog(HWND d,UINT message,WPARAM wp,LPARAM lp)
{
    (void)lp;
    switch(message) {
    case WM_INITDIALOG: {
        PcDebugOptions options;
        RECT r; int width,height;
        GetClientRect(d,&r); width=r.right; height=r.bottom;
        pc_debug_get(&options);
        unsigned row=0;
        for(unsigned i=0;i<PC_DEBUG_OPTION_COUNT;i++) {
            if(!pc_debug_option_available(i,PC_DEBUG_CAP_NETWORK))continue;
            int id=debug_control_id(i);
            control(d,"BUTTON",pc_debug_option(i)->label,WS_TABSTOP|BS_AUTOCHECKBOX,id);
            place(d,id,16,12+(int)row++*28,width-32,24);
            CheckDlgButton(d,id,pc_debug_option_value(&options,i)?BST_CHECKED:BST_UNCHECKED);
        }
        int extra=((int)pc_debug_option_count(PC_DEBUG_CAP_NETWORK)-5)*28;
        control(d,"STATIC","",0,ID_DEBUG_STATS); place(d,ID_DEBUG_STATS,16,164+extra,width-32,62);
        control(d,"STATIC","Recent log diagnostics (up to 12). Matches include failed calls, unsupported features and warnings;\nthis does not detect every gameplay or visual anomaly. Pause the game before opening settings.",0,ID_DEBUG_HELP);
        place(d,ID_DEBUG_HELP,16,236+extra,width-32,40);
        control(d,"EDIT","",WS_TABSTOP|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,ID_DEBUG_RECENT);
        place(d,ID_DEBUG_RECENT,16,284+extra,width-32,height-382-extra);
        control(d,"BUTTON","Open logs folder",WS_TABSTOP|BS_PUSHBUTTON,ID_DEBUG_FOLDER);
        place(d,ID_DEBUG_FOLDER,16,height-84,145,28);
        control(d,"BUTTON","Clear counters / recent",WS_TABSTOP|BS_PUSHBUTTON,ID_DEBUG_CLEAR);
        place(d,ID_DEBUG_CLEAR,173,height-84,180,28);
        control(d,"BUTTON","Save && Close",WS_TABSTOP|BS_DEFPUSHBUTTON,IDOK);
        control(d,"BUTTON","Cancel",WS_TABSTOP|BS_PUSHBUTTON,IDCANCEL);
        place(d,IDOK,width-260,height-40,125,28);
        place(d,IDCANCEL,width-123,height-40,107,28);
        debug_refresh(d);
        SetTimer(d,1,500,NULL);
        return TRUE;
    }
    case WM_TIMER: debug_refresh(d); return TRUE;
    case WM_COMMAND:
        switch(LOWORD(wp)) {
        case IDOK: {
            PcDebugOptions options;
            char error[320];
            pc_debug_get(&options);
            for(unsigned i=0;i<PC_DEBUG_OPTION_COUNT;i++)
                if(pc_debug_option_available(i,PC_DEBUG_CAP_NETWORK))pc_debug_option_set(&options,i,IsDlgButtonChecked(d,debug_control_id(i))==BST_CHECKED);
            if (!pc_debug_config_save(&options,error,sizeof error)) {
                MessageBoxA(d,error,"Debug settings were not saved",MB_OK|MB_ICONERROR); return TRUE;
            }
            EndDialog(d,IDOK); return TRUE;
        }
        case IDCANCEL: EndDialog(d,IDCANCEL); return TRUE;
        case ID_DEBUG_CLEAR: pc_debug_clear(); debug_refresh(d); return TRUE;
        case ID_DEBUG_FOLDER:
            if (!pc_debug_open_logs(d)) MessageBoxA(d,"The log folder is unavailable. Start the game with Play.cmd.","Debug logs",MB_OK|MB_ICONINFORMATION);
            return TRUE;
        }
        break;
    case WM_CLOSE: EndDialog(d,IDCANCEL); return TRUE;
    case WM_DESTROY: KillTimer(d,1); return TRUE;
    }
    return FALSE;
}

void pc_debug_show(HWND owner)
{
    dialog(owner,L"Debug - TimeSplitters Future Perfect",760,620+((int)pc_debug_option_count(PC_DEBUG_CAP_NETWORK)-5)*28,debug_dialog,0);
}
