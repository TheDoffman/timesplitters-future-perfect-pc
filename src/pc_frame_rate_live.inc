/* Included once by recomp_config.c. Publish at the next guest timing update;
 * the renderer/input threads only read immutable rate strings. */
#ifdef _WIN32
#include <windows.h>
static volatile LONG pc_rate_pending=-1, pc_rate_active=-1;
#define RATE_LOAD(p) InterlockedCompareExchange((p),-1,-1)
#define RATE_EXCHANGE(p,v) InterlockedExchange((p),(v))
#else
#include <stdatomic.h>
static atomic_int pc_rate_pending=-1, pc_rate_active=-1;
#define RATE_LOAD(p) atomic_load(p)
#define RATE_EXCHANGE(p,v) atomic_exchange((p),(v))
#endif
int recomp_config_request_native_rate(int rate)
{
    if(rate!=0 && rate!=120 && rate!=144)return 0;
    RATE_EXCHANGE(&pc_rate_pending,rate);
    return 1;
}
void recomp_config_commit_native_rate(void)
{
    int rate=(int)RATE_EXCHANGE(&pc_rate_pending,-1);
    if(rate>=0)RATE_EXCHANGE(&pc_rate_active,rate);
}
static const char *pc_frame_rate_live_value(void)
{
    switch(RATE_LOAD(&pc_rate_active)) {
    case 0:return "0";
    case 120:return "120";
    case 144:return "144";
    default:return NULL;
    }
}
#undef RATE_LOAD
#undef RATE_EXCHANGE
