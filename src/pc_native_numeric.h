#ifndef TSFP_PC_NATIVE_NUMERIC_H
#define TSFP_PC_NATIVE_NUMERIC_H
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <float.h>
#include <string.h>
extern ptrdiff_t g_xbox_mem_offset;
extern int g_tsfp_native_timing_active;
/* These original integer-to-float reads are continuous motion consumers of
 * the engine delta (animation, particles, camera easing). Native updates use
 * the adjacent fractional delta. Integer arithmetic and counters keep using
 * whole ticks, and the original path retains its exact numeric conversion.
 * Use the live float field: guest slow-motion/temporary pause scopes may
 * change it after the main timing routine has returned. */
static inline double pc_native_numeric(uint32_t address, int32_t value)
{
    if ((address == 0x4E7940u || address == 0x4E7944u) && g_tsfp_native_timing_active)
        return *(const float *)((uintptr_t)(address + 8u) + g_xbox_mem_offset);
    return (double)value;
}
#define RECOMP_I32_TO_F32(address, value) ((float)pc_native_numeric((uint32_t)(address), (int32_t)(value)))
#define RECOMP_I32_TO_F64(address, value) pc_native_numeric((uint32_t)(address), (int32_t)(value))
extern uint32_t pc_native_visual_store(uint32_t site,uint32_t value);
extern float pc_native_visual_sample(uint32_t value);
extern float pc_native_weapon_sway(float original);
/* Observe only the level clock's direct stores, without changing their
 * values. Smooth only the audited mesh/strip distortion absolute-time reads.
 * Discrete counters and every other absolute-time consumer remain original. */
#define RECOMP_STORE_GLOBAL_U32(site,address,value) \
    ((address)==0x7DE318u ? pc_native_visual_store((site),(uint32_t)(value)) : (uint32_t)(value))
#define RECOMP_I32_TO_F32_AT(site,address,value) \
    (((site)==0xF9726u || (site)==0xF9B4Bu) && (address)==0x7DE318u && g_tsfp_native_timing_active ? \
     pc_native_visual_sample((uint32_t)(value)) : RECOMP_I32_TO_F32((address),(value)))
