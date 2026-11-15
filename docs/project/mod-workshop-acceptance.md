# Full mod-system completion evidence

The active goal covers a usable public creator-to-game system, not just the
prototype or texture support. This table tracks that full scope. Tests prove
their named components; none substitute for actual host/game/public acceptance.

| Requirement | Current authoritative implementation/evidence | Still required |
| --- | --- | --- |
| Creator packaging and SDK | `pack/validate`, bounded package/keyed DDS validation, `pack --modules` links32 project Lua files into existing bounded source; original modular first-to-three pack executes correctly in the actual Windows VM with existing budgets | New modular SDK all-host/game/public acceptance; stable asset/gameplay API, maps/audio importers, richer asset starter projects and complete creator documentation |
| Real asset additions | Shared DDS worker; bounded reader tests; Windows build/host checks and actual native model install/update/rollback/removal-to-profile-scene rendering with original synthetic DDS pass | Story/gameplay scene tests and Mac/Linux live game builds/scenes; audio, map and other content loading/import workflows |
| Sandboxed gameplay scripting and custom modes | Pinned Lua5.5.1, native Windows LPAC/Linux seccomp/Mac Seatbelt workers and verified lifetime/resource supervision; shared guest adapters, score/end/message transactions, memory/time/host-call budgets; packaged First to Three finishes real offline matches through launcher-generated handoffs on all3 hosts, with original saves preserved; see STATUS | Stable published SDK/API, richer entity/content support, remaining crash recovery and public creator-to-native-game acceptance |
| Launcher browsing/search | Shared Mods page with category/creator filters, numeric version sorting, bounded pagination, scan information and verified HTTPS download; native three-platform CI checks pass at10197bc2 | Production endpoint, richer release/permission/report information and independent public acceptance |
| Native installation and lifecycle | `ModLibraryPage.cs`: immutable versions, strict JSON/ZIP/DDS validation, dependency install and activation reviews, same-origin numeric update discovery, enable/disable, retained-version rollback, uninstall and unknown-file preservation; actual Windows and Mac checks pass at42567c4f, including dispatcher-driven activation | Latest Linux CI, complete all content types, additional cross-filesystem acceptance, interrupted-work/recovery UX, storage and profile management at production scale |
| Normal/recovery launch | Shared launcher revalidates package/trust/source capsule and build capability before modded launch; Play without mods bypasses damaged profiles; real packaged-mode match and original-save preservation pass on Windows/Mac/Linux | Public install-to-play and update/rollback/removal preservation flow, interrupted/abrupt-crash recovery and final release compatibility checks |
| Multiplayer and saves | Native capsule bootstrap isolates mode saves under bound package/profile identities and forces scripted modes offline; three-host launcher handoffs preserve ordinary saves | Ordered multiplayer mod/hash negotiation, authority/mismatch behavior and actual multiplayer acceptance; broader save/profile recovery |
| Public Mac mini hosting without a purchased domain | Separate .nosync task worktree; actual Caddy loopback/private-path/expiry checks; pinned private scanner/watchdog and initial official definition-update login-agent check pass | Dedicated production identity, unattended boot, persistent public API/publication refresh, working HTTPS/tunnel identity and independent internet acceptance |
| Creator accounts/ownership/uploads | Private WSGI API with invitation accounts, scrypt, revocable sessions/recovery, transactional ownership and quota-reserved uploads; native Creator tab sign-in/review/upload/history; real native-client/backend wire checks pass on3 hosts and native creator-to-isolated-ClamAV clean/detection/error cases pass on Mac at4a933431 | OS credential vaults, public TLS deployment, full public creator-to-game flow and abuse/load acceptance |
| Virus/malicious-content handling | Bounded ZIP/JSON validation; v3 Seatbelt process-info denial, signed lifetime guardian and sampled resource supervision; native creator-to-real-Mac-scanner clean/EICAR/empty-defs/missing-engine cases preserve fail-closed review/rejection/retry/error states; prototype updater has completed3 actual6-hour cycles | Production service/update/uptime acceptance under the locked identity, malicious-script/content analysis and public abuse/capacity acceptance |
| Moderation and reporting | Authenticated moderator/admin API with transactional approval/revocation/audit; native scan/Lua inspection, private player reports and moderator open/history/inspect/confirmed close-reopen screens pass all3 launcher/package CI jobs at43011166; new bounded DDS preview passes Windows API and all3 native launcher/package CI checks at3cc247f2; real native creator-to-ClamAV review/revoke passes Mac at30fa6091 | Real user-host report/restore/DDS-preview acceptance, remaining audio/map asset previews, public moderator/abuse/load acceptance, notification and operational staffing/policy |
| Catalog and update trust | Producer RSA3072 signed metadata, offline root thresholds/rotation, bounded permanent revocations and SQLite publication counter; shared native signature/root verification, locked atomic signed-state persistence, replay/expiry/revocation checks and origin receipts; live signed export refresh withdraws changed downloads, with actual private Caddy revocation/withholding/controller-death acceptance | Independent public-client/internet acceptance, release-pinned production root, backup/high-water recovery, persistent production publication service and distributed internet revocation drill; see mod-catalog-trust.md |
| Storage and operations | Separate quarantine/approved/installed/active stores; package/upload budgets; private bounded SQLite/blob snapshots, new-root auth-revoking recovery barrier and explicit revocation/high-water reconciliation; actual Mac backup/restore/resume fixture drill passes | Off-device/key recovery, scheduled backup retention, production cutover/recovery, capacity above current limits, identity isolation and incident drills |
| Player distribution | Uses existing native launcher runtime, with no player Python requirement | Complete three-host packaging/signature/Gatekeeper checks, authorized source synchronization/review, actual public release and independent install-to-play verification |

