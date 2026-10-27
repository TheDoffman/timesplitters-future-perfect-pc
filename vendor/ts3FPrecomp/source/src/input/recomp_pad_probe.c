/*
 * recomp_pad_probe -- what the runtime sees of the pads, live.
 *
 *   recomp_pad_probe [seconds] [xinput]
 *
 * Reads the four slots through recomp_pad.h, the same code a title's input
 * goes through, and prints each pad's name once and then its state
 * whenever it changes. "xinput" reads through XInput instead of SDL3, the
 * same as RECOMP_PAD_API=xinput. A pad that shows up here but does nothing
 * in a game is a binding question (input_bindings.json); one that does
 * not show up here is not reaching the runtime at all.
 */
#include "recomp_pad.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
static void nap(void) { Sleep(20); }
static unsigned long long ms(void) { return GetTickCount64(); }
#else
#include <time.h>
static void nap(void) { struct timespec t = { 0, 20000000 }; nanosleep(&t, NULL); }
static unsigned long long ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (unsigned long long)t.tv_sec * 1000ull + (unsigned long long)t.tv_nsec / 1000000ull;
}
#endif

static void show(int slot, const RecompPadState *s)
{
    static const struct { unsigned bit; const char *name; } b[] = {
        { RECOMP_PAD_A, "A" }, { RECOMP_PAD_B, "B" }, { RECOMP_PAD_X, "X" }, { RECOMP_PAD_Y, "Y" },
        { RECOMP_PAD_LEFT_SHOULDER, "LB" }, { RECOMP_PAD_RIGHT_SHOULDER, "RB" },
        { RECOMP_PAD_START, "Start" }, { RECOMP_PAD_BACK, "Back" },
        { RECOMP_PAD_LEFT_THUMB, "LS" }, { RECOMP_PAD_RIGHT_THUMB, "RS" },
        { RECOMP_PAD_DPAD_UP, "Up" }, { RECOMP_PAD_DPAD_DOWN, "Down" },
        { RECOMP_PAD_DPAD_LEFT, "Left" }, { RECOMP_PAD_DPAD_RIGHT, "Right" },
    };
    size_t i;

    printf("pad %d: LT %3u RT %3u  L %6d,%6d  R %6d,%6d  ", slot + 1, s->left_trigger,
           s->right_trigger, s->lx, s->ly, s->rx, s->ry);
    for (i = 0; i < sizeof b / sizeof b[0]; i++)
        if (s->buttons & b[i].bit)
            printf("%s ", b[i].name);
    printf("\n");
    fflush(stdout);
}

int main(int argc, char **argv)
{
    int seconds = argc > 1 ? atoi(argv[1]) : 15;
    unsigned long long end;
    RecompPadState last[RECOMP_PAD_SLOTS];
    int seen[RECOMP_PAD_SLOTS] = { 0 }, i;

    if (argc > 2 && strcmp(argv[2], "xinput") == 0)
        recomp_pad_set_api(RECOMP_PAD_API_XINPUT);
    if (seconds <= 0)
        seconds = 15;
    printf("reading pads through %s for %d s; press things\n", recomp_pad_api_name(), seconds);
    fflush(stdout);
    memset(last, 0, sizeof last);
    end = ms() + (unsigned long long)seconds * 1000ull;
    while (ms() < end) {
        for (i = 0; i < RECOMP_PAD_SLOTS; i++) {
            RecompPadState s;
            int on = recomp_pad_read(i, &s);
            if (on && !seen[i]) {
                char name[80];
                recomp_pad_name(i, name, sizeof name);
                printf("pad %d connected: %s\n", i + 1, name);
                seen[i] = 1;
            } else if (!on && seen[i]) {
                printf("pad %d disconnected\n", i + 1);
                seen[i] = 0;
            }
            if (on && memcmp(&s, &last[i], sizeof s) != 0) {
                show(i, &s);
                last[i] = s;
            }
        }
        nap();
    }
    for (i = 0; i < RECOMP_PAD_SLOTS && !seen[i]; i++)
        ;
    if (i == RECOMP_PAD_SLOTS)
        printf("no pad answered\n");
    return 0;
}
