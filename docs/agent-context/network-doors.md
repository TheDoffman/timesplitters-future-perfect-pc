# Online door animation authority (#330)

The original US automatic-door update is B26C0; its level door table lives at
732A04 with count 732A08, stride 98 hex and a 65-row allocation. A world type 4
entity points back to its row at +7C. The row contains the entity's generation ID,
stable level tag/kind, opening phase, wait timer, progress, target and velocity.
Local AI navigation/collision can start doors on the host. Replicated guest bots
do not run those callbacks. Original 359810 sends only phase bytes during the
joining state transfer; 359880 maps phases 0/3 to fully closed and 1/2 to fully open.
It does not continuously synchronize the opening animation.

`src/network/pc_network_doors_guest.inc`, included by the shared display/menu
adapter, captures host numeric state and applies authenticated guest snapshots.
The existing network-frame wrapper calls it after the original pump and bot
presentation, including human-only matches. It does not run guest AI, invent
door triggers, change locks/speeds or replace the original world/render update.
Fresh state marks the original door dirty so its next update uses those numbers.
Table index plus stable tag/kind identify a level row; its local world generation
and reciprocal backlink must still match. Host pointers and runtime IDs never
cross the wire. Menu/offline/Story states clear the extension context.

`pc_network_doors.h` defines bounded pointer-free numeric rows. The transport
include in `pc_network.c` uses TSFDR01 extensions inside the existing compatible
LAN/Online framing. The expected host IP, admitted XNet session key and current
match must match before a whole batch is validated. At most 16 rows fit in 668
payload bytes; bad lengths, duplicate slots, nonfinite values, out-of-range
progress and unknown flag bits are rejected before publication. Per-row ticks
reject duplicate/reordered data with wrap-safe comparisons. Match/role/host
changes clear pending state. This shares existing socket locking and bridge
routing; it adds no directory/server API or launcher preference.

Hosts publish changed rows once per simulation tick and refresh quiet rows at
least every 15 ticks for loss recovery. Guests preserve the original lock/script
flags while applying phase, timer, progress, physical offset, target, velocity
and swing direction. No extrapolation runs in transport; the original door
callback performs normal between-packet movement, transforms and portal updates.

Tests use the existing `test_network_bot_results.py` CI route. `network_doors.c`
executes the production frame and transport adapters for Windows/native policy:
zero-bot matches, level generation rejection, disabled doors, authenticated
batches, packet loss/reorder, tick wrap and context resets. A mutation replaces
the received progress and must fail. `network_guest_dispatch.c` checks the frame
integration. `tests/network/pc_network.c` also exercises real UDP and Online
bridge receive routing, host session-key rejection and LAN sends.
The isolated socket double uses a namespaced byte-order helper so Darwin's
system-header `htons` macro cannot rewrite its declaration.
The header validator is static inline, allowing strict-warning transport clients
to include the wire contract without using its validation helper.

The supported local US executable additionally runs the original phase receiver
and endpoint setters, demonstrating that its joining stream snaps transitions to
endpoints. Generated instructions and game data remain private; these cases skip
without that supported executable.

Windows acceptance: the unmodified two-game Zeppelin run with six bots reproduced
fully open host doors remaining closed on the guest. The host-state build kept
all ten rows aligned over 1800 asynchronous samples (91.64 seconds): zero of 18000
progress comparisons differed by more than 0.1, versus 765 of 6000 in the baseline;
nine doors opened/closed. Maximum difference was 0.0991 at a moving edge and mean
error 0.0000916. Original60/Vulkan, private-host Online v2 on one computer; this is
not physical WAN or native Mac/Linux gameplay evidence. Build/run hashes and
observations stay in ignored logs. See STATUS for subsequent acceptance.

Related contracts: [actor lifetime](network-actor-lifetime.md),
[Gladiator selection](gladiator-selection.md), [transport](multiplayer-ui.md).

Bunker human-only acceptance: both players used file-driven movement to open
separate doors and cross their door planes. Both peers agreed; 25,200 comparisons
over 91.44 seconds had mean error 0.00000397 and one asynchronous moving-edge
error of 0.100000009. This separately verifies that door transport does not depend
on the experimental bot count or a running AI simulation.
