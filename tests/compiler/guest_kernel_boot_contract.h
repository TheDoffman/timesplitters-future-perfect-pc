/* Synthetic contract boundary, not a replacement kernel header. */
#include "platform/xbox_winnt.h"
#include "platform/win32_compat.h"
#define XBOX_LOG_ERROR 0
#define XBOX_LOG_WARN 1
#define XBOX_LOG_INFO 2
#define XBOX_LOG_DEBUG 3
#define XBOX_LOG_TRACE 4
void xbox_GuestLockInit(void);
void xbox_GuestLockEnter(void);
void xbox_GuestLiftedEnter(void);
void xbox_hal_init_timers(void);
void xbox_kernel_init(void);
void xbox_kernel_shutdown(void);
void xbox_log(int,const char *,const char *,...);

/* Count synchronization through actual host locks. Only the diagnostic source
 * keeps these aliases; the fixture undefines them before forwarding. */
void boot_enter(CRITICAL_SECTION *);
void boot_leave(CRITICAL_SECTION *);
#define EnterCriticalSection boot_enter
#define LeaveCriticalSection boot_leave
