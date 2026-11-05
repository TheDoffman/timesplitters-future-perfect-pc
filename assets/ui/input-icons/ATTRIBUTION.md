# Input prompt artwork

Julio Cacko, **FREE Input Prompts Pack - Icons for PC and Consoles**, texture
release 1.4 (18 October 2024).

- Author/download: https://juliocacko.itch.io/free-input-prompts
- Announcement: https://forums.unrealengine.com/t/1215646
- Asset license: CC0 1.0 Universal, https://creativecommons.org/publicdomain/zero/1.0/
- Selected styles: Keyboard_Mouse/Dark and XGamepad/Default (colored face letters).

CC0 permits copying, modification and redistribution, including commercial use,
without attribution. This voluntary credit preserves the artwork's provenance.
The author's separate MIT code license does not apply to these texture assets.

Modifications: the selected original128px PNG pixels are losslessly encoded
into RGBA runs without reducing their resolution. The runtime uploads native
textures with eight premultiplied mip levels, Morton ordering and linear
filtering. Seven-bit native UI tint/fade, geometry and clipping are retained.
Binding icons use a larger scoped canvas within their existing cells; wide key
faces retain alpha-derived height compensation. No separate pressed artwork is
provided, so idle art is reused. Manifest hashes make the import reproducible.
The author's second `T_3_Key_Dark-1.png` supplies the 4 key. Unsupported keys,
composite key labels and unmapped controller tokens retain native fallbacks.
Controller prompts retain the game's Xbox logical button convention; this
change does not add automatic PlayStation/Switch controller-family detection.

Original downloaded PNGs remain private under `private/input-icons/Source`.
Regenerate the tracked, CC0-derived span data with:

```sh
python tools/graphics/import-input-icons.py --source-dir private/input-icons/Source
```

Game packaging retains this notice as `Licenses/Keyboard-mouse-icons.txt` for
compatibility with existing Windows, Mac and Linux package verifiers; it now
covers keyboard, mouse and controller artwork.
