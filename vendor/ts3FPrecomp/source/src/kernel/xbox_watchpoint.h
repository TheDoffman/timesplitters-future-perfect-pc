/**
 * Guest memory watchpoints: "what wrote this address?"
 *
 * See xbox_watchpoint.c for why this exists and how to use it.
 */
#ifndef XBOX_WATCHPOINT_H
#define XBOX_WATCHPOINT_H

#include <stdint.h>

#ifdef _WIN32
#include <windows.h>

/* Arm whatever RECOMP_WATCH_WRITE asks for. Call once, after guest memory is
 * mapped and the XBE is loaded -- the pages have to exist to be protected. */
void xbox_watch_init(void);

/* Offer an access violation to the watchpoints. Returns non-zero when it was
 * a watched access and has been serviced, in which case the caller must
 * return EXCEPTION_CONTINUE_EXECUTION and must not report a crash. */
int xbox_watch_handle_av(PEXCEPTION_POINTERS ep, uintptr_t fault_addr,
                         int is_write);

/* Offer a single-step exception to the watchpoints. Same return convention.
 * Must be called before any other handling of EXCEPTION_SINGLE_STEP. */
int xbox_watch_handle_step(PEXCEPTION_POINTERS ep);

#else
#define xbox_watch_init()                       ((void)0)
#define xbox_watch_handle_av(ep, addr, wr)      (0)
#define xbox_watch_handle_step(ep)              (0)
#endif

/* Print every guest address currently holding `value`. Answers "who is
 * holding this bad pointer?", which is the question that gives the
 * watchpoint above an address to watch. Safe to call from a fault handler. */
/* RECOMP_WATCH_ARM_ON=<text>: hold the watches until the title opens a file
 * whose path contains <text>. Called with every translated guest path. */
#ifdef _WIN32
void xbox_watch_note_path(const char *xbox_path);
#else
#define xbox_watch_note_path(p)                 ((void)0)
#endif

void xbox_watch_scan_value(uint32_t value);

/* Called from the crash path: runs the scan if RECOMP_FIND_VALUE is set. */
void xbox_watch_scan_on_crash(void);

#endif /* XBOX_WATCHPOINT_H */
