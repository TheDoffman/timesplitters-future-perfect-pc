# Launcher mod workshop

Invites/PCM/Pulse listening: ../development/mod-creator-service.md (not deployed).
Origin-bound defaults/custom roots and DDS/dependencies: mod-workshop.md.
Core.Allowed admits only five named game helpers under Bin/ModRuntime/<64 lowercase
hex>/; uploaded mod admission stays separate. Package regression: build-test-details.md.

Installed activation now uses ModLibrary.PreviewEnable/ApplyEnable and an in-page
ModEnablePlan review. It resolves exact installed dependencies (32 packages),
keeps unrelated active mods, and shows added/replaced versions before confirmation.
It rejects missing/cyclic/incompatible requirements, payload conflicts and stale
profile/package bindings; confirmation revalidates bytes, capability and trust
under the library lock before publishing one profile. Back/cancel leaves the
profile unchanged; retained versions support reviewed rollback. Gameplay remains
one mode with isolated saves and a new match. Shared UI/model checks cover this.

Canonical workshop HTTPS authorities now permit the default port or explicit
8443/10000 only. Signed roots/catalogs/cache identities retain the exact port;
cross-port metadata and root rotations are rejected. Python and native validators
reject explicit443/leading-zero aliases, paths, credentials and unapproved ports
for signed origins. Transport URLs normalize through Uri before root matching.
Caddy's Host matcher gets only the validated hostname; API Host/Origin retain the
port. Existing private443 routing must stay private; separate8443 is planned.
Golden port-bound signatures and actual Caddy transport/API tests cover this.

ModCreatorPage includes creator invitations/uploads/history and opt-in saved
sessions. ModSessionVault uses Windows Credential Manager, Mac Keychain and
Linux secret-tool/Secret Service; unavailable/locked stores retain memory-only
access with a visible warning, never a plaintext fallback. Resume is explicit,
checks live pinned authority and GET/account role, and refuses expired/changed/
revoked/wrong-account sessions. Sign-out clears local/vault state on logout failure;
failed deletion stays visible. Forget saved sign-in preserves a current session.
Resume clears unsent secrets; passwords/invitations are never saved. Tests run;
TSFP_TEST_OS_VAULT=1 enables disposable native-store roundtrip/update/isolation/
delete proof. Windows native proof passes; Mac/Linux acceptance remains pending.
See ../development/mod-creator-service.md and build-test-details.md.

Dependency install flow: ModWorkshopPage now prepares a fresh signed dependency
plan, displays exact versions/reuse/new expanded payload size and requires an
in-page install confirmation. ModWorkshopClient re-plans before execution,
rejects changed manifests/installed state, verifies reused archive/payload bytes,
downloads dependency-first under a shared512MiB package-transfer cap and checks
availability of the entire closure before each publication. ModLibrary.Install
compares the extracted strict manifest to the reviewed metadata before publishing.
Installed archives also receive bounded ZIP preflight before manifest reads.
Planning rejects portable case-folded payload collisions. Cancellation/failure
keeps completed immutable packages inactive and preserves the active profile;
UI reports partial completion honestly. No auto-enable or batch rollback implied.
Signed public fixture vectors cover real trust verification, ZIP extraction,
reuse/tamper, stale plan, archive/listing mismatch, revoked dependencies, scaled
combined transfer limit and cancellation after a completed dependency. Fixture
keys were generated in memory and discarded; these are injected HTTPS responses,
not public connectivity or a production signing ceremony.

WorkshopInstallPlan consumes freshly verified catalogs and strict manifests,
orders exact dependency versions first and rejects missing/revoked entries,
cycles, incompatible versions, portable payload collisions, >32 packages or
>256MiB expanded payload. Reuse requires matching digest and origin receipts.
Connected confirmation/execution rechecks bytes and availability; incomplete
installs retain immutable inactive packages and preserve the active profile.

Workshop discovery (feature/mod-platform, basebc67c1c9): shared ModWorkshopPage
adds content/creator filters, name/creator/numeric-version sorting, latest release
per ID by default and explicit older-release browsing. Browse renders at most20
cards per page; changed filters/catalog reset the page. Categories, expanded
payload bytes and exact dependencies come from the same strict manifest parser
used by native installation. Mixed packages match each content category. The
model supports all three launchers without a new platform adapter. Windows
behavioral tests pass; native Mac/Linux builds and visual checks pending. Public
HTTPS, creator UI, map/audio activation and the full release remain unfinished.

[Script handoff](../development/mod-source-handoff.md): ModScriptLaunch.cs is a
shared bounded encoder, compiled on Windows/Mac/Linux. Its golden wire/hash/save-
identity vector also drives the native reader tests. Package authorization must
precede encoding. ModLibrary now writes and revalidates a generation capsule for one
main.lua mode, gated by BUILD.json mod_script_version=1. ModCacheLocation stores an
optional user-selected cache parent in Settings/mod-cache-location.txt; Installed
exposes the shared field. Modes emit it only after validation; without-mods bypass
never reads it. Native owner/ACL/filesystem policy remains required; see mod-workshop.md.
