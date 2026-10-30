/* US 45410066 UI groups. Placement values are the renderer's AUTO=0, STRETCH=1,
 * CENTRE=2, LEFT=3, RIGHT=4, SIDE=5 ABI. Keep complete widgets together;
 * the renderer applies the same anchor to their viewport and scissors. */
extern void sub_00022FF0(void), sub_001CDAE0(void);
extern void host_SetTwoDPlacement(int placement,uint32_t tag);
#include "pc_ui_layout.h"
static RECOMP_TLS int ui_placement, radar_group, health_group;
static RECOMP_TLS uint32_t ui_tag;
static void ui_place(int placement,uint32_t tag)
{
    /* Flush guest sprite batches before changing their recorded placement.
       Host state alone would move the preceding batch with the next widget. */
    guest_call(sub_00022FF0,0);
    host_SetTwoDPlacement(placement,tag);
    ui_placement=placement;ui_tag=tag;
}
/* US sub_000C8120 composes a camera before its optional texture copy.
 * Camera flag 8 identifies a texture feed; flag 0x400000 renders it into a
 * corner of the back buffer and copies that corner instead of binding an RT.
 * Such a pass still has g_cur_rt == NULL. Its logo, scanlines and alpha mask
 * are texture pixels, not HUD widgets: keep every layer in guest coordinates.
 * Restore the enclosing placement after flushing the final feed batch. */
void pc_display_camera_feed(void (*original)(void))
{
    uint32_t camera=WORD_AT(0x007B0CC0u);
    int old=ui_placement;uint32_t old_tag=ui_tag;
    int feed=guest_writable(camera,0x224u) &&
        (WORD_AT(camera+0x21Cu)&9u)==9u;
    if(feed)ui_place(1,0x000C8120u);
    original();
    if(feed)ui_place(old,old_tag);
}

void pc_display_radar_begin(void)
{
    /* The radar routine first draws world target markers. Only its later
       HUD disc and all following blips belong at the top-right corner. */
    if(radar_group)ui_place(4,0x00133AB0u);
}
void pc_display_health_side(void)
{
    /* sub_0013AB60 emits three passes over each side. At this primitive
       allocation call, its side index is above four arguments + return PC.
       Change placement only between halves, never inside an open batch. */
    if(health_group) {
        int side=WORD_AT(g_esp+0x34u)==0?3:4;
        if(side!=ui_placement)ui_place(side,0x0013AB60u);
    }
}
void pc_display_health(void (*original)(void))
{
    uint32_t view=WORD_AT(0x007E71C8u),frame=g_esp,saved;
    int old=ui_placement,old_health=health_group;
    uint32_t old_tag=ui_tag;
    float scale,x,y,left,top,right,bottom,adjusted;
    if(pc_ui_hide_results_health(WORD_AT(frame),WORD_AT(0x007de30cu),
                                (WORD_AT(0x007de454u)>>8)&0xffu)) {
        g_esp+=4u;return;
    }
    if(guest_call(sub_001CDAE0,0)!=1 || !guest_writable(view,0x24)) {
        original();return;
    }
    saved=WORD_AT(frame+16u);memcpy(&scale,&saved,4);
    uint32_t xb=WORD_AT(frame+8u),yb=WORD_AT(frame+12u);
    memcpy(&x,&xb,4);memcpy(&y,&yb,4);
    left=(float)(int32_t)WORD_AT(view);top=(float)(int32_t)WORD_AT(view+0x14u);
    right=(float)(int32_t)WORD_AT(view+4u)+1;bottom=(float)(int32_t)WORD_AT(view+0x18u)+1;
    x+=(float)(int32_t)WORD_AT(view+8u)-left;
    y+=(float)(int32_t)WORD_AT(view+0x1Cu)-top;
    adjusted=pc_ui_health_scale(scale,left,top,right,bottom,x,y);
    memcpy(&WORD_AT(frame+16u),&adjusted,4);
    health_group=pc_display_wide_now() && pc_display_hud_scale()<1.f;
    ui_place(2,0x0013AB60u); /* Directional hit marker stays centred. */
    original();
    ui_place(old,old_tag);
    health_group=old_health;WORD_AT(frame+16u)=saved;
}
void pc_display_ui(void (*original)(void),uint32_t tag)
{
    int old=ui_placement,old_radar=radar_group,placement=0;
    uint32_t old_tag=ui_tag;
    if(!pc_display_wide_now() || pc_display_hud_scale()>=1.f || guest_call(sub_001CDAE0,0)!=1) {
        original();return; /* Preserve the existing split-screen layout. */
    }
    switch(tag) {
    case 0x001334D0u: /* Ammo: fifth argument identifies the right-hand gun. */
        placement=WORD_AT(g_esp+20)==1?4:3;break;
    case 0x00133AB0u: radar_group=1;break;
    case 0x00142D10u:
        /* Fit the complete marker around its projected target, not the
           screen centre. The renderer retains the outline's anchor for text. */
        placement=2;break;
    case 0x001358A0u: placement=5;break; /* Zoom controls; centre graphic stays centred. */
    case 0x0012E3F0u: /* Arcade RANK and SCORE share one horizontal layout. */
        /* The original compositor moves RANK into SCORE's vacated space.
           Giving only RANK a left anchor shifts SCORE toward it on widescreen. */
        if(WORD_AT(g_esp)!=0x0012EC8Bu && WORD_AT(g_esp)!=0x0012EA9Bu){original();return;}
        placement=3;break;
    case 0x0013FC50u: /* Story date/title, including all outline/shadow passes. */
    case 0x0012F670u: case 0x0014B930u: case 0x001409C0u: placement=3;break;
    default:original();return;
    }
    if(placement)ui_place(placement,tag);
    original();
    ui_place(old,old_tag);
    radar_group=old_radar;
}

