# GOG MM8 base integration — checkpoint 076

Checkpoint 077 resolves the missing chest item images described below. See ITEM_ICON_VALIDATION.md
for the910image import, source pixel comparisons, actual Mac chest visuals/pointer transfers/Chinese
inspection and save reload evidence. The new current package is item-icons-077/runtime-001.
The remaining paragraphs preserve checkpoint076's historical results and limits.

The extracted GOG base from checkpoint 075 now composes with the user's existing Merge/Chinese
directory and converts successfully. The new complete conversion is
`../local-data/validation/mm8-runtime-076/runtime-002`; retain runtime 063 as the previous package.
This validates resource integration, not complete MM8 gameplay or physical iPad acceptance.

## Source and import rules

The composed `source-001` contains 566 files totaling 1,907,275,367 logical bytes. It selects the
GOG base first and the existing Merge/Chinese directory second, using case-insensitive path identity.
Twenty-four paths are overridden. Independent APFS clones preserve the two original directories;
the composition receipt records hashes, owners and overrides. All storage is on NVMeSSD.

- Sound import adds original `Data/EnglishD.lod` before the existing five layers. Prefixed
  `mm8.D.lod` may override this original base; the explicit Chinese layer remains last. Other
  cross-world collisions still fail. The output profile is `MMergeOriginalWavZHCNV2`.
- Media import recognizes original `Magicdod.vid`/`mightdod.vid` regardless of filename case while
  preserving actual source spelling in provenance. Duplicate case-equivalent archive names fail.
  Original clips belong to `worlds/mm8/videos`; explicit MM8 or Merge replacements may replace an
  original clip. Other collisions fail, and shadowed payloads are not exported. Original NPC images
  are not duplicated into the already remapped Merge NPC registry. Profile: `MMergeNativeMediaV2`.
- Shared frame import allows known Merge/patch icon layers to override original `icons.lod`,
  independently of directory enumeration order. Unknown layers and competing overlay owners still
  fail. Selected image provenance and shared `engine/icons` ownership are retained.

The first full conversion exposed the original/merged `pc27-43` frame collision and rolled back.
The explicit frame precedence above resolves it; the complete retry loads all 140 supported MM6/MM7
maps. Both limited resource-audit missing lists are now empty: three base archives and eight chest
backgrounds are present. MM8 scenes are still outside this conversion's supported scene set.

## Validation and limits

| Check | Result |
| --- | --- |
| Focused sound, catalog, media and frame tests | 91 passed: 67 + 24; earlier failing tests retained |
| Native tools | Final production rebuild 004 passed; capture tool final rebuild 007 passed |
| Generic unsigned iOS Release | `ios-release-001.log`: BUILD SUCCEEDED |
| WAV output | 8,574 sounds, 129,602,372 bytes; all decode to finite, nonempty stereo PCM |
| Existing shared resources | All 7,886 old WAVs and 118 shared TXT/Lua/YAML files unchanged byte-for-byte |
| Video output | 438 clips, 774,388,004 bytes; one original clip replaced by Merge |
| Original MM8 `ab_druid` | Five-second loop-mode sample: 77 frames and 268,746 queued audio frames |
| Player portrait frames | 4,032 images imported, versus 2,465 previously; 56 pose images still missing |
| Actual Mac Metal chest | Background visible; trap, item/gold transfer and empty reopen logic pass |

The video check uses the production decoder/player with dummy audio and no GPU. It verifies a
five-second playback sample, not a complete loop boundary, all 438 clips, audible output or an MM8
playable scene. Its first run used an incorrect silent-clip expectation; the audio-expecting retry
passes. The WAV pass is decoder/cache validation, not perceptual listening or iPad audio acceptance.
One sound sample reference, ID 6359 (`pc6330b`), and 631 voice lookup IDs remain unresolved. The frame
report still has 156 missing interface-frame images and one missing normal portrait.

**Chest item visuals remain unaccepted.** Both immediate and delayed real captures show an empty
chest background although the active chest contains three entries. Diagnostic 004 confirms a valid
9×9 grid and zero rendered inspectable items. The missing engine images are `cndlabra`/`herb03`,
present in source `mm6.icons.lod`, and `item204`–`item206`, present in original `icons.lod`.
The current import stages do not package these item images. Fix item icon import with explicit
ownership, then check visible contents, pointer/touch selection and Chinese inspection text.
Logical Return-key transfer alone does not establish usable chest UI.

No game stats, item contents, save schema or original source files were changed for the captures.
The capture tool now requests the open-chest screenshot ten frames after opening to avoid first-frame
ambiguity. Temporary tracing was removed. No iPad or simulator UI operations occurred.

Evidence is under `../local-data/validation/mm8-runtime-076/`: `result.json`, `source-composition.json`,
`source-media-audit.json`, `image-overlap-audit.json`, `audio-decode-001.jsonl`,
`audio-and-overlay-verification.json`, `video-original-002.json`, `chest-visual-diagnosis.json`,
test XML/logs, build logs, conversion reports and the unaltered Mac screenshot BMPs. Overall project
estimate remains **40–45%, low confidence**; resource counts are not a full-game completion metric.
