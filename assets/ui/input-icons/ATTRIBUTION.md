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

Modifications: selected 128px PNGs are downsampled with Pillow LANCZOS to32px,
then losslessly encoded into RGBA rectangle spans. Alpha bounds retain wide key face height within the measured label width.
Native text measurement,
gray tint and opacity are retained. Images have no separate pressed artwork;
idle art is reused. Manifest source hashes make the import reproducible.
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