/* Verified US full-screen fade call sites. Scope the solid rectangle itself:
 * a fade can follow proportional UI in the same deferred sprite batch. Keep
 * opacity, blend state and guest geometry intact, and flush at both boundaries. */
int pc_display_transition_overlay(void (*original)(void))
{
    uint32_t frame=g_esp,caller=WORD_AT(frame),view=WORD_AT(0x007E71C8u);
    int loading_band=0;
    switch(caller) {
    case 0x0006F227u: case 0x0006F264u: case 0x0006F2F0u:
    case 0x0006F381u: case 0x0006F3ABu: /* Story fades/tints and blackout. */
    case 0x000FEA07u: /* Additive gameplay white flash (sub_000FE990). */
    case 0x00070755u: /* Loading/startup blackout. */
    case 0x001CF429u: case 0x001CF5E5u: /* Game transition fade out/in. */
    case 0x002BF803u: /* Frontend transition. */
    case 0x002BDA88u: /* Frontend transition backdrop blackout. */
    case 0x002DA4D0u: case 0x002DA715u: /* Menu canvas clear/tint. */
        break;
    case 0x00155F8Eu: case 0x00156102u: /* Loading picture top/bottom borders. */
    case 0x0015682Du: /* Loading picture fade starts below its top border. */
        loading_band=1;break;
    default:return 0;
    }
    /* Only a complete authored canvas. Some frontend paths add one to the
       viewport's inclusive right/bottom, producing 641x481. A solid rectangle
       can also inherit a non-solid texture binding: AUTO then squeezes its
       six-vertex triangle list despite its full-screen bounds. Tag the known
       canvas passes explicitly. Loading bands span the same canvas width but
       retain their authored vertical bounds, alpha and timing. Their borders
       and fade together cover the display; panels/player views stay fitted. */
    if(!pc_display_wide_now() || !guest_writable(view,0x24) ||
       WORD_AT(view)!=0 || (WORD_AT(view+4u)!=639 && WORD_AT(view+4u)!=640) ||
       WORD_AT(view+0x14u)!=0 || (WORD_AT(view+0x18u)!=479 && WORD_AT(view+0x18u)!=480) ||
       WORD_AT(frame+4u)!=0 ||
       (WORD_AT(frame+12u)!=640 &&
          !(WORD_AT(view+4u)==640 && WORD_AT(frame+12u)==641)))return 0;
    uint32_t top=WORD_AT(frame+8u),bottom=WORD_AT(frame+16u);
    if(loading_band) {
        if(top>=bottom || bottom>480 ||
           (caller==0x00155F8Eu ? top!=0 : bottom!=480))return 0;
    } else if(top!=0 || (bottom!=480 &&
              !(WORD_AT(view+0x18u)==480 && bottom==481)))return 0;
    int old=ui_placement;uint32_t old_tag=ui_tag;
    ui_place(1,caller);
    original();
    ui_place(old,old_tag);
    return 1;
}

