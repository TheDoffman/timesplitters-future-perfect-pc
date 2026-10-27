/*
 * d3d8_overlay.h -- a line of text drawn over the finished frame.
 *
 * The renderer's own on-screen display: whatever the caller hands it, drawn
 * top-left over the back buffer just before the frame is presented. It knows
 * nothing about what the text says, so what to show and when to show it stays
 * with the caller (src/hle decides; see RECOMP_FPS_OVERLAY there).
 *
 * The text is rendered by GDI into a bitmap and uploaded, rather than drawn
 * from an embedded font, so it is a real font at a real size and there is no
 * glyph table to maintain.
 *
 * What it touches on the device: the render target, the viewport, the input
 * layout and topology, the vertex and pixel shader, one shader-resource slot
 * (8, past the four texture stages a title can use) and one vertex-shader
 * constant slot (7). It puts the render target and viewport back. Everything
 * else in that list is set again by the next draw -- the blend, depth and
 * rasterizer states come from d3d8_states_apply, which runs per draw -- with
 * one exception the caller must handle: d3d8_states_apply_sampler does not
 * re-bind an unchanged sampler, so nothing here binds a sampler at all (the
 * pixel shader reads the text by Load, at integer pixels, which is also what
 * keeps it crisp). No vertex buffer is bound either: the quad comes from
 * SV_VertexID, so a title's stream source survives the frame.
 */
#ifndef XBOXRECOMP_D3D8_OVERLAY_H
#define XBOXRECOMP_D3D8_OVERLAY_H

#ifdef __cplusplus
extern "C" {
#endif

/* Draws one line over the current back buffer. Safe to call every frame with
 * the same text: the bitmap is only re-rendered when the text changes. A
 * failure to create anything it needs disables it for the rest of the run,
 * once, with a line on stderr -- an overlay must never be the reason a title
 * stops drawing. */
void d3d8_overlay_draw(const char *text);

/* Releases everything. Called from the device teardown. */
void d3d8_overlay_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
