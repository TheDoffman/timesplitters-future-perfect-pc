# Local online synchronization telemetry

The first release instrumentation changes observation only: no packet format,
update cadence, smoothing, authority, timeout or gameplay change. It builds on
native-multiplayer-ui d477b47a; source is on feature/online-sync-telemetry.

`src/network/pc_network_sync.h` owns bounded numeric counters. `pc_network.c`
updates them under its existing transport lock after compatibility/session and
bot-state admission. Bot tick differences distinguish duplicates from older
updates; new contexts reset counters and per-slot timing while incrementing a
context serial. Queue coalescing counts a newer admitted bot state replacing an
unconsumed state. Duplicate/reordered frames do not change timing history.
Transport shutdown also resets the context. Packet counters count title
payloads returned by the transport, excluding extension/control traffic.

`pc_network_sync_snapshot` returns sixteen integers under the read lock. The
existing shared camera sampler (TSFP_PC_DISCORD, independent of the user's
Discord presence preference) publishes at most once per ten seconds while LAN
or Online is active. Its fields are schema, context serial, transport mode,
bot-context role, bot count, title packet count, current maximum bot silence,
accepted bot updates, duplicates, reordered updates, coalesced updates, maximum
bot arrival interval, mean absolute change between successive per-bot intervals,
maximum acceptance-to-consumption delay, fresh existing RTT (-1 unavailable),
and maximum camera-callback interval during this reporting window. Time units
are milliseconds. Counts and maxima accumulate within a context, except the
camera interval, which resets after each sample. Exported integers saturate.
Role0 means no active bot context; it does not identify a human-only host/guest.

Arrival timing is observed when the game polls receive, not at the NIC. It can
include local stalls. Queue age is local acceptance-to-consumption time, not
one-way transit or authoritative state age. Tick gaps are not presented as
packet loss; loss, send suppression and delay cannot be distinguished here.
Camera callback gaps are neither GPU completion nor monitor scanout. There is
no native human-player correction-distance instrumentation in this first pass.

`pc_capture_network` uses a separate fixed64-entry numeric breadcrumb ring on
Windows and native POSIX. Sixteen fields per ten-second sample retain up to four
samples without evicting gameplay/model/allocation context. Producers allocate
nothing and perform no file I/O. Interrupted writes can omit individual entries;
interpret complete groups by their sample timestamp, not as a perfect event log.
Windows shared capture schema6 requires the matching rebuilt helper.

Updated game/helper builds are required; the updated launcher is needed to
include the additional file in ZIP export. No release assets are changed by
source-only validation.

The capture helper atomically replaces one bounded `network-sync.txt` next to
`capture-health.json` when a new complete sample is available, using the existing
validated capture session and safe report root. It includes executable SHA256,
platform and PID for provenance; executable hashing occurs once in the helper.
A normal session leaves the latest file available for reporting nonfatal stutter.
This is one latest file per report root; simultaneous games sharing a root can
replace one another's latest file. Isolated tests use separate report roots.
Fatal/hang reports contain `network-history.txt`; the shared launcher allowlist
includes that file in ZIP exports (8192-byte cap). No extra network requests,
player names, addresses, invitation codes, tokens or packet payloads are added.
Nothing is uploaded automatically; users choose what to share.

Testing: `test_network_sync.py` checks deterministic jitter/coalescing/order,
zero timestamps, slot isolation, resets and saturation without assets/network.
The real UDP fixture verifies production telemetry admission. Crash tests check
fatal capture, bounded rings and normal-session export; native POSIX fixtures
exercise the same report on their host. Windows launcher tests verify ZIP export.
CI selects the new measurement test through runtime-suites.json. Host results
and unavailable checks belong in STATUS; Mac/Linux execution remains separate.

Release telemetry supplies observational correlations. Before changing buffering
or recovery, reproduce the pattern with independently varied RTT, jitter, loss,
reordering, burst delivery and local frame stalls. Existing constant-delay runs
are not physical-WAN or jitter/loss acceptance. Never infer that these counters
identify the historical corrupted weapon-record producer.

## Vehicle acceptance and the current Online route

`TSFP_ONLINE_TEST_COMMAND` makes `online_activate` in
`src/interface/pc_display_menu.c` bypass the native multiplayer browser. The
default `test-online-bot-motion.py` route uses an isolated v2 directory and
direct UDP. Its gameplay observations do not validate the current Community
menu, v3 room coordination or relay delivery. Current-service acceptance must
identify the actual native menu path and the service-advertised transport;
directory HTTP RTT is still not gameplay latency.

The supported US Siberia jeep is class `0x400000`. Its rigid-body serialization
uses original `35B990`/`35BB10`, with local-driver admission through `177CE0`.
`35DB60` and `35BD70` traverse the `7A20C0` list. The separate class
`0x10000000`, registry `78B720` and `358A90`/`358BF0` message44 path are not
evidence about this jeep. A read-only vehicle probe must validate the physics
handle, entity class and driver ownership, and require actual travel. A seated
player against a pillar is not a moving-driver acceptance case. Asynchronous
body/position reads do not establish packet arrival cadence or rendered pacing.