/* US script dispatcher sub_00228870 uses this load only as a pause gate.
 * At native rates a zero whole tick still contains fractional game time.
 * Skipping the dispatcher loses animation events whose previous/current
 * bounds are replaced by the next animation update. Let it consume every
 * advancing interval without changing the whole-tick field used by timers.
 * NPC dispatcher sub_0015EAE0 likewise tests its load against one, then
 * overwrites the register before use. Skipping it discards fractional root
 * motion and slows its elapsed-time counters by 60 / native FPS.
 * Player dispatcher sub_000D5EB0 also overwrites its pause-gate register
 * immediately after the test; its movement consumers use fractional time.
 * Camera translation sub_000C73C0 discards its gate register before use;
 * direction easing sub_000C7610 only tests it, then overwrites it on the
 * direct-copy path. Their motion and easing must consume each live interval.
 * Camera effects have an outer gate at 0xCEA8D and a whole-tick decay loop
 * bounded by 0xC7D73 / 0xC7E0C. In native mode run that loop once per live
 * interval, with its two decay factors raised to elapsed ticks below.
 * Camera tracks gate evaluation at 0xCD70F, after advancing their float clock.
 * Their keyframe span at 0xCD851 needs an adjacent sample even when the
 * integer delta is zero; the existing fractional weight then selects the
 * pose at that clock time. Keep multi-tick spans and cut detection intact.
 * Entity post-update at 0xA75E7 divides position change by the float delta,
 * but its integer guard otherwise replaces velocity with zero on fractional
 * frames. Use the live game interval for that guard; its register is replaced
 * before subsequent use, and paused intervals still avoid the division.
 * Weapon sound helper 0x1B6FF0 must accept an event on any live interval;
 * its repeating-sound timer already integrates the fractional game delta.
 * Its enclosing firing/state-machine gates remain separate audit sites.
 * Mothership updater 0x217050 integrates fractional forces, displacement and
 * crash timers after the gate at 0x217059. Skipping fractional intervals
 * loses motion time while its two whole-tick damping loops still decay at
 * 60 Hz, changing the landing position. The gate register is overwritten at
 * 0x217090. Keep those separate damping-loop loads unchanged.
 * Ship debris initialization divides a position difference by live time at
 * 0x216972. Its guard at 0x216958 must also accept fractional intervals;
 * otherwise the inherited velocity is scaled down by the frame's fraction.
 * EAX is replaced at 0x216987. The spawn loop/count and RNG stay unchanged.
 * Projectile updater 0xF4A50 computes displacement from the fractional
 * delta, but its gate at 0xF4A66 discarded zero-whole-tick intervals. Its
 * age advances independently in 0xDC7B0, so this shortened shot range.
 * EAX only receives flag bytes before replacement at 0xF4B18; collision
 * and expiry now see every advancing interval.
 * Damage application at 0xBB6C0 has a pause-only gate as well. One-shot
 * hits (including the sniper) can arrive on fractional frames and retire
 * immediately, so rejecting them loses damage permanently. EAX is replaced
 * at 0xBB6D2 before use. Keep the hit amount and the separate continuous-
 * damage scaling unchanged; zero live time still rejects application.
 * NPC aim at 0x2B93DA gates both linear recovery and fixed-factor tracking.
 * Advance both on live intervals; the tracking coefficient and RNG decision
 * cadence are handled separately below. ECX is replaced before arithmetic.
 * HUD message update 0x14A5A0 ages delayed notices and retires them using
 * fractional game seconds. Its gate at 0x14A5CD otherwise discards time on
 * zero-whole-tick frames, delaying objectives, pickups and subtitles. EAX is
 * replaced by the queue index at 0x14A670; no counter consumes this gate value.
 * All other loads, including other reads of this same address, stay exact. */
