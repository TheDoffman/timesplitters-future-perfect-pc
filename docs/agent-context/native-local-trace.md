# Completed local update observations

`pc_native_local_trace.h` is an optional shared diagnostic. Set
`TSFP_NATIVE_LOCAL_TRACE` to an isolated output CSV, or use the Windows bounded
runner's `--trace-local-poses`. The timing hook calls it before publishing the
next interval, at US return address3D3EB after3C440. Other callers do not read
guest state. It runs in Original and native modes; it does not change guest
memory, clocks, input or animation. Disabled/open-failure paths are sticky.

The collector admits one local human, scene IDs below100, and Story/active
Arcade match states1/7. Those are scope filters, not proof of active control:
inspect suppression, delta, identity and actual route evidence. Menus can retain
old player pointers. LAN/split-screen retain their separate pose diagnostics.

Rows contain frame/level ticks, the completed interval's delta/carried ticks,
suppression, player reference position (`+B4`)/yaw/pitch, camera position/direction and three
animation records (body, hand, weapon). Animation records identify entity/model,
clip/cycle, current/previous primary time, speed and next clip/time. Valid-mask
bits0/1/2 identify usable animation records and bit3 identifies a finite camera.
Invalid optional records are zeroed and marked absent; do not count them as
held animations. Root pointers and mandatory finite state must be valid.
Pointer checks bound complete records within the existing64MiB guest heap.

No observation survives into guest execution as gameplay input. The writer stops
at20,000 rows or8MiB including the header, and never reopens after completion or
failure. The existing two-player LAN CSV and its schema remain unchanged.

`test_native_local_trace.py` compiles the actual collector/writer. It checks
fresh changing fields on zero-carried-tick updates, zero delta, scope/address/
finite guards, optional validity, CSV schema and disabled/open-failure/row/byte
limits. Wrong-boundary, stale-time and off-by-one-limit mutations must compile
and fail. Both native CI hosts run these asset-free checks. The existing actual
timing-hook fixture checks integration and unchanged Original/native clocks.

For live acceptance, compare same-build isolated Original/native routes. Require
successive completed frame IDs; distinguish positive fractional intervals from
pause, suppression, clip/cycle/entity changes and legitimately stationary state.
Compare event/input windows and original behavior, rather than requiring every
property to change every frame. Retain synthetic input and capture provenance.
CSV writes add overhead: use separate capture/trace-free pacing runs. These CPU
state observations do not establish bone matrices, submitted mesh freshness,
GPU completion, scanout, physical input latency or complete visual fidelity.
