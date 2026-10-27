/* Frontend pointer policy, independent of the host window and guest ABI. */
#ifndef TSFP_PC_MENU_MOUSE_H
#define TSFP_PC_MENU_MOUSE_H
#include <math.h>
#include <stdint.h>

typedef struct PcMenuPointer {
    float x, y, width, height; /* client pixels */
    int valid, left, right, wheel; /* wheel: signed notches, positive is up */
} PcMenuPointer;
typedef struct PcMenuMouse {
    uint32_t menu;
    float x, y;
    int valid, left, right, blocked, mouse_navigation, wheel_pending;
    int scrolling;
    uint64_t wheel_at, pointer_at;
} PcMenuMouse;

/* Snap pointer position to the setting's inclusive discrete range. */
static inline unsigned pc_menu_slider_value(float x,float left,float width,unsigned maximum)
{
    if(!isfinite(x) || !isfinite(left) || !isfinite(width) || !(width>0)) return 0;
    float fraction=(x-left)/width;
    if(fraction<=0) return 0;
    if(fraction>=1) return maximum;
    return (unsigned)floorf(fraction*(float)maximum+.5f);
}

/* Invert presentation letterboxing, then the centred proportional UI fit.
 * Coordinates outside the displayed image stay invalid, not edge-clamped. */
static inline int pc_menu_mouse_position(PcMenuPointer p, float aspect,
        float guest_w, float guest_h, float *x, float *y)
{
    float w=p.width,h=p.height,scale,ox,oy;
    if(!p.valid || !isfinite(p.x) || !isfinite(p.y) || !(w>0) || !(h>0) ||
       !isfinite(aspect) || !(aspect>0) || !(guest_w>0) || !(guest_h>0)) return 0;
    if(w>h*aspect) w=h*aspect; else h=w/aspect;
    ox=(p.width-w)*.5f; oy=(p.height-h)*.5f;
    if(p.x<ox || p.x>=ox+w || p.y<oy || p.y>=oy+h) return 0;
    scale=fminf(w/guest_w,h/guest_h);
    *x=guest_w*.5f+(p.x-p.width*.5f)/scale;
    *y=guest_h*.5f+(p.y-p.height*.5f)/scale;
    return isfinite(*x) && isfinite(*y);
}

/* Return hover=1, fresh left=2, fresh right=4, wheel up=8/down=16.
 * A stationary cursor must not
 * undo keyboard navigation. Held buttons never select the next screen. */
static inline unsigned pc_menu_mouse_actions(PcMenuMouse *s,uint32_t menu,PcMenuPointer p,uint64_t now)
{
    unsigned result=0;
    if(menu!=s->menu || !p.valid) {
        s->blocked=p.left||p.right;
        s->valid=0;
        s->wheel_pending=0;
        s->scrolling=0;
        p.wheel=0; /* discard input collected for a different screen */
    }
    /* A click can open a different menu without any further pointer motion.
     * Keep ownership there so an old held direction cannot start repeating
     * again. An invalid pointer or an explicit empty menu releases it. */
    if(!p.valid || !menu) s->mouse_navigation=0;
    if(!p.left && !p.right) s->blocked=0;
    if(p.valid && menu) {
        int moved=s->valid && (p.x!=s->x || p.y!=s->y);
        if(moved) s->pointer_at=now;
        /* Wheel packets are intermittent while pointer packets arrive every
         * frame. Keep one gesture's ownership between notches and through
         * continuous motion, rather than snapping back to a row under it.
         * After both inputs settle, only a NEW move resumes hover. */
        if(s->scrolling && now-s->wheel_at>=250 && now-s->pointer_at>=100)
            s->scrolling=0;
        if(moved && !s->scrolling) result|=1;
        if(!s->blocked && p.left && !s->left) result|=2;
        if(!s->blocked && p.right && !s->right) result|=4;
        if(p.wheel) {
            /* Reversing the wheel cancels any unconsumed opposite motion. */
            if((p.wheel>0)!=(s->wheel_pending>0)) s->wheel_pending=0;
            s->wheel_pending+=p.wheel>8?8:p.wheel<-8?-8:p.wheel;
            if(s->wheel_pending>8) s->wheel_pending=8;
            if(s->wheel_pending<-8) s->wheel_pending=-8;
        }
        if(result&6) {s->wheel_pending=0;s->scrolling=0;} /* clicks win immediately */
        else if(s->wheel_pending) {
            result=(s->wheel_pending>0)?8:16; /* no simultaneous hover snap */
            s->scrolling=1;s->wheel_at=now;s->pointer_at=now;
            s->wheel_pending+=s->wheel_pending>0?-1:1;
        }
    }
    s->menu=menu; s->x=p.x; s->y=p.y; s->valid=p.valid;
    s->left=p.left; s->right=p.right;
    return result;
}
#endif
