# Network actor/body lifetime

`src/network/pc_network_bot_lifecycle.inc`, included by the shared display adapter,
repairs the Online Arcade guest-bot destruction boundary. The existing
`pc_story_network_world_remove` wrapper still captures a corpse first, then routes
the original world destructor through this adapter. For a current guest bot
body (type8, pool slot, reciprocal ownership, active Online session), remove the
actor via original15BCC0 and retire only its hand-owned type32 attachments before
world destruction. Afterwards clear still-matching actor/bot-table links. Retain
scores and inventory definitions; do not run host AI/death/inventory routines or
spawn pickups. Host, human, Story and foreign/reused objects retain their existing
destruction path. Callback-installed replacement links are preserved.

`pc_network_bots_revive` uses originalDAC30/D7570, validates the new body backlink,
then registers the actor via original15BCA0 only if absent. Failed construction
does not publish an empty actor. Neither duplicate membership nor preservation
of a retired actor's list entry is required for replication; scores live in its
stable actor row, independently of list membership.

`pc_arcade_join_guest.inc` no longer restores a snapshot of the whole actor list
after original15DDA0 retires a bot for a newcomer. It additionally removes that
stable actor identity before clearing/reconstructing the row, including when its
old body was already retired or reused. Bot-body table compaction occurs at
retirement, so a later construction retry cannot retain the old body's pointer.
Only successful construction registers the human. Other actor identities, scores,
team selection and camera/global restoration remain as before.

`pc_network_bots_pose_weapons` validates live type8/body/actor ownership before
calling original1BE870 for either hand. Previously ownership was checked only
after both native calls. Local asset/bone checks still follow those calls so
their existing model preparation remains intact.

`tests/network/test_bot_lifecycle.py` runs the production adapters for Windows
and TSFP_NATIVE_NETWORK with asset-free engine doubles, plus original US
15BCA0/15BCC0/D8C00 when the verified private XBE is available. It covers removal,
owned versus foreign weapons, failed construction/revival, score preservation,
single registration, callback replacement and early pose admission. Negative
controls deliberately retain the retired actor or omit revival registration and
must fail at their corresponding assertions. It is registered in the runtime CI
suite selection. `arcade_join_guest.c` injects a failed replacement and retry and
asserts absence during construction, table compaction and restored final roster.
Existing corpse/limb/Story/dispatch/promotion tests cover surrounding contracts.

These are verified source-level lifetime defects. The November14 dumps establish
an invalid weapon index and a listed null body, but do not identify the original
producer. Do not claim these changes prove a common cause for all reports or a
fix for the differently symbolized November7 build. Preserve that distinction
in gameplay acceptance and release notes; current host results belong in STATUS.

Windows build note from this investigation: MSVC14.42/O2 spent about20minutes
on originalDD1B0 (2708 instructions) while its object stayed zero-length and CPU
time advanced. A quiet compiler is not by itself a failed build. Temporary
worker/cache experiments and exact object provenance are recorded in STATUS;
repository optimization defaults were not changed. The diagnostic Debug route
also exposed the existing tsfp_guest_maps /O2 versus /RTC1 conflict from patch0213.
Its local override is validation-specific, not a repair to the normal Debug
configuration or a substitute for the successful Release build/gameplay checks.
