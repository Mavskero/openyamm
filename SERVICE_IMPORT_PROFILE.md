# MMMerge chest and house import profile

This native converter targets the supplied 2023/2024 merged installation. It
produces two shared data tables and any available matching chest backgrounds.
It reads LOD/TXT assets, never executes Windows programs, and does not require an
EXE/DLL on iPad. It does not yet establish a playable chest or house service.

## Current house presentation checkpoint (2026-09-10)

Physical follow-up: the user entered tavern 260 using E at its actual door and
confirmed ongoing animation and Chinese names. Standalone XCTest then selected
NPC 786, clicked letter topic 1314, observed 10000 -> 11000 gold, and repeated
replacement topic 1315 without another reward. It exited through normal UI and
saved to the third empty slot with the default Chinese map name. Native readback
of the actual device player save confirms 1105 cleared, 1106 set, 11000 gold and
one retained letter 2125; both earlier user saves remain byte-identical. Evidence:
`../local-data/validation/ipad-gameplay-031/tavern-quest/quest-comparison.json`.
This accepts the initial/repeat reward and saved state on iPad; rewarded-save
reload/repeat, physical audio, full combat and broad house coverage remain open.

Media follow-up: `media_import` now supplies 297 original video payloads and
1,646 NPC portraits to `gameplay-base-031`. The formal Mac Metal game displays
house 260's animation and portraits with Chinese NPC dialogue; entry used the
existing debug console. Source media, shared loop timing and validation scope
are documented in `MEDIA_IMPORT_PROFILE.md`. House sound effects, shops, physical
navigation and full gameplay remain unaccepted. Earlier metadata-only statements
below describe the `house_import` stage independently of this media follow-up.

The additional native command `house_import SOURCE CONVERTED_BASE NEW_DESTINATION`
now produces `engine/data_tables/house_animations.txt` and `house_movies.txt`.
It reads the original `House Movies.txt`, `2devents.txt` and `npcdata.txt` directly,
and validates identities against the already converted house/NPC/sound tables.
The converted house name supplies Chinese display text; source animation, house,
NPC, topic and portrait identities remain unchanged. Output and both input roots
are protected, original-source symlink escapes are rejected, and output is fresh.

All 488 movie rows survive, including blank video names, spaces, apostrophes and
other ordinary filename characters. Paths/control characters are rejected. The
derived animation table contains all 1,561 active houses and 877 resident links
across 759 houses. NPC 765 refers to source house 1089, an unnamed inactive slot;
that association is reported instead of inventing a new active building.

The ten-column animation schema is house ID, animation ID, building name, sorted
resident NPC IDs, video name, unused enter sound, room sound ID, explicit house
sound base ID, proprietor picture ID and movie house type. The original movie
table retains its five-column schema. A nonzero movie portrait overrides the
base proprietor picture; zero retains the base value, matching the shared loader.

The source `MiscTweaks.lua` patches house sounds at `0x4b067d` / `0x4b0683` to
`(group + 3000) * 10`, while the older unpatched struct comment describes a stride
of 100. Our native sound catalog preserves the original merged numeric IDs.
Thus the converter emits an explicit `houseSoundBaseId = 30000 + group * 10`
for nonzero groups and leaves `roomSoundId` zero. The real loader and
`deriveHouseSoundId` validate that the greeting resolves to base + 1. All referenced
greeting IDs exist in the converted catalog; their audio samples are still absent.
This profile targets the supplied merge's ten-ID groups, not arbitrary older mods.

House 260 (`孤独骑士`) resolves to animation 394, video `tavrich`, picture 27,
residents 786/787 and sound base 31390. All 1,561 joins and all movie fields were
independently compared with source data in `house-source-comparison.json`.
Output `../local-data/house-resources-001` is assembled into `gameplay-base-030`.

