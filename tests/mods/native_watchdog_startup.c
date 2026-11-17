/* Exercise production watchdog initialization and the unchanged Linux seal. */
#define _POSIX_C_SOURCE 200809L
#include "mod_watchdog.h"
#include "mod_seal.h"
#include <stdio.h>
#include <time.h>
#include <unistd.h>

int main(void) {
    struct timespec delay={0,500000000};
    setvbuf(stdout,NULL,_IONBF,0);setvbuf(stderr,NULL,_IONBF,0);
    if(!tsfp_mod_watchdog_start())return 2;
    if(!tsfp_mod_seal())return 126;
    /* The watchdog is intentionally idle; let a delayed libc thread startup
       finish before returning. All production syscall/descriptor limits apply. */
    if(nanosleep(&delay,NULL))return 3;
    return write(STDOUT_FILENO,"PASS\n",5)==5?0:4;
}
