/* Shared sidebar presentation; the game-thread adapter owns actual settings. */
#include "pc_frame_rate_choices.h"
enum { VIDEO_ASPECT=100,VIDEO_RESOLUTION,VIDEO_APPLY_BUTTON,VIDEO_REVERT_BUTTON,
       VIDEO_KEEP_BUTTON,VIDEO_CACHE_BUTTON,VIDEO_RESTART_BUTTON };
static PcVideoSnapshot sidebar_video_snapshot;
static PcVideoValues sidebar_video_draft;
static unsigned sidebar_video_wait;
static int sidebar_video_ready,sidebar_video_aspect,sidebar_video_dropdown=-1,sidebar_video_option_start;
static float sidebar_video_anchor;
static double sidebar_video_fov(void) {return sidebar_video_draft.value[VIDEO_FOV];}
static int sidebar_video_dirty(void) {
    return sidebar_video_ready&&memcmp(&sidebar_video_draft,&sidebar_video_snapshot.current,sizeof sidebar_video_draft)!=0;
}
static void sidebar_video_sync(void) {
    int dirty=sidebar_video_dirty();PcVideoSnapshot next;pc_developer_video_snapshot(&next);
    if(sidebar_video_wait&&next.completed==sidebar_video_wait) {
        sidebar_video_wait=0;
        if(strstr(next.message,"Could not")||strstr(next.message,"Invalid")||strstr(next.message,"Cannot"))snprintf(status,sizeof status,"%s",next.message);else status[0]=0;
        if(!next.preview){sidebar_video_draft=next.current;dirty=0;}
    }
    if(next.available&&(!sidebar_video_ready||(!dirty&&!sidebar_video_wait&&!next.preview)))sidebar_video_draft=next.current;
    sidebar_video_snapshot=next;sidebar_video_ready=next.available;
}
static void sidebar_video_send(int action) {
    if(sidebar_video_wait)return;
    sidebar_video_wait=pc_developer_video_request(action,&sidebar_video_draft);
    sidebar_video_dropdown=-1;
}
static int sidebar_category(int id,const char *name,const char *keywords) {
    if(!sidebar_match(name)&&!sidebar_match(keywords))return 0;
    int open=sidebar_expanded[id]||sidebar_search[0];
    sidebar_widget(UI_GROUP,id,0,0,17,sidebar_y,408,37,name);
    sidebar_icon(19,sidebar_y+10,15,UI_MUTED,open?PC_DEV_DOWN:PC_DEV_RIGHT);
    sidebar_text_style(39,sidebar_y+5,370,26,UI_TEXT,name,14,600,0);
    sidebar_box(17,sidebar_y+36,408,1,UI_EDGE,0);sidebar_y+=47;return open;
}
static int sidebar_video_options(int field,int values[128],char labels[128][80]) {
    int count=0;
#define OPTION(v,t) do {values[count]=(v);snprintf(labels[count++],80,"%s",(t));} while(0)
    if(field==VIDEO_ASPECT) {
        const char *names[]={"All",
#define PC_DISPLAY_ASPECT(label,w,h) label,
#define PC_DISPLAY_PRESET(w,h)
#include "pc_display_modes.inc"
#undef PC_DISPLAY_PRESET
#undef PC_DISPLAY_ASPECT
        };
        for(int i=0;i<8;i++)OPTION(i,names[i]);
    } else if(field==VIDEO_RESOLUTION) {
        static const int ratios[][2]={{0,0},
#define PC_DISPLAY_ASPECT(label,w,h) {w,h},
#define PC_DISPLAY_PRESET(w,h)
#include "pc_display_modes.inc"
#undef PC_DISPLAY_PRESET
#undef PC_DISPLAY_ASPECT
        };
        for(unsigned i=0;i<sidebar_video_snapshot.mode_count&&count<128;i++) {
            unsigned w=sidebar_video_snapshot.modes[i][0],h=sidebar_video_snapshot.modes[i][1];
            int a=sidebar_video_aspect;
            if(a==7&&w!=1)continue;
            if(a>0&&a<7&&w>1&&llabs((long long)w*ratios[a][1]-(long long)h*ratios[a][0])>(long long)ratios[a][0])continue;
            values[count]=(int)i;
            if(!w)snprintf(labels[count],80,"Original");else if(w==1)snprintf(labels[count],80,"Auto");
            else snprintf(labels[count],80,"%u \xc3\x97 %u",w,h);count++;
        }
    } else if(field==VIDEO_RATE) {
        for(int i=0;i<PC_FRAME_RATE_COUNT;i++)OPTION(pc_frame_rate_value(i),pc_frame_rate_label(i));
    } else if(field==VIDEO_TEXTURE) {OPTION(0,"Classic");OPTION(1,"Enhanced");}
    else if(field==VIDEO_AF||field==VIDEO_MSAA) {
        if(field==VIDEO_AF)OPTION(1,"Original");else OPTION(1,"Off");
        OPTION(2,"2\xc3\x97");OPTION(4,"4\xc3\x97");OPTION(8,"8\xc3\x97");if(field==VIDEO_AF)OPTION(16,"16\xc3\x97");
    } else if(field==VIDEO_AA) {OPTION(0,"Off");OPTION(1,"FXAA");OPTION(2,"SMAA");}
    else if(field==VIDEO_SS) {OPTION(0,"100% (Off)");OPTION(1,"125%");OPTION(2,"150%");OPTION(3,"200%");}
#undef OPTION
    return count;
}
static int sidebar_video_selected(int field) {
    if(field==VIDEO_ASPECT)return sidebar_video_aspect;
    if(field==VIDEO_RESOLUTION) {
        for(unsigned i=0;i<sidebar_video_snapshot.mode_count;i++)if(sidebar_video_snapshot.modes[i][0]==sidebar_video_draft.width&&sidebar_video_snapshot.modes[i][1]==sidebar_video_draft.height)return (int)i;
        return -1;
    }
    return sidebar_video_draft.value[field];
}
static void sidebar_video_choose(int field,int value) {
    if(field==VIDEO_ASPECT) {
        sidebar_video_aspect=value;
        int values[128];char labels[128][80];int count=sidebar_video_options(VIDEO_RESOLUTION,values,labels),best=-1;
        unsigned long long distance=~0ull;
        for(int i=0;i<count;i++) {
            int index=values[i];unsigned w=sidebar_video_snapshot.modes[index][0],h=sidebar_video_snapshot.modes[index][1];
            if(w==sidebar_video_draft.width&&h==sidebar_video_draft.height)return;
            if(value!=7&&w<2)continue;
            unsigned long long score=((unsigned long long)llabs((long long)h-sidebar_video_draft.height)<<32)+(unsigned long long)llabs((long long)w-sidebar_video_draft.width);
            if(score<distance){distance=score;best=index;}
        }
        if(best>=0){sidebar_video_draft.width=sidebar_video_snapshot.modes[best][0];sidebar_video_draft.height=sidebar_video_snapshot.modes[best][1];}
    }
    else if(field==VIDEO_RESOLUTION) {
        if(value>=0&&(unsigned)value<sidebar_video_snapshot.mode_count){sidebar_video_draft.width=sidebar_video_snapshot.modes[value][0];sidebar_video_draft.height=sidebar_video_snapshot.modes[value][1];}
    } else if(field>=0&&field<VIDEO_VALUE_COUNT)sidebar_video_draft.value[field]=value;
}
static void sidebar_video_activate(int field,int direction) {
    if(field==VIDEO_REVERT_BUTTON) {
        if(sidebar_video_snapshot.preview)sidebar_video_send(VIDEO_REVERT);
        else {sidebar_video_draft=sidebar_video_snapshot.current;sidebar_video_dropdown=-1;}return;
    }
    if(sidebar_video_wait||!sidebar_video_ready)return;
    if(field==VIDEO_KEEP_BUTTON){sidebar_video_send(VIDEO_KEEP);return;}
    if(sidebar_video_snapshot.preview)return;
    if(field==VIDEO_APPLY_BUTTON){sidebar_video_send(VIDEO_APPLY);return;}
    if(field==VIDEO_CACHE_BUTTON){sidebar_video_send(VIDEO_CLEAR_CACHE);return;}
    if(field==VIDEO_RESTART_BUTTON){sidebar_video_send(VIDEO_RESTART);return;}
    if(field==VIDEO_FULLSCREEN||field==VIDEO_AO||field==VIDEO_CACHE||field==VIDEO_SUBTITLES||field==VIDEO_FEED||field==VIDEO_COUNTER) {
        sidebar_video_draft.value[field]=!sidebar_video_draft.value[field];return;
    }
    if(direction) {
        int values[128];char labels[128][80];int count=sidebar_video_options(field,values,labels),index=-1,current=sidebar_video_selected(field);
        for(int i=0;i<count;i++)if(values[i]==current)index=i;
        if(count)sidebar_video_choose(field,values[(index+direction+count)%count]);sidebar_video_dropdown=-1;
    } else {sidebar_video_dropdown=sidebar_video_dropdown==field?-1:field;sidebar_video_option_start=0;}
}
static void sidebar_video_choice(const char *label,int field,int restart,int available) {
    if(!sidebar_row_matches(label))return;
    sidebar_text_style(31,sidebar_y+(restart?3:9),204,20,available?UI_TEXT:UI_MUTED,label,12,400,0);
    if(restart)sidebar_text_style(31,sidebar_y+22,204,15,0xd7bd91ffu,"Restart required",9,400,0);
    int values[128];char labels[128][80],text[80]="Custom";int count=sidebar_video_options(field,values,labels),value=sidebar_video_selected(field);
    for(int i=0;i<count;i++)if(values[i]==value)snprintf(text,sizeof text,"%s",labels[i]);
    if(field==VIDEO_RESOLUTION&&value<0)snprintf(text,sizeof text,"%u x %u",sidebar_video_draft.width,sidebar_video_draft.height);
    sidebar_box(245,sidebar_y+6,164,30,UI_EDGE,4);sidebar_box(246,sidebar_y+7,162,28,UI_FIELD,3);
    sidebar_text_style(253,sidebar_y+6,132,30,available?UI_TEXT:UI_MUTED,text,11,400,0);
    sidebar_icon(388,sidebar_y+14,13,UI_MUTED,PC_DEV_DOWN);
    if(available&&!sidebar_video_wait&&!sidebar_video_snapshot.preview)sidebar_widget(UI_VIDEO,field,0,0,245,sidebar_y+6,164,30,label);
    if(sidebar_video_dropdown==field)sidebar_video_anchor=sidebar_y+39;
    sidebar_y+=42;
}
static void sidebar_video_toggle(const char *label,int field,int available) {
    if(!sidebar_row_matches(label))return;
    int on=sidebar_video_draft.value[field];sidebar_text_style(31,sidebar_y+8,325,21,available?UI_TEXT:UI_MUTED,label,12,400,0);
    sidebar_box(378,sidebar_y+10,32,18,on?UI_ACCENT:0x49535dffu,9);sidebar_box(on?394.f:381.f,sidebar_y+13,12,12,on?0x12304bffu:UI_TEXT,6);
    if(available&&!sidebar_video_wait&&!sidebar_video_snapshot.preview)sidebar_widget(UI_VIDEO,field,0,0,24,sidebar_y+2,393,34,label);sidebar_y+=38;
}
static void sidebar_video_settings(void) {
    if(!sidebar_category(12,"Video Settings","video display aspect resolution frame rate fullscreen graphics texture filtering ambient occlusion anti-aliasing fov field of view show hide hud ui version watermark subtitles kill feed fps ping advanced msaa supersampling shader cache"))return;
    if(!sidebar_video_ready){sidebar_text_style(31,sidebar_y,380,24,UI_MUTED,"Video settings will be available after startup.",11,400,0);sidebar_y+=34;return;}
    int effects=sidebar_video_snapshot.effects;
    if(sidebar_group(13,"Display","aspect resolution frame rate fullscreen",NULL)) {
        sidebar_video_choice("Aspect ratio",VIDEO_ASPECT,0,1);sidebar_video_choice("Resolution",VIDEO_RESOLUTION,0,1);
        sidebar_video_choice("Frame rate",VIDEO_RATE,0,1);sidebar_video_toggle("Fullscreen",VIDEO_FULLSCREEN,1);
        if(sidebar_row_matches("Frame rate")){sidebar_text_style(31,sidebar_y+4,380,20,UI_MUTED,"High refresh is in beta. Gameplay issues may occur.",10,400,0);sidebar_y+=28;}sidebar_group_end();
    } else sidebar_group_closed();
    if(sidebar_group(14,"Graphics","texture style enhanced classic anisotropic filtering ambient occlusion post-process anti-aliasing fxaa smaa",NULL)) {
        sidebar_video_choice("Texture style",VIDEO_TEXTURE,0,1);sidebar_video_choice("Anisotropic filtering",VIDEO_AF,0,effects);
        sidebar_video_toggle("Ambient occlusion",VIDEO_AO,effects);sidebar_video_choice("Post-process anti-aliasing",VIDEO_AA,0,effects);
        if(!effects){sidebar_text_style(31,sidebar_y,380,20,UI_MUTED,"These effects require Vulkan.",10,400,0);sidebar_y+=24;}sidebar_group_end();
    } else sidebar_group_closed();
    if(sidebar_group(15,"View & HUD","vertical field of view fov show hide hud ui version watermark subtitles kill feed fps ping",NULL)) {
        if(sidebar_row_matches("Vertical field of view")) {
            sidebar_text_style(31,sidebar_y+5,260,21,UI_TEXT,"Vertical field of view",12,400,0);
            sidebar_text_style(280,sidebar_y+5,129,21,UI_MUTED,"40\xe2\x80\x93" "100\xc2\xb0",11,400,2);sidebar_y+=28;
            char value[32];if(edit_number==5)snprintf(value,sizeof value,"%s",number);else snprintf(value,sizeof value,"%d",sidebar_video_draft.value[VIDEO_FOV]);
            sidebar_box(29,sidebar_y,163,34,UI_EDGE,5);sidebar_box(30,sidebar_y+1,161,32,UI_FIELD,4);
            if(!sidebar_video_wait&&!sidebar_video_snapshot.preview) {
                sidebar_widget(UI_STEP,5,-1,0,30,sidebar_y+1,32,32,"Decrease field of view");sidebar_widget(UI_NUMBER,5,0,0,63,sidebar_y+1,95,32,"Vertical field of view");sidebar_widget(UI_STEP,5,1,0,159,sidebar_y+1,32,32,"Increase field of view");
            }
            sidebar_box(30,sidebar_y+1,32,32,0x2c353effu,3);sidebar_text_style(30,sidebar_y+1,32,32,UI_TEXT,"-",18,400,1);
            sidebar_text_face(64,sidebar_y+1,77,32,UI_TEXT,value,17,700,1,PC_FONT_MONO);sidebar_text_style(140,sidebar_y+1,16,32,UI_MUTED,"\xc2\xb0",11,400,1);
            sidebar_box(159,sidebar_y+1,32,32,0x2c353effu,3);sidebar_text_style(159,sidebar_y+1,32,32,UI_TEXT,"+",18,400,1);
            if(!sidebar_video_wait&&!sidebar_video_snapshot.preview)sidebar_button(UI_PRESET,5,0,55,200,sidebar_y,34,34,"Reset field of view",0);
            sidebar_text_style(250,sidebar_y,159,34,UI_MUTED,"Default: 55\xc2\xb0",11,400,2);sidebar_y+=40;
            float fraction=(sidebar_video_draft.value[VIDEO_FOV]-40)/60.f;
            sidebar_box(31,sidebar_y+10,378,5,0x4c555effu,2);sidebar_box(31,sidebar_y+10,378*fraction,5,UI_ACCENT,2);sidebar_box(25+378*fraction,sidebar_y+5,14,14,UI_ACCENT,7);
            if(!sidebar_video_wait&&!sidebar_video_snapshot.preview)sidebar_widget(UI_SLIDER,5,0,0,31,sidebar_y,378,27,"Field of view slider");
            sidebar_text_style(31,sidebar_y+22,110,18,UI_MUTED,"Narrower",10,400,0);sidebar_text_style(309,sidebar_y+22,100,18,UI_MUTED,"Wider",10,400,2);sidebar_y+=44;
        }
        PcDeveloperState visibility;pc_developer_get(&visibility);
        sidebar_toggle("HUD & UI",NULL,!visibility.hide_hud,1,SETTINGS,4);
        sidebar_toggle("Version watermark",NULL,!visibility.hide_watermark,1,SETTINGS,5);
        sidebar_video_toggle("Subtitles",VIDEO_SUBTITLES,1);sidebar_video_toggle("Kill Feed",VIDEO_FEED,1);sidebar_video_toggle("FPS / Ping",VIDEO_COUNTER,1);sidebar_group_end();
    } else sidebar_group_closed();
    if(sidebar_group(16,"Advanced","msaa supersampling shader cache clear",NULL)) {
        sidebar_video_choice("MSAA",VIDEO_MSAA,1,effects);sidebar_video_choice("Supersampling",VIDEO_SS,1,effects);sidebar_video_toggle("Shader cache",VIDEO_CACHE,effects);
        if(!sidebar_video_wait&&!sidebar_video_snapshot.preview)sidebar_button(UI_VIDEO,VIDEO_CACHE_BUTTON,0,0,29,sidebar_y+4,380,29,"Clear shader cache",0);sidebar_y+=42;
        sidebar_text_style(31,sidebar_y,380,20,UI_MUTED,"MSAA and supersampling changes require a restart.",10,400,0);sidebar_y+=25;sidebar_group_end();
    } else sidebar_group_closed();
}
static void sidebar_video_actions(float y) {
    sidebar_box(0,y,452,57,0x1b2229ffu,0);sidebar_box(0,y,452,1,UI_EDGE,0);
    int dirty=sidebar_video_dirty();char message[64];
    snprintf(message,sizeof message,sidebar_video_snapshot.preview?"Keep resolution? %us left":sidebar_video_wait?"Applying video settings...":dirty?"Pending video changes":sidebar_video_snapshot.restart?"Restart required":"No pending changes",sidebar_video_snapshot.seconds);
    sidebar_text_style(20,y+7,235,20,UI_TEXT,message,11,400,0);
    const char *help=sidebar_video_snapshot.message[0]?sidebar_video_snapshot.message:"Input settings apply immediately";
    sidebar_text_style(20,y+29,235,18,UI_MUTED,help,10,400,0);
    if(sidebar_video_wait)return;
    if(sidebar_video_snapshot.preview) {
        sidebar_button(UI_VIDEO,VIDEO_REVERT_BUTTON,0,0,265,y+13,74,30,"Revert",0);sidebar_button(UI_VIDEO,VIDEO_KEEP_BUTTON,0,0,347,y+13,83,30,"Keep",1);
    } else if(dirty) {
        sidebar_button(UI_VIDEO,VIDEO_REVERT_BUTTON,0,0,265,y+13,74,30,"Revert",0);sidebar_button(UI_VIDEO,VIDEO_APPLY_BUTTON,0,0,347,y+13,83,30,"Apply",1);
    } else if(sidebar_video_snapshot.restart)sidebar_button(UI_VIDEO,VIDEO_RESTART_BUTTON,0,0,315,y+13,115,30,"Restart game",0);
    else {
        sidebar_box(265,y+13,74,30,UI_EDGE,4);sidebar_box(266,y+14,72,28,0x202930ffu,3);
        sidebar_text_style(265,y+13,74,30,0x74818bffu,"Revert",12,400,1);
        sidebar_box(347,y+13,83,30,0x3c5872ffu,4);sidebar_box(348,y+14,81,28,0x334e68ffu,3);
        sidebar_text_style(347,y+13,83,30,0x8b9cacffu,"Apply",12,600,1);
    }
}
static void sidebar_video_popup(void) {
    if(sidebar_video_dropdown<0||(page!=SETTINGS&&!sidebar_search[0])||(!sidebar_expanded[12]&&!sidebar_search[0]))return;
    int values[128];char labels[128][80];int count=sidebar_video_options(sidebar_video_dropdown,values,labels);
    int visible=(int)fminf(7,fmaxf(1,floorf((sidebar_bottom-sidebar_top-40)/28)));
    if(visible>count)visible=count;if(!visible)return;
    sidebar_video_option_start=(int)fmaxf(0,fminf((float)(count-visible),(float)sidebar_video_option_start));
    float height=visible*28.f+(count>visible?30:0)+4,y=fmaxf(sidebar_top,fminf(sidebar_video_anchor,sidebar_bottom-height));
    sidebar_box(243,y-2,170,height+4,UI_EDGE,5);sidebar_box(244,y-1,168,height+2,UI_FIELD,4);
    for(int i=0;i<visible;i++) {
        int index=sidebar_video_option_start+i;float row=y+2+i*28;
        if(sidebar_video_selected(sidebar_video_dropdown)==values[index])sidebar_box(246,row,164,28,0x263645ffu,2);
        sidebar_widget(UI_VIDEO_OPTION,sidebar_video_dropdown,values[index],0,246,row,164,28,labels[index]);sidebar_text_style(253,row,150,28,UI_TEXT,labels[index],11,400,0);
    }
    if(count>visible){sidebar_button(UI_VIDEO_PAGE,-visible,0,0,246,y+visible*28+3,80,26,"Previous",0);sidebar_button(UI_VIDEO_PAGE,visible,0,0,329,y+visible*28+3,80,26,"Next",0);}
}
