# Opening regression checks

`tests/gameplay/native_refresh_game.py` uses the isolated Test Lab copy under
`private/test-saves/high-refresh`. Confirm that its previous run has stopped
before preparing another. Use a distinct evidence label for each comparison;
preserve the canonical fixture and personal data.

The fixed gameplay replay skips the cinematic at45s, pauses at80/83s, and
walks/fires during85..115s. Run its Original reference and native diagnostic
separately, for example:

```
.venv/Scripts/python.exe tests/gameplay/native_refresh_game.py --fps 60 --label opening-reference --stress --pause-check --seconds 120
.venv/Scripts/python.exe tests/gameplay/native_refresh_game.py --fps 144 --native --label opening-native --stress --pause-check --seconds 120
```

144 is a legacy diagnostic target, not a player-menu choice. The public choices
remain60/120/180/240. Check the generated validation JSON and actual gameplay
samples: movement, changing/advancing hand clips, weapon14, ammunition use and
pause/resume must pass. Compare ammunition including reserve and both magazines
per type, so a reload is not counted as firing. For paused animation acceptance,
inspect multiple interior pause samples rather than just a pause flag.

For a natural cinematic, omit both `--stress` and `--pause-check` and use
`--no-intro-skip`. Those fixed input windows assume the45s skip; combining them
can skip a still-playing cinematic at80s, then leave newly loaded gameplay
paused at83s. This is an invalid test sequence, not evidence of a game stall.

A natural-intro startup validation can pass without proving the handoff:
`opening_firearm_check` emits no result until at least60s of Story-player
observations exist. Use a sufficiently long bounded run (for example300s),
require `opening_firearm_equipped_at_end` to be present/true for handoff
acceptance, and inspect the actual scene/animation evidence. Reaching Story
alone does not establish completion.

The same-build300s Original60/native144 natural comparison provides over141s
of player observations per run, matching advancing hand clips and an explicit
successful weapon14-at-end result. First observed weapon14 occurs about24.5s
after the first player observation in each. This resolves the earlier180s
run's missing handoff coverage, not exact phase or cinematic visual parity.

`--capture` retains three renderer BMPs and adds readback overhead; it is for
visual regression, not clean pacing. Match capture timestamps and camera state
when comparing images. Different reload/idle phases in asynchronous captures
do not establish a timing bug. Selected additional images and private capture
wrappers must record their timing/provenance and stay bounded/ignored.

Measured results and unverified cinematic/gameplay scope belong in docs/project/STATUS.md.

For below-target behavior, retain actual position/aim/ammo/pause observations
alongside the independent timing-call/Present bounds. The private delayed-main
Venice comparison holds only the owned game's main thread for30ms between100ms
waits, resumes it before helper shutdown and leaves other workers unrestricted.
Its117.4FPS result is artificial stress, not normal throughput or latency.
Whole-process single-core affinity starves interrupt workers and is unsuitable
for a moderate slowdown reference. Count incomplete asynchronous player reads;
do not turn missing observations or a shared blocked segment into a game bug.