static inline uint32_t pc_native_global_load(uint32_t site, uint32_t address,
                                           uint32_t value)
{
    /* The rigid-body clock divides its elapsed interval across this many
     * solver steps. A carried whole count can be zero while time advances.
     * Split long intervals as well, so each native substep is <= one tick.
     * The denominator uses this same integer register, not a float hook. */
    if ((site==0x171FB9u || site==0x17208Eu || site==0x172119u) &&
        address==0x4E7944u && g_tsfp_native_timing_active) {
        float delta=*(const float *)((uintptr_t)0x4E794Cu+g_xbox_mem_offset);
        if (!(delta>0.f) || !isfinite(delta)) return 0;
        if (delta<2147483648.f) return (uint32_t)ceilf(delta);
        return value;
    }
    /* Animated surfaces cache their procedural-texture dispatch per view.
     * A zero-whole native frame can arrive on a new console vblank and cache
     * a no-op; the following whole tick then sees that same vblank and loses
     * its update. Key both comparison and store by the native update id.
     * Shared-resource/view deduplication and the callee's discrete tick gate
     * stay original here. Fractional simulation needs its own integration
     * changes, not simply more executions of the authored GPU passes. */
    if ((site==0x12764Cu || site==0x127694u) && address==0x7497A8u &&
        g_tsfp_native_timing_active)
        return *(const uint32_t *)((uintptr_t)0x7497B8u+g_xbox_mem_offset);
    /* Rocking still builds its transform while paused, but must not integrate
     * a stale whole count. Its settling branch is guarded separately below.
     * The procedural-texture dispatcher likewise must not evolve its image
     * during a zero-float pause; its gate register is replaced by a render-
     * state return value before use. Positive intervals stay original. */
    if ((site == 0xA8595u || site == 0xA8980u || site == 0x1956A0u) && address == 0x4E7944u &&
        g_tsfp_native_timing_active &&
        *(const float *)((uintptr_t)0x4E794Cu + g_xbox_mem_offset) == 0.0f)
        return 0;
    if (site == 0xCD851u && address == 0x4E7944u && value == 0 &&
        g_tsfp_native_timing_active &&
        *(const float *)((uintptr_t)0x4E794Cu + g_xbox_mem_offset) > 0)
        return 1;
    /* Driver dispatch must replenish throttle/brake inputs on every vehicle
     * update: 17E680 consumes and clears them even on fractional frames.
     * 17DA03 is only an entry gate; its EAX is overwritten before use.
     * Contact gate 166899 likewise only guards converting a character's
     * displacement to velocity. Its temporary EDI is restored immediately;
     * the conversion already divides by the live fractional game interval.
     * Spawn gate 172AB5 similarly guards joint-displacement inheritance:
     * skipping a fractional spawn permanently loses its initial motion.
     * EAX is overwritten, and 172B01 already divides by live game time. */
    if ((site == 0x228880u || site == 0x15EB0Cu || site == 0xD5EBFu ||
         site == 0xC73C0u || site == 0xC7610u || site == 0xCEA8Du ||
         site == 0xC7D73u || site == 0xC7E0Cu || site == 0xCD70Fu ||
         site == 0xA75E7u || site == 0x1B6FF0u || site == 0x217059u ||
         site == 0x216958u || site == 0xF4A66u || site == 0xBB6C0u ||
         site == 0x2B93DAu || site == 0x14A5CDu || site == 0x17E6C3u ||
         site == 0x17DA03u || site == 0x166899u || site == 0x172AB5u) &&
        address == 0x4E7944u &&
        g_tsfp_native_timing_active)
        return *(const float *)((uintptr_t)0x4E794Cu + g_xbox_mem_offset) > 0;
    return value;
}
/* At A8526, a taken branch bypasses callback retirement and reaches the
 * tick loop/transform builders. Preserve the original float comparison for
 * every other branch and every advancing interval, including fractions. */
static inline int pc_native_float_branch(uint32_t site, int taken)
{
    if (site == 0xA8526u && g_tsfp_native_timing_active &&
        *(const float *)((uintptr_t)0x4E794Cu + g_xbox_mem_offset) == 0.0f)
        return 1;
    return taken;
}
extern int pc_native_rocking_branch(uint32_t site,int taken);
#define RECOMP_FLOAT_BRANCH_AT(site, taken) \
    (((site)==0xA8526u || (site)==0xA86C0u) ? \
    pc_native_rocking_branch((uint32_t)(site),pc_native_float_branch((uint32_t)(site),(taken))) : (taken))
extern uint32_t pc_native_rocking_gate(uint32_t site,uint32_t value);
extern float pc_native_rocking_add(uint32_t site,float left,float right);
extern float pc_native_rocking_sub(float left,float right);
/* Arcade's match-start countdown adds raw float time, then truncates back
 * to an integer at 0x69051 on every update. A sub-tick interval therefore
 * never advances it. Consume the existing accumulated whole raw ticks at
 * this one integer counter; keep motion/animation additions fractional. */
static inline float pc_native_countdown_add(float left,float right)
{
    if(g_tsfp_native_timing_active && right>0.f)
        right=(float)*(const uint32_t *)((uintptr_t)0x4E7940u+g_xbox_mem_offset);
    return left+right;
}
/* The walking integrator adds the hit vector once before its 0.9 decay.
 * Integrate that same geometric sequence over fractional ticks as well as
 * normalizing the decay below. Scaling decay alone would increase total
 * knockback with refresh rate. Preserve each original one-tick addition. */
