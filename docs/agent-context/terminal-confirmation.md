# Mission computer confirmation (#605)

The report is on CachyOS/Linux0.7.1 with a saved60FPS cap: controller A sometimes
cannot click icons in What Lies Below, while leaving/re-entering still works.
No comments, controller mapping or failing guest capture were supplied. The
following are proven code-level failure paths, not a confirmed live reproduction
of that particular installation.

US3700D0 obtains held input through42760 at3700EC and compares the whole mask
with operator+20. Holding an additional input makes equality false even when
the configured confirmation control is down. The shared call-site wrapper
pc_terminal_confirm_buttons reads the operator saved at callee ESP+24, uses its
configured mask, calls the original reader and masks only this result. Bounds
checks use a fresh PcGuestRegion; invalid/zero masks retain the original result.
Other input-reader callers, bindings and global controller masks are unchanged.
The wrapper is defined alongside the shared native timing guest adapter, and
the generated call-site selection lives in pc_native_numeric.h.

At370461 the terminal also writes its held-input inhibition when whole-game
ticks are zero. Positive native fractional calls can therefore suppress a press
before the next authored terminal update. Only that store to76C288 retains its
previous value while native timing and finite positive fractional time are
active. Original mode and paused/invalid intervals retain their original store.
No text clock, icon callback, puzzle deadline, movement or firing gate changes.

test_original_terminal_confirmation in test_native_timing.py translates the
verified US XBE dispatcher370490/3700D0 and actual held-mask reader42760. Sound,
rendering, script input and icon callbacks are controlled boundaries. It covers
four local physical slots, the alternate input table, remapping, additional held
input, single press/hold/release, inhibition,60/120/144/180/240 schedules, quarter
speed, an irregular nominal60 schedule, pause/resume, ABI and clock restoration.
Independent mutations removing the button mask or fractional-store correction
must reproduce failure. Existing terminal text and mouse fixtures remain separate.

Windows151 timing/terminal cases pass. Linux/WSL GCC and Clang compile and execute
the same original-code confirmation fixture and both failing mutations. This
establishes CPU/input-policy behavior, not physical-controller or live mission
acceptance. Native Mac Clang also passes the same fixture and both mutations;
the complete Windows game build and isolated post-build checks pass. Native
full game builds, physical controller and reporter acceptance remain pending. Keep
#605 open until a published release containing an accepted fix is available.
