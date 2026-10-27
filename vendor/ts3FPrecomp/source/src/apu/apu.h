/*
 * MCPX APU Audio Emulation - Public API
 *
 * Extracted from xemu (LGPL v2+), adapted for standalone use.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MCPXAPUState MCPXAPUState;

/* Initialize the APU emulation.
 * ram_ptr: pointer to the base of Xbox physical RAM (64MB). Every address the
 * APU is handed -- voice buffers, DSP scatter-gather tables -- is physical,
 * and it reads them as ram_ptr + (addr & 0x03FFFFFF). In this runtime the
 * physical pages are the contiguous window, so pass the host address of guest
 * VA XBOX_CONTIG_BASE (0x80000000), not of VA 0: the low 64 MB of guest VA is
 * the image and heap, a different mapping, and reading it here returned zeros
 * for every page table.
 * Returns the APU state, or NULL on failure. */
MCPXAPUState *mcpx_apu_init_standalone(uint8_t *ram_ptr);

/* Shut down and free the APU state. */
void mcpx_apu_shutdown(MCPXAPUState *d);

/* MMIO read from APU register space (addr is offset from 0xFE800000). */
uint64_t mcpx_apu_mmio_read(MCPXAPUState *d, uint64_t addr, unsigned int size);

/* MMIO write to APU register space (addr is offset from 0xFE800000). */
void mcpx_apu_mmio_write(MCPXAPUState *d, uint64_t addr, uint64_t val, unsigned int size);

/* Nonzero while the APU's interrupt line is asserted (ISTS.GINTSTS). Hand it
 * to the kernel with xbox_SetApuInterruptSource() so the timer thread raises
 * the title's APU ISR (vector 6); nothing else delivers it. */
int mcpx_apu_irq_pending(void);

/* Play a 440Hz test tone through the APU pipeline to verify audio output.
 * Directly programs a voice without going through DirectSound. */
void mcpx_apu_play_test_tone(MCPXAPUState *d);

/* ============================================================
 * Software mixer - DirectSound buffer bridge
 *
 * DirectSound buffers register here. The APU frame thread
 * mixes active buffers directly into the waveOut output,
 * bypassing the VP hardware voice pipeline.
 * ============================================================ */

#define APU_MIXER_MAX_VOICES 64

typedef struct APUMixerVoice {
    volatile int     active;       /* 1 = playing, 0 = stopped */
    volatile int     looping;      /* 1 = loop, 0 = one-shot */
    const int16_t   *pcm_data;     /* Pointer to 16-bit PCM samples */
    uint32_t         pcm_bytes;    /* Total buffer size in bytes */
    uint32_t         num_channels;  /* 1=mono, 2=stereo */
    uint32_t         sample_rate;   /* e.g. 22050, 44100, 48000 */
    uint64_t         play_offset; /* Source frame position, 16 fractional bits */
    float            volume;       /* 0.0 to 1.0 */
} APUMixerVoice;

/* Allocate a mixer voice slot. Returns slot index or -1 if full. */
int apu_mixer_alloc_voice(void);

/* Free a mixer voice slot. */
void apu_mixer_free_voice(int slot);

/* Get pointer to a mixer voice for configuration. */
APUMixerVoice *apu_mixer_get_voice(int slot);

/* Seek in PCM bytes (rounded down to a whole frame). Returns 0 if invalid. */
int apu_mixer_set_position(int slot, uint32_t byte_offset);

/* Snapshot playback state under the mixer lock; output pointers may be NULL. */
void apu_mixer_get_state(int slot, uint32_t *byte_offset, int *active, int *looping);

/* Start/resume or stop playback without changing the current position. */
void apu_mixer_play(int slot, int looping);
void apu_mixer_stop(int slot);


/* ---- APU register aperture, and the fault handler that serves it ----
 *
 * The APU's 512 KB of registers are mapped PAGE_NOACCESS so that a guest
 * access traps, and apu_hook_handle_mmio() decodes the faulting instruction,
 * performs the read or write against the emulated APU, advances RIP past it
 * and reports true. A host program installs a vectored exception handler and
 * calls this for a fault inside the aperture; returning false means the
 * instruction was not one the decoder knows, and the fault should be treated
 * as a real crash.
 *
 * Declared here because it is the boundary between the runtime and a
 * project's own main(): without a declaration the caller gets C89's implicit
 * int and the bool comes back truncated.
 */
#define XBOX_APU_MMIO_BASE  0xFE800000u
#define XBOX_APU_MMIO_SIZE  0x00080000u

/* The emulated APU the fault handler serves from.
 *
 * NULL until the host program stores what mcpx_apu_init_standalone()
 * returns here -- init does not assign it -- and apu_hook_handle_mmio()
 * declines every access while it is NULL -- silently, which is why trapping
 * the aperture without creating the device, or creating it and not storing
 * it, turns each APU access into a crash with no APU log line to explain it.
 */
extern MCPXAPUState *g_apu_state;

#ifdef _WIN32
struct _CONTEXT;
int apu_hook_handle_mmio(struct _CONTEXT *ctx, uintptr_t fault_addr,
                         uint32_t fault_xbox_va, int is_write);

/* Apply a guest write to the AC'97 page, dropping the DSP busy bit.
 *
 * That page is mapped PAGE_READONLY rather than PAGE_NOACCESS: the codec
 * status is read constantly and must stay plain memory, while the DSP command
 * bytes have to be caught at the moment they are written. So only writes
 * fault, and only writes come here. mcpx_offset is measured from the MCPX
 * aperture base, host_addr is where the page actually lives.
 */
int mcpx_ac97_handle_write(struct _CONTEXT *ctx, uintptr_t host_addr,
                           uint32_t mcpx_offset);

/* Apply a guest write to an NV2A interrupt register with the semantics the
 * hardware has: PCRTC_INTR_0 is write-1-to-clear and PMC_INTR_0 bit 24 is a
 * summary of it, not storage. A driver acknowledging a vblank writes the one
 * and spins on the other, so against plain memory it spins for ever.
 *
 * `aperture` is the host address the NV2A register window starts at; the
 * handler needs both registers and only the faulting one's address is known.
 *
 * It lives beside the APU handler because that is where the x86-64 store
 * decoder is, not because it has anything to do with audio.
 */
#define XBOX_NV2A_PCRTC_PAGE  0x00600000u   /* 0xFD600000, one 4 KB page */

int nv2a_intr_handle_write(struct _CONTEXT *ctx, uintptr_t host_addr,
                           uint32_t nv2a_offset, uintptr_t aperture);
#endif

#ifdef __cplusplus
}
#endif
