# Discord Rich Presence

Windows game builds with issue #63 support can show detailed game activity and an
elapsed session timer on your Discord profile. Open **Game settings → Discord**
in the launcher. Enable **Discord Rich Presence**, keep the project's Application
ID `1561627743353962566`, and save. Start the game with the Discord desktop app
running and activity sharing enabled in Discord. Restart the game after changing
this setting. Fresh packaged installs leave presence disabled.

The status follows Story, Arcade Custom, Arcade League, Challenge, LAN, Online,
menus and pause:

- Story shows the current mission title, difficulty, single player/co-op, and
  gameplay, cutscene or pause state.
- Arcade Custom shows the game type, map, human player count and bot count.
  League and Challenge also show the current event title where available.
- Online and LAN show the game type, map, occupied human slots, bots and the
  map's character capacity, with hosting/joined lobby and in-match states.
  Empty slots left by departed players do not count as players. The capacity
  includes bots; `2 players + 4 bots / 16` means six occupied character slots.

For example, a match can show `Online - Deathmatch` with
`Zeppelin | 2 players + 4 bots / 16 | Hosting | In game`. Story can show
`Story - Time to Split` with `Normal | Single player | Watching a cutscene`.
The game reads built-in titles from its loaded language tables. Missing or
unavailable metadata falls back to the mode; menu browsing clears stale match
and mission details. Player-created map names are not exported.

Changed statuses are coalesced and sent at most once every 15 seconds. The
elapsed timer starts with the game's presence worker and survives reconnects
and mode changes. Join invites and Discord party/join buttons are not included.

## Creating your own Application ID

1. Open the [Discord Developer Portal](https://discord.com/developers/applications)
   and sign in.
2. Choose **New Application**, enter the name you want shown (for example,
   **TimeSplitters: Future Perfect PC**), accept the terms, and create it.
3. On **General Information**, copy **Application ID**. It is a public numeric
   identifier. No bot token, client secret, OAuth redirect, or player login is
   needed for this integration.
4. Paste the ID in **Game settings → Discord → Application ID**, enable presence,
   save, and restart the game. Discord uses the application's registered name
   above the activity text.

Discord documents this desktop approach under
[Rich Presence without authentication](https://docs.discord.com/developers/discord-social-sdk/development-guides/setting-rich-presence#rich-presence-without-authentication).

## Source builds and configuration

The runtime reads `pc-discord.ini` beside the file in `RECOMP_INPUT_CONFIG`
(normally `config/pc-controls.json` for a source checkout, and
`Settings/pc-controls.json` for a package). `TSFP_DISCORD_CONFIG` can explicitly
select an isolated file. No file means disabled. For a source checkout, copy
`config/pc-discord.default.ini` to `config/pc-discord.ini` and change the first
number to `1`:

```text
TSFP_DISCORD_V1 1 1561627743353962566
```

Portable packages keep the disabled template and these instructions under
`Support/Discord`; they do not distribute a personal `Settings/pc-discord.ini`.
Use the launcher's Discord settings to create that preference when needed.
For manual configuration, copy `Support/Discord/pc-discord.default.ini` to
`Settings/pc-discord.ini` before enabling it. Updates preserve an existing file.

Use `0` instead of `1` to disable it. The second number is the Application ID;
`0` is accepted only when disabled. Settings are read once at game startup.
The launcher preserves the file under `Settings` across updates and rollback.
Its editor uses the same backup, concurrent-edit detection, and transactional
save behavior as other game settings. A new launcher cannot add this feature
to an older game executable: both launcher and game must be rebuilt/updated.

The Windows adapter uses Discord's local named-pipe RPC transport and bundles
no Discord SDK. It sends the process ID, public Application ID, built-in
mission/map/event and game-type titles, difficulty, population counts,
hosting/pause/menu state, session timestamp and request nonce. It does not send
player names, network addresses, saves, file paths or gameplay logs. Pipe replies are
bounded and never logged verbatim. Closing or restarting Discord does not
prevent gameplay; the worker retries every five seconds and shuts down when
the game window closes. Disconnecting clears that process's presence.

If presence does not appear, check activity sharing in Discord, confirm the ID,
and look for `[PC-DISCORD]` in the game log. The game does not launch or install
Discord for you. Mac adaptation is queued in `docs/project/MAC_PORT_BACKLOG.md`.

## Validation

Run `python -m pytest tests/network/test_discord_presence.py -q` for portable policy,
bounded guest-memory decoding and isolated Windows pipe regressions (including
changed match population). These tests use a private test pipe and never
connect to a real Discord account. `tsfp.ps1 Build` also runs the native policy
target. Launcher settings checks cover validation, persistence, cancellation
and explicit child-process routing.

For a bounded real-client smoke test, with Discord already running, use
`python tests/network/discord_presence_game.py --live-discord`. This explicitly publishes
the test session's presence and uses a disposable Test Lab profile, isolated
controls and both display files, a verified US executable, and a 95-second limit.
It checks the first mission title, difficulty, menus, Story cutscenes, gameplay
and pause. Add `--scenario arcade` or `--scenario league` for the corresponding
match replay. `--scenario lan` hosts a local loopback session and checks the
map, mode and single-player count through lobby and gameplay. Logs are capped at
4 MiB each and disposable virtual disks are recycled. These checks do not cover
every game mode or the appearance of the activity card in another user's client.

## Native Mac

The native launcher uses the same Discord tab and optional configuration. The
runtime shares Windows framing, activity formatting, guest decoding and reconnect
policy; `src/native/discord_posix.h` supplies local Unix sockets and worker I/O.
Native startup/shutdown and the shared camera snapshot hook are enabled by the
`native-launcher-services` patch. Network-specific status still depends on the
native networking adapter (MAC-013); it is not inferred from Windows peers.

Run `python -m pytest tests/network/test_discord_macos.py tests/network/test_discord_presence.py -q`
for isolated protocol/policy checks. The Mac end-to-end wrapper's `--discord`
option explicitly publishes its disposable game session and checks the running
desktop client's acknowledgement; see `docs/players/mac-launcher.md`.
