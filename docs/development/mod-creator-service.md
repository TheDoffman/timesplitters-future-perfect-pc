# Private creator service

The creator/moderator backend is implemented in this task branch. It is not
exposed to the internet. The launcher now includes creator sign-in, invitation
redemption, reviewed uploads and status history. Persistent credential-vault
integration, player reports, production supervision/backups and public host
acceptance remain required before release. The existing Caddy configuration
still serves only approved snapshot downloads and does not proxy this API.

## Native creator workflow

Open **Mods → Creator** in a launcher built from this branch. Sign in with your
workshop username/password, or redeem an operator invitation with a new password.
Passwords and invitations are masked and cleared after submission or leaving the
tab. Sessions last at most12 hours and stay in memory for this launcher process;
they are never written to preferences, logs or a credential file. Sign out also
forgets the local session if its server revocation request fails. Persistent
OS credential-vault storage remains unfinished.

Build a ZIP with the SDK, enter its full path, and choose **Review ZIP**. The
launcher validates the complete archive, manifest and payload without installing
or extracting it. Its author must match your signed-in username. Confirm the
displayed immutable version/digest to upload; changed bytes require another
review. The client reserves exact metadata, binds the returned ticket, and sends
the ZIP to private quarantine. It does not approve, publish or enable the mod.
Use **Refresh uploads** to see queued scans, failures and pending manual review;
history is paged at100 records. Cancelled or interrupted transfers can leave
reservations until expiry, so check status before retrying.

Every authenticated request uses the configured HTTPS authority and rechecks
the live installed trust-root chain before sending credentials. There are no
redirects, cookies, default OS credentials or automatic upload retries. Responses
have strict structural and size limits. UI-only adversarial checks inject API
responses. A separate developer backend mode runs the real account/upload API,
SQLite and SDK packager through private pipes, checking actual quarantine,
ownership, immutable versions, Unicode passwords and remote session revocation.
It opens no network server and does not scan or publish. Real internet TLS and
public creator-to-scanner-to-game acceptance remain unfinished.

An explicit Mac developer scanner mode extends the native client fixture through
the real isolated ClamAV queue. It requires a private configuration pinning the
accepted bundled engine and official definitions. It checks clean-to-review,
EICAR rejection and fail-closed scanner errors, including exhausted retry status
in the native client. It never approves or exposes the fixture packages. This
private-pipe acceptance is separate from public HTTPS and in-game acceptance.

## Native moderation

Moderator and administrator accounts see a **Moderation** section inside the
Creator tab. Refresh lists completed scan jobs, with pages of up to100 records.
Select a package to inspect its exact ID/version/archive digest, creator,
description, dependencies and declared file sizes/hashes (first32 files shown).
This metadata view does not download or execute quarantine. Review the complete
source and contents independently before approving; in-launcher source inspection
and scan-evidence details remain unfinished.

Enter a review note, choose **Review approval** or **Review revocation**, then
confirm the exact package and note. **Back** sends no decision. Approval is offered
only for pending review; the server rechecks the role, active creator and fresh
clean scan in its approval/audit transaction. Revocation withdraws repository
availability; signed publication and client refresh must propagate it to public
downloads and installed clients. Refresh after any interrupted or failed decision
instead of retrying blindly. Leaving the account clears local review selections.

The native client checks cursor ordering, duplicate records, strict manifests,
manifest identity and returned decision identity/digest/state. Creator accounts
cannot list or decide moderation. Shared native UI tests use injected responses;
the separate real-backend check verifies creator refusal and a moderator's empty
queue while uploads remain unscanned. Public moderator acceptance remains pending.

The separate Mac developer mode `--test-creator-moderation` runs the real scanner
cases first, then uses the native moderator client to approve only the genuinely
clean fixture and revoke it again. It verifies both actual SQLite audit trails,
creator/reviewer histories and the repository's real catalog/blob availability
guards. Its private repository observer is not exposed through HTTP. The fixture
never signs/publishes a catalog, opens a listener or installs/enables content.
This check requires the same explicitly selected sealed runtime and official
definitions as scanner acceptance; ordinary build tests never scan or approve.
This acceptance passes on the actual Mac at sourcec294267c, including both audit
trails and withdrawal after revocation. It does not establish public TLS,
publication, downloaded installation or locked-service-account operation.

