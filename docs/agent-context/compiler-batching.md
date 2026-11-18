# Compiler batch byte limits

Regular split batches also cap body text at512KiB, preserving oversized functions intact. test_translation_batch checks exact body/dispatch identity and compiled execution across partitions.

Local fidelity validation hit MSVC C1002 on a3.8MiB SDK chunk despite100-function
batches and two workers. translation_batch._chunks now caps emitted bodies at
512KiB, preserving complete bodies and order; an oversized single function
remains whole. This carries the focused bounded-batch helper already developed
in the separate compiler-memory checkout; that checkout is untouched. CLI count
defaults are unchanged here. Byte/count/UTF-8/order tests cover the partition;
the local Windows game now builds; native Mac/Linux build acceptance is pending.

Merge integration retains the shared `_chunks` helper and both byte-boundary and compiled body/dispatch identity tests.

The native multiplayer merge also retains canonical integer/vector register
alias selection from9ee19013. Additive patch0261-register-header-template
synchronizes the compatibility runtime template after0260; it does not change
the canonical header or guest behavior. Full-stack upgrades and
`test_runtime_header.py` require exact template/header identity.
