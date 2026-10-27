/* US 45410066 spirit renderer (sub_0024A1A0). This is a title compatibility
 * correction, not a change to Xbox culling or unwritten-fog semantics.
 * Keep the guest's alpha, depth testing, depth writes and animation intact. */
#include "hle.h"

#define GHOST_WORD(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a) + g_xbox_mem_offset))
#define GHOST_BYTE(a) (*(uint8_t *)((uintptr_t)(uint32_t)(a) + g_xbox_mem_offset))
static RECOMP_MANUAL_TLS unsigned spirit_render_depth;
static RECOMP_MANUAL_TLS int spirit_billboard_active;
static RECOMP_MANUAL_TLS unsigned spirit_billboard_previous;

static void pc_ghost_render(void (*original)(void))
{
    unsigned previous_depth = spirit_render_depth;
    int previous_active = spirit_billboard_active;
    unsigned previous_flags = spirit_billboard_previous;
    ++spirit_render_depth;
    spirit_billboard_active = 0;
    original();
    /* Also unwind the override if the guest exits before ending a batch. */
    if (spirit_billboard_active)
        hle_d3d8_effect_override(spirit_billboard_previous);
    spirit_billboard_active = previous_active;
    spirit_billboard_previous = previous_flags;
    spirit_render_depth = previous_depth;
}

static void pc_ghost_model_node(void (*original)(void))
{
    int rigid = 0;
    unsigned previous = 0;
    if (spirit_render_depth) {
        uint32_t instance = GHOST_WORD(g_esp + 4u);
        uint32_t node = GHOST_WORD(g_esp + 8u);
        uint32_t header = GHOST_WORD(instance + 4u);
        uint32_t count = GHOST_WORD(header);
        /* The model's node table precedes its header. Node types 1/2/3
         * carry skinning data (sub_0018EA90 at 0x18ED01); the skull is rigid,
         * whereas its animated ribbon trails require two-sided rendering. */
        if (count <= 4096u && node < count && header >= count * 0xC8u) {
            uint32_t address = header - count * 0xC8u + node * 0xC8u;
            unsigned type = GHOST_BYTE(address + 0x48u);
            rigid = type != 1u && type != 2u && type != 3u;
        }
    }
    if (rigid)
        previous = hle_d3d8_effect_override(HLE_D3D8_EFFECT_CULL_CW);
    /* The node renderer flushes its primitives before returning. */
    original();
    if (rigid) hle_d3d8_effect_override(previous);
}

static void pc_ghost_billboard_begin(void (*original)(void))
{
    uint32_t caller = GHOST_WORD(g_esp);
    if (spirit_render_depth &&
        (caller == 0x0024A5DCu || caller == 0x0024AA5Bu)) {
        uint32_t bias_argument = g_esp + 24u;
        uint32_t bias = GHOST_WORD(bias_argument);
        /* This effect supplies a legacy 200*256 depth offset. Its vertex
         * program adds that to normalized clip Z before viewport scaling,
         * clipping the entire sprite in the host pipeline. Draw the emissive
         * eyes at their authored positions, with depth testing still enabled.
         * Other billboard callers retain their own bias and fog behavior. */
        GHOST_WORD(bias_argument) = 0;
        original();
        GHOST_WORD(bias_argument) = bias;
        /* Begin flushes the preceding batch; only subsequent eye draws use
         * no fog. The eye program has no fog output, and treating that as
         * full fog replaces its red texture with solid fog-colored squares. */
        if (!spirit_billboard_active) {
            spirit_billboard_previous =
                hle_d3d8_effect_override(HLE_D3D8_EFFECT_NO_FOG);
            spirit_billboard_active = 1;
        }
    } else {
        original();
    }
}

static void pc_ghost_billboard_end(void (*original)(void))
{
    original(); /* Flush the eye batch before restoring fog. */
    if (spirit_render_depth && spirit_billboard_active) {
        hle_d3d8_effect_override(spirit_billboard_previous);
        spirit_billboard_active = 0;
    }
}

#undef GHOST_WORD
#undef GHOST_BYTE
