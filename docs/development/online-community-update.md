# Updating the community directory for visible Invite Only lobbies

The native client and bundled **player-hosted private directory** support this
flow. The installed community directory/relay/coordinator needs the matching
service changes below. Its source and installer are private, outside this game
checkout. Pulling this branch or copying the bundled private-directory module
will not update that service. No community update has been deployed by this task.

## Required service changes

Apply these changes in the private service checkout to both the v2 Story/legacy
handlers and v3 Arcade handlers, preserving relay credentials, migration,
checkpoints, late joins, version checks, membership limits and rate limits.

1. Interpret CREATE `public: false` as **listed, Invite Only**, replacing the old
   unlisted behavior. Keep `public: true` as Open. List both kinds when compatible;
   capacity/phase determines `joinable` independently of privacy.
2. Keep a public 16-character hexadecimal `code` as the stable lobby identifier.
   Generate a different, unpredictable 16-character hexadecimal `lobby_code` for
   locked-room admission. Prevent collisions with other room IDs/codes. Return
   `lobby_code` to the creator in the authenticated CREATE response. Do not put it
   in public lists, peer rosters, dashboard output or broadcast session state.
   Open rooms can keep their existing code/identifier behavior.
3. Add `invite_only: true/false` to each public listing. Keep existing name, mode,
   capacity, players, phase and map/rules metadata. Do not return owner, member,
   peer or relay credentials in listings.
4. Accept the entered `lobby_code` in POST `/v2/lobbies/{public-id}/join` and
   `/v3/lobbies/{public-id}/join`. For a locked room, validate its format and
   compare against that specific room's code before allocating any member or
   transport credential. Reject missing/wrong/another-room codes with HTTP 403.
   Use a constant-time comparison after validating ASCII hexadecimal input.
   Preserve compatibility, mode, migration-schema, full-room and active-match
   checks. An authentication failure must not fall through to another room.
5. Retain code-only invitations scoped to the selected directory by resolving a
   secret code supplied as the join path. Successful admission must return the
   canonical public `code`, so subsequent heartbeat/leave/migration RPCs use the
   stable room identifier. The client keeps the entered admission code locally
   for its lobby footer and invitation; admission/heartbeat replies need not
   publish it. Expiring/closing a room removes both identifiers.
6. Only after both handler families pass these checks, advertise
   `"invite_only_listing": true` in GET `/v2/lobbies` responses for both Arcade
   and Story queries. The native client checks this endpoint even before creating
   a v3 Arcade room. Adding the flag without implementing admission would falsely
   advertise support. Old services without the flag are refused for locked
   creation instead of silently creating an unlisted room.

The client-side request is, for example:

```http
POST /v3/lobbies/0123456789ABCDEF/join
Content-Type: application/json

{"compatibility":"<client build identity>","migration_schema":"tsfp-arcade-checkpoint-v1","lobby_code":"<entered 16-digit hex code>"}
```

The v2 body also carries `mode: "arcade"` or `mode: "story"`. Keep Arcade's
16-player and Story's 4-player limits independent of directory connection limits.
Existing [private-directory tests](../../tests/network/test_online_private.py)
and [client request tests](../../tests/network/test_online_menu.py) demonstrate
listing, wrong-code rejection and the wire contract; they do not validate the
external relay/coordinator implementation.

## Install on the Mac mini

1. Choose a maintenance window with no active matches. Retain the existing
   installed service/configuration, certificate/private key, hashes and launch
   agent as the rollback copy. The documented installation is
   `~/Library/Application Support/TimeSplitters Online`; confirm the actual root
   from the running launch agent before changing it.
2. Run the **private service checkout's** updater/installer with its existing
   root, public/LAN addresses and ports: HTTPS TCP 41820 and relay UDP 41821 for
   the documented deployment. Replace only server code and its source-hash
   manifest through that updater. Preserve its certificate/key, relay settings,
   dashboard access and router mappings. The updater is not included in this
   client repository, so an exact updater command must come from that checkout.
   Record the actual TCP/UDP listeners before and after the update. For this
   community deployment they must remain TCP41820 and UDP41821; some older
   installer commands default to30820/30821. Pass the private updater's explicit
   directory/relay port options (historical installer flags: `--directory-port
   41820 --relay-port 41821`) or preserve the equivalent existing configuration.
   Confirm those options in that updater's help rather than running a generic
   installation command with defaults. The game's player-hosted private-server
   defaults (TCP30820/UDP30821) are a separate setting.

   Read-only checks on the service Mac:

   ```sh
   lsof -nP -iTCP:30820 -iTCP:41820 -sTCP:LISTEN
   lsof -nP -iUDP:30821 -iUDP:41821
   ```

   If the service moved to30820/30821, restore its deployed41820/41821 settings
   while retaining the certificate and existing router forwards. If it still
   listens correctly, check that the router forwards TCP41820 and UDP41821 to
   the Mac's current reserved LAN IP. A successful connection on the host's
   network establishes local availability; outside-network access needs its
   own verification.

3. Restart and inspect the existing launch agent from the service user's Mac
   login session:

   ```sh
   launchctl kickstart -k "gui/$(id -u)/com.timesplitters.online"
   launchctl print "gui/$(id -u)/com.timesplitters.online"
   ```

   Restarting ends remaining service connections. Do not create a second service,
   replace its certificate, or change its ports merely to install this update.
4. From an updated game checkout **on another network**, verify the pinned
   public HTTPS route. This read-only probe uses the shipped public endpoint /
   fingerprint and an empty compatibility identity; it does not print lobby
   credentials. Explicitly bypass saved device-local routes so a working LAN
   connection cannot be mistaken for working Internet access:

   ```sh
   PYTHONPATH=tools/network .venv/bin/python - <<'PY'
   import json
   from pathlib import Path
   from online_client import Client
   settings = json.loads(Path('tools/network/online-service.json').read_text(encoding='utf-8'))
   client = Client(settings['directory'], settings['fingerprint'])
   client.local_address = None  # Force the canonical public host; retain its TLS pin.
   for mode in ('arcade', 'story'):
       reply = client.request('GET', '/v2/lobbies?compatibility='+'0'*64+'&mode='+mode)
       assert reply.get('invite_only_listing') is True, mode+' admission update missing'
       print(mode+': visible Invite Only capability available')
   client.request('GET', '/v3/lobbies?compatibility='+'0'*64)
   print('v3 Arcade directory reachable')
   PY
   ```

   A capability probe establishes reachability/advertisement, not admission
   correctness. In the service's own tests, exercise Open and locked v2/v3 rooms,
   invalid/missing/other-room codes without peer allocation, full/expired rooms,
   metadata refresh, authenticated UDP, Arcade migration and late joining.
5. Rebuild/install matching updated game and companion builds on two clients.
   Host an Invite Only Arcade room on the community server: the second client
   must see its lock, fail with a wrong code, join with the correct one and return
   to the same browser on leave. Verify Story with an enabled compatible build,
   Open rooms, Refresh and host migration as separate checks. Test an actual
   outside-network route before claiming Internet acceptance. Keep saves and
   personal controls/display settings intact.

If the service fails its admission/transport checks, restore the retained server
code and matching hash manifest and restart the same launch agent. Keep the
certificate, settings and ports unchanged. With the old service the updated
client will explain that visible Invite Only hosting needs a server update.
