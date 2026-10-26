#ifndef TSFP_PC_PROMPTS_H
#define TSFP_PC_PROMPTS_H
#include <stddef.h>
#include <stdint.h>
#include "recomp_pad.h"
#include "pc_settings.h"

/* Input events select a device; polling an unchanged held button does not. */
void pc_prompts_keyboard(void);
void pc_prompts_controller(const RecompPadState *pad, int connected);
int pc_prompts_keyboard_active(void);
void pc_prompts_reset(void);
int pc_prompts_format(const char *source, char *out, size_t capacity,
                      const int keys[PC_ACTION_COUNT], int mouse);
int pc_prompts_rewrite(const char *source, char *out, size_t capacity);
/* Synchronous word-wrap only; mapping facts expire on return. */
void pc_prompts_layout_begin(void);
void pc_prompts_layout_end(void);
/* Read-only chunk, possibly one page; string scanners must continue at its end.
 * Known-size argument frames require read_span so a page boundary is not EOF. */
size_t pc_prompts_read_region(uintptr_t address);
size_t pc_prompts_read_span(uintptr_t address,size_t minimum);
#endif
