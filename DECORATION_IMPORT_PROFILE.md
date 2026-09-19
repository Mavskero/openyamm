# Decoration and global sprite-frame import

Checkpoint: 2026-09-09. Native C++ `decoration_import SOURCE NEW_DESTINATION`
reads the user's imported originals. The library also compiles into the iPadOS
target; launcher orchestration and device gameplay validation remain pending.

## Registry and animation ownership

- `Data/Tables/DecList.txt` supplies all 673 global decoration records, including
  slot zero. The runtime TXT loader uses one-based row ordinals, so exported row
  number is source ID + 1; lookup still returns original IDs 0–672.
- Preserve internal names, hints, dimensions, light colors, all eleven flags and
  original global SFT references. No guessed map aliases are added. Hints are
  presently the supplied English values; Chinese hint integration is unvalidated.
- The original `DecListItem` in `Scripts/Structs/01 common structs.lua` declares
  `SoundId` as signed i2. Thirteen positive TXT values exceed 32767. Export their
  explicit signed storage representation, retain the raw TXT value in an additional
  `SourceSoundId` column and preserve the original input bytes. This does not
  implement or validate ambient sound playback or the original executable's use
  of those high-bit values.
- `Data/Tables/SFT.txt` supplies all 23,390 frames in 5,435 groups. Their indices,
  durations, fixed-point scales, light radii, palettes and flags remain intact.
  Seventeen unnamed groups remain at their original indices; blank-name lookup
  uses the last original group as `DataTables.UpdateSFTGroups` does. Fifty empty
  GroupStart cells are false, as defined by the original table parser.
- The shared native SFT converter also serves the existing actor presentation
  import. The previous 49 actor groups are identical to their full-table entries.
  Other groups now have metadata, which does not imply their artwork is imported.

Global output belongs in `engine/data_tables/decoration_data.txt` and
`engine/rendering/sprite_frame_data_common.yml`. Input bytes are retained under
`import_sources/`. The importer accepts only a fresh output outside the source,
checks canonical dependency paths and rolls back malformed resource output.
Resource names are lowercase and files have mode 0644.

## New Sorpigal presentation and orientation

The native map loader reads `mm6.games.lod/oute3.odm`. All 479 instance names
resolve through the shared decoration loader, overriding stale cached IDs. The
20 visible animation groups require 53 sprite images and five palettes; all
are present in this source. Export them into `worlds/mm6/sprites` and
`worlds/mm6/textures`. Missing assets are reported explicitly; corrupt present
assets fail the transaction. No substitute images are generated.

Sprite priority is optional `sprites.lod`, then `mm6.sprites.lod`,
`mm7.sprites.lod`, `mmmerge.sprites.lod`. Palette priority is optional
`bitmaps.lod`, then `mm6.bitmaps.lod`, `mm7.bitmaps.lod`, `mm8.bitmaps.lod`,
`mmmerge.bitmaps.lod`, `00 patch.bitmaps.lod`. Later owners win.

Instance 464 is `Party Start` at (-9728, -11319, 160). Its raw direction 512
means 90 degrees. The shared orientation helper now converts the original
2048-unit circle, and uses signed degree storage at offset 0x1a only when the
primary direction is zero. Raw map fields are retained for round trips. Both
map billboard paths, outdoor startup and binary indoor startup use this rule;
indoor startup still has integer-degree precision in its existing API.
Hidden decoration entries retain their records without requesting visible
sprite textures.

## Evidence and limits

- Twenty new synthetic checks cover original IDs, animation continuation,
  preservation of unrelated groups, signed sounds, direction fallback/wrapping,
  actual indexed pixels, missing/corrupt assets, precedence and path/output
  protection. The complete resource suite passes 515 checks. Mac and iPadOS
  builds pass; shared touch/keyboard/menu checks also pass.
- Independent comparison verifies all 673 decoration rows and 23,390 SFT
  frames, float32 scale bits, 1,824,349 sprite index pixels and five palettes.
  See `../local-data/validation/decoration-source-comparison.json`.
- `decoration-resources-002` is composed into `gameplay-base-028`. The formal
  Mac game visibly starts at the town entrance and shows trees, flowers and the
  fountain with the original HUD. `actual-start.png` and `actual-menu.png` are in
  `../local-data/validation/gameplay-app-start-028/`. The app closes with exit 0.
- Escape opens the Chinese game menu. A quick physical mouse click on Save did
  not open the save overlay; the current input path polls held mouse state and
  needs event delivery. Player save/load remains unaccepted.
- The headless full-map load returns success but still reports absent house
  animation metadata and monster family m529. The live application reports
  missing sky `plansky1` and four item images. Portraits, full Chinese UI, map
  events, combat, other maps and device input remain unaccepted. This checkpoint
  is not complete gameplay, and `runtime_ready` remains false.
