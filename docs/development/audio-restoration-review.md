# Audio asset restoration decision

No replacement asset is selected from the current structural and metric audit.
The original files remain authoritative. This decision does not certify every
recording's perceptual quality; it avoids applying a destructive transform when
the available evidence does not establish an improvement.

## Rechecked mean-offset candidates

The five largest reviewed mean-offset entries represent four distinct original
recordings; the two science-fiction handgun names contain identical encoded
audio. Current archive payload SHA-256 values and production-decoded PCM hashes
match the census. Their existing private WAV previews match the decoded bytes
exactly, with the documented base rates and channels.

A fresh bridge was compiled from the current production decoder and public
measurement bridge. Its output matches the catalogued PCM hashes for all five
entries. An older local DLL no longer matched the original census's DLL hash;
the recheck therefore uses the fresh source build, recording both source and
binary hashes rather than assuming an unchanged generated binary.

| Original asset | Mean in signed16 sample units | Samples outside signed16 after hypothetical mean subtraction |
| --- | ---: | ---: |
| `l14_r110_aug22v.xbs` | 4214.59 | 121 |
| `l14_r110_aug08v.xbs` | 3704.02 | 36 |
| `gun_handgun_scifi_22.xbs` | 2765.60 | 61 |
| `ts3_scifi_handgun22_01b.xbs` | 2765.60 | 61 |
| `l14_r110_aug23v.xbs` | 2750.94 | 8 |

Their approximately100ms means vary within each file, including a sign reversal
in the handgun recording. Original endpoints are near silence: the first/last
sample pairs are0/0,212/0,-30/52,-30/52 and0/1. Subtracting each whole-file mean
increases the step to silence at both endpoints and would require clipping the
out-of-range samples above. The subtraction was evaluated in memory only: no
mean-subtracted PCM or replacement asset was saved. The listening package uses
uniform quarter-gain derivatives of the original previews for audition level;
the original decoded previews and encoded assets remain unchanged.

That rejects a constant-offset repair for these candidates. It does not prove
whether another treatment would improve them. High-pass filtering, declipping,
denoising or synthesis would need an accepted reference and representative
listening, with transient, timbre, duration and loop checks. None is introduced
on a mean, full-scale count or true-peak flag alone.

## Other candidates and coverage

Known-bank decoding found no incomplete blocks, invalid step-index headers or
nonzero reserved headers. The production decoder's shift/add rounding agrees
with the existing LLE path; FFmpeg's small rounding differences are not evidence
for replacing it. Source true peaks precede original volume/headroom/distance
and host gain, so they do not by themselves establish output clipping.

The crane's authored loop is a playback-region issue, repaired in the shared
buffer model/pump. Its measured seam remains a listening candidate; no source
crossfade is applied without evidence that it improves the intended sound.
The source stays unchanged, and the private controlled before/after previews
demonstrate consumption of the authored region rather than asset restoration.

Coverage remains explicit:403 archives and six XMV files are indexed/hashed;
27,193 known language/music payloads are decoded.11,115 known audio copies in
other archives match their banks byte-for-byte. Of50,883 entries in those other
archives,39,768 remain unidentified; they are not all presumed to be audio or
silently excluded from scope. The census retains `complete:false`. Missing
identity/reference/listening evidence is still a gap, not a reason to rewrite
originals. See [the acceptance ledger](audio-overhaul-acceptance.md).
