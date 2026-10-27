# Per-title seed files

One file per title, named for the title ID in its XBE certificate --
`<TITLEID>.json`, eight uppercase hex digits, as `tools.xbe_parser` prints it.
`scripts/recompile.py` loads only the file matching the XBE it is given.

They used to live in one `config/seed_functions.json` that the driver loaded
for every XBE. A seed is an unconditional claim that a function starts at an
address, and `tools.disasm` only refuses one that lands mid-instruction -- so
each of Burnout 2's 214 addresses that happened to fall on an instruction
boundary in some other title became a fake function start there, splitting the
real function it sat inside and costing it its epilogue. Nothing said so.

`tools.seed_from_log` writes into the file for the title it observed.

The format is a list of objects, each with a `start` and a `note` saying how
the address was found. The note is the point: a seed with no reason cannot be
audited later.
