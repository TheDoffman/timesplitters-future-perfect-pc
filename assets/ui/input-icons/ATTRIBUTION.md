# Keyboard and mouse prompt artwork

**PC Keyboard & Mouse Icon Pack (UI Icons, Input Icons, Keyboard, Mouse, UI Pack)**
by **Rendrify**.

Source: https://www.fab.com/listings/b07743bb-31ad-476b-a942-ae47c3a0f82c

Licensed under **Creative Commons Attribution 4.0 International (CC BY 4.0)**:
https://creativecommons.org/licenses/by/4.0/
Legal code: https://creativecommons.org/licenses/by/4.0/legalcode

The signed-in Fab listing identified this license when the pack was acquired
on 2026-11-04. The creator is not affiliated with or endorsing this port.

Changes: source PNG images exported losslessly from the downloaded Unreal
Texture2D packages, then reduced from 128x128 to 32x32 with Lanczos filtering
and encoded as merged RGBA rectangles for the native UI renderer. Runtime
colour and opacity follow the surrounding game prompt. Enter and Backspace use
the pack's arrow variants. No original key labels have been redrawn. The pack
has no dedicated pressed images, so held keys reuse the same artwork.

The game uses these images for mapped keys and mouse inputs. Escape, Space,
mouse side buttons and other missing keys retain the existing outlined prompts.
Composite key labels also retain the existing renderer. Source artwork stays
local under `private/input-icons/rendrify/png`; the attributed compiled derivative
is `src/interface/pc_input_icon_data.inc`. The manifest records original PNG
SHA-256 values. Regenerate after obtaining the pack and exporting its source PNGs:

```
python tools/graphics/import-input-icons.py --source-dir private/input-icons/rendrify/png
```

The importer's `--source-dir` also supports another local folder containing the
manifest's filenames. Game builds do not require Epic Launcher, Unreal Engine,
Pillow or the original PNG files.
