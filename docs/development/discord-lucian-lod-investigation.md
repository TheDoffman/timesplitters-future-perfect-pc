# Neophyte Lucian LOD investigation

Discord post1567434611212951612 reports pale cream/gold clothing at a distance
and correct clothing nearby. The author confirms0.4.2,0.5.0-story-coop-beta.1
and0.5.0. All replies and the thirteen-second clip were reviewed.

Branch `fix/discord-lucian-lod` starts at main831c324a. This branch records an
investigation; it does not claim a fix or change rendering/game assets.

A fresh supported Windows baseline and native checks passed; executableSHA256
`550f988c6ec45e9504d74bcecd9486874928b5272712683b7e9e8b9152ac5936`.
As with the other Discord checks, MSVC needed the local ignored seven-generated-
unit Od/Ob0 workaround after its pre-existing default-O2 heap failure. This is
not a default-flags Release acceptance result.

The isolated unlocked Test Lab profile selected Chinese through ordinary menu
input. A custom bot set contained only Neophyte Lucian: actual roster character
108, preview-list index68. Powerups and Character Abilities were disabled,
skill0 selected, and all six Custom5 weapon slots were Unarmed. No player or
actor state was written. Normal movement/look/respawn input and read-only owned
process observations supplied the pictures and source identity.

The corrected observer maps the actual actor-pool entity field at offset0;
offset14 is a context field. A human's actor+18 value2 must not be treated as a
death flag. Earlier rejected observer runs are not evidence of a game defect.
The final run samples corpse/death flags and body health, and records NPC
camouflage flags rather than assuming a nearby actor is visible.

Actual close views show the cap, face and gray clothing. The recorded NPC
camouflage flags were zero. Far-distance frames mostly show intervening walls;
proximity telemetry alone is not visibility or a verified low-LOD appearance.
The reported pale low-LOD rendering remains unconfirmed and unresolved. No
resolution reply was posted to Discord.

Local ignored evidence is in logs/lucian-unarmed-observation, including the
near04/near08 images, provenance, read-only observations and retained captures.
The reproducible staged menu route is logs/lucian-controlled-route.jsonl.
Further work needs an unobstructed distant view of character108 and comparison
of that draw's model/material/vertex declaration with a close view. The packed
vertex conversion's frozen-reference tests alone cannot establish the original
Xbox rendering semantics of a newly encountered LOD layout.
