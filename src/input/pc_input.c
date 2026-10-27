#include "pc_input.h"
#include <math.h>
#include <string.h>

static void clear_transient(PcInput *s)
{
    memset(s->keys, 0, sizeof s->keys);
    pc_reload_reset(&s->reload);
    s->dx = s->dy = 0;
    s->motion_at = 0;
    s->wheel_queue = s->wheel_key = 0;
    s->wheel_until = 0;
}

void pc_input_focus(PcInput *s, int focused)
{
    clear_transient(s);
    s->focused = !!focused;
    if (!focused) s->captured = 0;
}

void pc_input_capture(PcInput *s, int captured)
{
    /* Never carry a held fire button or old movement through a capture change. */
    clear_transient(s);
    s->captured = s->focused && captured;
}

void pc_input_key(PcInput *s, unsigned key, int down)
{
    if (key < 256 && s->focused) s->keys[key] = !!down;
}

void pc_input_key_at(PcInput *s, unsigned key, int down, uint64_t now)
{
    if(key<256 && s->focused) {
        pc_reload_event(&s->reload,key,s->keys[key],down,now);
        pc_input_key(s,key,down);
    }
}

void pc_input_motion(PcInput *s, double dx, double dy, uint64_t now)
{
    if (!s->focused || !s->captured || !isfinite(dx) || !isfinite(dy)) return;
    /* Menus/cutscenes may not run the player update. Discard old motion
       rather than accumulating a large turn for the return to gameplay. */
    if (now - s->motion_at > 50) { s->dx = s->dy = 0; s->motion_at = now; }
    s->dx += dx;
    s->dy += dy;
}

void pc_input_wheel(PcInput *s, int steps)
{
    if (s->focused && s->captured) {
        s->wheel_queue += steps;
        if (s->wheel_queue > 16) s->wheel_queue = 16;
        if (s->wheel_queue < -16) s->wheel_queue = -16;
    }
}

int pc_input_down(PcInput *s, unsigned key, uint64_t now)
{
    /* F23/F24 are configurable wheel bindings. Give each notch a press and
       a release, including when several notches arrive in one raw packet. */
    int old_wheel=s->wheel_key;
    if (now >= s->wheel_until) {
        if (s->wheel_key) { s->wheel_key = 0; s->wheel_until = now + 40; }
        else if (s->wheel_queue) {
            s->wheel_key = s->wheel_queue > 0 ? 0x86 : 0x87;
            s->wheel_queue += s->wheel_queue > 0 ? -1 : 1;
            s->wheel_until = now + 60;
        }
    }
    if(old_wheel!=s->wheel_key) {
        if(old_wheel)pc_reload_event(&s->reload,(unsigned)old_wheel,1,0,now);
        if(s->wheel_key)pc_reload_event(&s->reload,(unsigned)s->wheel_key,0,1,now);
    }
    return s->focused && key > 0 && key < 256 && (s->keys[key] || key == (unsigned)s->wheel_key);
}

void pc_input_normalize_movement(int16_t *x, int16_t *y)
{
    double ax = fabs((double)*x), ay = fabs((double)*y);
    double length = hypot(ax, ay);
    double diagonal = ax + ay ? 2.0 * fmin(ax, ay) / (ax + ay) : 0.0;
    double scale = length > 32767.0 ? 32767.0 / length : 1.0;
    /* The title attenuates diagonal travel after reading its Xbox stick.
       Preserve cardinal speed, then compensate smoothly as both axes engage.
       This gain is measured at the player position in Story, not inferred
       from the outgoing stick radius alone. */
    scale *= 1.0 + 0.25 * diagonal;
    *x = (int16_t)fmax(-32767.0, fmin(32767.0, round(*x * scale)));
    *y = (int16_t)fmax(-32767.0, fmin(32767.0, round(*y * scale)));
}

int pc_input_radial_axis_magnitude(int16_t axis, int16_t other,
                                   int sign, int deadzone)
{
    double length = hypot((double)axis, (double)other);
    double value = (double)axis * sign;
    double gain;
    if (value <= 0 || length <= deadzone || deadzone >= 32767)
        return 0;
    /* The same radial gain applies to both components. Subtracting the
       deadzone from each component separately slows diagonal movement/aim. */
    gain = (fmin(length, 32767.0) - deadzone) / (32767.0 - deadzone);
    return (int)(255.0 * gain * value / length + 0.5);
}

uint64_t pc_input_consume_motion(PcInput *s, uint64_t now, double *dx, double *dy)
{
    uint64_t age = now - s->motion_at;
    *dx = s->dx; *dy = s->dy;
    s->dx = s->dy = 0;
    /* The consumed batch is empty now. Start its age at this consumption,
       otherwise a later packet can expire fresh counts against the previous
       batch's timestamp in the middle of an ordinary render interval. Keep
       the saved age for rejecting this batch if gameplay was suspended. */
    s->motion_at = now;
    return age;
}

int pc_input_look(PcInput *s, uint64_t now, float sensitivity, int invert_y,
                  float fov_scale, float *yaw, float *pitch)
{
    double dx, dy;
    uint64_t age = pc_input_consume_motion(s, now, &dx, &dy);
    if (!s->focused || !s->captured || age > 100 ||
        !isfinite(*yaw) || !isfinite(*pitch) || !isfinite(sensitivity) ||
        sensitivity <= 0 || !isfinite(fov_scale) || fov_scale <= 0 ||
        (!dx && !dy)) return 0;
    /* The Xbox camera uses degrees. Counts, not elapsed time or stick
       velocity, determine the turn. Wrap yaw and limit vertical view. */
    *yaw = (float)remainder((double)*yaw - dx * sensitivity * fov_scale, 360.0);
    *pitch += (float)(dy * sensitivity * fov_scale * (invert_y ? 1 : -1));
    if (*pitch > 80) *pitch = 80;
    if (*pitch < -80) *pitch = -80;
    return 1;
}

/* Do not carry a held shot into terminal selection, or a held selection back
 * into firing. A fresh press after release is required at either transition. */
void pc_input_terminal_context(PcInput *s, int active, int left_down)
{
    if (!!active != s->terminal_active) s->terminal_blocked = !!left_down;
    s->terminal_active = !!active;
    if (!left_down) s->terminal_blocked = 0;
}

int pc_input_terminal_click(PcInput *s, int left_down)
{
    if (!left_down) s->terminal_blocked = 0;
    return s->focused && s->captured && s->terminal_active &&
           !s->terminal_blocked && left_down;
}

int pc_input_terminal_suppresses_left(PcInput *s, int left_down)
{
    if (!left_down) s->terminal_blocked = 0;
    return s->terminal_active || s->terminal_blocked;
}
