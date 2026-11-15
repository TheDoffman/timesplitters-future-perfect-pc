# Dedicated Mac workshop identity

The private login-agent prototype runs under the owner's account. Production
hosting needs a separate service identity and root-owned code. The preparation
tool `hosting/provision-service-mac.py` performs only this identity/layout step;
it does not deploy code, migrate data, create signing keys, load jobs, change SSH
or sudo policy, or start a public endpoint.

The default invocation is a read-only plan:

```sh
python3.12 tools/mods/hosting/provision-service-mac.py
```

It checks local user/group numeric IDs, chooses an unused pair, and refuses an
existing `_tsfpworkshop` record or installation path. Its explicit `--apply` mode
requires administrator sudo locally on the Mac:

```sh
sudo python3.12 tools/mods/hosting/provision-service-mac.py --apply
```

Review/use a pinned source copy. Enter any administrator password only into the
local terminal's sudo prompt, never into chat, configuration or command arguments.
SSH access alone does not supply noninteractive administrator privileges.

The account is named `_tsfpworkshop`, with a private group and unused matching
UID/GID from600 upward. It receives a false shell and disabled authentication
before a usable numeric UID, an unusable password marker and no administrator
membership. It is hidden from interactive login selection as a service account.
The secure-token disabled tag follows [Apple's deployment guidance](https://support.apple.com/guide/deployment/dep24dbdcf9e/web);
Apple defines the disabled-user marker in its [Directory Services constants](https://github.com/apple-oss-distributions/DirectoryService/blob/main/APIFramework/DirServicesConst.h).
No real service password or FileVault credential is requested or generated.

The new `/Library/Application Support/TSFP Mod Workshop` layout contains:

| Path | Owner/mode | Purpose |
| --- | --- | --- |
| Installation root | root,0755 | Root-owned installation metadata |
| `identity.json` | root,0600 | Identity UUID/numeric-ID receipt |
| `releases` | root,0755 | Future pinned read-only code/runtime releases |
| `data` | service identity,0700 | Future private host data/home |
| `secrets` | service identity,0700 | Future online catalog key only |

New directories have inherited ACLs cleared before their final permissions are
set. Unknown existing paths, directory records and incomplete receipts are
preserved and cause refusal; there is no recursive deletion/chown or implicit
repair. A completed receipt supports repeat verification instead of recreating
the account. An interrupted administrator run requires inspection of that named
installation and account before continuing.

Unit tests create no users. Actual read-only host planning and privileged apply
are distinct acceptance gates. After successful apply, still verify running code
under that identity, root-owned release provenance, system-domain launchd job
control, public-server isolation, signing ceremony, boot operation and internet
connectivity. Do not point this identity at the owner's prototype secrets/data
or interpret a created account as a deployed workshop.