Private snapshot/staged restore tooling and conservative publication reconciliation
are now implemented; see [host recovery](mod-host-recovery.md). A disposable Mac recovery
drill passes; production cutover and off-device recovery remain acceptance work.

## Identity and ownership

`tools/mods/mod_accounts.py` stores accounts, one-time invitations, revocable
12-hour sessions, bounded persistent rate counters and private audit records in
the repository SQLite database. It uses scrypt N=131072/r=8/p=1 with a random
16-byte salt and32-byte derived key, following the
[OWASP password-storage profile](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html).
Passwords accept15–256 NFC-normalized Unicode characters, up to1024 UTF-8 bytes.
Only hashes of256-bit invitations/session tokens enter the database. One scrypt
derivation may run per backend process; the host profile has two sync workers.

Registration initially requires an operator invitation, valid24 hours. Creator,
moderator and admin roles are assigned locally; public redemption cannot select
a role. Password changes revoke all sessions. Local recovery revokes sessions
and suspends the account until a new one-time invitation is redeemed, preserving
its identity and owned mods. Suspension immediately invalidates authentication.
Audit identities distinguish `user:NAME` from `system:operator`/`system:scanner`,
so a creator's chosen name cannot impersonate a service actor in the history.
At most five active sessions per account,10,000 accounts/invitations and100,000
audit records are retained; exhausted capacity stops new mutations for operator
attention rather than silently deleting audit history. Audit archival/rotation
is still operational work.

The username is3–32 lowercase ASCII letters/digits/hyphens. Until the package
schema gains separate verified-publisher metadata, an uploaded manifest's
`author` must equal that username. `mod_uploads.py` claims a new mod ID only
after full body/hash/structure validation. Claiming and final ownership checks
are transactional, including two creators racing different versions of an ID.
Existing operator releases cannot be claimed remotely; local ownership adoption
or transfer requires an explicit note. Versions remain immutable.

## Uploads, scanning and review

An authenticated creator reserves `{id,version,sha256,size}` before sending ZIP
bytes. Default limits are256MiB/archive,1GiB/creator,4GiB globally, two pending
uploads/creator, eight globally, five reservations/creator per rolling day,
20 owned IDs/creator and10,000 retained upload records. Open reservations count
against quotas; invalid bodies release storage but still count toward daily
limits. Expired tickets release reservations. Quarantine/rejected/held content
remains charged. A minimum2GiB of free disk headroom is required, accounting for
unwritten reservations. These are application reservations plus a free-space
floor, not an operating-system filesystem quota.

Tickets expire after15 minutes; receiving leases last five minutes. Uploads
use private random filenames, exclusive creation and fsync, with no user-chosen
filesystem paths. An exact length/hash and the package's identity/author must
match the reservation. Authentication is checked again before queuing, so a
session revoked during transfer cannot complete the upload. Nothing in incoming
storage has a public download route.

`scan-next` processes one job under an OS file lock. Processing leases last ten
minutes, longer than the scanner child's180-second ordinary-process alarm and
parent deadline; active leases block another job. Recovering a committed release
does not rescan or reapprove it. Scan errors retry after five minutes, at most
three attempts. Unexpected failures are held and remain charged for operator
investigation. The new [rescan-next](mod-scan-maintenance.md) command renews existing reviews
only after fresh isolated scans; detection revokes and errors withhold availability.
The scanner still needs external memory supervision, persistent
service identity and automatic scheduling before production operation.

Clean scans stop at `pending-review`. An authenticated moderator/admin must
approve the exact scanned release. Role checks, approval/revocation and both
audit histories share one SQLite transaction. Audit failure rolls approval back.
Detected/error/expired scans cannot be approved. Revocation removes repository
availability; an already exported static snapshot still requires stopping or
switching publication, and installed-client revocation enforcement remains work.

## Bounded parsing

Both `mod_package.py` and the native launcher inspect the ZIP end record and
central directory before opening a general ZIP decoder. Single-disk standard
directories must have2–2,049 entries, names at most180 bytes, extras at most4KiB,
and entry/archive comments at most1KiB. Directory sizes and actual record counts
must agree. Self-extracting prefixes and global ZIP64/multi-disk records are
unsupported. This prevents allocation attacks that lie about entry counts.

Map descriptors are limited to4MiB and Lua source to1MiB/file. JSON budgets are
32 container levels,4,096 members/collection and65,536 value/key tokens before
object allocation. Numbers have bounded lengths/ranges compatible with the
native decoder. Native numeric parsing matches at the current position without
copying the remaining document for every number. These limits do not establish
gameplay script safety; scripts remain unavailable for activation.

