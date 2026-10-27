# Shared impact sparks at native refresh

`pc_native_numeric.h` admits the original type-9 impact-spark updater at
108793 whenever the live game interval advances, even if the carried 60 Hz
whole count is zero. The updater integrates its eight segment positions from
the existing fractional effect delta at 73BBA8. This affects impact sparks
emitted by multiple weapons; Original60 and zero-delta pauses retain their
original gate. The original guest routine and a mutation without the hook are
exercised by `native_impact_sparks.c` through `test_native_timing.py`.

Run `python -m pytest tests/gameplay/test_native_timing.py -q -k native_impact_sparks`.
It checks 60/120/180/240 FPS, pause, and a deliberately unhooked negative
case. Live firing and Mac visual checks remain separate from this CPU contract.
