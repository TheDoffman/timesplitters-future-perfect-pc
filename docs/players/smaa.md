# SMAA anti-aliasing

Open **Game settings → Graphics & resolution → Quality & performance**, choose
**Post-process anti-aliasing → SMAA**, save, and restart the game with a build containing this
feature. It defaults to Off; disabling it restores the existing display resolve.

You can also use **Settings → Video Options → Graphics → Post-process anti-aliasing** in the game,
including **Options → Video Options** while paused. Choose **Off**, **FXAA**, or **SMAA**, then
select **Apply**. This switches SMAA live and saves the same preference used by
the launcher. **Back** discards unapplied edits. If you also change resolution,
anti-aliasing waits until you confirm **Keep changes**. Only one post-process
method runs at a time. MSAA remains a separate choice under **Advanced**, where
you can also find shader caching and its clear action. Console render scale is
in the launcher's Advanced section and appears only at Original resolution.

SMAA 1x High smooths jagged edges in the completed picture. It works with Classic
and Enhanced textures and can be combined with console supersampling. It also
processes the HUD and menus, so compare text and fine details when choosing your
settings. It does not use previous frames or change game speed.

The implementation uses the shared Direct3D 11/Vulkan renderer. Legacy macOS
OpenGL does not apply this option, but retains the saved choice. An older game
build also cannot apply it. Native Mac visual acceptance remains pending.

SMAA adds three GPU passes and output-sized temporary render targets; performance
depends on the resolution and GPU. If shaders or resources cannot be created,
the game logs a SMAA fallback and uses the existing picture. Developers may
override the saved runtime `smaa = 0/1` setting at startup with `TSFP_SMAA=0/1`.
An explicit in-game Apply takes precedence for the running session.