The table above reflects current component evidence; the following foundation
notes are historical and do not override newer STATUS records.

Windows foundation evidence as of 2026-11-14: a full launcher build/test command passed;
12 focused workshop groups cover native library/client behavior; DDS/parser/
Classic/streaming tests and adversarial Python workshop cases pass. Logs and
exact counts are in [STATUS](STATUS.md) and ignored task logs. Subsequent focused
checks pass; the last full rerun hit an unrelated running-game guard. Native Mac
build/sign/package/tests pass with 60 UI groups; 186 Mac Python/C checks pass.
Live Caddy loopback and expiry shutdown pass. At ff580de3, WSL Ubuntu24.04
passes all77 workshop Python/C checks and the native Linux launcher build/test/
package command:61 shared UI groups plus package install, corruption rejection,
repair and restart/render checks. This is configured-host launcher evidence;
Linux gameplay and public internet acceptance remain unrun.

The primary Mac checkout contains unrelated edits and cloud-conflict copies;
work continues in a separate .nosync task worktree. Preserve the primary checkout
and transfer source through Git. Tailscale is installed but working sign-in/public
identity remains unverified; see mac-mod-hosting.md. ClamAV
is now installed with official databases; live checks are recorded below. These are
operational prerequisites, not evidence of a deployed host.

At59686d4e, real isolated ClamAV1.5.4 with official daily28183 on the Mac passes
clean-to-review-to-approval, EICAR rejection, empty database/missing engine errors
and revocation. Separate disposable canaries prove permitted content/temp access
and denied private reads/writes, symlink escape, child execution and loopback
network access, with inherited CPU/fd/core limits.86 mod checks pass on Windows
and Mac. The scanner's memory/concurrency supervision and persistent operation
remain incomplete. No public service or upload endpoint is exposed.

Windows game generation, compilation and required build checks now pass with
the verified US XBE and pinned runtime. The executable SHA256 is
`d85dde23beaa3b923eb635bcd2f513c74df169d5a1a4f00de212009239782f29`.
The FFmpeg dependency now uses a verified dated8.1 archive, preserving its hash
check. MSVC /MP required an explicit CL_MPCount=2 to avoid heap/pagefile pressure;
MSBuild's project parallelism alone did not bound compiler processes. Build
checks do not establish visual/gameplay acceptance.

Windows live profile-scene evidence: original solid-color BC1 packages were
packed, installed, enabled and version-switched through the compiled native
ModLibrary model. Version1.1.0 key91c931f8b951fbbb is recorded loaded by the actual
RHI worker; renderer-owned BMPs show the blue background replaced with purple
(201124/307200 purple pixels). Rolling back to retained1.0.0 restores blue
(0 purple pixels). Disable/uninstall of both versions emits no mount and the
following game run loads no replacements and retains blue. An unrelated
sentinel and canonical Test Lab fixture remain intact; all reviewed runs exit0.
Game frame captures, not occluded desktop screenshots, support this evidence.
This proves native model-to-game profile rendering; public download, launcher UI
flow, Story gameplay, other asset types and other game hosts remain incomplete.

Do not close the goal while any requirement above remains incomplete or lacks
the corresponding real-world acceptance evidence. Do not merge main without
the owner's explicit instruction.
