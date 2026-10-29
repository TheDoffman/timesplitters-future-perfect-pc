# Temporary material matrices and render epochs

Issue #398: Mars Prison cloud/ground UV transforms may flicker at native high
refresh rates. Original US `948B0` compares material `+10` with `7497AC`, and
returns its cached temporary matrix at `+14` on a hit. A miss builds the matrix
through `94660`, allocates 64 bytes through `3E970`, then stores the pointer and
stamp at `94995/94998`. `944D0` also installs a supplied pointer/stamp at
`94502/9450B`. Both producers and the consumer read the same `7497AC` word.

The matrix belongs to the two temporary arenas alternated/reset by `3E930`.
The native clock previously set `7497AC` to the 60 Hz ISR counter. More than
two renders can share that value, so a cached pointer survives until its arena
is reused for camera/light data. `18F3AD` passes this matrix to material rendering;
`2C460` transposes it through `1530E0` and uploads vertex constants 160..163.
Correct texture bytes with wrong matrices therefore produce patterned clouds.
This is an allocation-lifetime error, not a reason to freeze cloud animation,
change sampler filtering or raise the console interrupt clock.

`src/gameplay/pc_native_timing_guest.inc` now publishes `7497B8`, the existing
per-update/render serial, into `7497AC` after either the Original or native
clock path. All three cache reads therefore stay in one domain through live
rate switches. Same-render material/view sharing remains intact; the next
render rebuilds the temporary matrix. The original 60 Hz counters (`563918`,
`7497A8`, `7A58B0`) and raw/game deltas are unchanged. Paused rendering may
refresh temporary storage without advancing animation. No saved format changes.

Validation is registered in the existing Windows/macOS
`tests/gameplay/test_native_timing.py` route:

- `native_render_cache.c` composes the real clock adapter with controlled
  arena/cache services across Original/120/144/180/240, pause, live switches
  and serial wrap. The console-stamp mutation must compile and fail.
- `native_render_matrix_original.c` executes the supported checksum-verified
  US `948B0`, `3E930` and `3E970` bodies, injected locally from the XBE. Matrix
  production and arena overwrites are controlled services. The old ISR stamp
  reproduces stale matrices above 120; per-render stamps retain correct values
  and same-frame cache hits. Without the local XBE this case skips; the
  asset-free production-clock test still runs on both CI hosts.

Private draw captures and gameplay data stay ignored. Offline substitution of
only constants 160..163 removed the defect in the retained bad frame; this
isolates the affected data but is not itself a production replay fix. Actual
Windows scene evidence and physical Mac validation limits belong in STATUS
and MAC_PORT_BACKLOG.