/* sub_000F90D0 is the shared white wash used by gameplay flashes. It emits
 * a viewport-sized textured quad via sub_0001FB40, independently of the solid
 * fade helper above. Keep its colour, UVs, blend mode and intensity unchanged;
 * only the completed draw group must escape proportional HUD placement. */
void pc_display_flash_overlay(void (*original)(void))
{
    uint32_t view=WORD_AT(0x007E71C8u);
    if(!pc_display_wide_now() || !guest_writable(view,0x24) ||
       WORD_AT(view)!=0 || (WORD_AT(view+4u)!=639 && WORD_AT(view+4u)!=640) ||
       WORD_AT(view+0x14u)!=0 || (WORD_AT(view+0x18u)!=479 && WORD_AT(view+0x18u)!=480)) {
        original();return;
    }
    int old=ui_placement;uint32_t old_tag=ui_tag;
    ui_place(1,0x000F90D0u);
    original();
    ui_place(old,old_tag);
}

/* Frontend and Objectives bars mix a black rectangle, two half-width decorative
 * strips and proportional labels. Only its background primitives stretch.
 * Exact US call sites exclude other uses of these common drawing helpers. */
void pc_display_footer_background(void (*original)(void))
{
    uint32_t caller=WORD_AT(g_esp),view=WORD_AT(0x007E71C8u);
    int old=ui_placement;
    uint32_t old_tag=ui_tag;
    int objectives=caller==0x002F3AEDu || caller==0x002F3B0Fu ||
        caller==0x002F3B48u || caller==0x002F3B77u ||
        caller==0x002F3BA9u || caller==0x002F3BDCu;
    if((!objectives && caller!=0x002D90EBu && caller!=0x002D911Bu && caller!=0x002D9148u) ||
       (!objectives && WORD_AT(0x0079094Cu)<=100) ||
       (objectives && guest_call(sub_001CDAE0,0)!=1) || !pc_display_wide_now() ||
       pc_display_hud_scale()>=1.f || !guest_writable(view,0x24) ||
       WORD_AT(view)!=0 || (WORD_AT(view+4u)!=639 && WORD_AT(view+4u)!=640) ||
       WORD_AT(view+0x14u)!=0 || (WORD_AT(view+0x18u)!=479 && WORD_AT(view+0x18u)!=480)) {
        original();return;
    }
    ui_place(1,caller); /* STRETCH: full width, unchanged vertical geometry. */
    original();
    ui_place(old,old_tag); /* Flush before restoring the labels' placement. */
}

extern void sub_0001C5D0(void);
/* US title reveal: sub_002BDA00 surrounds the logo with four black 3D
 * quads. Hor+ exposes the outer edges of the left/right pair. Widen only
 * their two outer vertices about the camera centre; inner edges still meet
 * the unchanged logo, and its wipe, depth, colours and timing stay intact.
 * These are vertex writes inside an open primitive: do not flush or change
 * renderer placement here. Restore the caller's stack after emission. */
