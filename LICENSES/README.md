# Third-party licenses and credits

The root [LICENSE](../LICENSE) applies to this project's original work as
described in the [README](../README.md#license) and [NOTICE](../NOTICE). It does
not replace a dependency's license or transfer its copyright.

## Pinned runtime

`ts3FPrecomp/` contains verbatim copies from
[nemonicicon/ts3FPrecomp at d90c92988a9f99d0580d2ca3f7efd6ec3a7b7845](https://github.com/nemonicicon/ts3FPrecomp/tree/d90c92988a9f99d0580d2ca3f7efd6ec3a7b7845),
the revision pinned in `tsfp.ps1`:

| File | Purpose |
| --- | --- |
| [LICENSE](ts3FPrecomp/LICENSE) | Upstream fork's GPLv3 license. |
| [LICENSE.upstream-MIT](ts3FPrecomp/LICENSE.upstream-MIT) | Original xboxrecomp MIT license and copyright notice. |
| [NOTICE](ts3FPrecomp/NOTICE) | Upstream attribution, including doaxbv-re, xemu and reference material. |
| [LGPL-2.1.txt](ts3FPrecomp/LICENSES/LGPL-2.1.txt) | LGPL text for the xemu-derived components identified by upstream. |
| [CONTRIBUTORS.md](ts3FPrecomp/CONTRIBUTORS.md) | Upstream contributor credits. |

These copies document that exact revision. When updating the runtime pin,
refresh them from the new revision and review any changed licensing terms.
Existing notices in source files and patches must also be preserved.

## Other dependencies and artwork

- Keyboard/mouse and controller prompt images are Julio Cacko's FREE Input
  Prompts Pack 1.4, under [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/).
  [Attribution and modifications](../assets/ui/input-icons/ATTRIBUTION.md)
  accompany the generated native UI data; public game packages retain the full
  credit as `Licenses/Keyboard-mouse-icons.txt`. Root NOTICE also credits it.
- SDL and FFmpeg keep the licenses supplied in their pinned dependency folders.
  Public game ZIPs retain these in `Licenses/SDL.txt` and
  `Licenses/FFmpeg-LGPL-3.0.txt`.
- Bundled Python/Tcl/Tk notices remain in `Support/Python/LICENSE.txt` and
  `Licenses/Python.txt`. Online dependency notices, including cryptography,
  cffi, pycparser and bundled OpenSSL, remain in their wheel license directories;
  `Licenses/Online-dependencies.txt` points to them.
- The existing [TSFP icon lettering notice](../tools/launcher/GameMonogram.NOTICE.txt)
  credits Chequered Ink's Future TimeSplitters typeface. Public game ZIPs retain
  it as `Licenses/Game-icon-lettering.txt`. Its separate terms are unchanged.
- Original game artwork, logos, executables, translated game logic and assets
  are outside this project's GPL grant. See [NOTICE](../NOTICE).

This inventory does not substitute for the full license texts supplied with
dependencies, or for providing corresponding source when their terms require it.

## Native Developer Tools typography

[Bundled font provenance](../assets/ui/fonts/README.md) records the pinned Selawik
and Cousine font files (SIL OFL1.1). Their complete licenses are retained beside
the fonts and copied into game packages. The shared rasterizer is
[stb_truetype](../vendor/stb/stb_truetype.h), under its [MIT license](../vendor/stb/LICENSE).
