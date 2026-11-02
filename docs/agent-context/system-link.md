# System Link activation

The original US row callback 1A2700 checks event 8 and tail-calls browser 1A0A60
directly, bypassing the browser's manual lookup. Shared `pc_display_menu_lookup`
now intercepts the row callback before its original body and enables LAN only
on selection. Other events preserve Online; Online's direct browser call retains
its bridge. Test-SystemLink pre-enables networking and cannot prove this path.

`tests/graphics/test_fov_settings.py` runs without TSFP_NETWORK at startup,
selects the actual row callback through the production resolver, and checks LAN
activation before browser entry. Supported local US data additionally supplies
the original six-instruction callback and verifies that removing the row
interception fails. These cases use the existing Windows FOV CI route; Mac
execution and physical two-machine LAN remain distinct checks.

The shared menu emits `[PC-NET] System Link selected; enabling LAN transport`
before entering the browser. Transport startup then reports the chosen IPv4
and compatibility identity, followed by UDP bindings and bounded packet traces.
No network activity in the same-machine script establishes physical LAN discovery.
See [Online UI and host adapters](multiplayer-ui.md) for the separate bridge path.
