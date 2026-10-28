# Regression tests

| Folder | Responsibility |
| --- | --- |
| `compiler/` | Translation, input reading, generation and guest/native execution and memory contracts. |
| `repository/` | Patch migration, context, SDK pins, CI gates and repository maintenance. |
| `gameplay/` | Timing, animation, physics and bounded real-game acceptance. |
| `graphics/` | Renderer, shader, surface, overlay and UI contracts. |
| `input/` | Keyboard, mouse, controller, prompts and settings behavior. |
| `network/` | Online, networking, bot synchronization and Discord. |
| `media/` | Audio timing/output and movie playback. |
| `textures/` | Texture encoding, loaders, modes and optional-pack policy. |
| `launcher/` | Launcher lifecycle, installation, update and packaging. |
| `diagnostics/` | Profilers, trace readers, debug UI, crashes and hangs. |
| `storage/` | Save paths, profile acceptance and host storage. |
| `support/` | Shared native compiler, test headers and fixture fragments. |
| `fixtures/` | Versioned test inputs and historical patch-stack membership. |
| `rhi/` | Platform renderer contract harnesses. |
| `launcher_lifecycle/` | Standalone launcher lifecycle fixture project. |

Run `python -m pytest tests` from the repository root. CI keeps its explicit
suite selection; moving a file must also update that selection. Live-game
harnesses require private game data and isolated saves; they are separate from
asset-free tests. `conftest.py` shares fixture imports across these groups.
