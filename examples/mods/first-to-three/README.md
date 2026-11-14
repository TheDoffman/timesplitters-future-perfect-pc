# Experimental First to Three mode

This original Lua source demonstrates the draft API1 mode callbacks and queued
score/match commands. It is a developer example, not a playable released mod.
The launcher still refuses script activation until OS confinement, supervision,
the real game bridge and multiplayer/save contracts are implemented and verified.

Build the native worker with `python tools/build/build-mod-worker.py`. Run
`python -m pytest tests/mods/test_script_worker.py` to exercise this exact example
with synthetic authoritative match events, including the third-kill win condition.
No game files are needed. Package the example with the ordinary creator `pack`
command. See `docs/development/mod-gameplay-scripting.md` for API/budgets and the
remaining integration gates. Python is developer tooling; players will use the
native worker bundled with the game once that integration is complete.
