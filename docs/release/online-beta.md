# Online multiplayer beta

This beta brings online hosting and joining into the game's own menus. It is
published in the launcher's **Preview** update channel under **Beta**.

## What to try

- Browse the community server, create a lobby or join an existing match without
  opening the separate Multiplayer window.
- Host a private server and share its server invite. Everyone connected can
  create independent lobbies through its shared relay; only the server owner
  needs reachable hosting ports.
- Automatic UPnP attempts to open TCP 30820 and UDP 30821 for a private server.
  The hosting screen warns when automatic mapping is unavailable and identifies
  the ports to check. Existing manual forwards are preserved.
- Use the original-style match browser, visible Invite Only lobbies and lobby
  codes. Lobby names default to the local profile name and remain editable.
- Leave a lobby to browse other matches. Returning to the Multiplayer selector
  closes an owned private server and removes its owned UPnP mappings.

The beta includes the current Stable 0.6.2 controller fixes, native lobby/menu
updates, player-name reporting and bot weapon cleanup during host migration.

## Testing and limits

This first package is **Windows x64**. Mac/Linux packages are not included in
this publication; existing source-build support is separate from packaged
platform acceptance. Story co-op remains restricted to dedicated Story testing
builds; this general Beta does not enable it.

All players in a match must use matching beta builds. Test both community and
private Arcade lobbies, Invite Only admission, Return to Lobby and leaving the
private server. A useful Internet test is a Windows private-server host with a
guest on another network.

UPnP support depends on the router and does not bypass carrier-grade NAT, an
upstream router or a blocking firewall. Mapping success does not by itself prove
Internet reachability. The private server carries relayed match traffic and must
remain running for its lobbies; returning its owner to Multiplayer disconnects
those users. Host migration coverage remains limited to tested Arcade paths.

The launcher preserves installed game data, saves, settings and optional texture
packs. This is an opt-in online beta; the Stable game and launcher Latest release
are unchanged.
