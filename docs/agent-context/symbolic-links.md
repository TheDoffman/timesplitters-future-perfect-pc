# Original symbolic-link namespace

Stage0232 selects guest_symlinks.c/.h and guest_kernel_symlinks.c/.h through
patch0232 after0231 and before2090. It removes the legacy kernel_io symbolic-link
table/create/delete/borrowed-pointer lookup and their kernel.h declarations.
The kernel bridge includes original guest_kernel_symlinks.inc for ordinals67/69;
argument byte counts remain8/4. Kernel path translation requests a copied target
from the same original process namespace. The rest of kernel_path is transitional
legacy code; this does not claim full VFS/kernel I/O independence.

Core names/targets are opaque exact-case counted byte strings, length1..65535;
there is no truncation, normalization, path recursion or host/guest pointer
conversion. Create copies both spans before locked publication. Collision and
capacity checks serialize with remove/stop; rejected candidates are disposed
outside locks. Lookup copies one complete target under lock into disjoint caller
storage, preserving output/length on missing/invalid/short-buffer/closed results.
Copies stay valid after remove/rebind/stop. Stop closes admission and detaches
under lock, then disposes outside it; caller joins before reset/destruction.
Allocation/disposal callbacks can reenter and must return normally. Inputs remain
stable throughout each synchronous call. Namespace bytes own no object/OS handle.

The kernel adapter uses one C-statically initialized32-binding store, preserving
the prior count bound and exact-case matching. Each stored name/target may now use
the complete16-bit count. No lazy-init race or borrowed target pointer escapes.
Its process-lifetime entries are reclaimed at process exit; no concurrent reset.
Guest decode resolves full8-byte ANSI_STRING header and the complete counted
buffer as read-only; requires nonzero length/buffer, Length<=MaximumLength and
nonwrapping ranges. It copies synchronously; no guest memory pointer is retained.
Invalid arguments return INVALID_PARAMETER; duplicate COLLISION, absent MISSING,
and allocation/capacity INSUFFICIENT_RESOURCES are mapped explicitly. Empty or
unreadable target is rejected, replacing legacy empty-link acceptance. These are
bounded adapter contracts, not complete NT object-directory/case-table behavior.

The existing path adapter copies into private4096-byte scratch, rejects embedded
NUL/overlong targets rather than truncating and appends the caller's path suffix.
It retains the existing conservative static-rule fallback and drive-letter ASCII
normalization. An unsupported target still leaves the original path translation
policy in control. Namespace supports arbitrary counted bytes even when the path
adapter cannot represent those bytes as a host C path. No symlink-chain traversal,
path sandbox rewrite, file ownership or new drive policy is introduced here.

Maintained test_guest_symlinks covers long names differing in their final byte,
160/65535-byte targets, embedded bytes, full copied snapshots, private output
preservation, failure/capacity/collision/rebind/stop, stop during allocation,
reentrant disposal and1000 iterations across four native workers. Six core and
three guest-adapter bad variants compile before runtime rejection. Adapter cases
use actual original kernel namespace calls and mapped literal guest descriptors,
including malformed lengths/fullspan/range guards and32-binding capacity.
Shared runtime CI selects this on Windows/Mac. tests/storage/save_paths.c also
creates/removes a project namespace link and resolves lower-case and object-prefix
drive paths to the isolated save root through the actual kernel path translator.
Windows title gameplay and native Mac acceptance remain separate evidence.
