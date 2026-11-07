# Audio distance falloff audit

The overhaul retains the existing optional weapon-range extension. This audit
found no evidence for another multiplier or an extra fade. It verifies the
original calculation and the parameter change; headphone/speaker balance and
all-weapon listening are separate acceptance work.

## Original path and live parameters

The supported US XBE is hash-guarded before analysis. Original `GetVoiceData`
at407556 calls40692A, which dispatches through412B6C to409AEC. The latter is
the distance-attenuation routine, distinct from the stereo-panning routine at
409E28. `GetVoiceData` supplies listener/source rolloff factors, source minimum
and maximum distance, custom curve/count, calculated distance, mute-at-maximum
flag and a destination attenuation in hundredths of a dB.
A separate bounded32-second startup confirms all22 read-only table observations
select409AEC for distance and409E28 for stereo panning.

The source parameter fields are min/max at38/3C, source rolloff at48, curve
pointer/count at50/54, flags at58 and calculated distance at68. Native voice
data+4 holds the distance component. This component precedes cone, mix-bin,
headroom, channel routing and host master gain; it is not output dBFS.

Read-only snapshots in separate Original and Enhanced Vietnam bot matches
observe the same four-point curve `[0.5, 0.25, 0.125, 0]`, with an implicit
unity value at minimum distance. Both runs use minimum1 for the sampled voices.
Enhanced contains955 observations with maximum150; Original contains none.
The weapon wrapper changes authored50 to150 for its two guarded weapon caller
sites; other effects retain their own ranges. The live runs do not establish
the70/200 weapon classes. Those are covered by original caller inspection,
the existing wrapper contract and native parameter controls.

The curve branch never reads the rolloff-factor argument. The snapshot reader
does not resolve a trustworthy listener-factor chain, so its native comparisons
use an explicit unused control value1 for that branch. It does not guess a
listener factor for inverse-distance comparisons.

## Direct original-code comparison

A disposable32-bit Windows process executes the original409AEC bytes, its
original logarithm/conversion helpers and constants. Capstone validates direct
branches/calls stay within the copied function set. Nine absolute constant
operands are relocated; no attenuation implementation, OS callback or DSP is
stubbed. No private executable bytes enter Git. Local receipts retain the XBE,
function-block, case, helper executable and output hashes; the helper's PE
machine is014C. It writes only its own allocated memory and private reports.

The final6643 cases comprise678 controls and5965 live snapshots. Running each
at24/53/64-bit x87 precision makes19,929 native calls. Controls include inverse
and custom curves, original50/70/200 and extended150/210/600 maxima, minimums,
endpoint/over-range cases, both mute-at-maximum flag values and normalized
positions. Changing the otherwise-unused rolloff factor0/1/2/10 leaves the
custom-curve result unchanged.

For every tested range class and precision, the effective normalized curve is:

| Fraction from minimum to maximum | Distance attenuation |
| --- | ---: |
| 0 | 0 dB |
| 0.125 | -2.49 dB |
| 0.25 | -6.02 dB |
| 0.5 | -12.04 dB |
| 0.75 | -18.06 dB |
| 0.9 | -26.02 dB |
| 1 | Muted |

The original helper can return values below -10000 near the zero endpoint.
Its24/53/64-bit arithmetic differs in1295 raw cases, but every such difference
is removed by the existing -10000 mute clamp: all6643 effective results agree.
An initial probe assertion incorrectly assumed the component itself always
clamped at -10000; removing that assertion exposed the original behavior rather
than changing the game. The shared exact-mute contract already handles it.

Original snapshots agree with native53-bit output in2928/2929 raw cases and
2929/2929 after the mute clamp. Enhanced agrees in3034/3036 raw and3035/3036
effective cases. The remaining effective difference is a non-atomic observation
during settings reuse: record identity changes and maximum150 becomes65 while
the previous distance/level remain visible. The previous sample has that same
level under maximum150, and the next sample agrees with the new calculation.
It is not a synchronized post-commit comparison and does not establish a
playback defect. No distance implementation is rewritten on that evidence.

## Range decision and limits

The extension stretches the same curve over a larger maximum; the sampled
minimum remains1. It therefore intentionally makes a shot louder at a fixed
middle distance. For the50->150 control, attenuation at distance10 changes
from -3.97 to -1.11dB, at25 from -11.69 to -3.37dB, and at50 from muted to
-7.51dB. Source PCM, authored volume, independent buses and master gain are
unchanged. This is a range/mix choice, not recovered source detail.

Keep the independent range toggle and default-OFF enhancement master. Preserve
the50/70/200 ordering and existing3x option; do not add unmeasured per-weapon
factors or another distance fade. Representative listening can still justify
later tuning. There is no HRTF/surround acceptance claim.

Both126-second live matches reach verified first-person Vietnam gameplay and
remain live until intentionally terminated by their owner. Four20-second
process-output captures have no discontinuity/timestamp flags or full-scale
samples. Both runs report zero engine glitches, output failures and refusals.
Enhanced ends with12 starvation episodes; Original increases8->16 during the
run. Counters alone do not establish audibility, and the different matches are
not synchronized event A/B. Do not infer a quality ranking from their RMS or
starvation counts. Source assets, personal saves/settings and other work are
untouched. Physical listening and device/controller acceptance remain open.
