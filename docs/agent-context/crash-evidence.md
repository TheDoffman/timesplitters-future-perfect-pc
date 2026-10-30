# Crash evidence extension
The Windows helper now uses `pc_guest_capture.h` to write bounded `TSFPGST1`
guest-memory records. Each record stores kind, guest address, requested length
and captured length; zero captured length means unavailable, never zero-filled
successful memory. Default capture includes register pages, the guest stack and
a candidate EDI model tree. Candidate data does not establish a root cause.
`TSFP_CRASH_EXTENDED=1` expands register regions while retaining a 1 MiB file cap.
Windows shared packet schema is 5. The fatal thread still only publishes its
register packet; remote memory reads happen in the separate helper.
`tests/diagnostics/test_guest_capture.py` checks bounds, missing regions and both
modes. The Darwin helper uses the same bounded writer with `mach_vm_read_overwrite`;
process-access denial is recorded explicitly without requesting elevated access.
Native execution remains unverified here. Separate `model-history.txt` records
recent model addresses, node indices and recursion depth via the shared wrapper
at US guest 0x583E0, without crowding out gameplay breadcrumbs. These records do
not establish allocation/free history; backing-block lifetime tracking is described below.

## Backing allocations and match context

`2150-allocation-history.patch` installs a startup-only observer before device
workers exist. `guest_allocation.c` invokes the optional observer under its
existing lock, after successful allocation/release. The callback must never
allocate, block, or reenter the allocator. Events preserve kind (1 heap,
2 reservation, 3 physical), address and byte count; bit 8 denotes release.
`allocation-history.txt` has its own bounded ring, exported by both helpers and
launcher ZIPs. It records backing blocks, not the game's internal model-pool
suballocations; do not infer a specific model's death solely from this history.

The existing safe guest sampler additionally records changes in humans, bots,
difficulty and capacity. Names/paths/user profile text are not added. This is a
bounded sampled history, not a guarantee of every transient gameplay event or
30 seconds of coverage.

The patch loader places the allocation observer after the independent allocator
and final boot changes. Stopping the allocator discards its ownership state;
that shutdown does not synthesize individual release records. This history is
best-effort crash evidence, not a complete heap accounting ledger.