static inline float pc_native_hit_add(float left,float right)
{
    if(g_tsfp_native_timing_active) {
        float delta=*(const float *)((uintptr_t)0x4E794Cu+g_xbox_mem_offset);
        if(delta<=0.f)return right;
        if(delta!=1.f) {
            float decay=*(const float *)((uintptr_t)0x476040u+g_xbox_mem_offset);
            left*=-expm1f(delta*logf(decay))/(1.f-decay);
        }
    }
    return left+right;
}
/* Region-driven player motion adds force before damping. Splitting only the
 * damping would also change the accumulated force. Preserve the original
 * affine step v' = Q*(v+a) when dividing one raw interval. Q is q raised
 * to the original ceil(game-speed) drag count, including fast/slow game time.
 * Below the authored drag threshold the original is linear. */
/* Multiplication and division can put an exact integer speed one float ULP
 * above that integer. Do not let frame-time jitter add an entire drag step. */
static inline float pc_native_force_drag_count(float raw,float game)
{
    return fmaxf(1.f,ceilf(nextafterf(game/raw,0.f)));
}
static inline float pc_native_force_add(float force,float velocity)
{
    if(g_tsfp_native_timing_active) {
        float raw=*(const float *)((uintptr_t)0x4E7948u+g_xbox_mem_offset);
        float game=*(const float *)((uintptr_t)0x4E794Cu+g_xbox_mem_offset);
        float threshold=*(const float *)((uintptr_t)0x47D6A8u+g_xbox_mem_offset);
        if(raw>0.f && raw<1.f && game>0.f && isfinite(game/raw) && fabsf(force+velocity)>threshold) {
            float q=*(const float *)((uintptr_t)0x47D56Cu+g_xbox_mem_offset);
            if(q>0.f && q<1.f) {
                q=powf(q,pc_native_force_drag_count(raw,game));
                force*=powf(q,1.f-raw)*(-expm1f(raw*logf(q)))/((1.f-q)*raw);
            }
        }
    }
    return force+velocity;
}
/* 171560 sums eight secondary steps of primary/8. A rounded sum just below
 * primary otherwise takes a ninth step (12.5% extra time). Snap only within
 * the float addition error bound at the eighth boundary; Original 60 and
 * genuine intermediate sums retain their exact arithmetic. */
static inline float pc_native_physics_elapsed(float elapsed,float step)
{
    float sum=elapsed+step,full=step*8.f;
    if(g_tsfp_native_timing_active && step>0.f && isfinite(full) &&
       fabsf(sum-full)<=full*(4.f*FLT_EPSILON))return full;
    return sum;
}
#define RECOMP_F32_ADD_AT(site,left,right) \
    (((site)==0x259352u || (site)==0x259368u || (site)==0x25937Fu) ? \
    pc_native_hit_add((float)(left),(float)(right)) : \
    (site)==0xD4853u ? pc_native_force_add((float)(left),(float)(right)) : \
    (site)==0x69043u ? pc_native_countdown_add((float)(left),(float)(right)) : \
    (site)==0x17164Fu ? pc_native_physics_elapsed((float)(left),(float)(right)) : \
    ((site)==0xA85F2u || (site)==0xA863Du || (site)==0xA8651u) ? \
    pc_native_rocking_add((uint32_t)(site),(float)(left),(float)(right)) : ((left)+(right)))
#define RECOMP_F32_SUB_AT(site,left,right) \
    ((site)==0xA8645u ? pc_native_rocking_sub((float)(left),(float)(right)) : ((left)-(right)))
extern uint32_t pc_native_kind35_gate(uint32_t value);
#define RECOMP_LOAD_GLOBAL_U32(site, address, value) \
    ((site)==0xF08D0u ? pc_native_kind35_gate((uint32_t)(value)) : \
    ((site)==0xA8595u || (site)==0xA8980u) ? \
    pc_native_rocking_gate((uint32_t)(site),pc_native_global_load((uint32_t)(site),(uint32_t)(address),(uint32_t)(value))) : \
    pc_native_global_load((uint32_t)(site), (uint32_t)(address), (uint32_t)(value)))
