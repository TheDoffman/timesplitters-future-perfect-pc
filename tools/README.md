# Development tools

| Folder | Responsibility |
| --- | --- |
| `build/` | Runtime preparation, patch application, generation fingerprints, game/launcher builds and SDK setup. |
| `compiler/` | Project XBE reader, disassembler, analysis pipeline and instruction lowering. |
| `ci/` | CI scope/gates, issue maintenance and compatibility checks. |
| `analysis/` | Bounded runtime observations and timing/motion analysis. |
| `network/` | Online companion, server, transport and network packaging. |
| `release/` | Packaging, release publication and scripts shipped with releases. |
| `testing/` | Isolated test profiles, host smoke tests and validation runners. |
| `textures/` | Texture inventory, encoding, auditing and optional-pack preparation. |
| `launcher/` | Shared launcher implementation and native Mac UI adapter. |
| `router/` | Optional local advisory integration. |

Run commands from the repository root, for example
`python tools/build/apply-patches.py external/ts3FPrecomp patches`.
`project_paths.py` supplies shared import paths for directly executed commands.
`repo-context.py` and the supported play entry points retain their stable paths.
