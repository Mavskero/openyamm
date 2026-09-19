# Original sound payloads — checkpoint 054 (2026-09-11)

All work is on Mac with a generic unsigned iOS build. No iPad was queried, connected, installed,
launched or operated. SDL dummy output verifies decoding and software mixing, not physical audio,
listening quality, accessory routing or interruptions.

## Source and native import

The complete source LOD index audit found 8,732 WAV records: MM6 1,526, MM7 2,518, MM8 2,562,
merge additions 1,280, and 846 Simplified Chinese overlays. These resolve to 7,886 unique filenames.
There are no overlapping WAV names between the four base archives. Every language overlay replaces
the corresponding MM6 member. Independent wrapper/zlib/RIFF inspection succeeds for all records.

The existing `sound_import` stage now unwraps original WAVs from the explicit archive profile:
`Data/mm6.D.lod`, `mm7.D.lod`, `mm8.D.lod`, `mmmerge.D.lod`, then
`10 LocZHCN.EnglishD.lod`. Unexpected base-name collisions fail; the named Chinese archive is the only
declared overlay. This is not a generalized ordering rule for arbitrary mods. Missing archives remain
explicit in `sound_payloads.json`. No sound ID is reassigned and no absent sound is substituted.

All original WAVs stay byte-identical under `engine/audio/<original lowercase filename>.wav`, mode 0644.
The merged catalog and party voices are global, so samples share engine ownership instead of following
the currently visited world. The selected output totals 119,257,264 bytes. The original source files
are only read; generated regression sounds are kept outside production packages.

`visitLodTextResources` reads each archive index once and visits unwrapped payloads individually, reusing
the same wrapper validation as `readLodText`. The sound stage checks RIFF/chunk boundaries, unique
format/data/fact chunks, IMA ADPCM format 17, mono/stereo block geometry and source sample metadata.
Per-payload size is limited to 16 MiB, inspected payloads to 512 MiB and profile entries to 20,000.
Invalid WAVs, corrupt wrappers, source escapes and ambiguous base ownership remove the entire fresh
sound-stage output, including its catalog. Existing output protection remains in the outer converter.
These structural checks do not set playback acceptance flags to true.

## Shared decoder fix and actual audio checks

The first full decode stopped at `6clickmovingselector.wav`: SDL reported a successful WAV load with
zero audio bytes. Its valid 416-byte data chunk is shorter than the declared 512-byte ADPCM block.
SDL's default `dropblock` behavior drops the complete sample frames within that partial block.

The shared engine audio initialization now selects SDL's documented `dropframe` policy, preserving
complete samples in partial final blocks. The underlying WAV bytes and codec stay unchanged.
The fixed sample produces 1,796 stereo frames at 48 kHz, matching independent FFmpeg decoding.
Four generated regressions cover mono/stereo files with a partial block alone and following a full
block; expected output frame counts and nonzero PCM pass.

`sound_playback_check` runs the actual engine WAV loader, SDL IMA decoder, sample-rate conversion and
decode cache over every WAV in the final ZIP package. All 7,886 files decode to finite nonzero PCM,
totaling 509,753,301 stereo frames. Each clip is released before the next; the tool does not retain
the entire decoded sound library in memory. This is complete file decoding, not a complete gameplay
trigger test or listening acceptance.

The shared game audio system also resolves and mixes these diagnostic requests to SDL postmix:

- Sound IDs 42186 (`7rungrass.wav`) and 42602 (`6clickmovingselector.wav`). The first is an explicit
  ID check; it does not change normal walking rules to use that sound in other worlds.
- Chinese-overlay dialogue ID 31201 (`01_01.wav`), verified against the selected source bytes.
- Existing character IDs 1, 32 and 50 use their source default voice, the actual `Yes` speech reaction
  and deterministic seed 1. They select 5083/`pc5039b.wav`, 41166/`ef242b.wav` and
  40357/`hm142a.wav`. These three requests cover MM8/MM7 source voice payloads, not every voice set.

These are explicit diagnostic character/event requests. They do not modify player saves and do not
claim physical UI input, every game action's speech hook, spatial sound, voice arbitration or complete
dialogue playback. The additional ID 42608/`6error.wav` also passes in `sound-ids-001`.

## Runtime, regression and remaining work

Fresh `runtime-001` passes the existing 140-map load checks. Its four ZIPs have 27,529 CRC-checked
entries. All 19,643 checkpoint-053 payloads remain unchanged, with exactly the 7,886 WAVs added and
their bytes independently matched to the selected source members. Thirty-two importer/catalog
checks and four partial-block playback regressions pass. Generic unsigned iOS Release builds after
the shared decoder fix; no device installation occurs.

The Mac formal GameApplication also reopens the original checkpoint-048 normal MM6 save, renders
181 Metal frames, starts map music 37 and saves to a fresh diagnostic output. This confirms the new
package/shared decoder setting works with that application path; it does not exercise physical input
or every sound trigger. Evidence: `mm6-application-001.log` and its two captured frames/new save.

Evidence is under `../local-data/validation/sound-payloads-054`: `source-wav-audit.json`,
`sound-source-comparison.json`, `package-validation.json`, `all-samples-final.jsonl`,
`sound-ids-final.*`, `speech-001.*`, `localized-dialogue.*`, both test logs and
`ios-release-final.log`. The initial `all-samples` records the zero-output failure; the
`all-samples-dropframe` run verified the SDL setting before it was added to production initialization.

The preserved catalog has 723 nonzero IDs without corresponding supplied samples, including common
MM8 UI/footstep/effect names. Imported prefixed MM6/MM7 sounds are not silently mapped to those IDs.
Full gameplay sound dispatch, map-specific original script overrides, spell/combat and all speech
variants, positional mixing, music/voice coexistence and long-session cache behavior remain pending.
Missing resources, full campaigns/travel, import recovery/storage and physical-device audio/input
acceptance also remain open. Overall completion remains a low-confidence 40%–45% estimate.