/* Player+0x1008 is halved by the US update at 0xD5F60. Its hit-response
 * vector (+0xF4/+0xF8/+0xFC) likewise decays by 0.9 after each world step.
 * Once the update runs every fractional frame, retain these decay rates in game time.
 * Do not rewrite the shared 0.5 constant or unrelated multiplications. */
static inline float pc_native_multiply(uint32_t site, float left, float right)
{
    /* The old physics clock averages fifteen rendered-frame deltas, but only
     * runs on whole ticks. Native simulation consumes this frame's elapsed
     * time directly; the three count loads above partition that exact span. */
    if (site==0x172099u && g_tsfp_native_timing_active)
        right=*(const float *)((uintptr_t)0x4E794Cu+g_xbox_mem_offset);
    /* Airborne player paths relax the view-height offset by 0.8 per original
     * update, including at slow game speed. Split that factor in raw time;
     * using game time would change the original slow-motion response. The
     * caller normally skips paused updates; preserve the offset if reached
     * with zero game time too. Whole/catch-up updates retain their one factor. */
    if ((site == 0xD41D0u || site == 0xD46B4u || site == 0xD498Fu) && g_tsfp_native_timing_active) {
        float raw=*(const float *)((uintptr_t)0x4E7948u+g_xbox_mem_offset);
        float game=*(const float *)((uintptr_t)0x4E794Cu+g_xbox_mem_offset);
        if (game<=0.f) return left;
        if (raw>0.f && raw<1.f) right=powf(right,raw);
    }
    /* Original 60-Hz force mode applies ceil(game-speed) drag iterations.
     * Spread that exponent over the actual fractional loop count. The first
     * clamp leaves room for the remaining multiplies to reach the original
     * post-drag speed limit; later iterations can only reduce magnitude. */
    if (site == 0xD48D8u && g_tsfp_native_timing_active) {
        float raw=*(const float *)((uintptr_t)0x4E7948u+g_xbox_mem_offset);
        float game=*(const float *)((uintptr_t)0x4E794Cu+g_xbox_mem_offset);
        if(raw>0.f && raw<1.f && game>0.f && isfinite(game/raw)) {
            float ticks=pc_native_force_drag_count(raw,game),loops=ceilf(game);
            float exponent=ticks*raw/loops;
            float limit=powf(right,ticks-exponent*(loops-1.f));
            float low=*(const float *)((uintptr_t)0x47D6A4u+g_xbox_mem_offset)*limit;
            float high=*(const float *)((uintptr_t)0x475CC8u+g_xbox_mem_offset)*limit;
            return fminf(high,fmaxf(low,left*powf(right,exponent)));
        }
    }
    /* The player aim/weapon-offset easing already multiplies the error by
     * game delta before these six coefficient multiplies. Linear subdivision
     * changes its settling rate. Match the original 60-Hz response at the
     * current game speed: scale the authored weight to one raw tick, then
     * split its remaining-error factor over this fractional raw interval.
     * Keep whole/catch-up updates and the original snap branches unchanged. */
    if ((site == 0x25ABCEu || site == 0x25AC1Eu || site == 0x25AC80u ||
         site == 0x25ACCAu || site == 0x25B13Cu || site == 0x25B16Du) &&
        g_tsfp_native_timing_active) {
        float raw=*(const float *)((uintptr_t)0x4E7948u+g_xbox_mem_offset);
        float game=*(const float *)((uintptr_t)0x4E794Cu+g_xbox_mem_offset);
        if (raw>0.f && raw<1.f && game>0.f) {
            float weight=right*(game/raw);
            if (weight>0.f && weight<1.f)
                right=-expm1f(raw*log1pf(-weight))/game;
        }
    }
    if ((site == 0xD5F60u || site == 0xD64A4u ||
         site == 0xD64BDu || site == 0xD64D6u) && g_tsfp_native_timing_active) {
        float delta=*(const float *)((uintptr_t)0x4E794Cu + g_xbox_mem_offset);
        if (delta != 1.0f) right=powf(right,delta);
    }
    /* Here the left operand is the decay factor, unlike player damping.
     * The original camera-kick setter supplies 0.8 for both channels. */
    if ((site == 0xC7DA8u || site == 0xC7DC5u) && g_tsfp_native_timing_active) {
        float delta=*(const float *)((uintptr_t)0x4E794Cu + g_xbox_mem_offset);
        if (delta != 1.0f) left=powf(left,delta);
    }
    return left*right;
}
/* Smooth only the final visual sway value passed to weapon updates. Keep the
 * integer sine crossing and its random-amplitude event unchanged. */
