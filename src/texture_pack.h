#ifndef TSFP_TEXTURE_PACK_H
#define TSFP_TEXTURE_PACK_H
struct D3D8Texture;
struct RhiView;
void tsfp_texture_upload(struct D3D8Texture *tex);
void tsfp_texture_release(struct D3D8Texture *tex);
/* Finish at most one optional replacement upload on the renderer thread. */
void tsfp_texture_frame_complete(void);
unsigned tsfp_texture_binding_revision(void);
struct RhiView *tsfp_texture_view(struct D3D8Texture *tex);
/* Called before draws; implemented beside the renderer's stage bindings. */
void tsfp_texture_sync_bindings(void);
#endif
