# Script source handoff, version1

The shared launcher encoder is `tools/launcher/ModScriptLaunch.cs`; the native
reader is `src/mods/mod_capsule.cpp/.hpp`. This is the bounded source handoff for
the upcoming game bootstrap. No production startup or launcher activation calls
it yet. Public script activation still requires storage fencing, match lifetime,
offline policy and full native gameplay acceptance.

The trusted launcher authorizes the installed package set using its existing
manifest/archive hashes and fresh workshop trust/revocation checks. It verifies
the selected source against the primary package before creating a capsule.
An explicit local developer invocation supplies its own local authorization.
The expected whole-capsule SHA256 comes from that caller. Native code verifies
that exact file stream into an owned memory snapshot and validates the wire and
UTF8 source. Publisher authorization remains the launcher's responsibility.

The wire contains the following fields, with unsigned little-endian integers:

| Field | Bytes | Contract |
| --- | ---: | --- |
| Magic | 8 | ASCII `TSFPMOD1` |
| Package count | 4 | 1 through32, including the primary |
| Source length | 4 | 1 through1048576 bytes |
| Primary package SHA256 | 32 | Raw digest of the primary verified archive |
| Dependency SHA256 digests | 32 each | Count minus1; unique, ascending byte order, excluding primary |
| Lua source | Source length | Strict UTF8, no NUL, compiled Lua refused |

The file must have exactly this length; its maximum is1049616 bytes. The wire
carries package identities and source bytes. Helper paths/digests, save paths and
policy flags come from the game bootstrap, outside this format.

The save-profile identity is lowercase SHA256 of the ASCII domain
`TSFP isolated mode saves v1` plus one LF byte, the four package-count bytes and
the ordered raw package digests. The primary comes first; dependencies are sorted.
The same script package/dependency set therefore returns to the same namespace
after rollback. A different primary or dependency archive selects another
namespace, preserving earlier progress for explicit migration/recovery.

`read_pinned_file` shares the existing native CNG/CommonCrypto/OpenSSL providers.
It checks an absolute local nonlinked file path, opens a regular file once,
bounds its observed length before allocation, hashes while copying and rejects
short/growing/different input. The capsule reader has its own smaller size cap.
The retained source is independent of later changes to the launch file.

All three launcher test builds embed `tests/launcher/mod-launch-vectors.json`.
Shared C# tests and the native driver consume that same vector and verify exact
wire bytes, capsule hash and save identity. Adversarial native checks cover
digest policy, malformed/count/length/ordering variants, UTF8 edge cases, binary
Lua, size limits, aliases and replacement after capture. These are source handoff
checks; actual package authorization and game activation remain separate gates.