#define RECOMP_F32_MUL_AT(site, left, right) \
    ((site)==0x1C4539u && g_tsfp_native_timing_active ? \
     pc_native_weapon_sway((float)(left))*(float)(right) : \
     pc_native_multiply((uint32_t)(site), (float)(left), (float)(right)))
/* Scripted camera direction closes 1 / remaining_ticks of the angular gap
 * per original update. Normalize that decay to the live game interval,
 * preserving the original coefficient for one tick. Scaling the pause gate
 * alone would make this easing run native_fps / 60 times too quickly.
 * The original caller supplies a positive integer divisor here; its zero
 * and negative cases copy the target directly without reaching this divide. */
static inline float pc_native_divide(uint32_t site, float left, float right)
{
    /* Entity post-update A7360 already normalizes +F8/+FC/+100 to movement
     * per authored tick. The vehicle's actor-contact callback converts that
     * velocity to units/second; dividing by the event interval again made
     * carried-tick contacts stronger than intervening fractional contacts.
     * Preserve callback cadence and the original-mode conversion. */
    if (site == 0x17CBD6u && g_tsfp_native_timing_active)
        return left;
    /* Track blending uses an integer keyframe span. The original float delta
     * matched it; fractional catch-up no longer does. Normalize by the span
     * actually selected at 0xCD851, retaining the original multi-tick path. */
    if (site == 0xCD88Eu && g_tsfp_native_timing_active) {
        uint32_t span=*(const uint32_t *)((uintptr_t)0x4E7944u + g_xbox_mem_offset);
        right=span ? (float)span : 1.0f;
    }
    float value=left/right;
    if (site == 0xC7651u && g_tsfp_native_timing_active) {
        float delta=*(const float *)((uintptr_t)0x4E794Cu + g_xbox_mem_offset);
        if (delta == 1.0f) return value;
        if (delta <= 0.0f) return 0.0f;
        if (value >= 1.0f) return 1.0f;
        if (value > 0.0f)
            return -expm1f(delta*log1pf(-value));
    }
    return value;
}
extern float pc_native_kind35_turn(float value);
#define RECOMP_F32_DIV_AT(site, left, right) \
    ((site)==0xE237Au ? pc_native_kind35_turn((float)(left)/(float)(right)) : \
    pc_native_divide((uint32_t)(site), (float)(left), (float)(right)))
/* The original NPC aim tracker closes 5% of its gap per whole update.
 * Normalize only this argument, leaving the shared interpolation helper and
 * every other immediate unchanged. Its chance decision still runs only on
 * carried whole ticks; skipped decisions do not advance the shared RNG. */
