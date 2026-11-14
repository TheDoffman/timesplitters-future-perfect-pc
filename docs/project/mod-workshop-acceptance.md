# Full mod-system completion evidence

The active goal covers a usable public creator-to-game system, not just the
prototype or texture support. This table tracks that full scope. Tests prove
their named components; none substitute for actual host/game/public acceptance.

| Requirement | Current authoritative implementation/evidence | Still required |
| --- | --- | --- |
| Creator packaging and SDK | `tools/mods/mod-workshop.py pack/validate`, original example, bounded package validator and keyed DDS contract | Stable documented asset/gameplay API, importers, richer original starter projects and complete creator documentation |
| Real asset additions | Shared DDS worker resolves enabled overlays; compiled reader tests cover precedence, fallback and bounds | Updated game builds and live visual tests on Windows/Mac/Linux; audio, map and other content loading/import workflows |
| Sandboxed gameplay scripting and custom modes | Lua source can be packaged/installed; activation is refused | Pinned engine, restricted game API, OS isolation, enforced instruction/memory/event/entity/host-call quotas, crash containment and hostile-script tests |
| Launcher browsing/search | Shared Mods page, scan/creator information, bounded HTTPS catalog and verified download; Windows shared UI checks pass | Native Mac/Linux execution, categories/filters/pagination, production endpoint and richer release/permission/report information |
| Native installation and lifecycle | `ModLibraryPage.cs`: immutable versions, strict JSON/ZIP/DDS validation, dependency/cycle/conflict guards, enable/disable, version switch/rollback, uninstall and unknown-file preservation | Complete all content types, native Mac/Linux and cross-filesystem acceptance, interrupted-work/recovery UX, storage and profile management at production scale |
| Normal/recovery launch | Shared `GameSettings.StartInfo` configures verified texture overlay; Play without mods bypasses corrupt profiles without rewriting them | Actual game sessions, activation compatibility negotiation and end-to-end preservation across update/rollback/removal |
| Multiplayer and saves | Cosmetic texture path leaves guest data unchanged; originals/settings/saves are not modified by library code | Server authority, ordered gameplay-mod/hash negotiation, mismatch behavior, modded save namespaces and real multiplayer acceptance |
| Public Mac mini hosting without a purchased domain | SSH verified; same AGENTS hash; Caddy/Python available; approved static snapshot export and expiry-supervised loopback server prepared | Isolated source checkout/tests, actual persistent service setup, working HTTPS/tunnel identity, external connectivity and availability/expiry/reboot acceptance |
| Creator accounts/ownership/uploads | Local operator submission only; no public upload route exists | Authenticated identity/ownership, upload authorization, quotas/rate limits, private quarantine, ownership transfers and abuse protection |
| Virus/malicious-content handling | Structural/payload validation, fail-closed ClamAV adapter, explicit review and scan expiry; synthetic tests pass | Real ClamAV installation/detection, fresh database enforcement, isolated bounded workers, queue/retry policies, scheduled rescans and unsafe-script/content analysis |
| Moderation and reporting | Local approval/revocation and append-only operator events | Moderator roles/UI, player reports, takedowns, notification and operational staffing/policy |
| Catalog and update trust | HTTPS, exact URLs, immutable hashes, scan expiry and availability recheck | Signed catalog/root trust, anti-rollback/expiry, key rotation/recovery and distributed revocation enforcement before code loading/automatic activation |
| Storage and operations | Separate quarantine/approved exports/installed versions/active overlays; bounded archives and snapshot budgets | Persistent operation, global quotas, managed recovery, actual backup/restore, host/account isolation and incident/revocation drills |
| Player distribution | Uses existing native launcher runtime, with no player Python requirement | Complete three-host packaging/signature/Gatekeeper checks, authorized source synchronization/review, actual public release and independent install-to-play verification |

Windows evidence as of 2026-11-14: full launcher build/test command passes;
12 focused workshop groups cover native library/client behavior; DDS/parser/
Classic/streaming tests and adversarial Python workshop cases pass. Logs and
exact counts are in [STATUS](STATUS.md) and ignored task logs. Mac/Linux and
live gameplay/public service results must be added only after they are run.

The current Mac checkout contains unrelated edits and untracked cloud-conflict
copies. Preserve it. Use a separate task worktree, skip LFS artwork hydration,
and transfer source through Git. The existing Tailscale command points at a
missing application; a usable public identity has not been established. ClamAV
is absent. These are operational prerequisites, not evidence of a deployed host.

Do not close the goal while any requirement above remains incomplete or lacks
the corresponding real-world acceptance evidence. Do not merge main without
the owner's explicit instruction.
