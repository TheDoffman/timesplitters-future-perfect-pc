# Full mod-system completion evidence

The active goal covers a usable public creator-to-game system, not just the
prototype or texture support. This table tracks that full scope. Tests prove
their named components; none substitute for actual host/game/public acceptance.

| Requirement | Current authoritative implementation/evidence | Still required |
| --- | --- | --- |
| Creator packaging and SDK | `tools/mods/mod-workshop.py pack/validate`, original example, bounded package validator and keyed DDS contract | Stable documented asset/gameplay API, importers, richer original starter projects and complete creator documentation |
| Real asset additions | Shared DDS worker; bounded reader tests; Windows build/host checks and actual native model install/update/rollback/removal-to-profile-scene rendering with original synthetic DDS pass | Story/gameplay scene tests and Mac/Linux live game builds/scenes; audio, map and other content loading/import workflows |
| Sandboxed gameplay scripting and custom modes | Lua source can be packaged/installed; activation is refused | Pinned engine, restricted game API, OS isolation, enforced instruction/memory/event/entity/host-call quotas, crash containment and hostile-script tests |
| Launcher browsing/search | Shared Mods page, scan/creator information, bounded HTTPS catalog and verified download; Windows, native Mac and WSL Linux shared UI checks pass | Categories/filters/pagination, production endpoint and richer release/permission/report information |
| Native installation and lifecycle | `ModLibraryPage.cs`: immutable versions, strict JSON/ZIP/DDS validation, dependency/cycle/conflict guards, enable/disable, version switch/rollback, uninstall and unknown-file preservation; Windows/Mac/WSL Linux contracts pass | Complete all content types, additional cross-filesystem acceptance, interrupted-work/recovery UX, storage and profile management at production scale |
| Normal/recovery launch | Shared `GameSettings.StartInfo` configures verified texture overlay; Play without mods bypasses corrupt profiles without rewriting them | Actual game sessions, activation compatibility negotiation and end-to-end preservation across update/rollback/removal |
| Multiplayer and saves | Cosmetic texture path leaves guest data unchanged; originals/settings/saves are not modified by library code | Server authority, ordered gameplay-mod/hash negotiation, mismatch behavior, modded save namespaces and real multiplayer acceptance |
| Public Mac mini hosting without a purchased domain | Separate Git worktree at f6770b2b; native Mac tests pass; actual Caddy loopback catalog/write/private-path/expiry/shutdown checks pass | Persistent service setup, working HTTPS/tunnel identity, external connectivity and availability/reboot acceptance |
| Creator accounts/ownership/uploads | Private WSGI API with invitation accounts, scrypt, revocable sessions/recovery, transactional ownership, exact digest/size upload reservations, storage/pending/daily quotas and local ownership transfer; Windows HTTP/model tests pass | Actual Mac server/scan integration, native creator UI and credential vaults, public TLS deployment and abuse/load acceptance |
| Virus/malicious-content handling | Structural validation; Mac deny-default scanner sandbox with CPU/file/fd/time/output bounds, official signatures and2-day definition-age cap; real clean/EICAR/error/review/revocation and filesystem/symlink/exec/network canaries pass at59686d4e; legacy reports require rescan/review | Worker memory/concurrency/global storage limits, queue/retry policies, dedicated service identity, scheduled updates/rescans and unsafe-script/content analysis |
| Moderation and reporting | Authenticated moderator/admin API with transactional approval/revocation/audit; clean scan still requires review | Native moderator UI, player reports, notification and operational staffing/policy |
| Catalog and update trust | HTTPS, exact URLs, immutable hashes, scan expiry and availability recheck | Signed catalog/root trust, anti-rollback/expiry, key rotation/recovery and distributed revocation enforcement before code loading/automatic activation |
| Storage and operations | Separate quarantine/approved exports/installed versions/active overlays; bounded archives and snapshot budgets | Persistent operation, global quotas, managed recovery, actual backup/restore, host/account isolation and incident/revocation drills |
| Player distribution | Uses existing native launcher runtime, with no player Python requirement | Complete three-host packaging/signature/Gatekeeper checks, authorized source synchronization/review, actual public release and independent install-to-play verification |

Windows evidence as of 2026-11-14: a full launcher build/test command passed;
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

The current Mac checkout contains unrelated edits and untracked cloud-conflict
copies. Preserve it. Use a separate task worktree, skip LFS artwork hydration,
and transfer source through Git. The existing Tailscale command points at a
missing application; a usable public identity has not been established. ClamAV
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
