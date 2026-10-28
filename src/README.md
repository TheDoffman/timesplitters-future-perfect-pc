# Runtime source

| Folder | Responsibility |
| --- | --- |
| `input/` | Controls, bindings, prompts and weapon selection. |
| `interface/` | Display settings, menus, text, HUD and UI layout. |
| `gameplay/` | Guest camera, timing, animation, physics and simulation policy. |
| `graphics/` | Shared graphics algorithms, surface handling and renderer contracts. |
| `textures/` | Replacement texture loading, formats, streaming and mode selection. |
| `network/` | Multiplayer, bots, online services and Discord presence. |
| `diagnostics/` | Debug settings, breadcrumbs, crash capture and hang handling. |
| `storage/` | Save routing, host storage and profile persistence. |
| `runtime/` | Image loading, allocation ownership, guest state/interface, memory boundaries, continuations, faults, ABI reports and branch observations. |
| `native/` | Operating-system adapters and native renderer integration. |

Keep shared behavior in these folders and host calls in `native/`. The final
repository-layout patch connects these paths to the pinned runtime; historical
patch payloads keep their existing bytes.
