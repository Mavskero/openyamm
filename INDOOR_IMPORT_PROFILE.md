# Registered indoor scene import — checkpoint 045

Checkpoint 047 replaces IndoorResources with shared SceneResources and expands the runtime
profile to 64 MM6 scenes. The three-scene importer measurements below are historical; the
current scene report is reports/scene/scene_conversion.json. See SCENE_IMPORT_PROFILE.md.

Checkpoint 046 extends the earlier data checks below with actual application transitions,
short movement, indoor save/reload and cold-process restoration in all three dungeons.
It fixes missing indoor camera persistence with backward-readable save format 84.
See INDOOR_SESSION_VALIDATION.md for the tested scope; visual and combat acceptance remain open.


2026-09-11. Mac-only work, with generic unsigned iOS Release compilation. No device
was queried, connected, installed, launched or demonstrated.

## Scope and ownership

The native runtime profile now includes MM6 oute3.odm and three BLV/DLV pairs:
6d01 (Goblinwatch / 大耳怪前哨), 6d02 (Abandoned Temple / 废庙), and 6d18
(Gharik's Forge / 加里克的炼铁厂). Other outdoor maps and continents remain unconverted.

IndoorResources reads map identities/world ownership from the converted shared MapStats.
It preserves raw geometry and companion data and validates them with the existing runtime
BLV/DLV readers. The world determines the source games archive and output namespace.
The API accepts a list of registered indoor filenames; the current runtime profile selects three.

Scene material references include faces, TFT animation sequences and exported event texture
changes. Sprite references include companion monster_info_id, all three tiers of each map
encounter, resolved decorations, event sprite changes and placed object descriptors.
Monster and decoration registries remain engine-owned; geographic artwork stays world-owned.
Per-image original archive ownership and missing-resource lists are recorded in
reports/indoor/indoor_conversion.json. Nothing is synthesized to conceal missing source art.

Events run before indoor import so generated metadata can supply dynamic material/sprite
references. Multi-map event conversion emits one shared Global and four local programs,
preserves merged IDs and localized STR slots, and rejects missing/duplicate requested maps.
There are 1,525 exported events and 83 map-local string slots in this profile. This does not
mean all event semantics or MMExtension overlays have been implemented.

## Shared animation integration

SurfaceResources converts the supplied TFT.txt into engine/data_tables/texture_frame_data.txt.
All 33 source indices remain unchanged. Frame Time stays in source 1/32-second units;
the existing TextureFrameTable multiplies by eight for runtime animation ticks. Group duration
is the sum of its frame times, stored on its first frame. First/HasMore flags preserve group
boundaries. Schema, IDs, names, flags and signed source duration limits are checked.
Behavior was checked against the user's MMExtension TFTItem definition and group update routines;
their implementation was not copied.

MapAssetLoader now checks all requested monster animation names against the loaded common SFT
before requesting split family files. Split files remain required for absent animations.
This supports the complete merged common table without a missing redundant m529 file causing
all monster previews to disappear. The shared path applies indoors and outdoors.

The source SFT zero/null sentinel has zero scale and palette and is not exported as eight
nonexistent images. It is retained in the common registry without replacement artwork.

## Observed results

| Map | Faces | Authored actors | Spawn records | Surface textures | Actor previews | Missing actor previews |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 6d01.blv | 2,290 | 0 | 66 | 98 | 51 | 0 |
| 6d02.blv | 4,110 | 1 | 78 | 64 | 58 | 0 |
| 6d18.blv | 2,561 | 0 | 52 | 88 | 35 | 0 |

Each source companion contains 20 chest slots. Door records number 38, 10 and 53.
Spawn records include non-monster spawns, and preview counts are not final randomized populations.
No surface texture is missing for these three maps. Both 6d01 and 6d02 reference missing
campfire frames dec04a–dec04h and pal008; the supplied original MM8 base archives are absent.
This is an explicit presentation gap. Gharik's Forge has no missing resources among the
collected references. This does not establish every possible summoned creature/effect or overlay asset.

SceneImportRuntimeCheck loads the actual generated packages and executes imported events:

- Town events 101/102/103 request the expected dungeon. Event 101 rejects a party without
  the key; a controlled key-2109 fixture verifies consumption and QBit 1324. This does not
  claim that a player earned the key through a completed quest.
- All three local/global programs bind and their on-load handlers execute.
- Event 1 addresses existing door mechanisms; one chest event per map addresses chest 1.
- Exit events 51/50/90 request oute3 and the town reloads through GameDataLoader.
- TFT timing, name/index lookup, adjacent groups and six malformed-source cases pass.

These checks do not operate the on-screen transition confirmation, walk corridors, animate
doors visually, loot through the UI, fight monsters or test indoor save/reload. Those remain open.
Normal town creation/start and the earlier 043 combat-save reload also pass on the new packages.

OYResourceImport's Mac API copies the original source and converts/publishes the new package.
The source copy remains available. Manifest map lists agree with the four validated maps;
runtime_validated and full_gameplay_validated remain false. All four ZIPs pass CRC checks
(3,526 entries). The 26 focused event/importer tests pass. iOS Release compiles without signing.

## Entry points and evidence

Native full import: runtime_import SOURCE GAMEPLAY_LAYOUT_DIRECTORY NEW_OUTPUT.
Incremental indoor import: indoor_import SOURCE CONVERTED_BASE NEW_OUTPUT MAP.blv [MAP.blv ...].
The converted base must include shared tables, full SFT and requested map events/support.
Event selection: event_import SOURCE CONVERTED_BASE NEW_OUTPUT [MAP_STEM ...].
Verification: scene_import_runtime_check RUNTIME_DIRECTORY.

Evidence root: ../local-data/validation/indoor-import-045/.
The authoritative app-generated runtime is app-support-001/GameImports/
7D227D6C-44F7-44B5-B3DE-89F35AEF80C4 under that root.
Earlier exploratory failure logs are retained separately. Overall progress remains the
low-confidence 35%–40% estimate; additional converted scenes do not establish complete gameplay.
