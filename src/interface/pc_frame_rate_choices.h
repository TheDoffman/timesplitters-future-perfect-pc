#ifndef TSFP_PC_FRAME_RATE_CHOICES_H
#define TSFP_PC_FRAME_RATE_CHOICES_H
/* Shared UI choices; no persistence or host dependencies. */
#define PC_FRAME_RATE_COUNT 4
#define PC_FRAME_RATE_BETA "High refresh is in beta. Gameplay issues may occur."
#define PC_FRAME_RATE_APPLY "Changes apply without restarting."
static int pc_frame_rate_value(int choice)
{ return choice>=1 && choice<PC_FRAME_RATE_COUNT?(choice+1)*60:0; }
static int pc_frame_rate_choice(int rate)
{ return rate==120 || rate==144?1:rate==180?2:rate==240?3:0; }
static const char *pc_frame_rate_label(int choice)
{
    static const char *labels[]={"60 FPS (Original)","120 FPS (Beta)","180 FPS (Beta)","240 FPS (Beta)"};
    return labels[choice>=0 && choice<PC_FRAME_RATE_COUNT?choice:0];
}
#endif
