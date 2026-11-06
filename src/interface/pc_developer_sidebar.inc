/* Presentation-thread-owned immediate sidebar. Paint and pointer/keyboard hit
 * testing use this single value-only widget layout on all host renderers. */
#include "graphics/pc_developer_font.h"
enum { UI_ACTION,UI_GROUP,UI_TAB,UI_DOCK,UI_CLOSE,UI_SEARCH,UI_CLEAR,
       UI_NUMBER,UI_STEP,UI_SLIDER,UI_PRESET,UI_BIND,UI_SETTING,UI_SCROLL,UI_VIDEO,UI_VIDEO_OPTION,UI_VIDEO_PAGE };
typedef struct DeveloperWidget {
    int kind,a,b,content;double value;
    float x,y,w,h;
    char name[80];
} DeveloperWidget;
static DeveloperWidget sidebar_widgets[160];
static unsigned sidebar_count;
static int sidebar_focus=-1,sidebar_right,sidebar_search_edit,sidebar_quick_bind;
static int sidebar_select_all,sidebar_drag=-1,sidebar_dirty;
static int sidebar_expanded[18]={1,1,0,0,1,1,1,0,1,1,0,0,1,1,1,0,0,0};
static char sidebar_search[64];
static float sidebar_scroll[5],sidebar_max_scroll,sidebar_width=1280,sidebar_height=720;
static float sidebar_scale,sidebar_left,sidebar_top,sidebar_bottom,sidebar_y;
static float sidebar_pointer_x=-1,sidebar_pointer_y=-1;
static int sidebar_content,sidebar_group_match,sidebar_group_present;
static PcDeveloperLine *sidebar_output;
static unsigned sidebar_capacity,sidebar_draw_count;
static int sidebar_card=-1;static float sidebar_card_y;
static int sidebar_reserving;
static float sidebar_ui_scale(float width) {return fminf(fmaxf(.75f,pc_controls_developer_ui_scale()),width/476.f);}
static unsigned char sidebar_repeat_key;
static ULONGLONG sidebar_repeat_at;
static const uint32_t UI_BG=0x20262cffu,UI_CARD=0x1c2228ffu,UI_HEADER=0x293139ffu,
       UI_EDGE=0x39434dffu,UI_TEXT=0xedf0f3ffu,UI_MUTED=0x9aa6b2ffu,
       UI_ACCENT=0x80bfffffu,UI_FIELD=0x151b21ffu;
