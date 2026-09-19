# Native original media — checkpoint 053 (2026-09-11)

Checkpoint 076 supersedes the video source profile below: original MM8 Magicdod/mightdod clips are
included with explicit original-to-MM8/Merge replacement and case-insensitive archive discovery.
There are 438 selected clips and one override; no shadowed duplicate payload is exported. Existing
NPC portrait ownership and 49 music tracks remain. A five-second original ab_druid decoder/player
sample passes with audio queued; it is not complete-loop or physical playback acceptance.
See MM8_BASE_RUNTIME_VALIDATION.md for provenance, tests and outstanding limits.

The native media stage is connected to the app import flow since checkpoint 044. Checkpoint 053 adds
all 49 supplied original MP3s to the shared `engine/music` namespace, preserving numeric merged IDs
and bytes. The same FFmpeg/SDL player decodes all tracks; paused track replacement and resampler-tail
fixes pass native and generated-fixture checks. Actual Mac MM6/MM7 scene music starts, and generic
unsigned iOS Release builds. No iPad operations. See MUSIC_SOURCE_VALIDATION.md for evidence and limits.
Earlier statements below about import integration, missing audio and a requested pause are historical.

# Native original video and NPC presentation (2026-09-10)

`media_import SOURCE NEW_DESTINATION` reads the supplied MMerge installation
directly in C++. The same `openyamm_media_import` library builds for iPadOS.
It needs no desktop transcode tool or extracted Python intermediates. This stage
is not yet wired into the iPad file-picker/import flow. No iPad was updated.

## Source profile and ownership

For each of `mm6`, `mm7`, `mm8` and `mmmerge`, read the named
`Anims/<owner>.magicdod.vid`, `Anims/<owner>.mightdod.vid` and
`Data/<owner>.icons.lod` when present. This is an explicit merged-installation
profile, not a general mod ordering rule or a raw original-MM8 ID migration.
Missing profile archives are reported. Duplicate clip/portrait names across
owners fail until an explicit override profile is supplied.

VID indexes contain a little-endian 32-bit count and 44-byte entries: 40-byte
terminated filename, then a 32-bit absolute payload offset. Offsets determine
payload ends independently of index order. Validate the count, file/name limits,
offset bounds, overlaps and supported signature/extension before publication.
An archive is limited to 1 GiB / 4,096 entries; a clip is limited to 128 MiB.
Malformed present media removes the fresh output. Existing output, source paths
and source symlink escapes are protected. Inputs are never modified or executed.

Original `.smk` / `.bik` payloads remain byte-identical. Put them in the flat
`<package>/videos/` namespace: one clip can serve both a house and an event.
The shared HouseVideoPlayer first searches its existing categorized directories
for OGV/SMK/BIK, then the native flat video namespace for SMK/BIK. A present but
damaged selected file is not bypassed with another format. World videos belong
to `worlds/mm6`, `worlds/mm7`, `worlds/mm8`; merge additions belong to `engine`.

Only `npc` plus exactly four decimal digits selects a portrait member. The native
LOD decoder restores the original palette/image into lowercase BMP resources,
with the same package ownership. This does not import player character faces or
paperdolls. Every output file has mode 0644. `media_conversion.json` records
owners, offsets, sizes and dimensions and retains `runtime_ready=false` and
`playback_validated=false`: extraction alone is not playback acceptance.

## Supplied-installation result

`../local-data/media-resources-001` contains 297 original videos totaling
502,111,632 payload bytes and 1,646 NPC portraits:

| Owner | Videos | NPC portraits |
| --- | ---: | ---: |
| MM6 | 117 | 524 |
| MM7 | 173 | 504 |
| MM8 | 0 | 450 |
| Merge base | 7 | 168 |

The two named MM8 VID archives are absent. Of 392 distinct nonempty house video
references, 268 now resolve; the remaining 124 are listed without substitution
in `media-house-coverage.json`. All 163 distinct nonzero proprietor portrait
references resolve. Do not describe the present MM8 NPC portraits as missing
MM8 character/base art.

All 297 video payloads match independently indexed source ranges byte for byte.
All 7,568,012 RGB pixels in all 1,646 portraits match an independent source LOD /
zlib / palette decode. Evidence: `media-video-source-comparison.json` and
`media-portrait-source-comparison.json` under `../local-data/validation/`.
`gameplay-base-031` combines prior base 030 with this media. Regular files are
required by PhysicsFS; this local assembly uses hard links to converted files,
not symlinks to individual resources. Original game files remain separate.

## Playback and rendered evidence

The bundled static FFmpeg now enables Smacker/Bink demuxing, video, Smacker audio,
Bink DCT/RDFT audio and PCM U8/S16LE decoding alongside existing OGV. Video frames
are decoded on CPU and displayed by the existing bgfx/Metal UI texture path.
This does not establish accelerated Bink video decoding on Apple hardware.

Actual streaming checks found audio extending beyond Smacker's picture timeline.
Using the longer audio duration for every loop froze the final image. Looped
playback now limits queued audio to the declared picture interval and pads a
shorter track with silence; one-shot playback retains the full audio tail.

- `tavrich.smk`: complete 91-frame playback; repeated playback reaches 20 seconds
  across two loop boundaries with a maximum 0.1-second frame interval.
- `elemair.smk`: repeated playback reaches 4.2 seconds, maximum interval 0.1.
- Silent `thfpoor.smk` and MM7 `air guild.smk`: complete playback and final frames
  agree with independent source durations. Stop/reopen also succeeds.
- BIKf `7intro.bik` / BIKi `6intro.bik`: more than 2.5 seconds of advancing video
  and RDFT/DCT audio reach the shared player. Full long cutscenes remain untested.
- Original upstream `thfpoor.ogv` and `elemair.ogv` still play. A regression-only
  remux with a shorter synthesized audio track loops without drifting; it is not
  in the production asset base. Sources are pinned in `media-ogv-fixtures/`.

The formal Mac GameApplication opened house 260 using its existing debug console
command `event 11`. Its Metal window displayed the original tavern animation,
proprietor and resident portraits, Chinese house/name/topic labels and Andover's
letter-response text. Screenshots and precise scope are in
`media-gameplay-001/result.json`, `tavern.png`, `npc-final-state.png` and
`letter-dialog.png`. The window exited normally.

This was not physical navigation from the town entrance. External user input
changed the party/gold between observations, so this round does not claim a
controlled GUI reward or recruitment test. Some single clicks did not activate;
double clicks reached the participant and topic screens. The initial outdoor
view again showed background before the explicit house event. Those existing
input/startup issues, player art, remaining audio, full tasks/combat and iPad
acceptance remain open.

Mac and iPadOS application/library builds pass. There are 557 passing Python
checks and two explicitly skipped generated-OGV checks: installed desktop FFmpeg
lacks libtheora/libvorbis encoders. Three separate OGV runtime checks above do
pass with existing Theora payloads and a native Vorbis audio remux. Video tests
use a dummy SDL audio output; they prove decoding/queuing, not listening quality.

The user requested a pause after this section. Development is paused here;
overall completion remains a low-confidence 35%–40% estimate.