void pc_display_title_matte(void (*original)(void))
{
    uint32_t frame=g_esp,caller=WORD_AT(frame),view=WORD_AT(0x007E71C8u);
    float scale=pc_display_hud_scale();
    if((caller!=0x002BDDB4u && caller!=0x002BDE0Au) ||
       (int32_t)WORD_AT(0x0079094Cu)<=100 || !pc_display_wide_now() ||
       !(scale>0.f && scale<1.f) || !guest_writable(view,0x24) ||
       WORD_AT(view)!=0 || (WORD_AT(view+4u)!=639 && WORD_AT(view+4u)!=640) ||
       WORD_AT(view+0x14u)!=0 || (WORD_AT(view+0x18u)!=479 && WORD_AT(view+0x18u)!=480)) {
        original();return;
    }
    uint32_t saved[2]={WORD_AT(frame+4u),WORD_AT(frame+20u)};
    float outer[2];memcpy(outer,saved,sizeof outer);
    outer[0]/=scale;outer[1]/=scale;
    memcpy(&WORD_AT(frame+4u),&outer[0],4);
    memcpy(&WORD_AT(frame+20u),&outer[1],4);
    original();
    WORD_AT(frame+4u)=saved[0];WORD_AT(frame+20u)=saved[1];
}

void pc_display_menu_backdrop(void (*original)(void))
{
    /* Backdrop precedes the frontend's artwork and glyph batches. Present
       clears this host-only flag, so gameplay never inherits menu filtering. */
    pc_display_menu_filter_begin((int32_t)WORD_AT(0x0079094Cu)>100);
    const uint32_t projection=0x0055FA40u;
    uint32_t view=WORD_AT(0x007E71C8u),saved;
    float horizontal,scale=pc_display_hud_scale();
    /* Only the animated blue sheet in US sub_002DA440. The wireframe
       carousel, character previews and menu labels keep their own lens. */
    if(WORD_AT(g_esp)!=0x002DA51Bu || (int32_t)WORD_AT(0x0079094Cu)<=100 ||
       !pc_display_wide_now() || scale<=0.f || scale>=1.f ||
       !guest_writable(projection,64) || !guest_writable(view,0x24) ||
       WORD_AT(view)!=0 || (WORD_AT(view+4u)!=639 && WORD_AT(view+4u)!=640) ||
       WORD_AT(view+0x14u)!=0 || (WORD_AT(view+0x18u)!=479 && WORD_AT(view+0x18u)!=480)) {
        original();return;
    }
    guest_call(sub_00022FF0,0);
    saved=WORD_AT(projection);memcpy(&horizontal,&saved,4);
    /* Undo Hor+ for this screen-filling artwork, retaining its animation.
       Use the guest projection setter so GPU constants and cached combined
       matrices agree, then restore it before any other draw can inherit it. */
    horizontal/=scale;memcpy(&WORD_AT(projection),&horizontal,4);
    guest_call(sub_0001C5D0,1,projection);
    original();
    guest_call(sub_00022FF0,0);
    WORD_AT(projection)=saved;
    guest_call(sub_0001C5D0,1,projection);
}

/* Scope actual glyph scale during main-menu construction/measurement and
 * row drawing. The spacing setter alone does not change the letters. */
extern void sub_0003F8D0(void);
void pc_display_main_menu_font(void (*original)(void),int building)
{
    /* One menu label can invoke several wrapping and drawing passes. Keep
       their read spans together, then discard them before menu input/ticks. */
    pc_prompts_layout_begin();
    uint32_t menu=WORD_AT(g_esp+4u),item=WORD_AT(g_esp+8u);
    if((int32_t)WORD_AT(0x0079094Cu)<=100) {original();pc_prompts_layout_end();return;}
    if(!building) {
        if(!guest_writable(menu,0x23c) || !guest_writable(item,0x8c) ||
           WORD_AT(item)!=7 || WORD_AT(item+0x88u)!=menu) {original();pc_prompts_layout_end();return;}
        uint32_t descriptor=WORD_AT(menu+4u);
        if(!guest_writable(descriptor,4) || WORD_AT(descriptor)!=0x002D0BE0u) {
            original();pc_prompts_layout_end();return;
        }
    }
    uint32_t saved_x=WORD_AT(0x007E7270u),saved_y=WORD_AT(0x007E7280u);
    float x,y;memcpy(&x,&saved_x,4);memcpy(&y,&saved_y,4);
    guest_call(sub_00022FF0,0);
    x*=.75f;y*=.75f;
    memcpy(&WORD_AT(0x007E7270u),&x,4);memcpy(&WORD_AT(0x007E7280u),&y,4);
    guest_call(sub_0003F8D0,0);
    int previous_font_filter=pc_display_menu_font_filter(!building);
    original();
    pc_prompts_layout_end();
    guest_call(sub_00022FF0,0);
    pc_display_menu_font_filter(previous_font_filter);
    WORD_AT(0x007E7270u)=saved_x;WORD_AT(0x007E7280u)=saved_y;
    guest_call(sub_0003F8D0,0);
}