## Operator commands

Use a separate private repository on the Mac, owned by the service account with
mode0700. Do not point it at a game installation, public snapshot or personal
save directory. No password or session token belongs in a command-line argument.

```sh
python tools/mods/mod-workshop.py invite creator-name --repository /private/workshop
python tools/mods/mod-workshop.py invite reviewer-name --role moderator --repository /private/workshop
python tools/mods/mod-workshop.py recover-account creator-name --note "Identity verified locally" --repository /private/workshop
python tools/mods/mod-workshop.py suspend-account creator-name --note "Investigation" --repository /private/workshop
python tools/mods/mod-workshop.py assign-owner creator-name --mod-id example-mod --note "Verified transfer" --repository /private/workshop
python tools/mods/mod-workshop.py scan-next --repository /private/workshop --clamscan /path/to/clamscan --clamav-database /private/official-definitions
```

Invitation/recovery commands print only a path and expiry. The one-time secret
is in a newly created0600 JSON file inside0700 `operator-secrets`; deliver it
through a private channel and remove the delivered copy. These commands require
a Unix host so they cannot silently rely on Windows's non-enforcing mode bits.
Recovery or delivery failures do not reactivate old sessions.

Install the separate host dependency with
`python -m pip install --require-hashes -r tools/mods/hosting/requirements.txt`.
Gunicorn27.0.1 is pinned to its published wheel SHA-256, with no extras; see the
[upstream release notes](https://gunicorn.org/news/). From `tools/mods`, start
`python -m gunicorn --config hosting/gunicorn.conf.py 'mod_api:create_app()'` only
after setting `TSFP_WORKSHOP_REPOSITORY` and `TSFP_WORKSHOP_ORIGIN` (an HTTPS
origin, no credentials). The profile binds127.0.0.1:8767, uses two sync workers,
180-second request deadlines and bounded headers/request lines. The production
factory rejects superuser/unsupported-host execution before opening storage;
use an unprivileged Unix account. It trusts HTTPS
scheme headers only from loopback and does not log request URLs/credentials.
Do not expose this backend port directly. Caddy/TLS routing is not installed by
this command, and public deployment remains incomplete.

## API and checks

The JSON API rejects unknown/duplicate fields, bodies over16KiB, unexpected
Host/Origin/scheme, cross-site browser requests and ambiguous framing. Sessions
are bearer headers, never cookies or query parameters. Responses are JSON with
`no-store`/`nosniff`. Rate limits currently use the WSGI peer address; behind the
proxy this conservatively groups clients. Trusted original-client attribution
and real public load acceptance remain deployment work.

| Route | Behavior |
| --- | --- |
| `POST /v1/accounts/redeem` | Redeem invitation/recovery with a chosen password |
| `POST /v1/sessions` | Sign in; receive a short-lived session |
| `GET /v1/account` | Read own username/role |
| `DELETE /v1/sessions/current` | Revoke current session |
| `POST /v1/account/password` | Change password and revoke all sessions |
| `POST /v1/uploads` | Reserve exact package metadata and storage |
| `PUT /v1/uploads/ID` | Transfer package bytes; remain private |
| `GET /v1/uploads`, `GET /v1/uploads/ID` | Own bounded history/status |
| `GET /v1/moderation` | Moderator's bounded queue |
| `POST /v1/moderation/ID` | Moderator approval/revocation with a note |

List routes accept an integer `after` cursor and return at most100 records.
No route serves quarantine bytes. Public snapshots remain separate.

Run `pytest tests/mods`. `test_creator_service.py` uses real scrypt and adversarial
state/concurrency cases. `test_creator_api.py` includes WSGI conformance and a
real loopback HTTP workflow; scanners in ordinary unit tests are synthetic.
Run `python tests/mods/live_creator_service.py --database PATH --clamscan PATH`
on the Mac with the pinned server/real ClamAV for actual HTTP-to-scan-to-review
acceptance. This is a private loopback/proxy-protocol check, not internet TLS.
`live_scan_isolation.py` separately verifies filesystem/exec/network canaries and
hard resource limits plus the alarm surviving exec. Native launcher host checks
cover the same ZIP/JSON allocation guards on Windows/Mac/Linux.
