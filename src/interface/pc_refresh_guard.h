#ifndef TSFP_PC_REFRESH_GUARD_H
#define TSFP_PC_REFRESH_GUARD_H
/* Called only by the existing helper's explicit --restore-refresh mode. */
static int pc_refresh_guard(int argc,WCHAR **argv)
{
    if(argc!=10 || wcscmp(argv[1],L"--restore-refresh")) return 1;
    unsigned long values[6];
    for(int i=0;i<6;i++) {
        WCHAR *end;values[i]=wcstoul(argv[i==0?2:i+3],&end,10);
        if(*end || !values[i]) return 1;
    }
    DWORD pid=values[0],width=values[1],height=values[2],bits=values[3],old_hz=values[4],target=values[5];
    if((target!=120 && target!=144) || old_hz>1000 || width>32768 || height>32768 || bits>64 ||
        wcslen(argv[3])>=CCHDEVICENAME || wcsncmp(argv[3],L"\\\\.\\DISPLAY",11)) return 1;
    HANDLE parent=OpenProcess(SYNCHRONIZE,FALSE,pid);
    HANDLE ready=OpenEventW(EVENT_MODIFY_STATE|SYNCHRONIZE,FALSE,argv[9]);
    if(!parent || !ready) { if(parent)CloseHandle(parent);if(ready)CloseHandle(ready);return 2; }
    DEVMODEW old={0},current={0};int found=0;
    for(DWORD index=0;;index++) {
        old.dmSize=sizeof old;
        if(!EnumDisplaySettingsW(argv[3],index,&old)) break;
        if(old.dmPelsWidth==width && old.dmPelsHeight==height && old.dmBitsPerPel==bits && old.dmDisplayFrequency==old_hz) {found=1;break;}
    }
    if(!found) { CloseHandle(ready);CloseHandle(parent);return 3; }
    SetEvent(ready);
    WaitForSingleObject(parent,INFINITE);CloseHandle(parent);
    int active=WaitForSingleObject(ready,0)==WAIT_OBJECT_0;CloseHandle(ready);
    if(!active) return 0;
    current.dmSize=sizeof current;
    if(EnumDisplaySettingsW(argv[3],ENUM_CURRENT_SETTINGS,&current) && current.dmPelsWidth==width &&
        current.dmPelsHeight==height && current.dmBitsPerPel==bits && current.dmDisplayFrequency==target) {
        old.dmFields=DM_DISPLAYFREQUENCY;
        return ChangeDisplaySettingsExW(argv[3],&old,NULL,CDS_FULLSCREEN,NULL)==DISP_CHANGE_SUCCESSFUL?0:4;
    }
    return 0;
}
#endif