/* Only the circular weapon scope's four outer masks. Its textured aperture
 * and reticle retain the existing proportional placement. */
int pc_display_scope_mask(void (*original)(void))
{
    uint32_t frame=g_esp,caller=WORD_AT(frame),view=WORD_AT(0x007E71C8u);
    /* The futuristic scope darkens through destination alpha, then adds its
       textured ring. Expand that full-view composite, not the ring artwork. */
    if(caller==0x00136856u && ui_tag==0x001364A0u && ui_placement==2) {
        uint32_t old_tag=ui_tag;int old=ui_placement;
        ui_place(1,caller);
        original();
        ui_place(old,old_tag);
        return 1;
    }
    unsigned side;
    switch(caller) {
    case 0x00136014u:side=0;break;
    case 0x00136032u:side=1;break;
    case 0x0013604Du:side=2;break;
    case 0x0013606Eu:side=3;break;
    default:return 0;
    }
    float ratio=pc_display_hud_scale();
    if(!pc_display_wide_now() || !(ratio>0.f) || ratio==1.f ||
       guest_call(sub_001CDAE0,0)!=1 || !guest_writable(view,0x24) ||
       WORD_AT(view)!=0 || (WORD_AT(view+4u)!=639 && WORD_AT(view+4u)!=640) ||
       WORD_AT(view+0x14u)!=0 || (WORD_AT(view+0x18u)!=479 && WORD_AT(view+0x18u)!=480))return 0;
    int32_t rect[4];uint32_t saved[4],old_tag=ui_tag;int old=ui_placement;
    for(unsigned i=0;i<4;i++){saved[i]=WORD_AT(frame+4+i*4);rect[i]=(int32_t)saved[i];}
    pc_ui_scope_mask(rect,side,ratio);
    ui_place(1,caller);
    for(unsigned i=0;i<4;i++)WORD_AT(frame+4+i*4)=(uint32_t)rect[i];
    original();
    ui_place(old,old_tag);
    for(unsigned i=0;i<4;i++)WORD_AT(frame+4+i*4)=saved[i];
    return 1;
}

extern void sub_0001EFB0(void),sub_00019650(void),sub_00019D70(void);
static void scope_outer_fill(float ratio,uint32_t colour)
{
    float sx=pc_ui_scale_x(ratio),sy=pc_ui_scale_y(ratio);
    if(sx<1.f) {
        unsigned edge=(unsigned)ceilf(320.f*(1.f-sx));
        guest_call(sub_0001EFB0,5,0u,0u,edge,480u,colour);
        guest_call(sub_0001EFB0,5,640u-edge,0u,640u,480u,colour);
    }
    if(sy<1.f) {
        unsigned edge=(unsigned)ceilf(240.f*(1.f-sy));
        guest_call(sub_0001EFB0,5,0u,0u,640u,edge,colour);
        guest_call(sub_0001EFB0,5,0u,480u-edge,640u,480u,colour);
    }
}

/* Fill the extra host canvas even when the guest omits an exterior draw.
 * Conventional scopes are opaque. The futuristic scope instead builds a
 * destination-alpha mask (mode 14), composites the world through it (mode 6),
 * and adds its artwork. Extend that same translucent mask before its draws. */
