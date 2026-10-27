/* Project-owned fatal diagnostic boundary for the x86 divide-error trap.
 * The translator has not committed quotient/remainder and publishes its local
 * frame register before entering here. This is not guest SEH dispatch.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#endif

#if defined(_MSC_VER)
__declspec(noreturn)
#else
__attribute__((noreturn))
#endif
void tsfp_guest_divide_error(uint32_t site, unsigned reason)
{
    fprintf(stderr, "[GUEST-DIVIDE] site=0x%08X reason=%s; guest exception dispatch unavailable\n",
            (unsigned)site, reason ? "quotient-overflow" : "zero-divisor");
    fflush(stderr);
#ifdef _WIN32
    ULONG_PTR details[2] = {site, reason};
    RaiseException(EXCEPTION_INT_DIVIDE_BY_ZERO, EXCEPTION_NONCONTINUABLE, 2, details);
#else
    raise(SIGFPE);
#endif
    abort(); /* Never resume if a host signal handler unexpectedly returns. */
}
