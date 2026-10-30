# Original shared console path routing

Stage0233 selects guest_paths.c/.h through0233 after0232 before2090. It removes
the old rule table/prefix/cache/link planners, both host translate bodies and raw
device-path helpers from kernel_path.c. Original guest_kernel_paths.inc supplies
xbox_translate_path and Windows xbox_LastHostPath after retained host helpers.
Host initialization, profile validation/snapshot locks and partition-image creation
remain transitional; this does not retire all kernel_path or file I/O code.

The shared counted planner accepts0..4095 non-NUL bytes and emits an owned relative
snapshot plus root/category/directory-length metadata. Whole ASCII-insensitive
components and either slash are recognized. C/D/Y and DVD paths select GAME;
TDATA/T select SAVE/TitleData; UDATA/U select PROFILE/UserData (SAVE if no active
profile). Partition1 ordinary children and Partition2 filesystem children select
GAME. Partition3-5 children and Z select SAVE/Cache/<uppercase8-digit title id>;
title0 omits the id. Bare Partition0-5 devices with optional trailing separators
select SAVE/PartitionN.img. Both hosts use these six initialized image devices;
the old Windows-only allowance for nonexistent Partition6-9 is not retained.
Object-prefix drive forms, mixed separators and component boundaries share one
parser. Unsupported/relative-drive paths pass through with host separators.
Dot-component/traversal containment and a complete console namespace are outside
this planner. No host filesystem calls or retained guest/namespace pointers.

One optional drive-link lookup uses a copied counted target. Missing, NUL,
oversized or unrecognized expansions leave original routing in control. A
recognized expansion is planned once; no recursive drive-link chain/cycle walks.
Output and plan are preserved on failure; callers supply disjoint stable spans.
Host joins preflight complete capacity and preserve POSIX/drive/UNC root anchors.

The adapter snapshots the active profile with existing locks; only PROFILE uses
it. Title/cache/raw partitions retain SAVE. Windows preserves the guest ACP and
strict UTF8 profile-root conversion policies, joins UTF16 without truncation and
remembers every successful result in TLS, including passthrough/partition paths.
POSIX uses the same plan with byte joins. Existing native mkdir helpers create
save subdirectories only after full-output preflight. Watch hooks remain on
recognized nonpartition paths (POSIX still a no-op). Overlong output now fails
without publishing partial bytes. Profile selection/storage remains external.

Maintained test_guest_paths covers literal game/save/profile/cache/partition
routes, whole-component boundaries, title0/uppercase IDs, both separators/root
joins, copied overrides, malformed targets, nonrecursive linked drives and output
preservation. Seven bad planners compile before runtime rejection. Host fixtures
check actual Windows ACP/UTF16/UTF8 conversion and the POSIX adapter, TLS snapshots,
profile routing and directory/watch boundaries. Windows compiles the POSIX fixture
as policy evidence; native Mac filesystem/title validation remains separate.
Real tests/storage/save_paths.c and tests/gameplay/native_paths.c retain existing
profile/raw-image checks and add bareDVD, Partition2 children, drive links and
short-buffer preservation. Image creation/table/size/preservation logic is not
changed here. CI selects shared contracts on both hosts; Windows conversion cases
skip Mac. Bounded Windows Story is separate from complete campaign/Mac acceptance.