void pc_display_scope(void (*original)(void),uint32_t tag)
{
    uint32_t view=WORD_AT(0x007E71C8u),old_tag=ui_tag;int old=ui_placement;
    float ratio=pc_display_hud_scale();
    if(!pc_display_wide_now() || !(ratio>0.f) || ratio==1.f ||
       guest_call(sub_001CDAE0,0)!=1 || !guest_writable(view,0x24) ||
       WORD_AT(view)!=0 || (WORD_AT(view+4u)!=639 && WORD_AT(view+4u)!=640) ||
       WORD_AT(view+0x14u)!=0 || (WORD_AT(view+0x18u)!=479 && WORD_AT(view+0x18u)!=480)) {
        original();return;
    }
    int translucent=tag==0x001364A0u;
    if(!translucent) {ui_place(2,tag);original();}
    ui_place(1,tag);
    uint32_t blend=guest_call(sub_00019650,1,0u);
    uint32_t mode=guest_call(sub_00019D70,1,translucent?14u:1u);
    scope_outer_fill(ratio,translucent?0x40u:0x7Fu);
    ui_place(translucent?2:old,translucent?tag:old_tag);
    guest_call(sub_00019650,1,blend);
    guest_call(sub_00019D70,1,mode);
    if(translucent) {
        original();
        ui_place(old,old_tag);
    }
}

/* Reticle positions are projected aim points: keep their screen location while
 * fitting only their local geometry. Fitting about the screen centre instead
 * would move a displaced reticle away from its target on a wide display.
 * The exact US call sites cover the shared weapon, alternate and turret
 * reticles plus every weapon using the common rotating zoom indicator. */
static int aim_full_view(void)
{
    uint32_t view=WORD_AT(0x007E71C8u);
    return pc_display_wide_now() && pc_display_hud_scale()>0.f &&
        guest_call(sub_001CDAE0,0)==1 && guest_writable(view,0x24) &&
        WORD_AT(view)==0 && (WORD_AT(view+4u)==639 || WORD_AT(view+4u)==640) &&
        WORD_AT(view+0x14u)==0 && (WORD_AT(view+0x18u)==479 || WORD_AT(view+0x18u)==480);
}
void pc_display_aim_primitive(void (*original)(void),unsigned kind)
{
    uint32_t frame=g_esp,caller=WORD_AT(frame),offset=4,points=2;
    int anchor=1,matched=0;
    if(kind==2) menu_character_quad();
    if(kind==0) {
        matched=caller==0x0014CC10u || caller==0x001481E9u ||
            caller==0x001482F7u || caller==0x00136FFCu;
    } else if(kind==1) {
        matched=caller==0x00135F1Eu;offset=8;points=4;
    } else if(kind==2) {
        switch(caller) {
        case 0x00135BB4u:case 0x00135C5Au:case 0x00135CA7u:anchor=3;matched=1;break;
        case 0x00135CF6u:case 0x00135D58u:case 0x00135D95u:anchor=4;matched=1;break;
        }
    }
    if(!matched || !aim_full_view()){original();return;}
    uint32_t saved[8],old_tag=ui_tag;int old=ui_placement;
    float xy[8];
    for(unsigned i=0;i<points*2;i++)saved[i]=WORD_AT(frame+offset+i*4);
    memcpy(xy,saved,points*2*sizeof(float));
    if(kind!=2) {
        pc_ui_aim_shape(xy,points,pc_display_hud_scale());
        for(unsigned i=0;i<points*2;i++)memcpy(&WORD_AT(frame+offset+i*4),&xy[i],4);
    }
    ui_place(anchor,caller);original();ui_place(old,old_tag);
    for(unsigned i=0;i<points*2;i++)WORD_AT(frame+offset+i*4)=saved[i];
}
void pc_display_aim_panel(void (*original)(void),uint32_t tag)
{
    if(!aim_full_view()){original();return;}
    uint32_t old_tag=ui_tag;int old=ui_placement;
    ui_place(1,tag);original();ui_place(old,old_tag);
}

/* Special scope: keep the ring/border art fitted. Its gradient rectangles
 * extend to the output edges and its framebuffer composite spans the screen. */
