# Full mod-system completion evidence

The active goal covers a usable public creator-to-game system, not just the
prototype or texture support. This table tracks that full scope. Tests prove
their named components; none substitute for actual host/game/public acceptance.

| Requirement | Current authoritative implementation/evidence | Still required |
| --- | --- | --- |
| Creator packaging and SDK | `pack/validate`, bounded package/keyed DDS validation, `pack --modules` links32 project Lua files into existing bounded source; original modular first-to-three pack executes correctly in the actual Windows VM with existing budgets | New modular SDK all-host/game/public acceptance; stable asset/gameplay API, maps/audio importers, richer asset starter projects and complete creator documentation |
| Real asset additions | Shared DDS worker; bounded reader tests; Windows build/host checks and actual native model install/update/rollback/removal-to-profile-scene rendering with original synthetic DDS pass | Story/gameplay scene tests and Mac/Linux live game builds/scenes; audio, map and other content loading/import workflows |
| Sandboxed gameplay scripting and custom modes | Pinned Lua5.5.1, native Windows LPAC/Linux seccomp/Mac Seatbelt workers and verified lifetime/resource supervision; shared guest adapters, score/end/message transactions, memory/time/host-call budgets; packaged First to Three finishes real offline matches through launcher-generated handoffs on all3 hosts, with original saves preserved; see STATUS | Stable published SDK/API, richer entity/content support, remaining crash recovery and public creator-to-native-game acceptance |
| Launcher browsing/search | Shared Mods search/category/creator filters and version selection; actual Windows signed public discovery passes. Fresh bbed native clients verify public signed catalogs v12 Linux/v13 Mac with all three test revocations and persisted trust; native UI/backend packages pass112/111 groups respectively | Richer permission/report information, all content categories and complete public cross-host player acceptance |
| Native installation and lifecycle | `ModLibraryPage.cs`: immutable versions, strict JSON/ZIP/DDS validation, dependency install and activation reviews, same-origin numeric update discovery, enable/disable, retained-version rollback, uninstall and unknown-file preservation; actual Windows and Mac checks pass at42567c4f, including dispatcher-driven activation | Latest Linux CI, complete all content types, additional cross-filesystem acceptance, interrupted-work/recovery UX, storage and profile management at production scale |
| Normal/recovery launch | Shared trust/capsule/build revalidation and Play without mods; packaged private matches pass all3 hosts. Windows real public install/play/update/rollback matches finish3 ->4 ->3 with normal saves preserved; withdrawal blocks launch and native removal succeeds | Mac/Linux public install-to-play and update/rollback/removal preservation, interrupted/abrupt-crash recovery and final release compatibility |
| Multiplayer and saves | Native capsule bootstrap isolates mode saves under bound package/profile identities and forces scripted modes offline; three-host launcher handoffs preserve ordinary saves | Ordered multiplayer mod/hash negotiation, authority/mismatch behavior and actual multiplayer acceptance; broader save/profile recovery |
| Public Mac mini hosting without a purchased domain | Actual locked UID/GID600 services at immutable601/7be, nine persistent jobs, private readiness and seven restart checks pass. Public HTTPS Funnel8443, independent normal TLS/signatures and native Windows/Mac/Linux public reads pass; private443 preserved. See STATUS for source, seal and endpoint | Physical unattended boot, continued uptime/capacity/incident acceptance and complete cross-host public gameplay/release |
| Creator accounts/ownership/uploads | Authenticated API/native Creator ownership, quota uploads and revocable sessions; actual Windows owner/admin account activation and Credential Manager resume pass. Real native client/backend checks pass all3 hosts; Windows public uploads receive fresh confined scans and exact source review before signed approval | Mac/Linux OS-vault persistence, complete public creator-to-game flow on those hosts, abuse/load acceptance and final account/admin UX |
| Virus/malicious-content handling | Bounded archive/content validation and sandboxed script budgets; production locked-account ClamAV controls, exact macos-seatbelt-v3 fresh scan evidence and public native upload-to-review checks pass. Missing/stale/error scanner paths fail closed; isolated workers and signed lifetime supervision remain enforced | Long-running update/uptime and public abuse/capacity acceptance, broader malicious-script/content analysis and all new asset import/preview validation |
| Moderation and reporting | Authenticated moderator/admin API with transactional approval/revocation/audit; native scan/Lua inspection, private player reports and moderator open/history/inspect/confirmed close-reopen screens pass all3 launcher/package CI jobs at43011166; new bounded DDS preview passes Windows API and all3 native launcher/package CI checks at3cc247f2; real native creator-to-ClamAV review/revoke/DDS/player-report flow and API/restore70 checks pass user Mac at07f7f0a7 | Locked-service/public report/restore/DDS-preview acceptance, remaining audio/map asset previews, public moderator/abuse/load acceptance, notification and operational staffing/policy |
| Catalog and update trust | Production root/origin pinned in source; shared native signature/expiry/rollback/revocation checks. Actual public Windows lifecycle withdrawal and package404 pass; actual Mac/Linux clients reload signed state and retain all test revocations. Old off-device production snapshot reconciles later unknown revoked hashes and signed high-water11 | Complete public cross-host lifecycle, ongoing rotation/expiry/incident operations, full release-pinned default activation and live recovery/cutover drills; see mod-catalog-trust.md |
| Storage and operations | Separated bounded stores and snapshots; actual UID600 production capture/clone drill, private Windows off-device copy/return and conservative unserved restore pass. Account/ownership/audits preserved; sessions revoked/accounts suspended. Portable encrypted signing authority proves both root keys on Mac independently of Windows; no decrypted keys written | Scheduled bounded retention/off-device automation, production cutover/recovery, physical boot, capacity/identity/incident acceptance and routine operator documentation |
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
