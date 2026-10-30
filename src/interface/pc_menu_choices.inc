/* The native type-1 choice renderer (sub_0007B7B0) draws both arrows
 * after measuring the current value. Capture that final geometry: choices
 * can be left/centre/right aligned and have variable-width translated text. */
typedef struct MenuChoiceArrow {
    uint32_t menu,descriptor,item,id,choice,direction;
    float left,top,right,bottom,row[4];
    unsigned sw,sh;
    uint64_t time;
} MenuChoiceArrow;
static MenuChoiceArrow menu_choice_arrows[512];
static uint32_t menu_choice_drawing;

/* Custom type-1 row painters reuse ESI for their own temporaries. */
void pc_display_menu_choice_draw(void (*original)(void))
{
    uint32_t previous=menu_choice_drawing;
    menu_choice_drawing=WORD_AT(g_esp+8);
    original();
    menu_choice_drawing=previous;
}

void pc_display_menu_choice_arrow(void (*original)(void),unsigned direction)
{
    uint32_t item=g_esi,menu=0,caller=WORD_AT(g_esp);
    menu_team_arrow(direction);
    if((direction==4 && (caller==0x0019F174u || caller==0x00360EDDu)) ||
       (direction==8 && (caller==0x0019F1D8u || caller==0x00360F74u)))
        item=menu_choice_drawing;
    else if(!((direction==4 && caller==0x0007BA56u) ||
              (direction==8 && caller==0x0007BA30u))) item=0;
    if(item) {
        if(guest_writable(item,0x10c)) menu=WORD_AT(item+0x88);
        if(guest_writable(menu,0x23c) && WORD_AT(item+0xb4)==1 &&
           !(WORD_AT(item+0x28)&5u) && !(WORD_AT(item+0xb8)&0x400000u)) {
            float l=FLOAT_AT(g_esp+4),t=FLOAT_AT(g_esp+8);
            float r=FLOAT_AT(g_esp+12),b=FLOAT_AT(g_esp+16);
            if(isfinite(l) && isfinite(t) && isfinite(r) && isfinite(b) &&
               r>l && b>t && r-l<10000 && b-t<10000) {
                MenuChoiceArrow *slot=&menu_choice_arrows[0];
                for(unsigned i=0;i<512;i++) {
                    MenuChoiceArrow *a=&menu_choice_arrows[i];
                    if(a->item==item && a->direction==direction) {slot=a;break;}
                    if(a->time<slot->time) slot=a;
                }
                slot->menu=menu;slot->descriptor=WORD_AT(menu+4);
                slot->item=item;slot->id=WORD_AT(item+0x14);
                slot->choice=WORD_AT(item+0xc8);slot->direction=direction;
                /* A small target margin still stays inside the live row/menu. */
                float margin=(b-t)*.4f;
                slot->left=l-margin;slot->top=t-margin;
                slot->right=r+margin;slot->bottom=b+margin;
                memcpy(slot->row,(void *)((uintptr_t)item+0x78+g_xbox_mem_offset),sizeof slot->row);
                d3d8_display_output_shape(WORD_AT(0x004E8CF0u),WORD_AT(0x004E8CF4u),&slot->sw,&slot->sh);
                slot->time=GetTickCount64();
            }
        }
    }
    original();
}

static unsigned menu_choice_hit(uint32_t menu,float x,float y,unsigned sw,unsigned sh,uint32_t *item)
{
    if(x<FLOAT_AT(menu+0xe4) || x>=FLOAT_AT(menu+0xec) ||
       y<FLOAT_AT(menu+0xe8) || y>=FLOAT_AT(menu+0xf0)) return 0;
    for(unsigned i=0;i<512;i++) {
        const MenuChoiceArrow *a=&menu_choice_arrows[i];
        uint32_t row=a->item;
        if(a->menu!=menu || a->descriptor!=WORD_AT(menu+4) ||
           a->sw!=sw || a->sh!=sh || GetTickCount64()-a->time>100 ||
           !guest_writable(row,0x10c) || WORD_AT(row+0x88)!=menu ||
           WORD_AT(row+0x14)!=a->id || WORD_AT(row+0xb4)!=1 ||
           WORD_AT(row+0xc8)!=a->choice || (WORD_AT(row+0x28)&5u) ||
           (WORD_AT(row+0xb8)&0x400000u) ||
           memcmp(a->row,(void *)((uintptr_t)row+0x78+g_xbox_mem_offset),sizeof a->row)) continue;
        if(x>=a->left && x<a->right && y>=a->top && y<a->bottom &&
           y>=a->row[1] && y<a->row[3]) {
            *item=row;return a->direction;
        }
    }
    return 0;
}