void pc_display_effect_scope(void (*original)(void))
{
    if(!aim_full_view()){original();return;}
    uint32_t old_tag=ui_tag;int old=ui_placement;
    ui_place(2,0x00137F00u);original();ui_place(old,old_tag);
}

/* US primitive ABIs: 0 = integer textured rectangle; 1 = four pointers
 * to XYZ vertices (after width, height, colour); 2 = float gradient rectangle.
 * ESI still holds the lock-on target record at these exact call sites. Using
 * that shared centre keeps the two box halves and orbiting pieces together. */
void pc_display_weapon_effect(void (*original)(void),unsigned kind)
{
    uint32_t frame=g_esp,caller=WORD_AT(frame),addresses[8],saved[8];
    unsigned points=2;int lock=0,gradient=0,composite=0;
    if(kind==0) {
        lock=caller==0x0013871Bu || caller==0x0013874Au;
        /* Ghost Gun living-actor reveal (US sub_00122F10): this rectangle
         * composites a filtered screen copy over the actor's projected bounds.
         * Its coordinates already match the world camera, so centering it as
         * a 4:3 HUD element detaches the image from Past Cortez in widescreen.
         * Only the final composite changes placement; keep its UVs, tint and
         * the preceding 256x256 blur passes exactly as the guest authored them. */
        composite=(caller==0x00138471u && ui_tag==0x00137F00u) ||
                  caller==0x001232A9u;
    } else if(kind==1) {
        points=4;
        lock=caller==0x00138A6Cu || caller==0x00138C5Cu ||
            caller==0x00138D73u || caller==0x00138F1Du;
    } else if(kind==2 && ui_tag==0x00137F00u) {
        switch(caller) {
        case 0x00138227u:case 0x0013825Au:case 0x00138285u:case 0x001382B1u:
        case 0x001382DDu:case 0x00138309u:case 0x00138339u:case 0x0013836Cu:
            gradient=1;break;
        }
    }
    if((!lock && !gradient && !composite) || !aim_full_view()){original();return;}
    float cx=320.f,cy=240.f,xy[8],ratio=pc_display_hud_scale();
    if(lock) {
        if(!guest_writable(g_esi,8)){original();return;}
        memcpy(&cx,&WORD_AT(g_esi),4);memcpy(&cy,&WORD_AT(g_esi+4),4);
        if(!isfinite(cx) || !isfinite(cy)){original();return;}
    }
    if(!composite) {
        for(unsigned i=0;i<points;i++) {
            uint32_t address=kind==1?WORD_AT(frame+16+i*4):frame+4+i*8;
            if(kind==1 && !guest_writable(address,12)){original();return;}
            for(unsigned axis=0;axis<2;axis++) {
                unsigned n=i*2+axis;addresses[n]=address+axis*4;
                saved[n]=WORD_AT(addresses[n]);
                if(kind==0)xy[n]=(float)(int32_t)saved[n];else memcpy(&xy[n],&saved[n],4);
                if(!isfinite(xy[n])){original();return;}
                float centre=axis?cy:cx,scale=axis?pc_ui_scale_y(ratio):pc_ui_scale_x(ratio);
                float extent=axis?480.f:640.f;
                if(gradient && xy[n]<=1.f)xy[n]=0.f;
                else if(gradient && xy[n]>=extent-1.f)xy[n]=extent;
                else xy[n]=centre+(xy[n]-centre)*scale;
                /* Reject invalid target/vertex data before modifying anything. */
                if(!isfinite(xy[n]) || xy[n]<-1000000.f || xy[n]>1000000.f){original();return;}
            }
        }
        /* Save every vertex first: pointer arguments may alias. */
        for(unsigned i=0;i<points*2;i++) {
            if(kind==0)WORD_AT(addresses[i])=(uint32_t)(int32_t)roundf(xy[i]);
            else memcpy(&WORD_AT(addresses[i]),&xy[i],4);
        }
    }
    uint32_t old_tag=ui_tag;int old=ui_placement;
    ui_place(1,caller);original();ui_place(old,old_tag);
    if(!composite)for(unsigned i=0;i<points*2;i++)WORD_AT(addresses[i])=saved[i];
}
