#ifndef TSFP_PC_DIALOG_THEME_H
#define TSFP_PC_DIALOG_THEME_H
#include "pc_window_theme.h"
#include <commctrl.h>

typedef struct PcDialogTheme { int dark;HBRUSH brush; } PcDialogTheme;

static BOOL CALLBACK pc_dialog_theme_child(HWND child,LPARAM data)
{
    PcDialogTheme *theme=(PcDialogTheme *)data;
    WCHAR kind[64];GetClassNameW(child,kind,64);
    if(!lstrcmpiW(kind,WC_LISTVIEWW)) {
        ListView_SetBkColor(child,theme->dark?RGB(32,32,32):GetSysColor(COLOR_WINDOW));
        ListView_SetTextBkColor(child,theme->dark?RGB(32,32,32):GetSysColor(COLOR_WINDOW));
        ListView_SetTextColor(child,theme->dark?RGB(240,240,240):GetSysColor(COLOR_WINDOWTEXT));
    }
    InvalidateRect(child,NULL,TRUE);
    return TRUE;
}

static void pc_dialog_theme_refresh(HWND window,PcDialogTheme *theme)
{
    theme->dark=pc_window_theme_dark();
    if(theme->brush) DeleteObject(theme->brush);
    theme->brush=CreateSolidBrush(theme->dark?RGB(32,32,32):GetSysColor(COLOR_BTNFACE));
    pc_window_theme_apply(window);
    EnumChildWindows(window,pc_dialog_theme_child,(LPARAM)theme);
    RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
}

static int pc_dialog_theme_message(HWND window,PcDialogTheme *theme,UINT message,WPARAM wp,LPARAM lp,INT_PTR *result)
{
    if(!theme->dark) return 0;
    if(message==WM_CTLCOLORDLG || message==WM_CTLCOLORSTATIC || message==WM_CTLCOLOREDIT || message==WM_CTLCOLORLISTBOX) {
        SetTextColor((HDC)wp,IsWindowEnabled((HWND)lp)?RGB(240,240,240):RGB(140,140,140));
        SetBkColor((HDC)wp,RGB(32,32,32));*result=(INT_PTR)theme->brush;return 1;
    }
    if(message==WM_NOTIFY) {
        NMHDR *header=(NMHDR *)lp;
        WCHAR kind[64];GetClassNameW(header->hwndFrom,kind,64);
        if(header->code==NM_CUSTOMDRAW && !lstrcmpiW(kind,WC_HEADERW)) {
            NMCUSTOMDRAW *draw=(NMCUSTOMDRAW *)lp;
            if(draw->dwDrawStage==CDDS_PREPAINT) *result=CDRF_NOTIFYITEMDRAW;
            else if(draw->dwDrawStage==CDDS_ITEMPREPAINT) {
                WCHAR label[100];HDITEMW item={0};RECT text=draw->rc;
                item.mask=HDI_TEXT;item.pszText=label;item.cchTextMax=100;
                SendMessageW(header->hwndFrom,HDM_GETITEMW,draw->dwItemSpec,(LPARAM)&item);
                SetDCBrushColor(draw->hdc,RGB(48,48,48));FillRect(draw->hdc,&text,(HBRUSH)GetStockObject(DC_BRUSH));
                text.left+=6;SetBkMode(draw->hdc,TRANSPARENT);SetTextColor(draw->hdc,RGB(240,240,240));
                DrawTextW(draw->hdc,label,-1,&text,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
                *result=CDRF_SKIPDEFAULT;
            } else return 0;
            SetWindowLongPtr(window,DWLP_MSGRESULT,*result);*result=TRUE;return 1;
        }
        if(header->code==NM_CUSTOMDRAW && !lstrcmpiW(kind,L"Button")) {
            NMCUSTOMDRAW *draw=(NMCUSTOMDRAW *)lp;
            RECT text=draw->rc,box;WCHAR label[300];LONG_PTR style=GetWindowLongPtr(header->hwndFrom,GWL_STYLE);
            int checkbox=(style&BS_TYPEMASK)==BS_AUTOCHECKBOX;
            if(draw->dwDrawStage!=CDDS_PREPAINT) return 0;
            SetDCBrushColor(draw->hdc,RGB(32,32,32));
            FillRect(draw->hdc,&text,(HBRUSH)GetStockObject(DC_BRUSH));
            if(!checkbox) {
                HPEN border=CreatePen(PS_SOLID,1,(draw->uItemState&CDIS_FOCUS)?RGB(96,185,250):RGB(80,80,80));
                HPEN old_pen=(HPEN)SelectObject(draw->hdc,border);
                HBRUSH old_brush=(HBRUSH)SelectObject(draw->hdc,GetStockObject(DC_BRUSH));
                SetDCBrushColor(draw->hdc,(draw->uItemState&CDIS_SELECTED)?RGB(41,41,41):
                    (draw->uItemState&CDIS_HOT)?RGB(61,61,61):RGB(51,51,51));
                RoundRect(draw->hdc,text.left,text.top,text.right-1,text.bottom-1,6,6);
                SelectObject(draw->hdc,old_brush);SelectObject(draw->hdc,old_pen);DeleteObject(border);
            }
            SetTextColor(draw->hdc,(draw->uItemState&CDIS_DISABLED)?RGB(140,140,140):RGB(240,240,240));
            SetBkMode(draw->hdc,TRANSPARENT);GetWindowTextW(header->hwndFrom,label,300);
            if(checkbox) {
                int checked=(int)SendMessage(header->hwndFrom,BM_GETCHECK,0,0)==BST_CHECKED;
                box=text;box.left+=2;box.top+=(box.bottom-box.top-14)/2;box.right=box.left+14;box.bottom=box.top+14;
                {
                    HPEN border=CreatePen(PS_SOLID,1,checked?RGB(96,185,250):RGB(160,160,160));
                    HPEN old_pen=(HPEN)SelectObject(draw->hdc,border);
                    HBRUSH old_brush=(HBRUSH)SelectObject(draw->hdc,GetStockObject(DC_BRUSH));
                    SetDCBrushColor(draw->hdc,checked?RGB(8,127,196):RGB(43,43,43));
                    RoundRect(draw->hdc,box.left,box.top,box.right,box.bottom,4,4);
                    SelectObject(draw->hdc,old_brush);SelectObject(draw->hdc,old_pen);DeleteObject(border);
                }
                if(checked) {
                    HPEN pen=CreatePen(PS_SOLID,2,RGB(240,240,240)),old=(HPEN)SelectObject(draw->hdc,pen);
                    MoveToEx(draw->hdc,box.left+3,box.top+7,NULL);LineTo(draw->hdc,box.left+6,box.top+10);LineTo(draw->hdc,box.left+11,box.top+3);
                    SelectObject(draw->hdc,old);DeleteObject(pen);
                }
                text.left=box.right+7;
            }
            DrawTextW(draw->hdc,label,-1,&text,DT_SINGLELINE|DT_VCENTER|(checkbox?DT_LEFT:DT_CENTER));
            if(checkbox && (draw->uItemState&CDIS_FOCUS)) {InflateRect(&text,-2,-2);DrawFocusRect(draw->hdc,&text);}
            SetWindowLongPtr(window,DWLP_MSGRESULT,CDRF_SKIPDEFAULT);*result=TRUE;return 1;
        }
    }
    return 0;
}
#endif
