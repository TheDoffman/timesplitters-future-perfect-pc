# Reusable gameplay test profile

The local **Test Lab** profile unlocks story levels, Arcade content/characters
and challenges. Collectible/pickup progress is preserved from a fresh profile,
so collection can still be tested. Story completion masks and challenge awards
are already filled; use a separate fresh profile when testing those unlocks.

On Windows, launch a disposable copy for up to ten minutes:

```powershell
python tools/testing/testing_profile.py --play --seconds 120
```

This restores the canonical profile into `private/test-saves/quick-test/saves`,
uses isolated controls and both display configurations, and skips startup videos
through the existing launcher switch `RECOMP_XMV_PLAY=0`. Select **Test Lab** in
the game's profile picker. The session ends at the chosen duration or when you
close its window. Logs are capped at 4 MiB each; known disposable partition
images are recycled after exit. Profile changes made during this session are
discarded on the next preparation. Never prepare the same directory while a
game is using it. An interrupted runner can leave `run.lock`; check that its game
has stopped before removing that marker.

The shared unlocked-profile environment connects the checkout's `assets/textures`
pack and starts in Classic mode, matching the normal Windows launch defaults.
Press F10 to toggle enhanced replacements. Inherited texture-pack, mode and dump
overrides are discarded along with other diagnostic settings. Normal builds
hydrate and audit the pack; missing replacements still fall back to Classic.
The native Mac unlocked runner receives the same texture settings.

On Apple Silicon, use the bounded native runner:

```sh
.venv/bin/python tools/testing/test-native-mac.py --unlocked-profile --seconds 120 --capture
```

This verifies the same local fixture and resets a disposable copy under
`private/test-saves/native-mac/unlocked/saves`. Controls, both display configs
and XDG directories stay inside the unlocked test directory; startup movies
are skipped. Select **Test Lab** in the profile picker. The canonical fixture
and the ordinary native test profile remain unchanged. Without
`--unlocked-profile`, the native runner retains its existing test profile and
startup behavior, suitable for fresh-profile/unlock tests. The native runner
locks its test root and the active fixture copy, bounds runtime/logs, and recycles
only known partition images after the child exits, including on exceptions.

A separate Test Lab copy is also installed in this PC's personal save location,
`private/saves/UserData/45410066/544553544C41`, for manual testing through the
normal launcher. Select **Test Lab** in the profile picker. Progress made there
persists like any personal profile; the bounded runner instead resets its own
disposable copy. This local installation does not travel through Git.

For a custom test harness:

```powershell
python tools/testing/testing_profile.py --prepare private/test-saves/my-test
```

This prints only the required runtime environment settings. Apply them to that
test's child process; it does not change your terminal environment. Python
harnesses can import `prepare_run` from `tools/testing/testing_profile.py` to obtain the
complete child environment. Custom harnesses must bound execution/logging and
recycle their known partition images after exit. Use a dedicated directory;
the tool refuses unowned nonempty directories and personal save seeds.

The canonical fixture is ignored local data at `private/test-fixtures/unlocked`.
It is already created on this PC. On another checkout, create one fresh profile
using an isolated save and isolated input/runtime/display configurations, exit
the game, then run:

```powershell
python tools/testing/testing_profile.py --seed private/test-saves/fresh-profile/saves
python tools/testing/testing_profile.py
```

The seed must be a fresh disposable profile, never `private/saves`. The tool
verifies the supported US XBE hash, save version and runtime HMAC signature,
applies the US game's unlock masks/grades, and re-signs the save. All other
profile bytes, including collectible/pickup data, remain unchanged. The fixture
manifest detects accidental edits; creation refuses to replace an existing
fixture. For a seed created with the ordinary Mac runner, use
`--seed private/test-saves/native-mac` after that game has exited. Saves and
original game assets are never committed. MAC-025 and docs/project/STATUS.md record native
validation scope; a prepared fixture alone does not verify unlocked gameplay.
