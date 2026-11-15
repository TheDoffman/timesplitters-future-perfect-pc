# Modular First to Three

This original creator example uses the existing API1 offline Arcade Deathmatch
hooks. It separates the target/message from score bookkeeping and callbacks:

- `modular-first-to-three/scripts/main.lua`: the API1 callback table.
- `modular-first-to-three/scripts/rules/settings.lua`: target and opening message.
- `modular-first-to-three/scripts/rules/scores.lua`: cached per-match score state.

Copy the project, choose your own ID/name/version and set `author` to your workshop
username before uploading. Change `settings.lua` to build a mode variation.
From the repository root, using the pinned Python environment:

```sh
python tools/mods/mod-workshop.py pack examples/mods/modular-first-to-three modular-mode.zip --modules
python tools/mods/mod-workshop.py validate modular-mode.zip
```

Use a new output filename for each build; packaging never overwrites an existing
ZIP. Build outputs belong outside the source project. `--modules` is required for
this project. It produces one `scripts/main.lua` containing all linked source and
the package-local loader. Project files remain unchanged. It does not execute Lua.

Install the resulting ZIP through the existing supported mod workflow, enable it
in a game advertising `mod_script_version: 1`, then start a new offline Arcade
Deathmatch. Mod saves remain separate. This example's packed source has native VM
tests; actual all-host in-game acceptance of this new sample remains required.

Imports use dot-separated names relative to `scripts/`: `require("rules.scores")`
selects `scripts/rules/scores.lua`. Only this project's linked sources exist; the
loader does not read files, other packages, native libraries or the network.
Returned values are cached once per VM; a module returning `nil` caches `true`.
Missing/cyclic imports fail the mode. Each module receives the local `require`;
its first vararg (`...`) is its packaged module name.
Module names are case-sensitive; ambiguous names such as both `rules.x.lua` and
`rules/x.lua`, case-colliding paths and file/directory collisions are refused.

Projects may contain at most32 Lua files including main. The generated source,
including the loader, must fit the existing1MiB runtime source limit. All modules
share the existing VM, memory/instruction/host-call budgets and authority rules;
module boundaries are an authoring convention. They grant no new host permissions.
Unknown Lua syntax/logic still fails during VM preflight; format validation and
antivirus scanning do not establish gameplay correctness.
