# Discord Chinese wall visibility investigation (unresolved)

Report: https://discord.com/channels/1566295768489263136/1567758794346340402
Branch: `fix/discord-chinese-wall-visibility`, based on main `831c324a`.
No runtime patch and no resolution comment: the reported failure is not fixed.

The supplied 19-second `bug1ts.mp4` was opened and reviewed in Discord,
including fullscreen and individual paused times. At about 0:01 a small whole
character body is visible over the brick beside the staircase. This is more
than a teammate nameplate. The blue human name `yoto` is a separate element;
the clip later reports a kill of bot Duckman Drake. The hypothesis that the
report only concerns names showing through walls was rejected.

Actual isolated Windows test: main executable SHA256
`550f988c6ec45e9504d74bcecd9486874928b5272712683b7e9e8b9152ac5936`,
Classic, 960x540, one sample, Original 60, Chinese scene 0x19, recommended
six-bot roster, Custom 5 unarmed weapons, character abilities and powerups off.
The roster was confirmed through the ordinary options screen and six other
live type-8 actors. Only normal movement/mouse inputs were published; memory
reads recorded positions, health and controller effects. No actors were moved
or made visible through memory writes.

The bounded observation follows the nearest living actor. One early near
capture shows a normal full-size bot in the open. Later middle-distance frames
show an opaque stair wall while the tracked living actor is about 10 units
away; no character is visible over that wall. Their read-only records include
zero active-camouflage bits. Distance alone is not a visibility oracle: these
frames establish only that the inspected baseline did not show the particular
body-over-wall failure. The movement driver can stop at walls, and the tested
roster/settings are not an exact reconstruction of the reporter's multiplayer
match. This is a failed reproduction, not proof of absence.

Existing production terminal-depth-clear, renderer-state-cache and guest state
layout tests passed: 12 cases. These check clear propagation and state binding,
including mutation controls, not every game's occlusion pass. The independent
MSAA repair is on `fix/discord-msaa-rendering`; this one-sample run did not test
its multisample path and cannot resolve this separate report by association.

Local evidence: `logs/wall-visibility-route.jsonl`,
`logs/wall-visibility-check.py`, `logs/wall-visibility-observation` (provenance,
read-only world records and paired images), `logs/discord-depth-baseline-tests.log`.
The same local compiler workaround as the other main-baseline probes applies;
this was not a default-flags Release acceptance build.

Next discriminating check: recreate the clip's multiplayer roster, abilities,
powerups, FPS and samples, obtain a frame where the body crosses the brick,
and isolate that character draw against the preceding wall depth in a capture.
Changing culling or disabling overlays globally without that case would hide
symptoms and risk unrelated rendering. No such speculative change was made.
