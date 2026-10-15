#ifndef TSFP_NATIVE_INPUT_H
#define TSFP_NATIVE_INPUT_H
#include <SDL3/SDL.h>
struct XBOX_GAMEPAD;
int tsfp_native_key_down(int vk);
int tsfp_native_input_focused(void);
void tsfp_native_input_event(const SDL_Event *event, SDL_Window *window);
void tsfp_native_input_frame(SDL_Window *window);
void tsfp_native_mouse_sample(struct XBOX_GAMEPAD *out);
void tsfp_native_mouse_config(int enabled, double sensitivity, int invert_y);
int tsfp_native_input_test(void);
#endif
