/* The title's own layout, transitions, row callbacks and footer navigation.
 * All callbacks occupy verified unused INT3 bytes in the supported US XBE. */
#include "pc_settings.h"
#include "pc_keyboard_menu_model.h"
#define KB_BUILD 0x1201cu
#define KB_TICK 0x1201du
#define KB_SELECT 0x1201eu
#define KB_BACK 0x1201fu
#define KB_DRAW 0x12017u
#define KB_TEXT_BYTES 192u
#define KB_TEXT(i) (kb_storage+32u+(uint32_t)(i)*KB_TEXT_BYTES)
enum { KB_HOME,KB_MOUSE,KB_LIST,KB_ACTION,KB_CAPTURE,KB_CONFLICT,KB_CLEAR,KB_ESCAPE,KB_RESET,KB_ERROR };
static uint32_t kb_storage,kb_parent,kb_previous,kb_menu,kb_enabled_row,kb_invert_row;
static int kb_page,kb_group,kb_offset,kb_action,kb_slot,kb_key,kb_conflict,kb_capture_pending;
static PcSettings kb_settings,kb_saved;
static uint64_t kb_mouse_changed;
static char kb_status[192];
static void kb_refresh(int page,int selected);
static uint32_t kb_text(unsigned slot,const char *format,...)
{
    va_list args;va_start(args,format);
    vsnprintf((char *)((uintptr_t)KB_TEXT(slot)+g_xbox_mem_offset),KB_TEXT_BYTES,format,args);
    va_end(args);return KB_TEXT(slot);
}
static uint32_t kb_caption(void)
{
    if(!kb_storage) {
        kb_storage=xbox_HeapAlloc(32+32*KB_TEXT_BYTES,4);
        if(kb_storage) {
            memset((void *)((uintptr_t)kb_storage+g_xbox_mem_offset),0,32+32*KB_TEXT_BYTES);
            WORD_AT(kb_storage)=KB_BUILD;WORD_AT(kb_storage+4)=KB_TICK;
            kb_text(31,"Keyboard and Mouse");
        }
    }
    return kb_storage?KB_TEXT(31):0;
}
static uint32_t kb_row(int id,const char *label,int note)
{
    uint32_t item=guest_call(sub_00079D60_gen,6,kb_text((unsigned)id+1,"\vs%d%s\vs100",note?65:80,label),
        (uint32_t)id,note?4u:0u,0u,note?0u:KB_SELECT,0u);
    if(guest_writable(item,0x2c))WORD_AT(item+0x28)&=~0x2000u;
    return item;
}
static int kb_save(void)
{
    char error[320];
    if(pc_settings_save(&kb_settings,error,sizeof error)) {
        kb_saved=kb_settings;*kb_status=0;kb_mouse_changed=0;return 1;
    }
    kb_settings=kb_saved;kb_mouse_changed=0;
    pc_controls_set_mouse(kb_saved.sensitivity,kb_saved.enabled,kb_saved.invert_y);
    snprintf(kb_status,sizeof kb_status,"Could not save. Previous controls kept.");
    fprintf(stderr,"[PC-KEYBOARD-MENU] %s\n",error);return 0;
}
static void kb_flush_mouse(void) { if(kb_mouse_changed)kb_save(); }
static void kb_leave(void)
{
    kb_flush_mouse();pc_settings_capture_end();
    guest_call(sub_00079B70,2,kb_parent,kb_previous);
}
static void kb_back_action(void)
{
    kb_capture_pending=0;pc_settings_capture_end();kb_flush_mouse();
    switch(kb_page) {
    case KB_HOME:case KB_ERROR:kb_leave();break;
    case KB_LIST:case KB_MOUSE:case KB_RESET:kb_refresh(KB_HOME,0);break;
    case KB_ACTION:kb_refresh(KB_LIST,0);break;
    default:kb_refresh(KB_ACTION,kb_slot);break;
    }
}
static void kb_mouse_preview(void)
{
    pc_controls_set_mouse(kb_settings.sensitivity,kb_settings.enabled,kb_settings.invert_y);
    kb_mouse_changed=GetTickCount64();
    kb_text(2,"\vs80Sensitivity: %.3f\vs100",kb_settings.sensitivity);
}
static void menu_slider_keyboard_pointer(float x,float left,float width)
{
    if(kb_page!=KB_MOUSE)return;
    unsigned step=pc_menu_slider_value(x,left,width,1000);
    double value=pc_keyboard_sensitivity(step);
    if(fabs(value-kb_settings.sensitivity)>1e-8) {kb_settings.sensitivity=value;kb_mouse_preview();}
}
extern void sub_00071B60(void),sub_00041DC0(void);
static void kb_label(float x,float y,const char *text,int scale,uint32_t color)
{
    uint32_t align=WORD_AT(0x007E7260u);WORD_AT(0x007E7260u)=0;
    guest_call(sub_00041DC0,4,bits(x),bits(y),kb_text(30,"\vs%d%s\vs100",scale,text),color);
    WORD_AT(0x007E7260u)=align;
}
static void kb_box(float x,float y,float w,float h,uint32_t color)
{
    guest_call(sub_000814B0,9,bits(1.f),(uint32_t)(int)x,(uint32_t)(int)y,
        (uint32_t)(int)w,(uint32_t)(int)h,color,color,color,0u);
}
static void kb_device(float x,float y,float scale,uint32_t menu)
{
    uint32_t dim=guest_call(sub_00154FE0,2,0x2637497fu,WORD_AT(menu+0xf4));
    uint32_t edge=guest_call(sub_00154FE0,2,0x61778b7fu,WORD_AT(menu+0xf4));
    uint32_t light=guest_call(sub_00154FE0,2,0x8cabc97fu,WORD_AT(menu+0xf4));
    int id=(int)WORD_AT(menu+0x18c),action=id>=0 && id<6?pc_keyboard_groups[kb_group][kb_offset+id]:-1;
    int a=action>=0?kb_settings.keys[action][0]:0,b=action>=0?kb_settings.keys[action][1]:0;
    kb_label(x,y-19*scale,"KEYBOARD + MOUSE",50,light);
    kb_box(x-3*scale,y-3*scale,144*scale,69*scale,edge);
    kb_box(x-2*scale,y-2*scale,142*scale,67*scale,dim);
    static const char *rows[]={"1234567890","QWERTYUIOP","ASDFGHJKL","ZXCVBNM"};
    for(int row=0;row<4;row++)for(int col=0;rows[row][col];col++) {
        char key[2]={rows[row][col],0};float kx=x+(col*13+row*3)*scale,ky=y+row*13*scale;
        int on=a==key[0] || b==key[0];
        kb_box(kx,ky,12*scale,12*scale,on?light:edge);
        kb_label(kx+2*scale,ky+scale,key,32,on?dim:light);
    }
    kb_box(x+35*scale,y+53*scale,70*scale,9*scale,(a==32 || b==32)?light:edge);
    float mx=x+49*scale,my=y+83*scale;
    kb_box(mx,my,39*scale,51*scale,edge);kb_box(mx+2*scale,my+2*scale,35*scale,47*scale,dim);
    kb_box(mx+3*scale,my+3*scale,15*scale,19*scale,(a==1 || b==1)?light:edge);
    kb_box(mx+21*scale,my+3*scale,15*scale,19*scale,(a==2 || b==2)?light:edge);
    kb_box(mx+18*scale,my+6*scale,3*scale,11*scale,(a==4 || b==4 || a==134 || b==134 || a==135 || b==135)?light:edge);
    kb_label(x,y+144*scale,"Select a binding to change it",42,light);
}
static void kb_binding_draw(uint32_t menu,uint32_t item,const float b[4])
{
    int id=(int)WORD_AT(item+0x14);
    if(id<0 || id>=6)return;
    int action=pc_keyboard_groups[kb_group][kb_offset+id];if(action<0)return;
    uint32_t color=guest_call(sub_00154FE0,2,0xc7d6e57fu,WORD_AT(menu+0xf4));
    float w=b[2]-b[0],y=b[1];char name[80];
    /* The row uses the same native panel/highlight as controller Customize. */
    kb_label(b[0],y,pc_keyboard_action_labels[action],58,color);
    for(int slot=0;slot<PC_BINDING_SLOTS;slot++) {
        pc_settings_key_name(kb_settings.keys[action][slot],name,sizeof name);
        kb_label(b[0]+w*(slot?.79f:.55f),y,name,53,color);
    }

}
static void kb_draw(void)
{
    GuestRegisters r;save_registers(&r);
    uint32_t menu=WORD_AT(g_esp+4),rect=WORD_AT(g_esp+12);
    if(WORD_AT(g_esp)==0x00076B50u) {
        if(kb_page==KB_LIST && guest_writable(menu,0x23c)) {
            uint32_t item=WORD_AT(menu+0x178);
            for(unsigned n=0;n<64 && item && guest_writable(item,0x10c) && WORD_AT(item+0x14)!=0;n++)item=WORD_AT(item+4);
            if(item && guest_writable(item,0x10c)) {
                float left=FLOAT_AT(item+0x78),right=FLOAT_AT(item+0x80),top=FLOAT_AT(item+0x7c);
                float x=FLOAT_AT(menu+0xe4)+10.f;
                kb_device(x,top+12.f,(left-x-14.f)/144.f,menu);
                uint32_t color=guest_call(sub_00154FE0,2,0x8cabc97fu,WORD_AT(menu+0xf4));
                kb_label(left+(right-left)*.55f,top-17.f,"PRIMARY",45,color);
                kb_label(left+(right-left)*.79f,top-17.f,"ALTERNATE",45,color);
            }
        }
        restore_registers(&r);g_esp+=4;return;
    }
    if(guest_writable(menu,0xf8) && guest_writable(rect,16)) {
        float b[4];memcpy(b,(void *)((uintptr_t)rect+g_xbox_mem_offset),sizeof b);
        if(kb_page==KB_LIST) {
            kb_binding_draw(menu,WORD_AT(g_esp+8),b);restore_registers(&r);g_esp+=4;return;
        }
        int x=(int)(b[2]-112.f),y=(int)((b[1]+b[3]-8.f)*.5f);
        uint32_t dark=guest_call(sub_00154FE0,2,0x11193c7fu,WORD_AT(menu+0xf4));
        uint32_t light=guest_call(sub_00154FE0,2,0x69737f7fu,WORD_AT(menu+0xf4));
        menu_slider_capture(MENU_KEYBOARD_SENSITIVITY,menu,WORD_AT(g_esp+8),(float)x,(float)y,110.f,8.f);
        guest_call(sub_000814B0,9,bits(pc_keyboard_sensitivity_step(kb_settings.sensitivity)/1000.f),
            (uint32_t)x,(uint32_t)y,110u,8u,dark,dark,light,0u);
    }
    restore_registers(&r);g_esp+=4;
}
static void kb_build(void)
{
    GuestRegisters r;save_registers(&r);kb_menu=WORD_AT(g_esp+4);
    const char *title="Keyboard and Mouse";char line[192],name[80];
    if(kb_page==KB_MOUSE)title="Mouse options";
    if(kb_page==KB_LIST)title=pc_keyboard_group_names[kb_group];
    if(kb_page==KB_ACTION || kb_page==KB_CLEAR || kb_page==KB_ESCAPE)title=pc_action_labels[kb_action];
    if(kb_page==KB_CAPTURE)title="Choose a binding";
    if(kb_page==KB_CONFLICT)title="Key already assigned";
    if(kb_page==KB_RESET)title="Restore default controls?";
    guest_call(sub_00075B70,5,0xa000u,kb_text(0,"%s",title),bits(85.f),0u,KB_BACK);
    guest_call(sub_00071B10,4,1u,bits(2.f),1u,1u);guest_call(sub_00071C50,1,0x26200u);
    kb_enabled_row=kb_invert_row=0;
    WORD_AT(kb_menu+0x10)=kb_page==KB_LIST?KB_DRAW:0;
    if(kb_page==KB_HOME) {
        kb_row(0,"Mouse options",0);
        for(int i=0;i<4;i++)kb_row(i+1,pc_keyboard_group_names[i],0);
        kb_row(5,"Restore defaults",0);kb_row(6,"Back",0);
    } else if(kb_page==KB_MOUSE) {
        kb_enabled_row=kb_row(0,"Mouse look",0);
        guest_call(sub_0007D480,2,0x8400u,0u);
        guest_call(sub_000717D0,1,kb_text(20,"Off"));guest_call(sub_000717D0,1,kb_text(21,"On"));
        guest_call(sub_00075430,2,kb_enabled_row,(uint32_t)kb_settings.enabled);
        uint32_t slider=guest_call(sub_00079D60_gen,6,kb_text(2,"\vs80Sensitivity: %.3f\vs100",kb_settings.sensitivity),1u,0x400u,0u,0u,KB_DRAW);
        if(guest_writable(slider,0x2c))WORD_AT(slider+0x28)&=~0x2000u;
        kb_invert_row=kb_row(2,"Invert vertical look",0);
        guest_call(sub_0007D480,2,0x8400u,0u);
        guest_call(sub_000717D0,1,KB_TEXT(20));guest_call(sub_000717D0,1,KB_TEXT(21));
        guest_call(sub_00075430,2,kb_invert_row,(uint32_t)kb_settings.invert_y);
        kb_row(3,"Reset mouse options",0);kb_row(6,"Back",0);
    } else if(kb_page==KB_LIST) {
        kb_row(10," ",1);
        for(int i=kb_offset;i<kb_offset+6 && i<12;i++) {
            int action=pc_keyboard_groups[kb_group][i];
            if(action<0){kb_row(i-kb_offset," ",1);continue;}
            guest_call(sub_00071B60,3,1u,bits(30.f),bits(85.f));
            uint32_t item=guest_call(sub_00079D60_gen,6,kb_text((unsigned)(i-kb_offset)+1,"\vs90 \vs100"),
                (uint32_t)(i-kb_offset),0x400u,0u,KB_SELECT,KB_DRAW);
            if(guest_writable(item,0x2c))WORD_AT(item+0x28)&=~0x2000u;
        }
        guest_call(sub_00071B60,3,1u,bits(30.f),bits(85.f));
        if(kb_offset)kb_row(7,"Previous page",0);
        if(!kb_offset && pc_keyboard_groups[kb_group][6]>=0)kb_row(8,"Next page",0);
        kb_row(9,"Back",0);
    } else if(kb_page==KB_ACTION || kb_page==KB_CLEAR || kb_page==KB_ESCAPE) {
        for(int i=0;i<PC_BINDING_SLOTS;i++) {
            pc_settings_key_name(kb_settings.keys[kb_action][i],name,sizeof name);
            snprintf(line,sizeof line,"%s%s: %s",kb_page==KB_CLEAR?"Clear ":kb_page==KB_ESCAPE?"Assign Escape to ":"",i?"Alternate":"Primary",name);
            kb_row(i,line,0);
        }
        if(kb_page==KB_ACTION) {kb_row(4,"Clear a binding",0);kb_row(5,"Bind Escape",0);}
        kb_row(6,"Back",0);
    } else if(kb_page==KB_CAPTURE) {
        /* Native update callbacks require a selectable row. Capture consumes
         * its input before the ordinary Select handler. */
        kb_row(0,"Press a key, mouse button or mouse wheel",0);
        kb_row(1,"Escape cancels. Release any held keys first.",1);
    } else if(kb_page==KB_CONFLICT) {
        pc_settings_key_name(kb_key,name,sizeof name);
        snprintf(line,sizeof line,"%s is assigned to %s",name,pc_action_labels[kb_conflict]);kb_row(0,line,1);
        kb_row(1,"Move this binding",0);kb_row(2,"Choose another key",0);kb_row(6,"Cancel",0);
    } else if(kb_page==KB_RESET) {
        kb_row(0,"Cancel",0);kb_row(1,"Restore all keyboard and mouse defaults",0);
    }
    if(*kb_status)kb_row(15,kb_status,1);
    if(kb_page==KB_ERROR)kb_row(6,"Back",0);
    guest_call(sub_000771D0,1,kb_menu);
    restore_registers(&r);g_esp+=4;
}
static void kb_refresh(int page,int selected)
{
    kb_page=page;guest_call(sub_00078980,1,kb_menu);
    if(page==KB_MOUSE) {
        if(kb_enabled_row)guest_call(sub_00075430,2,kb_enabled_row,(uint32_t)kb_settings.enabled);
        if(kb_invert_row)guest_call(sub_00075430,2,kb_invert_row,(uint32_t)kb_settings.invert_y);
    }
    uint32_t item=WORD_AT(kb_menu+0x178),index=0,chosen=0;
    for(;item && index<64 && guest_writable(item,0x10c);index++,item=WORD_AT(item+4))
        if((int)WORD_AT(item+0x14)==selected){chosen=index;break;}
    guest_call(sub_000770E0,2,kb_menu,chosen);
}
static void kb_begin_capture(void)
{
    *kb_status=0;kb_refresh(KB_CAPTURE,0);
    /* Arm capture once the new page reaches its update callback. */
    kb_capture_pending=1;
}
static void kb_assign(int key,int move)
{
    char error[320];
    if(!pc_settings_read(&kb_settings,error,sizeof error)) {
        snprintf(kb_status,sizeof kb_status,"Controls unavailable. Reopen this menu.");kb_refresh(KB_ERROR,6);return;
    }
    kb_saved=kb_settings;kb_key=key;
    kb_conflict=pc_settings_conflict(&kb_settings,kb_action,key);
    if(key && kb_conflict>=0 && !move){kb_refresh(KB_CONFLICT,6);return;}
    if(pc_settings_assign(&kb_settings,kb_action,kb_slot,key,move)==1)kb_save();
    else snprintf(kb_status,sizeof kb_status,"That key is reserved. Choose another key.");
    kb_refresh(KB_ACTION,kb_slot);
}
static void kb_select(void)
{
    GuestRegisters r;save_registers(&r);uint32_t menu=WORD_AT(g_esp+4);
    int id=guest_writable(menu,0x190)?(int)WORD_AT(menu+0x18c):-1;
    if(kb_page==KB_HOME) {
        if(id==0)kb_refresh(KB_MOUSE,0);
        else if(id>=1 && id<=4){kb_group=id-1;kb_offset=0;kb_refresh(KB_LIST,0);}
        else if(id==5)kb_refresh(KB_RESET,0);else if(id==6)kb_leave();
    } else if(kb_page==KB_LIST) {
        if(id>=0 && id<6 && pc_keyboard_groups[kb_group][kb_offset+id]>=0) {
            kb_action=pc_keyboard_groups[kb_group][kb_offset+id];kb_slot=0;
            uint32_t item=WORD_AT(menu+0x198);PcMenuPointer pointer=pc_controls_menu_hover_pointer();
            unsigned sw,sh;float x,y;
            d3d8_display_output_shape(WORD_AT(0x004E8CF0u),WORD_AT(0x004E8CF4u),&sw,&sh);
            if(guest_writable(item,0x84) && sh && pc_menu_mouse_position(pointer,(float)sw/sh,
                (float)WORD_AT(0x004E8CF0u),(float)WORD_AT(0x004E8CF4u),&x,&y)) {
                float left=FLOAT_AT(item+0x78),width=FLOAT_AT(item+0x80)-left;
                if(x>left+width*.5f){kb_slot=x>left+width*.76f?1:0;kb_begin_capture();}
                else kb_refresh(KB_ACTION,0);
            } else kb_refresh(KB_ACTION,0);
        } else if(id==7 || id==8){kb_offset=id==8?6:0;kb_refresh(KB_LIST,0);}
        else if(id==9)kb_back_action();
    } else if(kb_page==KB_ACTION) {
        if(id>=0 && id<PC_BINDING_SLOTS){kb_slot=id;kb_begin_capture();}
        else if(id==4)kb_refresh(KB_CLEAR,0);else if(id==5)kb_refresh(KB_ESCAPE,0);else if(id==6)kb_back_action();
    } else if(kb_page==KB_CLEAR || kb_page==KB_ESCAPE) {
        if(id>=0 && id<PC_BINDING_SLOTS){kb_slot=id;kb_assign(kb_page==KB_CLEAR?0:27,0);}else if(id==6)kb_back_action();
    } else if(kb_page==KB_CONFLICT) {
        if(id==1)kb_assign(kb_key,1);else if(id==2)kb_begin_capture();else if(id==6)kb_back_action();
    } else if(kb_page==KB_RESET) {
        if(id==1){pc_settings_defaults(&kb_settings);kb_save();}kb_refresh(KB_HOME,0);
    } else if(kb_page==KB_MOUSE) {
        if(id==3){kb_settings.sensitivity=.1;kb_settings.enabled=1;kb_settings.invert_y=0;kb_save();kb_refresh(KB_MOUSE,0);}
        else if(id==6)kb_back_action();
    } else if(id==6)kb_back_action();
    restore_registers(&r);g_esp+=4;
}
static void kb_tick(void)
{
    GuestRegisters r;save_registers(&r);uint32_t menu=WORD_AT(g_esp+4);
    if(kb_page==KB_CAPTURE) {
        if(kb_capture_pending){pc_settings_capture_begin();kb_capture_pending=0;}
        int event=pc_settings_capture_event();
        if(event<0)kb_back_action();
        else if(event>0) {
            int key=event-1;
            if(pc_settings_key_allowed(key))kb_assign(key,0);
            else {snprintf(kb_status,sizeof kb_status,"That key is reserved. Choose another key.");kb_refresh(KB_CAPTURE,0);kb_capture_pending=1;}
        }
    } else if(kb_page==KB_MOUSE) {
        int changed=0;
        if(guest_writable(kb_enabled_row,0xcc) && WORD_AT(kb_enabled_row+0xc8)<=1 && (int)WORD_AT(kb_enabled_row+0xc8)!=kb_settings.enabled)
            {kb_settings.enabled=(int)WORD_AT(kb_enabled_row+0xc8);changed=1;}
        if(guest_writable(kb_invert_row,0xcc) && WORD_AT(kb_invert_row+0xc8)<=1 && (int)WORD_AT(kb_invert_row+0xc8)!=kb_settings.invert_y)
            {kb_settings.invert_y=(int)WORD_AT(kb_invert_row+0xc8);changed=1;}
        if(guest_writable(menu,0x190) && WORD_AT(menu+0x18c)==1) {
            unsigned buttons=WORD_AT(menu+0xc4),step=pc_keyboard_sensitivity_step(kb_settings.sensitivity);
            guest_call(sub_00074910,3,menu,0u,1u);
            if((buttons&0x40000u) && step){step=step<5?0:step-5;changed=1;}
            else if((buttons&0x80000u) && step<1000){step=step>995?1000:step+5;changed=1;}
            if(changed)kb_settings.sensitivity=pc_keyboard_sensitivity(step);
        }
        if(changed)kb_mouse_preview();
        if(kb_mouse_changed && GetTickCount64()-kb_mouse_changed>=250 && !pc_controls_menu_hover_pointer().left) {
            if(!kb_save())kb_refresh(KB_MOUSE,0);
        }
    }
    restore_registers(&r);g_esp+=4;
}
static void kb_back(void)
{ GuestRegisters r;save_registers(&r);kb_back_action();restore_registers(&r);g_esp+=4; }
static void kb_open(void)
{
    GuestRegisters r;save_registers(&r);uint32_t source=WORD_AT(g_esp+4);char error[320];
    if(guest_writable(source,0x3c) && kb_caption()) {
        *kb_status=0;kb_page=KB_HOME;kb_mouse_changed=0;kb_capture_pending=0;kb_group=kb_offset=kb_action=kb_slot=0;
        if(!pc_settings_read(&kb_settings,error,sizeof error)) {
            kb_page=KB_ERROR;snprintf(kb_status,sizeof kb_status,"Select a keyboard profile in PC Settings first.");
        }
        kb_saved=kb_settings;kb_parent=WORD_AT(source+0x2c);kb_previous=WORD_AT(source+4);
        guest_call(sub_00079B70,2,kb_parent,kb_storage);
        fprintf(stderr,"[PC-KEYBOARD-MENU] Opened themed live settings\n");
    }
    restore_registers(&r);g_esp+=4;
}