static int sidebar_match(const char *text) {
    if(!sidebar_search[0])return 1;
    size_t n=strlen(sidebar_search);
    for(;*text;text++) {
        size_t i=0;
        for(;i<n&&text[i];i++) {
            unsigned a=(unsigned char)text[i],b=(unsigned char)sidebar_search[i];
            if(a>='A'&&a<='Z')a+='a'-'A';if(b>='A'&&b<='Z')b+='a'-'A';
            if(a!=b)break;
        }
        if(i==n)return 1;
    }
    return 0;
}
static int sidebar_row_matches(const char *text) {return sidebar_group_match||sidebar_match(text);}
static void sidebar_emit(unsigned kind,float x,float y,float w,float h,uint32_t color,float radius,const char *text) {
    if(w<=0||h<=0||!sidebar_output||sidebar_draw_count>=sidebar_capacity)return;
    if(sidebar_content&&!sidebar_reserving&&(y+h<=sidebar_top||y>=sidebar_bottom))return;
    PcDeveloperLine *l=&sidebar_output[sidebar_draw_count++];memset(l,0,sizeof *l);
    l->kind=kind;l->x=sidebar_left+x*sidebar_scale;l->y=y*sidebar_scale;
    l->width=w*sidebar_scale;l->height=h*sidebar_scale;l->color=color;l->radius=radius*sidebar_scale;
    if(sidebar_content){l->clip_top=sidebar_top*sidebar_scale;l->clip_bottom=sidebar_bottom*sidebar_scale;}
    if(text)snprintf(l->text,sizeof l->text,"%s",text);
}
static void sidebar_box(float x,float y,float w,float h,uint32_t color,float radius) {sidebar_emit(PC_DEVELOPER_BOX,x,y,w,h,color,radius,NULL);}
static void sidebar_text_spaced(float x,float y,float w,float h,uint32_t color,const char *text,float font,unsigned weight,unsigned align,unsigned face,float tracking) {
    char fitted[160];float px=fminf(96,fmaxf(6,font*sidebar_scale));
    pc_developer_font_fit_spaced(text,px,weight,face,tracking*sidebar_scale,w*sidebar_scale,fitted,sizeof fitted);
    unsigned before=sidebar_draw_count;sidebar_emit(PC_DEVELOPER_LABEL,x,y,w,h,color,0,fitted);
    if(sidebar_draw_count>before){PcDeveloperLine *l=&sidebar_output[before];l->font_px=px;l->font_weight=weight;l->align=align;l->font_face=face;l->letter_spacing=tracking*sidebar_scale;}
}
static void sidebar_text_face(float x,float y,float w,float h,uint32_t color,const char *text,float font,unsigned weight,unsigned align,unsigned face) {
    sidebar_text_spaced(x,y,w,h,color,text,font,weight,align,face,0);
}
static void sidebar_text_style(float x,float y,float w,float h,uint32_t color,const char *text,float font,unsigned weight,unsigned align) {
    sidebar_text_face(x,y,w,h,color,text,font,weight,align,PC_FONT_UI);
}
static void sidebar_icon(float x,float y,float size,uint32_t color,unsigned icon) {
    unsigned before=sidebar_draw_count;sidebar_emit(PC_DEVELOPER_ICON,x,y,size,size,color,0,NULL);
    if(sidebar_draw_count>before)sidebar_output[before].icon=icon;
}
static DeveloperWidget *sidebar_widget(int kind,int a,int b,double value,float x,float y,float w,float h,const char *name) {
    if(sidebar_count>=sizeof sidebar_widgets/sizeof sidebar_widgets[0])return NULL;
    DeveloperWidget *v=&sidebar_widgets[sidebar_count++];memset(v,0,sizeof *v);
    v->kind=kind;v->a=a;v->b=b;v->value=value;v->x=x;v->y=y;v->w=w;v->h=h;v->content=sidebar_content;
    snprintf(v->name,sizeof v->name,"%s",name?name:"");return v;
}
static int sidebar_hover(float x,float y,float w,float h) {
    return sidebar_pointer_x>=x&&sidebar_pointer_x<x+w&&sidebar_pointer_y>=y&&sidebar_pointer_y<y+h&&
        (!sidebar_content||(sidebar_pointer_y>=sidebar_top&&sidebar_pointer_y<sidebar_bottom));
}
static void sidebar_button(int kind,int a,int b,double value,float x,float y,float w,float h,const char *text,int accent) {
    unsigned index=sidebar_count;sidebar_widget(kind,a,b,value,x,y,w,h,text);
    int hover=sidebar_hover(x,y,w,h),focus=(int)index==sidebar_focus;
    sidebar_box(x,y,w,h,focus||accent?0x4f7799ffu:UI_EDGE,4);
    sidebar_box(x+1,y+1,w-2,h-2,hover?0x394852ffu:accent?0x263645ffu:0x26313affu,3);
    if(kind==UI_DOCK||kind==UI_CLOSE||kind==UI_CLEAR)sidebar_icon(x+(w-18)*.5f,y+(h-18)*.5f,18,UI_MUTED,kind==UI_DOCK?PC_DEV_DOCK:PC_DEV_CLOSE);
    else if(kind==UI_PRESET&&w<60)sidebar_icon(x+(w-18)*.5f,y+(h-18)*.5f,18,UI_MUTED,PC_DEV_RESET);
    else if(kind==UI_GROUP) {
        sidebar_icon(x+5,y+(h-15)*.5f,15,UI_MUTED,sidebar_expanded[a]?PC_DEV_DOWN:PC_DEV_RIGHT);
        sidebar_text_style(x+25,y,w-100,h,UI_TEXT,"Hold-key speeds",13,600,0);
        sidebar_text_style(x+w-77,y,64,h,UI_MUTED,"[ / ]",10,400,2);
    } else sidebar_text_style(x+5,y,w-10,h,accent?UI_ACCENT:UI_TEXT,text,kind==UI_NUMBER?16.f:kind==UI_BIND||kind==UI_PRESET?11.f:12.f,kind==UI_NUMBER?600:400,kind==UI_ACTION||kind==UI_TAB?0:1);
}
static int sidebar_group(int id,const char *name,const char *keywords,const char *detail) {
    sidebar_group_present=sidebar_match(name)||sidebar_match(keywords);
    if(!sidebar_group_present)return 0;
    sidebar_group_match=sidebar_search[0]&&sidebar_match(name);
    int open=sidebar_expanded[id]||sidebar_search[0];
    sidebar_card_y=sidebar_y;sidebar_card=sidebar_output?(int)sidebar_draw_count:-1;sidebar_reserving=1;
    sidebar_box(17,sidebar_y,408,36,UI_EDGE,6);sidebar_box(18,sidebar_y+1,406,34,UI_CARD,5);sidebar_reserving=0;
    sidebar_box(18,sidebar_y+1,406,34,UI_HEADER,5);
    if(open)sidebar_box(18,sidebar_y+20,406,16,UI_HEADER,0);
    sidebar_widget(UI_GROUP,id,0,0,17,sidebar_y,408,36,name);
    sidebar_icon(27,sidebar_y+10,15,UI_MUTED,open?PC_DEV_DOWN:PC_DEV_RIGHT);
    sidebar_text_face(49,sidebar_y+6,267,23,UI_TEXT,name,13,600,0,PC_FONT_UI);
    if(detail)sidebar_text_spaced(315,sidebar_y+8,97,18,UI_MUTED,detail,10,400,2,PC_FONT_UI,.25f);
    sidebar_y+=open?39.f:36.f;return open;
}
static void sidebar_row_bg(float height) {(void)height;}
static void sidebar_group_end(void) {
    if(sidebar_card>=0&&sidebar_output&&(unsigned)(sidebar_card+1)<sidebar_draw_count) {
        float height=sidebar_y-sidebar_card_y+9;
        sidebar_output[sidebar_card].height=height*sidebar_scale;
        sidebar_output[sidebar_card+1].height=(height-2)*sidebar_scale;
    }
    sidebar_card=-1;sidebar_y+=19;
}
static void sidebar_group_closed(void) {sidebar_card=-1;if(sidebar_group_present)sidebar_y+=10;}
static void sidebar_toggle(const char *name,const char *description,int on,int available,int target,int row) {
    if(!sidebar_row_matches(name))return;
    float h=description?52.f:38.f;sidebar_row_bg(h);
    sidebar_text_face(31,sidebar_y+8,325,21,available?UI_TEXT:UI_MUTED,name,12,400,0,PC_FONT_UI);
    if(description)sidebar_text_face(31,sidebar_y+29,350,17,UI_MUTED,description,11,400,0,PC_FONT_UI);
    int hover=sidebar_hover(378,sidebar_y+10,32,18);
    sidebar_box(378,sidebar_y+10,32,18,on?UI_ACCENT:hover?0x65727dffu:0x49535dffu,9);
    sidebar_box(on?394.f:381.f,sidebar_y+13,12,12,on?0x12304bffu:0xe4e9edffu,6);
    if(available)sidebar_widget(UI_ACTION,target,row,0,24,sidebar_y+2,393,h-3,name);
    sidebar_y+=h;
}
static void sidebar_binding(int action,int binding,int quick,float x,float y,float w,const char *name) {
    PcSettings current;char error[160],text[48];
    if(quick) {if(!pc_settings_read(&current,error,sizeof error))pc_settings_defaults(&current);}
    else current=draft;
    int key=current.keys[action][binding];
    if(capture_action==action&&slot==binding)snprintf(text,sizeof text,conflict_key?"Conflict":"Press key");
    else if(key)pc_settings_key_name(key,text,sizeof text);else snprintf(text,sizeof text,"Unbound");
    if(quick){float chip=fminf(w,fmaxf(24.f,pc_developer_font_measure(text,11,400,PC_FONT_UI).advance+16));x+=w-chip;w=chip;}
    sidebar_button(UI_BIND,action,binding,quick,x,y,w,25,text,capture_action==action&&slot==binding);
    if(sidebar_count)snprintf(sidebar_widgets[sidebar_count-1].name,80,"%s",name);
}
static double sidebar_video_fov(void);
static double sidebar_value(int id) {
    PcDeveloperState s;pc_developer_get(&s);
    return id==5?sidebar_video_fov():id==1?s.manual_speed:id==2?draft.sensitivity:id==3?s.slow_speed:s.fast_speed;
}
static void sidebar_number(const char *name,int id) {
    if(!sidebar_row_matches(name))return;
    if(id==1)sidebar_y+=9;
    sidebar_row_bg(63);sidebar_text_face(31,sidebar_y+5,265,21,UI_TEXT,name,12,400,0,PC_FONT_UI);
    if(id==1)sidebar_text_style(284,sidebar_y+5,125,21,UI_MUTED,"Step: 0.10\xc3\x97",10,400,2);
    char text[32];
    if(edit_number==id)snprintf(text,sizeof text,"%s",number);
    else snprintf(text,sizeof text,id==2?"%.4g":"%.2f",sidebar_value(id));
    sidebar_box(29,sidebar_y+28,163,34,0x4c5863ffu,5);sidebar_box(30,sidebar_y+29,161,32,UI_FIELD,4);
    sidebar_widget(UI_STEP,id,-1,0,30,sidebar_y+29,32,32,"Decrease");sidebar_box(30,sidebar_y+29,32,32,0x2c353effu,3);sidebar_text_style(30,sidebar_y+29,32,32,UI_TEXT,"-",18,400,1);
    sidebar_widget(UI_NUMBER,id,0,0,63,sidebar_y+29,95,32,name);
    sidebar_text_face(64,sidebar_y+29,77,32,edit_number==id?UI_ACCENT:UI_TEXT,text,17,700,1,PC_FONT_MONO);
    if(id!=2)sidebar_text_style(140,sidebar_y+29,16,32,UI_MUTED,"\xc3\x97",11,400,1);
    sidebar_widget(UI_STEP,id,1,0,159,sidebar_y+29,32,32,"Increase");sidebar_box(159,sidebar_y+29,32,32,0x2c353effu,3);sidebar_text_style(159,sidebar_y+29,32,32,UI_TEXT,"+",18,400,1);
    sidebar_button(UI_PRESET,id,0,id==1?1.:id==2?.1:id==3?.5:2.,200,sidebar_y+28,34,34,"Reset",0);
    sidebar_text_style(250,sidebar_y+29,159,32,UI_MUTED,id==2?"0.001 - 10":id==1?"0.05\xe2\x80\x93" "4.00\xc3\x97":"Hold shortcut",11,400,2);
    sidebar_y+=63;
}
static void sidebar_held_speed(int id,const char *label,const char *key) {
    if(!sidebar_row_matches(label))return;
    sidebar_box(39,sidebar_y+5,24,25,UI_EDGE,4);sidebar_box(40,sidebar_y+6,22,23,UI_FIELD,3);
    sidebar_text_style(39,sidebar_y+5,24,25,UI_MUTED,key,11,400,1);
    sidebar_text_style(74,sidebar_y,175,35,UI_MUTED,label,11,400,0);
    sidebar_box(264,sidebar_y+3,136,29,UI_EDGE,4);sidebar_box(265,sidebar_y+4,134,27,UI_FIELD,3);
    sidebar_widget(UI_STEP,id,-1,0,265,sidebar_y+4,25,27,"Decrease held speed");sidebar_box(265,sidebar_y+4,25,27,0x2c353effu,3);sidebar_text_style(265,sidebar_y+4,25,27,UI_TEXT,"-",15,400,1);
    char text[32];if(edit_number==id)snprintf(text,sizeof text,"%s",number);else snprintf(text,sizeof text,"%.2f",sidebar_value(id));
    sidebar_widget(UI_NUMBER,id,0,0,291,sidebar_y+4,82,27,label);
    sidebar_text_face(291,sidebar_y+4,59,27,edit_number==id?UI_ACCENT:UI_TEXT,text,12,600,1,PC_FONT_MONO);
    sidebar_text_style(350,sidebar_y+4,19,27,UI_MUTED,"\xc3\x97",11,400,1);
    sidebar_widget(UI_STEP,id,1,0,374,sidebar_y+4,25,27,"Increase held speed");sidebar_box(374,sidebar_y+4,25,27,0x2c353effu,3);sidebar_text_style(374,sidebar_y+4,25,27,UI_TEXT,"+",15,400,1);
    sidebar_y+=35;
}
static void sidebar_disabled_group(const char *name,const char *reason) {
    if(!sidebar_match(name)&&!sidebar_match(reason))return;
    sidebar_box(17,sidebar_y,408,36,UI_EDGE,6);sidebar_box(18,sidebar_y+1,406,34,UI_CARD,5);
    sidebar_text_style(31,sidebar_y+5,382,24,UI_MUTED,name,13,600,0);sidebar_y+=40;
    sidebar_text_style(31,sidebar_y,382,22,UI_MUTED,reason,11,400,0);sidebar_y+=32;
}
static void sidebar_tools(void) {
    PcDeveloperState s;pc_developer_get(&s);
    AcquireSRWLockShared(&dev_lock);int online=network,available=gameplay&&(!network||match_tools_allowed),campaign=story&&available&&!network;ReleaseSRWLockShared(&dev_lock);
    if(!pc_developer_tools_allowed()) {
        sidebar_text_style(27,sidebar_y+9,392,24,UI_MUTED,"Tools are disabled by the host for this match.",12,400,0);sidebar_y+=42;
        sidebar_text_style(27,sidebar_y,392,22,UI_MUTED,"Settings and Diagnostics remain available.",11,400,0);sidebar_y+=32;return;
    }
    if(sidebar_group(0,"Camera & movement","free camera freecam no-clip noclip bindings shortcut flight","2 TOOLS")) {
        const char *names[]={"Free camera","No-clip"};
        for(int i=0;i<2;i++)if(sidebar_row_matches(names[i])||sidebar_match("bindings shortcut")) {
            sidebar_row_bg(36);sidebar_text_face(31,sidebar_y+1,225,19,available?UI_TEXT:UI_MUTED,names[i],12,400,0,PC_FONT_UI);
            sidebar_text_face(31,sidebar_y+19,345,17,UI_MUTED,i?"Fly through walls with player control.":"Explore without moving the player.",11,400,0,PC_FONT_UI);
            sidebar_binding(i?PC_NOCLIP_ACTION:PC_FREECAM_ACTION,0,1,288,sidebar_y+6,88,i?"No-clip shortcut":"Free camera shortcut");
            int on=i?s.noclip:s.freecam;
            sidebar_box(378,sidebar_y+10,32,18,on?UI_ACCENT:0x49535dffu,9);sidebar_box(on?394.f:381.f,sidebar_y+13,12,12,on?0x12304bffu:UI_TEXT,6);
            if(available)sidebar_widget(UI_ACTION,HOME,i?2:3,0,372,sidebar_y+2,46,34,names[i]);
            sidebar_y+=36;
        }
        sidebar_row_bg(23);sidebar_text_style(31,sidebar_y+3,385,17,UI_MUTED,"Move with your bindings. ",11,400,0);
        float before_shift=pc_developer_font_measure("Move with your bindings. ",11,400,PC_FONT_UI).advance;
        float shift_width=pc_developer_font_measure("Shift",11,700,PC_FONT_UI).advance;
        sidebar_text_style(31+before_shift,sidebar_y+3,shift_width,17,UI_MUTED,"Shift",11,700,0);
        sidebar_text_style(31+before_shift+shift_width,sidebar_y+3,385-before_shift-shift_width,17,UI_MUTED," boosts flight speed.",11,400,0);sidebar_y+=23;
        sidebar_group_end();
    } else sidebar_group_closed();
    if(online)sidebar_disabled_group("Time & audio","Unavailable in multiplayer.");
    else if(sidebar_group(1,"Time & audio","game speed time scale audio sound slow fast held shortcuts outside menu","LIVE VALUES")) {
        int show=sidebar_row_matches("Game speed");
        sidebar_number("Game speed",1);
        if(show) {
            sidebar_row_bg(95);float fraction=(float)((s.manual_speed-.05)/3.95);
            sidebar_box(31,sidebar_y+15,378,5,0x4c555effu,2);sidebar_box(31,sidebar_y+15,378*fraction,5,UI_ACCENT,2);
            sidebar_box(25+378*fraction,sidebar_y+10,14,14,UI_ACCENT,7);
            sidebar_widget(UI_SLIDER,1,0,0,31,sidebar_y,378,27,"Game speed slider");
            sidebar_text_style(31,sidebar_y+22,110,18,UI_MUTED,"Slower",10,400,0);
            sidebar_text_style(309,sidebar_y+22,100,18,UI_MUTED,"Faster",10,400,2);
            const double speeds[]={.5,1,2,4};const char *presets[]={"0.50\xc3\x97","1.00\xc3\x97","2.00\xc3\x97","4.00\xc3\x97"};
            for(int i=0;i<4;i++)sidebar_button(UI_PRESET,1,0,speeds[i],29+i*96.f,sidebar_y+52,90,28,presets[i],fabs(s.manual_speed-speeds[i])<.001);
            sidebar_y+=95;
        }
        sidebar_toggle("Audio follows game speed",NULL,s.audio_speed,1,HOME,13);
        sidebar_row_bg(40);sidebar_button(UI_GROUP,10,0,0,29,sidebar_y,380,38,sidebar_expanded[10]?"v  Hold-key speeds   [ / ]":">  Hold-key speeds   [ / ]",0);sidebar_y+=40;
        if(sidebar_expanded[10]||sidebar_search[0]) {
            sidebar_text_style(39,sidebar_y,185,23,UI_MUTED,"While holding a shortcut",11,400,0);
            sidebar_text_style(221,sidebar_y,179,23,UI_MUTED,"Release restores game speed",11,400,2);sidebar_y+=24;
            sidebar_held_speed(3,"Slow speed","[");sidebar_held_speed(4,"Fast speed","]");
            sidebar_toggle("Use speed keys with menu closed",NULL,s.speed_keys,1,HOME,8);
        }
        sidebar_group_end();
    } else sidebar_group_closed();
    if(online)sidebar_disabled_group("Player & progression","Unavailable in multiplayer.");
    else if(sidebar_group(2,"Player & progression","god mode infinite ammo checkpoint story progress","3 TOOLS")) {
        sidebar_toggle("God mode",NULL,s.god,available,HOME,11);
        sidebar_toggle("Infinite ammo",NULL,s.infinite_ammo,available,HOME,12);
        if(sidebar_row_matches("Advance to next checkpoint")) {
            sidebar_row_bg(42);
            if(campaign)sidebar_button(UI_ACTION,HOME,5,0,29,sidebar_y+6,380,29,"Advance to next checkpoint   >",0);
            else sidebar_text_face(31,sidebar_y+8,380,21,UI_MUTED,"Checkpoint advance: offline Story only",12,400,0,PC_FONT_UI);
            sidebar_y+=42;sidebar_row_bg(25);sidebar_text_face(31,sidebar_y+2,380,17,UI_MUTED,"The last checkpoint starts the level ending.",11,400,0,PC_FONT_UI);sidebar_y+=25;
        }
        sidebar_group_end();
    } else sidebar_group_closed();

}
static int sidebar_save(void) {
    if(!pc_settings_save(&draft,status,sizeof status))return 0;
    pc_controls_set_mouse(draft.sensitivity,draft.enabled,draft.invert_y);sidebar_dirty=0;
    snprintf(status,sizeof status,"Keyboard and mouse settings saved.");return 1;
}
#include "pc_video_sidebar_ui.inc"
static void sidebar_settings(void) {
    if(page!=KEYBOARD) {char error[160];if(!pc_settings_read(&draft,error,sizeof error))pc_settings_defaults(&draft);}
    if(sidebar_category(11,"Input Settings","input keyboard mouse sensitivity bindings")) {
    if(sidebar_group(4,"Keyboard & mouse","mouse sensitivity invert look keyboard enabled key bindings developer tools shortcut",NULL)) {
        sidebar_number("Mouse sensitivity",2);
        const char *names[]={"Invert vertical look","Keyboard & mouse enabled"};
        for(int i=0;i<2;i++)if(sidebar_row_matches(names[i])) {
            sidebar_row_bg(38);sidebar_text_face(31,sidebar_y+8,325,21,UI_TEXT,names[i],12,400,0,PC_FONT_UI);
            int on=i?draft.enabled:draft.invert_y;sidebar_box(378,sidebar_y+10,32,18,on?UI_ACCENT:0x49535dffu,9);
            sidebar_box(on?394.f:381.f,sidebar_y+13,12,12,on?0x12304bffu:UI_TEXT,6);
            sidebar_widget(UI_SETTING,i,0,0,24,sidebar_y+2,393,34,names[i]);sidebar_y+=38;
        }
        if(sidebar_row_matches("Edit key bindings")) {sidebar_row_bg(42);sidebar_button(UI_ACTION,SETTINGS,0,0,29,sidebar_y+5,380,29,"Edit key bindings   >",0);sidebar_y+=42;}
        if(sidebar_row_matches("Developer Tools shortcut")) {
            sidebar_row_bg(40);sidebar_text_face(31,sidebar_y+9,265,21,UI_TEXT,"Developer Tools shortcut",12,400,0,PC_FONT_UI);
            sidebar_binding(PC_DEVELOPER_ACTION,0,1,321,sidebar_y+5,98,"Developer Tools shortcut");sidebar_y+=40;
        }
        sidebar_group_end();
    } else sidebar_group_closed();
    }
    sidebar_video_settings();
}
static void sidebar_keyboard(void) {
    sidebar_button(UI_TAB,SETTINGS,0,0,17,sidebar_y,408,32,"<  Settings (discard unsaved bindings)",0);sidebar_y+=42;
    sidebar_text_face(26,sidebar_y,382,18,UI_MUTED,"Click a slot, press a key. Delete clears. Save to apply.",11,400,0,PC_FONT_UI);sidebar_y+=28;
    int any=0;for(int i=0;i<PC_ACTION_COUNT;i++)if(sidebar_match(pc_action_labels[i]))any=1;
    if(any) {
        sidebar_group_match=0;
        sidebar_box(17,sidebar_y,408,31,UI_HEADER,4);sidebar_text_face(28,sidebar_y+5,208,20,UI_TEXT,"Action",12,400,0,PC_FONT_UI);
        sidebar_text_face(260,sidebar_y+5,78,20,UI_MUTED,"Primary",12,400,0,PC_FONT_UI);sidebar_text_face(345,sidebar_y+5,78,20,UI_MUTED,"Alternate",12,400,0,PC_FONT_UI);sidebar_y+=31;
        for(int i=0;i<PC_ACTION_COUNT;i++)if(sidebar_match(pc_action_labels[i])) {
            sidebar_row_bg(36);sidebar_text_face(31,sidebar_y+7,220,21,UI_TEXT,pc_action_labels[i],12,400,0,PC_FONT_UI);
            sidebar_binding(i,0,0,259,sidebar_y+4,78,pc_action_labels[i]);sidebar_binding(i,1,0,341,sidebar_y+4,78,pc_action_labels[i]);sidebar_y+=36;
        }
        sidebar_group_end();
    }
    sidebar_button(UI_ACTION,KEYBOARD,PC_ACTION_COUNT+4,0,17,sidebar_y,408,34,"Save and apply bindings",1);sidebar_y+=43;
    sidebar_button(UI_ACTION,KEYBOARD,PC_ACTION_COUNT+3,0,17,sidebar_y,408,32,"Restore defaults (save to apply)",0);sidebar_y+=42;
}
static void sidebar_diagnostics(void) {
    PcDebugOptions options;pc_debug_get(&options);
    PcDebugSnapshot snap;pc_debug_snapshot(&snap);
    if(!sidebar_search[0]||sidebar_match("fps frame time counters")) {
        char value[40];sidebar_box(17,sidebar_y,203,65,UI_CARD,5);sidebar_box(229,sidebar_y,206,65,UI_CARD,5);
        snprintf(value,sizeof value,"%.1f",snap.fps);sidebar_text_face(31,sidebar_y+8,175,29,UI_ACCENT,value,23,400,0,PC_FONT_MONO);
        snprintf(value,sizeof value,"%.2f",snap.frame_ms);sidebar_text_face(241,sidebar_y+8,175,29,UI_ACCENT,value,23,400,0,PC_FONT_MONO);
        sidebar_text_face(31,sidebar_y+42,175,16,UI_MUTED,"GAME FPS",11,400,0,PC_FONT_UI);sidebar_text_face(241,sidebar_y+42,175,16,UI_MUTED,"FRAME INTERVAL (MS)",11,400,0,PC_FONT_UI);sidebar_y+=76;
    }
    char keys[800]="Debug options performance fps frame time issues latency network counter log";
    for(unsigned i=0;i<PC_DEBUG_OPTION_COUNT;i++){size_t used=strlen(keys);snprintf(keys+used,sizeof keys-used," %s",pc_debug_option(i)->label);}
    if(sidebar_group(6,"Debug options",keys,NULL)) {
        for(int i=0;i<PC_DEBUG_OPTION_COUNT;i++) {
            char name[100];snprintf(name,sizeof name,"%s",pc_debug_option(i)->label);
            sidebar_toggle(name,NULL,pc_debug_option_value(&options,i),1,DEBUG,i);
        }
        sidebar_group_end();
    } else sidebar_group_closed();
    if(sidebar_group(8,"Logs","recent diagnostics log folder clear messages",NULL)) {
        const char *names[]={"Recent diagnostics   >","Open logs folder","Clear recent diagnostics"};
        for(int i=0;i<3;i++)if(sidebar_row_matches(names[i])) {
            sidebar_row_bg(40);sidebar_button(UI_ACTION,DEBUG,PC_DEBUG_OPTION_COUNT+i+1,0,29,sidebar_y+5,380,29,names[i],0);sidebar_y+=40;
        }
        sidebar_group_end();
    } else sidebar_group_closed();
}
static void sidebar_build(unsigned width,unsigned height,PcDeveloperLine *out,unsigned capacity) {
    sidebar_width=(float)width;sidebar_height=(float)height;
    sidebar_scale=sidebar_ui_scale((float)width);
    float h=height/sidebar_scale,w=452;
    char message[160];AcquireSRWLockShared(&dev_lock);
    snprintf(message,sizeof message,"%s",status[0]?status:result_message[0]&&GetTickCount64()<result_until?result_message:"");ReleaseSRWLockShared(&dev_lock);
    int show_status=message[0]||sidebar_dirty;
    sidebar_left=sidebar_right?width-w*sidebar_scale:0;
    sidebar_output=out;sidebar_capacity=capacity;sidebar_draw_count=0;sidebar_count=0;sidebar_content=0;sidebar_card=-1;
    /* Small translucent shadow, then a DPI-sized panel independent of height. */
    sidebar_box(0,0,w,h,0x20262cf5u,0);
    for(int i=12;i>0;i--)sidebar_box(sidebar_right?-(float)i:452+(float)i-1,0,1,h,(uint32_t)(13-i),0);sidebar_box(sidebar_right?0.f:451.f,0,1,h,UI_EDGE,0);
    sidebar_box(20,18,35,35,0x80bfff50u,7);sidebar_box(21,19,33,33,0x263645ffu,6);sidebar_icon(26,23,23,UI_ACCENT,PC_DEV_TOOLS);
    sidebar_text_spaced(67,11,280,30,UI_TEXT,"Developer Tools",20,600,0,PC_FONT_UI,-.3f);
    sidebar_text_spaced(67,41,285,17,UI_MUTED,"TIMESPLITTERS \xc2\xb7 FUTURE PERFECT",11,400,0,PC_FONT_UI,.6f);
    sidebar_button(UI_DOCK,0,0,0,359,21,30,30,"",0);sidebar_button(UI_CLOSE,0,0,0,399,21,30,30,"",0);
    sidebar_box(0,71,452,1,UI_EDGE,0);
    const char *tabs[]={"Tools","Settings","Diagnostics"};const int pages[]={HOME,SETTINGS,DEBUG};
    int active=page==HOME?0:page==SETTINGS||page==KEYBOARD?1:2;
    float tab_x=17;
    for(int i=0;i<3;i++) {
        float tab_w=pc_developer_font_measure(tabs[i],13,600,PC_FONT_UI).advance+4;
        int tab_allowed=i!=0||pc_developer_tools_allowed();
        if(tab_allowed)sidebar_widget(UI_TAB,pages[i],0,0,tab_x,72,tab_w,46,tabs[i]);
        sidebar_text_style(tab_x+2,72,tab_w-4,46,!tab_allowed?0x626d77ffu:active==i?UI_ACCENT:UI_MUTED,tabs[i],13,600,0);
        if(active==i)sidebar_box(tab_x,117,tab_w,2,UI_ACCENT,0);
        tab_x+=tab_w+22;
    }
    sidebar_box(0,118,452,1,UI_EDGE,0);
    sidebar_box(19,129,413,35,sidebar_search_edit?UI_ACCENT:UI_EDGE,5);sidebar_box(20,130,411,33,UI_FIELD,4);
    sidebar_icon(29,138,17,UI_MUTED,PC_DEV_SEARCH);
    sidebar_text_style(54,129,343,35,sidebar_search[0]?UI_TEXT:UI_MUTED,sidebar_search[0]?sidebar_search:"Find a tool or setting...",12,400,0);
    sidebar_widget(UI_SEARCH,0,0,0,19,129,375,35,"Search");
    if(sidebar_search[0])sidebar_button(UI_CLEAR,0,0,0,400,133,28,27,"",0);
    AcquireSRWLockShared(&dev_lock);int net=network,playing=gameplay,campaign=story;ReleaseSRWLockShared(&dev_lock);
    sidebar_box(21,177,6,6,net?0xd9b36affu:UI_ACCENT,3);
    sidebar_text_face(34,171,388,18,UI_MUTED,net?(pc_developer_tools_allowed()?"Multiplayer - host allows camera & HUD tools":"Multiplayer - Tools disabled by host"):playing?(campaign?"Offline Story":"Offline gameplay"):"Frontend - gameplay tools unavailable",11,400,0,PC_FONT_UI);
    sidebar_video_sync();
    int video_bar=page==SETTINGS||sidebar_video_snapshot.preview||sidebar_video_wait||(sidebar_search[0]&&sidebar_video_dirty());
    sidebar_top=198;sidebar_bottom=fmaxf(sidebar_top+5,h-(show_status?122:96)-(video_bar?57:0));sidebar_y=sidebar_top-sidebar_scroll[page];sidebar_content=1;
    if(page==KEYBOARD)sidebar_keyboard();
    else if(sidebar_search[0]) {sidebar_tools();sidebar_settings();sidebar_diagnostics();}
    else if(page==HOME)sidebar_tools();else if(page==SETTINGS)sidebar_settings();
    else if(page==DEBUG)sidebar_diagnostics();else {
        sidebar_button(UI_TAB,DEBUG,0,0,17,sidebar_y,408,32,"<  Diagnostics",0);sidebar_y+=42;
        if(!recent_count){sidebar_text_face(27,sidebar_y,380,21,UI_MUTED,"No recent diagnostics.",12,400,0,PC_FONT_UI);sidebar_y+=31;}
        for(int i=0;i<recent_count;i++){sidebar_text_face(27,sidebar_y,396,19,UI_TEXT,recent[i],12,400,0,PC_FONT_UI);sidebar_y+=25;}
    }
    if(sidebar_y==sidebar_top-sidebar_scroll[page]) {
        sidebar_text_face(27,sidebar_y+12,380,22,UI_MUTED,"No matching tools or settings.",12,400,0,PC_FONT_UI);sidebar_y+=46;
    }
    if(sidebar_focus>=0&&(unsigned)sidebar_focus<sidebar_count) {
        DeveloperWidget *v=&sidebar_widgets[sidebar_focus];int content=sidebar_content;sidebar_content=v->content;
        sidebar_box(v->x,v->y,v->w,1,UI_ACCENT,0);sidebar_box(v->x,v->y+v->h-1,v->w,1,UI_ACCENT,0);
        sidebar_box(v->x,v->y,1,v->h,UI_ACCENT,0);sidebar_box(v->x+v->w-1,v->y,1,v->h,UI_ACCENT,0);sidebar_content=content;
    }
    sidebar_max_scroll=fmaxf(0,sidebar_y+sidebar_scroll[page]-sidebar_bottom);
    sidebar_content=0;
    if(sidebar_max_scroll>0) {
        float view=sidebar_bottom-sidebar_top,thumb=fmaxf(25,view*view/(view+sidebar_max_scroll));
        sidebar_box(444,sidebar_top,4,view,0x2b343dffu,2);
        sidebar_box(444,sidebar_top+(view-thumb)*sidebar_scroll[page]/sidebar_max_scroll,4,thumb,0x62717effu,2);
        sidebar_widget(UI_SCROLL,0,0,0,438,sidebar_top,14,view,"Scroll tools");
    }
    if(video_bar)sidebar_video_actions(h-151-(show_status?26:0));
    if(show_status)sidebar_box(0,h-120,452,26,UI_BG,0);
    if(show_status)sidebar_text_style(20,h-119,412,23,edit_number||capture_action>=0?UI_ACCENT:UI_MUTED,message[0]?message:sidebar_dirty?"Bindings changed - Save and apply to keep them.":"Click values to type. Use - / + for precise steps.",10,400,0);
    sidebar_box(0,h-94,452,54,0x2a2720ffu,0);sidebar_box(0,h-94,452,1,0x655333ffu,0);
    sidebar_icon(19,h-82,17,0xe8b761ffu,PC_DEV_WARNING);
    const char *warning="Usage of Developer Tools can cause instability and unintended behavior. Use with caution.";
    float prefix=pc_developer_font_measure("Warning: ",11,600,PC_FONT_UI).advance;
    size_t split=0;for(size_t i=0;warning[i];i++)if(warning[i]==' ') {
        char candidate[160];memcpy(candidate,warning,i);candidate[i]=0;
        if(pc_developer_font_measure(candidate,11,400,PC_FONT_UI).advance+prefix<=386)split=i;else break;
    }
    char first[160];memcpy(first,warning,split);first[split]=0;
    sidebar_text_style(46,h-84,prefix,16.5f,0xf2cc8cffu,"Warning: ",11,600,0);
    sidebar_text_style(46+prefix,h-84,386-prefix,16.5f,0xd7bd91ffu,first,11,400,0);
    sidebar_text_style(46,h-67.5f,386,16.5f,0xd7bd91ffu,warning+split+1,11,400,0);
    sidebar_box(0,h-40,452,40,0x1b2127ffu,0);sidebar_box(0,h-40,452,1,UI_EDGE,0);
    char key[104];pc_developer_shortcut_name(key,sizeof key);
    float key_width=fminf(150,pc_developer_font_measure(key,11,400,PC_FONT_UI).advance+10);
    sidebar_box(20,h-29,key_width,19,UI_EDGE,3);sidebar_box(21,h-28,key_width-2,17,0x1b2127ffu,2);
    sidebar_text_style(20,h-29,key_width,19,UI_TEXT,key,11,400,1);
    sidebar_text_style(28+key_width,h-36,190,32,UI_MUTED,"Close menu",11,400,0);
    sidebar_text_style(262,h-36,168,32,UI_MUTED,page==SETTINGS?"Video changes use Apply":"Changes apply live",11,400,2);
    sidebar_video_popup();

}
static void sidebar_layout(unsigned width,unsigned height,PcDeveloperLine *out,unsigned capacity) {
    sidebar_build(width,height,out,capacity);
    float clamped=fminf(sidebar_max_scroll,fmaxf(0,sidebar_scroll[page]));
    if(clamped!=sidebar_scroll[page]){sidebar_scroll[page]=clamped;sidebar_build(width,height,out,capacity);}
}
static void sidebar_set_value(int id,double value) {
    if((id==1||id==3||id==4)&&network)return;
    if(id==5){sidebar_video_draft.value[VIDEO_FOV]=(int)fmin(100,fmax(40,round(value)));}
    else if(id==1)speed_adjust(value);
    else if(id==2){draft.sensitivity=fmin(10.,fmax(.001,value));if(page==KEYBOARD)sidebar_dirty=1;else sidebar_save();}
    else {AcquireSRWLockExclusive(&dev_lock);if(id==3)state.slow_speed=fmin(4.,fmax(.05,value));else state.fast_speed=fmin(4.,fmax(.05,value));ReleaseSRWLockExclusive(&dev_lock);}
}
static int sidebar_commit_number(void) {
    if(!edit_number)return 1;
    char *end;double v=strtod(number,&end);int id=edit_number;
    if(!*number||*end||!isfinite(v)||v<(id==5?40:id==2?.001:.05)||v>(id==5?100:id==2?10.:4.)) {snprintf(status,sizeof status,"Enter a value from %s.",id==5?"40 to 100":id==2?"0.001 to 10":"0.05 to 4");return 0;}
    sidebar_set_value(id,v);edit_number=0;status[0]=0;return 1;
}
static void sidebar_go(int next) {
    sidebar_video_dropdown=-1;
    navigate(next);sidebar_focus=-1;sidebar_drag=-1;sidebar_search_edit=0;
    if(next!=KEYBOARD)sidebar_dirty=0;
}
static void sidebar_activate(const DeveloperWidget *v,int direction) {
    if(v->kind!=UI_NUMBER&&v->kind!=UI_SEARCH&&edit_number&&!sidebar_commit_number())return;
    sidebar_search_edit=0;
    if(v->kind==UI_ACTION&&v->a==HOME&&!pc_developer_tools_allowed())return;
    switch(v->kind) {
    case UI_VIDEO:sidebar_video_activate(v->a,direction);break;
    case UI_VIDEO_OPTION:sidebar_video_choose(v->a,v->b);sidebar_video_dropdown=-1;break;
    case UI_VIDEO_PAGE:sidebar_video_option_start+=v->a;break;
    case UI_ACTION: {
        if(v->a==KEYBOARD&&v->b==PC_ACTION_COUNT+4){sidebar_save();break;}
        int before=page;page=v->a;selected=v->b;activate(direction);
        if(v->a==KEYBOARD&&v->b==PC_ACTION_COUNT+3)sidebar_dirty=1;
        if(page==v->a)page=before;else sidebar_focus=-1;
        break;}
    case UI_GROUP:sidebar_video_dropdown=-1;sidebar_expanded[v->a]=!sidebar_expanded[v->a];break;
    case UI_TAB:sidebar_search[0]=0;sidebar_go(v->a);break;
    case UI_DOCK:sidebar_right=!sidebar_right;break;
    case UI_CLOSE:pc_developer_close();break;
    case UI_SEARCH:sidebar_search_edit=1;edit_number=0;capture_action=-1;sidebar_select_all=0;break;
    case UI_CLEAR:sidebar_search[0]=0;sidebar_search_edit=1;sidebar_scroll[page]=0;break;
    case UI_NUMBER:edit_number=v->a;capture_action=-1;sidebar_select_all=1;snprintf(number,sizeof number,v->a==2?"%.4g":"%.2f",sidebar_value(v->a));snprintf(status,sizeof status,"Type a value; Enter applies, Escape cancels.");break;
    case UI_STEP:sidebar_set_value(v->a,sidebar_value(v->a)+(v->a==5?1:v->a==2?.01:.1)*(direction?direction:v->b));break;
    case UI_PRESET:sidebar_set_value(v->a,v->value);break;
    case UI_BIND: {
        if(v->value){char error[160];if(!pc_settings_read(&draft,error,sizeof error))pc_settings_defaults(&draft);}
        capture_action=v->a;slot=v->b;sidebar_quick_bind=(int)v->value;conflict_key=0;edit_number=0;
        snprintf(status,sizeof status,"Press a key. Delete clears; Escape cancels.");break;}
    case UI_SETTING:if(v->a)draft.enabled=!draft.enabled;else draft.invert_y=!draft.invert_y;sidebar_save();break;
    default:break;
    }
}
static void sidebar_capture_done(void) {
    if(sidebar_quick_bind) {if(!sidebar_save()) {capture_action=-1;return;}}
    else sidebar_dirty=1;
    capture_action=-1;conflict_key=0;status[0]=0;
}
static int sidebar_edge(const unsigned char keys[256],int key) {return keys[key]&&!previous[key];}
static void sidebar_text_keys(const unsigned char keys[256]) {
    char *text=edit_number?number:sidebar_search;size_t max=edit_number?sizeof number:sizeof sidebar_search;
    if(keys[0x11]&&sidebar_edge(keys,'A')){sidebar_select_all=1;return;}
    ULONGLONG now=GetTickCount64();
    for(int key=8;key<256;key++)if(keys[key]) {
        int edge=sidebar_edge(keys,key);
        if(!edge && !(key==sidebar_repeat_key&&now>=sidebar_repeat_at))continue;
        int c=0;
        if(key==8)c=8;else if(key>='0'&&key<='9')c=key;else if(key>=0x60&&key<=0x69)c='0'+key-0x60;
        else if(key==0xbe||key==0x6e)c='.';
        else if(!edit_number){if(key>='A'&&key<='Z')c=key+'a'-'A';else if(key==32)c=' ';else if(key==0xbd)c='-';}
        if(!c||keys[0x11])continue;
        size_t length=strlen(text);
        if(sidebar_select_all){text[0]=0;length=0;sidebar_select_all=0;}
        if(c==8){if(length)text[length-1]=0;}
        else if(length+1<max){text[length]=(char)c;text[length+1]=0;}
        sidebar_repeat_key=(unsigned char)key;sidebar_repeat_at=now+(edge?350:55);
        if(!edit_number)sidebar_scroll[page]=0;
        break;
    }
}
void pc_developer_frame(void) {
    unsigned char keys[256]={0};int focused=pc_controls_developer_keys(keys);
    int shortcut=0;
    for(int k=1;k<256;k++)if(focused&&sidebar_edge(keys,k)&&pc_developer_shortcut_key(k)){shortcut=1;break;}
    if(shortcut){pc_developer_toggle();memcpy(previous,keys,sizeof previous);return;}
    int open=pc_developer_open();
    if(!open||!focused){sidebar_video_dropdown=-1;capture_action=-1;edit_number=0;sidebar_search_edit=0;sidebar_drag=-1;}
    if(open&&focused) {
        PcMenuPointer p=pc_controls_developer_pointer();
        float width=p.valid&&p.width>0?p.width:sidebar_width,height=p.valid&&p.height>0?p.height:sidebar_height;
        float ui_scale=sidebar_ui_scale(width);
        sidebar_pointer_x=p.valid?(p.x-(sidebar_right?width-452*ui_scale:0))/ui_scale:-1;
        sidebar_pointer_y=p.valid?p.y/ui_scale:-1;
        sidebar_layout((unsigned)width,(unsigned)height,NULL,0);
        int enter=sidebar_edge(keys,13),escape=sidebar_edge(keys,27);
        if(capture_action>=0) {
            if(escape){capture_action=-1;conflict_key=0;status[0]=0;}
            else if(conflict_key&&enter){pc_settings_assign(&draft,capture_action,slot,conflict_key,1);sidebar_capture_done();}
            else if(!conflict_key)for(int k=1;k<256;k++)if(sidebar_edge(keys,k)) {
                int key=k==0x2e?0:k,result=pc_settings_assign(&draft,capture_action,slot,key,0);
                if(result<0){conflict_key=key;snprintf(status,sizeof status,"Already bound. Enter moves it; Escape cancels.");}
                else if(result)sidebar_capture_done();else snprintf(status,sizeof status,"That key is reserved. Choose another.");break;
            }
        } else {
            if(escape) {
                if(sidebar_video_snapshot.preview)sidebar_video_send(VIDEO_REVERT);
                else if(sidebar_video_dropdown>=0)sidebar_video_dropdown=-1;
                else if(edit_number){edit_number=0;status[0]=0;}
                else if(sidebar_search_edit||sidebar_search[0]){sidebar_search[0]=0;sidebar_search_edit=0;}
                else if(page==KEYBOARD||page==LOGS)sidebar_go(page==KEYBOARD?SETTINGS:DEBUG);
                else pc_developer_close();
            } else if(edit_number||sidebar_search_edit) {
                if(enter){if(edit_number)sidebar_commit_number();else sidebar_search_edit=0;enter=0;}
                else sidebar_text_keys(keys);
            }
            if(keys[0x11]&&sidebar_edge(keys,'F')){sidebar_search_edit=1;edit_number=0;sidebar_select_all=1;}
            if(keys[0x11]&&sidebar_edge(keys,9)){int tab=page==HOME?0:page==SETTINGS||page==KEYBOARD?1:2;sidebar_go((int[]){HOME,SETTINGS,DEBUG}[(tab+(keys[0x10]?2:1))%3]);}
            if(!edit_number&&!sidebar_search_edit) {
                int delta=sidebar_edge(keys,0x28)?1:sidebar_edge(keys,0x26)?-1:sidebar_edge(keys,9)?(keys[0x10]?-1:1):0;
                if(delta&&sidebar_count) {
                    if(sidebar_focus<0)sidebar_focus=delta<0?0:-1;
                    sidebar_focus=(sidebar_focus+delta+(int)sidebar_count)%(int)sidebar_count;
                    DeveloperWidget *v=&sidebar_widgets[sidebar_focus];
                    if(v->content) {if(v->y<sidebar_top)sidebar_scroll[page]-=sidebar_top-v->y;else if(v->y+v->h>sidebar_bottom)sidebar_scroll[page]+=v->y+v->h-sidebar_bottom;}
                }
                if(sidebar_edge(keys,0x22))sidebar_scroll[page]+=sidebar_bottom-sidebar_top-30;
                if(sidebar_edge(keys,0x21))sidebar_scroll[page]-=sidebar_bottom-sidebar_top-30;
                if(sidebar_focus>=0&&(unsigned)sidebar_focus<sidebar_count) {
                    DeveloperWidget v=sidebar_widgets[sidebar_focus];int dir=sidebar_edge(keys,0x27)?1:sidebar_edge(keys,0x25)?-1:0;
                    if(enter)sidebar_activate(&v,0);
                    else if(dir) {
                        if(v.kind==UI_NUMBER||v.kind==UI_SLIDER)sidebar_set_value(v.a,sidebar_value(v.a)+dir*(v.a==5?1:v.a==2?.01:.1));
                        else sidebar_activate(&v,dir);
                    }
                }
            }
            if(p.valid&&sidebar_pointer_x>=0&&sidebar_pointer_x<=452) {
                if(p.wheel&&sidebar_pointer_y>=sidebar_top&&sidebar_pointer_y<sidebar_bottom)sidebar_scroll[page]-=p.wheel*48;
                if(p.left&&!previous_left)for(unsigned remaining=sidebar_count;remaining>0;remaining--) {
                    unsigned i=remaining-1;
                    DeveloperWidget v=sidebar_widgets[i];
                    if(sidebar_pointer_x<v.x||sidebar_pointer_x>=v.x+v.w||sidebar_pointer_y<v.y||sidebar_pointer_y>=v.y+v.h||
                       (v.content&&(sidebar_pointer_y<sidebar_top||sidebar_pointer_y>=sidebar_bottom)))continue;
                    if(sidebar_video_dropdown>=0&&v.kind!=UI_VIDEO_OPTION&&v.kind!=UI_VIDEO_PAGE){sidebar_video_dropdown=-1;break;}
                    sidebar_focus=(int)i;
                    if(v.kind==UI_SLIDER||v.kind==UI_SCROLL)sidebar_drag=(int)i;
                    else sidebar_activate(&v,0);
                    break;
                }
            }
            if(p.valid&&p.left&&sidebar_drag>=0&&(unsigned)sidebar_drag<sidebar_count) {
                DeveloperWidget v=sidebar_widgets[sidebar_drag];
                if(v.kind==UI_SLIDER){float fraction=fmaxf(0,fminf(1,(sidebar_pointer_x-v.x)/v.w));sidebar_set_value(v.a,v.a==5?40+round(fraction*60):.05+round(fraction*79)*.05);}
                else if(v.kind==UI_SCROLL)sidebar_scroll[page]=sidebar_max_scroll*fmaxf(0,fminf(1,(sidebar_pointer_y-v.y)/v.h));
            } else sidebar_drag=-1;
        }
        previous_left=p.left;
    } else previous_left=0;
    AcquireSRWLockExclusive(&dev_lock);state.speed=network?1.:state.manual_speed;
    if(focused&&(gameplay||movie_active)&&!network&&(open||state.speed_keys)&&capture_action<0&&!edit_number&&!sidebar_search_edit) {
        if(keys[0xdd])state.speed=state.fast_speed;else if(keys[0xdb])state.speed=state.slow_speed;
    }
    ReleaseSRWLockExclusive(&dev_lock);
    if(audio_set_speed)audio_set_speed((float)pc_developer_audio_speed());
    if(!open&&focused&&pc_developer_enabled()&&!pc_settings_capture_blocked()) {
        if(action_pressed(PC_NOCLIP_ACTION,keys))flight_toggle(1);if(action_pressed(PC_FREECAM_ACTION,keys))flight_toggle(2);
    }
    InterlockedExchange(&shortcut_blocked,capture_action>=0||edit_number);memcpy(previous,keys,sizeof previous);
}
unsigned pc_developer_lines(unsigned width,unsigned height,PcDeveloperLine *out,unsigned capacity) {
    if(!out||!capacity||width<100||height<100)return 0;
    if(!pc_developer_open()) {
        /* Preserve the dormant named disclaimer without putting it back in the
         * lobby. Player availability is rendered by the native wrench path. */
        if(!lobby_disclaimer_enabled)return 0;
        AcquireSRWLockShared(&dev_lock);int show=lobby_notice[0]&&GetTickCount64()-lobby_notice_at<1000;
        if(show){memset(out,0,sizeof *out);snprintf(out->text,sizeof out->text,"Developer Tools: %.130s",lobby_notice);out->x=10;out->y=height-30.f;out->width=width-20.f;out->height=24;}
        ReleaseSRWLockShared(&dev_lock);return show?1:0;
    }
    if(capacity>PC_DEVELOPER_MAX_DRAW)capacity=PC_DEVELOPER_MAX_DRAW;
    sidebar_layout(width,height,out,capacity);return sidebar_draw_count;
}
