# Opening regression checks

`tests/native_refresh_game.py` uses the isolated Test Lab copy under
`private/test-saves/high-refresh`. Confirm that its previous run has stopped
before preparing another. Use a distinct evidence label for each comparison;
preserve the canonical fixture and personal data.

The fixed gameplay replay skips the cinematic at45s, pauses at80/83s, and
walks/fires during85..115s. Run its Original reference and native diagnostic
separately, for example:

```
.venv/Scripts/python.exe tests/native_refresh_game.py --fps 60 --label opening-reference --stress --pause-check --seconds 120
.venv/Scripts/python.exe tests/native_refresh_game.py --fps 144 --native --label opening-native --stress --pause-check --seconds 120
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

`--capture` retains three renderer BMPs and adds readback overhead; it is for
visual regression, not clean pacing. Match capture timestamps and camera state
when comparing images. Different reload/idle phases in asynchronous captures
do not establish a timing bug. Selected additional images and private capture
wrappers must record their timing/provenance and stay bounded/ignored.

Measured results and unverified cinematic/gameplay scope belong in STATUS.md.
