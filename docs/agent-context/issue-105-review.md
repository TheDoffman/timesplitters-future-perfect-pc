# Why the context layer exists: Issue #105 investigation

Reviewed the supervised local run started 2026-10-20 20:48 EDT, thread
`01a15bed-fcee-7f31-93cd-5ab887108f4e`, at root baseline `a0deeea`.
Sources were the local session JSONL and controller audit ending
`1792543697484-cd1d3f57-99eb-47e3-a9ec-3baa5e847974.json`.
Those machine-local records are not committed or required to use this guide.

## Observed

- The controller recorded 3,600 seconds, 466 commands, 24 checkpoints, six
  successful Sol calls and a budget timeout. It did not verify task completion.
- The native session contained 20 compaction records. Their growing replacement
  histories do not quantify time spent compacting; they establish repeated
  context turnover during this run.
- Initial commands searched `build/windows/generated` and build subdirectories
  before locating guest C in the runtime title's `src/recomp/gen`.
- Searches spread through patches and generated C looking for menu/weapon names,
  then through flip-gate/vblank code. The agent repeatedly reconstructed patch
  ordering, migration state and source provenance instead of retaining a map.
- Sol supplied useful experimental plans, including stable/slow/stable timing,
  confirmation of the effective fixed-60 source and executable, and a warning
  that gate waits were unmeasured hypotheses. Advisories had only selected
  task/command context, without a standing repository architecture guide.
- Later work became patch/build recovery: repeated `0240-pc-debug.patch` unwind
  conflicts, ledger searches and patch-byte/line-ending edits. The run also
  changed crash-helper compiler/linker flags without establishing the original
  FPS cause. These are separate blockers, not evidence that #105 was fixed.

## Consequences for future work

The map now names actual guest/runtime/build paths, effective ordering and
ledger discovery. The bounded finder replaces repeated whole-stack reads;
runtime and build guides separate patch provenance from performance hypotheses.
The investigation note keeps actual evidence across context windows. Relevant
guide excerpts accompany Sol advice so it can give concrete repository steps.

The unfinished per-thread flip-gate patch is not treated as a confirmed fix.
This context work does not modify it or certify its application, build or
gameplay. A durable index reduces repeated discovery; it does not substitute
for profiling, a supported build or reproduced acceptance evidence.