`house_import_gameplay_check` loads the imported tables and OUTE3 through the real
game loader, then uses the shared dialog controller with the existing test world
runtime. It follows entrance event 11 → resident 786 → topic 1314. Without the
letter, no reward is issued. After starting event 100 grants item 2125, the topic
awards 1,000 gold, clears QBit 1105, sets 1106 and replaces topic slot 0 with 1315.
The letter remains, as in the original event. Repeating the conversation and
reloading its native save cannot earn a second reward. Chinese NPC identity is
checked against the loaded table. The save is a controlled party-state fixture,
not a UI-created full outdoor save; it contains no screenshot or world snapshot.

Evidence: `../local-data/validation/house-quest-check-001.json`,
`house-quest-save-readback.json`, `house-quest-state-001/letter.oysav`,
`house-import-tests.log`, `house-resource-suite.log` and
`house-import-ios-build.log`. An earlier generic `--headless-open-event` probe
tried its default `outa1.odm` before OUTE3 and stopped; the focused check explicitly
sets the requested initial map with the existing loader API. That probe did not
verify a dialog, and is not counted as successful gameplay evidence.

This stage adds metadata and verifies the quest/controller logic. It does not yet
render a complete house screen, walk the player to the tavern, play house video,
import portraits or produce sound. The source contains MM6/MM7 VID archives and
the tavern/NPC portraits in `mm6.icons.lod`; these remain available for the next
presentation stage. The earlier general absence of MM8 base assets does not mean
these particular MM6 assets are missing. No iPad was updated.

Final checks: 540 resource/import tests pass, including twelve house cases; the
Mac tools and iPadOS service library/app target build pass. A raw animation-ID
consistency check also rejects stale converted dependencies (numeric movie IDs
normalize to empty runtime video names, so the typed comparison alone was too
weak). Re-export to `house-resources-002` is byte-identical to the three files
already mounted from `house-resources-001`. Evidence:
`house-resource-suite-final.log`, `house-import-ios-build-final.log` and
`house-reexport-comparison.json`.

## Chest metadata evidence

The source `Data/Tables/Chest.txt` has eight entries, IDs 0–7, with five fields:
ID, internal name, storage width, storage height and image index. Storage is 14×10;
image indices are 1–8. Static comparison with all eight 36-byte records in the
supplied `DataFiles/dchest.bin` confirms these source values. The loose TXT remains
authoritative for conversion; the binary cache is evidence, not a fallback.

The original program SHA-256 is
`97be86a7597fbd9763f9b4b0ed1f4a7e47c960703fa30857c32daa056b7d403b`.
Read-only PE section translation located four arrays of eight signed 32-bit values:

| Address | Meaning | Values for chest IDs 0–7 |
| --- | --- | --- |
| `0x4F3AC4` | Screen grid X | 42, 18, 18, 42, 42, 42, 18, 42 |
| `0x4F3AE4` | Screen grid Y | 49, 45, 45, 49, 49, 49, 45, 49 |
| `0x4F3B04` | Visible grid columns | 9, 9, 9, 9, 9, 9, 9, 9 |
| `0x4F3B24` | Visible grid rows | 9, 9, 9, 9, 9, 9, 9, 9 |

Draw function `0x41FDBE` indexes the arrays by the chest picture/type ID. Its
instructions at `0x41FDF6`, `0x41FDFF`, `0x41FE08` and `0x41FE0E` load these four
arrays. The `chest%02d` name uses `DChestItem.ImageIndex`. Background drawing at
`0x41FE6C` pushes screen Y=23 and X=8; the item-position calculations at
`0x41FFD1`–`0x42000E` use the array coordinates and 32-pixel cells.

The shared `GameplayScreenRuntime::resolveChestGridArea` adds its table offset to
the resolved background origin. Exported offsets must therefore be relative to
the background: subtract (8, 23). Chest 0 gets (34, 26), and chest 1 gets (10, 22).
Copying absolute screen coordinates into those columns would offset item drawing
and hit testing a second time. The runtime table schema is:

