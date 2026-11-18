# Full mod-system completion evidence

The active goal covers a usable public creator-to-game system, not just the
prototype or texture support. This table tracks that full scope. Tests prove
their named components; none substitute for actual host/game/public acceptance.

| Requirement | Current authoritative implementation/evidence (exact host scope in STATUS) | Still required |
| --- | --- | --- |
| Creator packaging and SDK | `new` creates gameplay/texture/mixed/audio projects; `import-texture` converts authored PNG or copies compatible DDS, `import-audio` preflights/copies PCM against the independent native contract. Creator-only CLI and explicit-inventory source SDK pass316 checks per host; all3 independent exports match107219 bytes/SHA1c552851...5538d1 at9c415226. Existing creator/package/native-reader/VM checks pass all3; `pack --modules` links32 local Lua files; at2196da14 exported SDK new/import-audio/pack/validate completes native launcher-installed six-bot game acceptance on all3, with independent SDK exports matching SHA658d92e2...dcf8f78c (STATUS) | new creator-tool public acceptance; stable published asset/gameplay API, map importers/richer asset starters and SDK publication |
| Real asset additions | Shared DDS worker and bounded readers; native profile-scene install/update/rollback/removal rendering passes. Original PCM audio and Lua audio commands run through production native workers and launcher-installed six-bot matches on Windows/Mac/Linux (fdab5e79/dc478459; STATUS). Creator SDK imports validate DDS/PCM against independent native contracts | Further story/content scene acceptance, map and richer content loading/import workflows, exact final SDK-to-public-game acceptance |
| Sandboxed gameplay scripting and custom modes | Pinned Lua5.5.1, native Windows LPAC/Linux seccomp/Mac Seatbelt workers and verified lifetime/resource supervision; shared guest adapters, score/end/message transactions, memory/time/host-call budgets; packaged First to Three finishes real offline matches through launcher-generated handoffs on all3 hosts, with original saves preserved; see STATUS | Stable published SDK/API, richer entity/content support, remaining crash recovery and public creator-to-native-game acceptance |
| Launcher browsing/search | Shared Mods search/category/creator filters and version selection. At e8945093, fresh native Windows/Mac/Linux clients each read production signedv31 twice with default TLS and compiled origin/root, without local service/root files; at2196da14 fresh native clients read signedv40 twice on all3 hosts and launcher checks pass114/115/116 UI groups (STATUS) | Richer permission/report information, all supported content categories and complete final public cross-host player acceptance |
| Native installation and lifecycle | Shared immutable versions, dependency/activation reviews and preservation; native260d UI/backend checks pass111 Windows/112 Mac/113 Linux groups. Actual genuine Mac/Linux game packages install1408/1557 files with corruption/personal-preservation checks; public initial/update/rollback/removal now completes on all3 hosts | Complete all content types, further interrupted/cross-filesystem/storage/profile acceptance and production-scale operations |
| Normal/recovery launch | Shared trust/capsule/build revalidation and Play without mods. Genuine installed Mac/Linux packages now join Windows public3 ->4 ->3 matches; old namespace survives update and is reused on rollback, ordinary saves unchanged. Signed withdrawal blocks active launch and permits original recovery/removal on all3 hosts | One captured Mac pre-match signal11 remains undetermined despite a successful retry; resolve stability, abrupt-crash recovery and final exact-source release compatibility |
| Multiplayer and saves | Native capsule bootstrap isolates mode saves under bound package/profile identities and forces scripted modes offline; three-host launcher handoffs preserve ordinary saves | Ordered multiplayer mod/hash negotiation, authority/mismatch behavior and actual multiplayer acceptance; broader save/profile recovery |
| Public Mac mini hosting without a purchased domain | Actual locked UID/GID600 services at immutable601/7be, nine persistent jobs, private readiness and seven restart checks pass. Public HTTPS Funnel8443, independent normal TLS/signatures and native Windows/Mac/Linux public reads pass; private443 preserved. See STATUS for source, seal and endpoint | Physical unattended boot, continued uptime/capacity/incident and cross-content/stability acceptance, final release |
| Creator accounts/ownership/uploads | Authenticated ownership, quota uploads and revocable sessions; actual owner activation/Windows Credential Manager resume. Native client/backend checks pass all3 hosts. Production uploads pass fresh confined AV/exact source review and signed approval before public installed-game play on all3 hosts | Mac/Linux OS-vault persistence, public creator-role/account UX acceptance on those hosts, abuse/load tests and final admin workflow |
| Virus/malicious-content handling | Bounded archive/content validation and sandboxed script budgets; production locked-account ClamAV controls, exact macos-seatbelt-v3 fresh scan evidence and public native upload-to-review checks pass. Missing/stale/error scanner paths fail closed; isolated workers and signed lifetime supervision remain enforced | Long-running update/uptime and public abuse/capacity acceptance, broader malicious-script/content analysis and all new asset import/preview validation |
| Moderation and reporting | Authenticated moderator/admin API with transactional approval/revocation/audit; native scan/Lua inspection, private player reports and moderator open/history/inspect/confirmed close-reopen screens pass all3 launcher/package CI jobs at43011166; new bounded DDS preview passes Windows API and all3 native launcher/package CI checks at3cc247f2; real native creator-to-ClamAV review/revoke/DDS/player-report flow and API/restore70 checks pass user Mac at07f7f0a7. Atf1d6c699 private PCM waveform/API checks pass all3; aaa310ea/3a555fb4 add explicit bounded listening with actual silent Windows/Mac/Linux output-service acceptance (STATUS) | Locked-service/public report/restore/DDS/audio-preview acceptance, physical listening and stop-latency, map previews, public moderator/abuse/load acceptance, notification and operational staffing/policy |
| Catalog and update trust | Production origin/root compiled and bound by exact HTTPS origin; custom origins require explicit trust. Native signature/expiry/replay/revocation checks and actual public player lifecycles pass all3 hosts. Verified signedv31 retains all five test revocations and withdrawn packages return404. Off-device old snapshot reconciles post-backup revoked hashes/high-water11 | Ongoing rotation/expiry/incident operations, published clients containing the verified defaults and live production recovery/cutover drills |
| Storage and operations | Separated bounded stores and snapshots; actual UID600 production capture/clone drill, private Windows off-device copy/return and conservative unserved restore pass. Account/ownership/audits preserved; sessions revoked/accounts suspended. User confirmed portable encrypted production signing-key recovery check exit0 on Mac. Managed pool/worker/registrar source passes actual Mac/Linux checks (STATUS). Encrypted export/real SSH/portable fixture restore and journaled receiver retention pass Windows/Linux checks, including actual child-exit recovery (STATUS). Fresh managed export selection/read-only sender and pointer recovery pass Linux source checks; Mac local writes await disk capacity and protected setup awaits local authentication | Actual protected scheduled-job/capture acceptance, off-device automation, production cutover/recovery, physical boot, capacity/identity/incident acceptance and routine operator documentation |
| Player distribution | Native self-contained launcher packages, no player Python. Actual game install gates pass on Mac/Linux after narrow shared helper-companion allowlist fix; persistent genuine installed packages complete public mod lifecycle. Local candidates only, with source/packaging pins in STATUS | Final same-source three-host release, signatures/Gatekeeper/notarization, Mac crash follow-up, clean-machine/physical acceptance and authorized publication |

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
