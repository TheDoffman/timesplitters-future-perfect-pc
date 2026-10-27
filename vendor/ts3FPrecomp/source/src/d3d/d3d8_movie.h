/*
 * d3d8_movie.h -- a decoded movie frame drawn over the finished frame.
 *
 * The Xbox shows some movies on its video overlay, a plane the scan-out puts
 * in front of the frame buffer (D3DDevice_UpdateOverlay). The host has no
 * such plane, so the frame is drawn over the back buffer just before it is
 * presented, filling it at the movie's own aspect with black bars.
 *
 * Same rules as d3d8_overlay.h, which it is modelled on: the render target
 * and viewport are put back, nothing binds a sampler or a vertex buffer, and
 * everything else it sets is set again by the next draw. It uses
 * shader-resource slot 9 and constant slot 7.
 */
#ifndef XBOXRECOMP_D3D8_MOVIE_H
#define XBOXRECOMP_D3D8_MOVIE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Hands over a new frame: BGRA, top row first, width*height*4 bytes. Copied
 * before it returns. */
void d3d8_movie_set_frame(const uint8_t *bgra, uint32_t width, uint32_t height);

/* Draws the last frame handed over across the back buffer. Does nothing if
 * there is none. */
void d3d8_movie_draw(void);

/* Forgets the frame, so d3d8_movie_draw does nothing until the next one. */
void d3d8_movie_clear(void);

void d3d8_movie_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
