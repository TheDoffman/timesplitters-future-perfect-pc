# Embedded Developer Tools fonts

Unmodified fonts, embedded at build time; no system font installation is needed.

- Microsoft Selawik1.01: regular `selawk.ttf`, semibold `selawksb.ttf`, bold
  `selawkb.ttf`, from [the publisher release](https://github.com/microsoft/Selawik/releases/tag/1.01).
  [SIL Open Font License1.1](Selawik-OFL.txt). Upstream source revision:
  `89362e84731d1f5777fa078fd4d5ebbd339e4378`. This is the publisher's open-source
  replacement for Segoe UI; it does not redistribute Microsoft's Segoe UI.
- Cousine Bold for numeric fields, from
  [Google Fonts](https://github.com/google/fonts/tree/24ddc1f324cac5d9f5aedca126d298f56e3f1640/ofl/cousine).
  [SIL Open Font License1.1](Cousine-OFL.txt).
- Shared rasterizer: [stb_truetype](../../../vendor/stb/stb_truetype.h), v1.26,
  upstream revision `d14d46f6b44456eb88beac58cba833a4135d4c89`, under its MIT option.
  [License](../../../vendor/stb/LICENSE).

`manifest.cmake` pins complete-file SHA-256 hashes. The CMake embedding helper
verifies them and generates C arrays inside each build tree. No network access,
font subsetter, external font parsing or browser runtime is involved in builds.
Original files and reserved font names are retained. Packaging must retain both
font licenses and the stb license alongside the executable.