static inline uint32_t pc_native_push_imm(uint32_t site,uint32_t value)
{
    /* Vehicle adjustment: one authored step per original live update, capped
     * at one even for catch-up/fast game speed; slow motion uses game time. */
    if (site==0x17E6ECu && g_tsfp_native_timing_active) {
        float raw=*(const float *)((uintptr_t)0x4E7948u+g_xbox_mem_offset);
        float game=*(const float *)((uintptr_t)0x4E794Cu+g_xbox_mem_offset);
        float factor;memcpy(&factor,&value,sizeof factor);
        factor*=raw>0.f && game>0.f ? fminf(1.f,fminf(raw,game)) : 0.f;
        memcpy(&value,&factor,sizeof value);
    }
    if (site==0x2B963Fu && g_tsfp_native_timing_active) {
        float delta=*(const float *)((uintptr_t)0x4E794Cu+g_xbox_mem_offset);
        if (delta!=1.0f) {
            float factor;memcpy(&factor,&value,sizeof factor);
            factor=delta>0 ? -expm1f(delta*log1pf(-factor)) : 0;
            memcpy(&value,&factor,sizeof value);
        }
    }
    return value;
}
extern void pc_native_rng_interval_skip(void);
static inline int pc_native_skip_aim_random(void)
{
    return g_tsfp_native_timing_active &&
        *(const uint32_t *)((uintptr_t)0x4E7944u+g_xbox_mem_offset)==0 &&
        *(const float *)((uintptr_t)0x4E794Cu+g_xbox_mem_offset)>0;
}
/* Test the constant site outside the helper so other calls remain direct,
 * including on compilers that decline to inline function-pointer helpers. */
#define RECOMP_PUSH_IMM_U32_AT(site,value) \
    (((site)==0x2B963Fu || (site)==0x17E6ECu) ? pc_native_push_imm((site),(uint32_t)(value)) : (uint32_t)(value))
/* Vehicle rest (176673 MOV) and rigid-body sleep (171F83 memory DEC) still
 * decrement integer bytes. Compensate only their load on intervals without
 * an authored tick; the byte DEC then retains the stored count. Its actual
 * stored result feeds the generated flags (rigid-body sleep consumes ZF).
 * At fast game speed the original updater runs at raw 60-Hz cadence; at
 * slow speed it runs on game ticks. Do not intersect the two carried clocks,
 * whose phases can diverge across pauses. Existing expired values stay due. */
static inline uint32_t pc_native_load_u8(uint32_t site,uint32_t value)
{
    if ((site==0x176673u || site==0x171F83u) && g_tsfp_native_timing_active) {
        float raw=*(const float *)((uintptr_t)0x4E7948u+g_xbox_mem_offset);
        float game=*(const float *)((uintptr_t)0x4E794Cu+g_xbox_mem_offset);
        uint32_t address=game<raw ? 0x4E7944u : 0x4E7940u;
        if (!(game>0.f) || !*(const uint32_t *)((uintptr_t)address+g_xbox_mem_offset))
            return (value+1u)&255u;
    }
    return value;
}
#define RECOMP_LOAD_U8_AT(site,value) \
    (((site)==0x176673u || (site)==0x171F83u) ? \
     pc_native_load_u8((site),(uint32_t)(value)) : (uint32_t)(value))
extern void pc_native_kind35_rng(void);
extern void pc_native_kind35_target(void);
extern void pc_native_physics_solver(void),pc_native_physics_random(void);
#define PC_NATIVE_PHYSICS_CALLEE(site,function) \
    ((site)==0x172114u ? pc_native_physics_solver : \
    ((site)==0x1715A0u || (site)==0x170E51u) ? pc_native_physics_random : (function))
#define RECOMP_DIRECT_CALLEE_AT(site,function) \
    ((site)==0xF0CA7u ? pc_native_kind35_target : \
    ((site)==0xF1741u || (site)==0xF1770u || (site)==0xF17A0u || \
     (site)==0xF17F0u || (site)==0xF181Fu || (site)==0xF184Fu || \
     (site)==0xF189Fu || (site)==0xF18CEu || (site)==0xF18FEu) ? pc_native_kind35_rng : \
    ((site)==0x2B9478u && pc_native_skip_aim_random()) ? pc_native_rng_interval_skip : \
    PC_NATIVE_PHYSICS_CALLEE(site,function))
#endif
