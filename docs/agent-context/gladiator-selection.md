# Online Gladiator bot selection (#229)

The shared pc_network_bots_guest_active adapter in
src/network/pc_network_bots_guest.inc wraps the original US 3568C0 activity
predicate. That predicate requires a human controller-roster entry even when
an AI actor and body exist. Only audited actor consumers may accept bot slots;
controller enumeration and human serializers must retain their original checks.

US 35A5C0 is the host's Gladiator (mode 18) owner publication. Its call to 3568C0
returns at 35A609. Rejecting the selected AI there calls 27C2F0 again every update,
causing repeated reassignment even after the suicide fallback selected a bot.
Add only that return PC to the existing bot-activity allowlist. Online enabled,
configured human/bot ranges, actor-pool presence and all human fallback rules
remain unchanged. No random-choice, score, suicide, death or packet rules change.
The host needs this fix; existing bot actor-reference serialization and the guest
35A670 receiver are retained. Shared Windows and TSFP_NATIVE_NETWORK source
covers native Mac/Linux as well.

Asset-free CI: test_network_guest_dispatch.py checks the exact new caller for
all human/bot/out-of-range slots on Windows, native networking and the disabled
fallback. Existing score, controller, shot and replica tests remain active.

Supported local US XBE only: test_network_bot_results.py lifts the original
64350 selector, 27C2F0 reassignment, 35A5C0 publisher, 35A670 receiver and 3568C0
predicate into a temporary fixture. gladiator_selection.c executes host/guest
human suicides, 120 stable host updates, bot suicide, human/bot killer transfers,
a disconnected human and human-only selection. Production bot encode/decode
and the original receiver verify that selected bot refs arrive and restore full
health. Removing the new allowlist case must reproduce repeated reselection.
Generated game instructions and assets stay private and are skipped in CI.

These original-code and boundary checks are distinct from a live two-instance
Online gameplay test. Record build and live acceptance separately.

Windows validation: 14 focused cases pass, including the deliberately removed
fix that reproduces reselection. Release D3D11/Vulkan build and post-build
checks pass; the unrelated virtual-pad focus probe skipped when foreground
activation was refused. Executable identity and matching crash symbols sealed.
Live Windows acceptance now passes on the same-machine private-host Online v2
path with two native games, six bots, Original60 and Vulkan. The stock Rockets
weapon preset and file-driven mouse/fire input produced a guest self-kill while
both peers identified that guest as Gladiator immediately before death. Both
self-kill counters incremented, both peers selected the same bot, and one shared
bot ownership interval lasted 16.19 seconds within the next 40 seconds. Play
continued without disconnect or an endless selector. A separate normal-loadout
pair also passed movement/replication and sustained bot ownership. No guest RAM
was written to seed the role or death. Saves/settings were isolated.

The live host-player suicide case was not observed; the original-code fixture
covers it. Physical WAN/community-relay and native Mac/Linux gameplay remain
unverified. Ignored logs/issue-229-live-summary.json records the executable hash
and acceptance scope; screenshots and numeric observations stay local.
