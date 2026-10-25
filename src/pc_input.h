#ifndef TSFP_PC_INPUT_H
#define TSFP_PC_INPUT_H
#include <stdint.h>
#include "pc_reload_input.h"

typedef struct PcInput {
    unsigned char keys[256];
    PcReloadInput reload;
    int focused, captured;
    int terminal_active, terminal_blocked;
    double dx, dy;
    uint64_t motion_at;
    int wheel_queue, wheel_key;
    uint64_t wheel_until;
} PcInput;

void pc_input_focus(PcInput *s, int focused);
void pc_input_capture(PcInput *s, int captured);
void pc_input_key(PcInput *s, unsigned key, int down);
void pc_input_key_at(PcInput *s, unsigned key, int down, uint64_t now);
/* Host adapter: apply shared reload policy to keyboard magnitudes only. */
void pc_controls_reload_sample(const unsigned char roles[256], int active,
                               int *reload, int *fire, int *secondary);
void pc_input_motion(PcInput *s, double dx, double dy, uint64_t now);
/* Consume/rebase the motion batch and return its previous age in milliseconds. */
uint64_t pc_input_consume_motion(PcInput *s, uint64_t now, double *dx, double *dy);
void pc_input_wheel(PcInput *s, int steps);
int pc_input_down(PcInput *s, unsigned key, uint64_t now);
void pc_input_terminal_context(PcInput *s, int active, int left_down);
int pc_input_terminal_click(PcInput *s, int left_down);
int pc_input_terminal_suppresses_left(PcInput *s, int left_down);
void pc_input_normalize_movement(int16_t *x, int16_t *y);
/* Remap one stick axis after a circular deadzone, preserving its direction. */
int pc_input_radial_axis_magnitude(int16_t axis, int16_t other,
                                   int sign, int deadzone);
int pc_input_look(PcInput *s, uint64_t now, float sensitivity, int invert_y,
                  float fov_scale, float *yaw, float *pitch);
#endif