`id, name, storage_width, storage_height, image_id, texture_name, grid_offset_x,
grid_offset_y, grid_width, grid_height`.

All eight exported rows were compared with the actual arrays and binary
descriptors. Evidence is in `../local-data/validation/service-profile-verification.json`.
This establishes the original MM8 logical layout, not Windows patch-specific
scaling or a visually verified iPad chest screen. New UI work must retain this
relative coordinate convention. Profiles that reorder images, change storage
dimensions or add chest types require separately verified metadata.

## Chest image ownership and availability

Matching unprefixed `chest01`–`chest08` backgrounds belong in `engine/icons`.
Archive priority, from lower to higher, is `Data/icons.lod`, `Data/mm8.D.lod`,
`Data/mmmerge.D.lod`, and `Data/10 LocZHCN.EnglishD.lod` when present. Selected
owners and image dimensions are recorded. Decoding uses the existing native LOD
image path, with lowercase output names and file mode 0644. Grid extents must fit
within each present background; malformed or too-small images roll back the
whole fresh output. Missing backgrounds are listed separately rather than hidden.

The supplied installation lacks all eight unprefixed backgrounds. Its
`6chest01`–`6chest09` and `7chest01`–`7chest09` are world-specific art and are not
silently substituted for MM8's layout. No images are produced in this real-source
round. This is part of the previously identified missing MM8 base icons, not a
request to reconnect the iPad. The metadata is sufficient for loading the shared
registries; the interactive chest and full runtime readiness flags remain false.

## House identity and localization

Base `mmmerge.T.lod/2devents.txt` contains 1,607 numbered rows with 24 columns.
The production house loader skips 46 unnamed rows; conversion records those IDs
and exports all 1,561 named rows at their original global IDs. Localization cannot
activate an unnamed base row or create a new venue from a translation-only ID.

The optional `Data/Text localization/ZHCN_2DEvents.txt` joins by ID and overlays
only name, proprietor name, proprietor title and entrance text: runtime columns
5, 6, 7 and 23. Type names stay in the source language because they select services
and default skill offerings. Prices, hours, map/picture IDs, restrictions and
all remaining columns stay byte-identical to the base table. Missing/extra
translation IDs and attempted inactive-row activation are reported. A translation
that blanks an active venue's name is rejected.

The native production loader validates localized display fields and compares
mechanical fields and offered skills with its English load result. Source/output
comparison separately checks every non-display cell for all 1,561 real houses.
The provided translation covers all active houses and restores 8,701 glyphs.
House animation/resident metadata is now provided by the command described above.
Videos, shop stock/rules, travel and rendered service interactions still need
integration. `House Movies.txt` is not renamed to `house_animations.txt`: the
separate output tables retain their different schemas and identities.

## Reproduction and latest result

```sh
cmake --build build-apple --target service_import
build-apple/service_import /path/to/MMMerge /path/to/new-service-output
python -m pytest tests/test_service_resources.py -q
cmake --build build-ios --target openyamm_service_import
```

Original inputs, existing outputs and source ancestors/descendants are protected;
source symlink escapes are rejected. The new output is transactional. Fifteen
synthetic tests cover bilingual data, coordinate spaces, IDs, storage validation,
language-version gaps, inactive rows, image ownership/bounds/rollback and input
preservation. All 107 resource tests pass, and the iPadOS arm64 library builds
without a connected device.

Original chest/house-table checkpoint output: `../local-data/service-tables-002/`.
Its historical combined asset root was `../local-data/gameplay-base-005/`.
At that checkpoint formal startup reloaded chest and house tables; it reported the
optional missing `house_animations.txt` and then stops at the required
`engine/data_tables/english/awards.txt`. No loader requirement was weakened.
Evidence is under `../local-data/validation/gameplay-start-005/` and
`../local-data/validation/service-source-comparison.json`.
