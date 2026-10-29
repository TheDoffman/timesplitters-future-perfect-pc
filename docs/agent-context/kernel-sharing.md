# Original I/O sharing and guest ABI

Stage0220 replaces inherited no-op/always-success bridges63/78/80 with
`src/runtime/guest_io_share.c/.h` and `guest_kernel_share.inc`. The additive
`patches/compiler/0220-independent-kernel-sharing.patch` removes the three old
bridge bodies, selects the provider once in shared kernel CMake, and corrects
ordinal80 cleanup to16bytes. The loader orders it after0219 and before2090.

Interface facts: [nxdk declarations](https://github.com/XboxDev/nxdk/blob/master/lib/xboxkrnl/xboxkrnl.h)
show IoSetShareAccess takes four arguments; Check takes five and Remove two.
The repository export map already describes Set@16. Xbox SHARE_ACCESS has
seven one-byte counters, unlike a host Windows ULONG layout. FILE_OBJECT
access/sharing bits occupy bits1..6 of byte+2; DeletePending/reserved bits survive.
No external implementation bodies were copied. Behavioral references:
[Check](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-iocheckshareaccess)
and [Set](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-iosetshareaccess).

Callers supply granted/expanded rights, stable disjoint writable mappings and
serialization for their sharing group. No native structures or host file handles
are serialized. Read includes execute; write includes append; delete uses DELETE.
New access must be permitted by all prior opens, and the new sharing mask must
permit each existing accessor. Metadata-only access does not consume counters.
Check-only preserves aggregates while recording compatible granted file flags.
Set initializes aggregates; successful Remove decrements contributions and clears
only the six flags to avoid a repeated decrement. This is explicit project policy.
Invalid spans/masks, inconsistent counters, saturated update and underflow reject
without altering either destination. These defensive edge policies are not proof
of every Xbox kernel behavior. Full file lifetime/security, NtCreateFile sharing
and driver request completion/queues remain separate unfinished work.

Maintained `tests/compiler/test_io_share.py` compiles literal byte/guard oracles
for all4096 two-open combinations, source fault controls, actual adapters, full
mapping lengths and actual installed stdcall cleanup preserving a caller's saved
word. Set reads exactly four arguments; Check narrows Update to one guest byte.
The compile-only probe uses actual installed kernel headers. CI selects the test
on both hosts. Supported US direct151 imports omit63/78/80; bounded gameplay is
regression evidence, while native tests establish the new service behavior.
Build selection/archive, source hashes, actual-header contracts, gameplay and
Mac host/title checks remain distinct. See STATUS and MAC-118 for measured results.


Windows acceptance:141 maintained checks passed18.40s, Node23 passed21.54s,
path/symbol4 passed0.17s; Release/built-in checks and fresh isolated Story
movement/firing/pause/resume passed. Selection/archive audit includes all three
sharing symbols and generated22 files unchanged. All26 protected files unchanged.
Actual dirty-a0e36a91 source hashes retained; current-stage migration/new-head CI
remain pending. See STATUS/MAC-118; no native Mac title or independence claim.
