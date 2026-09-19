# Native scene expansion — checkpoint 047

Checkpoint 079 collects skies from map/delta metadata, continent/map rules and runtime cloud names.
They now reside in world-local physical textures, as required by the virtual sky_textures category.
All 177 sky exports match source pixels; actual MM8 clouds render. Shared dynamic object images are
collected in a separate global stage. See DYNAMIC_PRESENTATION_VALIDATION.md; 201 map loads remain.

Historical checkpoint 078 includes all 61 MM8 maps, for 201 MM6/MM7/MM8 scene loads. Original MM8 minimaps
have explicit base-to-Merge precedence; empty sprite selections and building-free outdoor maps are
handled without weakening source validation. Normal MM8 entry/reload and short Mac Metal navigation
pass. Runtime-only sky/projectile/splash references remain uncollected. See MM8_SCENE_VALIDATION.md;
all older counts and exclusions below describe their named checkpoint.

Checkpoint 052 adds original direction arrows and explicit world presentation bindings. Corrected ZIP
namespace mounts allow world-qualified loading. MM6/MM7 Mac captures verify eight directions, zoom
limits and short movement tracking, with original-art pixel comparisons. Coverage remains 140 map loads.
See MINIMAP_NAVIGATION_VALIDATION.md; this supersedes the 051 pending arrow-binding note below.

Checkpoint 051 adds original minimap images for all 28 converted outdoor scenes under world-local icons.
All 7,340,032 RGB pixels match the original sources; MM6/MM7 Mac Metal captures show the images.
Map loading remains at 140; party arrow binding and navigation behavior still need acceptance.
See MINIMAP_SOURCE_VALIDATION.md. Earlier defect observations below describe their checkpoint only.

Current extension: checkpoint 048 adds all 76 registered MM7 scenes to the native pipeline,
for 140 MM6/MM7 map loads. MM7 normal creation, cold reload and the Moon Temple round trip pass
on Mac. Metal captures expose water and wide-HUD defects. See MM7_GAMEPLAY_VALIDATION.md;
the counts and exclusions below describe the historical checkpoint 047 result.

On 2026-09-11 the Mac native runtime pipeline expanded from four scenes to 64 MM6 scenes:
all 15 outdoor ODM/DDM pairs and 49 indoor BLV/DLV pairs. Both command-line conversion and
the application's OYResourceImport API produce the same four ZIP packages. No iPad was used.
This is package/data-load coverage, not a completed MM6 campaign or acceptance of all scene visuals.

## Source coverage and explicit exclusions

The supplied source has 207 registered scenes, with geometry present for 206. MM6 owns 67 entries:

- 64 are included in the current runtime profile and load through GameDataLoader's gameplay path.
- zddb05.blv is missing from the original source archives; its registry entry remains intact.
- 6d09.blv (斯纳格的铁矿) requires unimplemented legacy commands 27/28 in events 21/39.
- znwc.blv (NWC) requires unimplemented legacy command 20 in event 71.

The last two are explicit profile exclusions. Their instructions are not removed or treated as no-ops.
An unknown command in another requested scene still fails conversion. Source absence and unsupported
commands have separate conversion.json fields. MM7/MM8 and Breach are not in this runtime scene profile.

## Conversion and ownership

SceneResources replaces the former indoor-only converter. It accepts registered ODM or BLV filenames,
resolves world ownership from shared MapStats, and reads the matching DDM or DLV. patch.games.lod overrides
base geometry and companions independently. A separate actual-source check confirms that both 7d37.blv
and 7d37.dlv are byte-for-byte the patch archive payloads. This MM7 diagnostic is not part of the 64-scene
runtime package and does not establish MM7 gameplay.

Geometry remains byte-identical to the original decompressed payload. Textures, sprites, palettes and
available sky images are world-owned. Full SFT, TFT and terrain definitions remain engine-owned.
All three Tile/Tile2/Tile3 tables retain their 882 rows, original indices, flags and empty slots.
The importer reuses the runtime terrain-index calculation, including MM8 direct-tile remapping.
Shared schema checks cover flags, index preservation, empty slots and malformed inputs.

Actor/decor/object/event presentation collection is shared between indoor and outdoor scenes.
Encounter lookup uses the same picture-name-to-monster-statistics association as runtime population;
it does not invent A/B/C versions of a unique monster. Every referenced TFT surface-animation frame
is collected, with the existing source timing/index rules preserved.

One exporter defect exposed by expansion was fixed: a MouseOver/Exit-only event with an absent hint
now retains an explicit inert registration. It previously vanished when emitted as nil with no hint.
This does not change the rejection of unknown event commands. A synthetic regression recreates the
source shape of outd1 event 28, including its unavailable hint index.

## Validation and remaining gaps

- 31 native converter stages plus packaging and validation, with 64 map identities in the manifest.
- 3,729 source events across one Global and 64 local programs; 1,134 local STR slots.
- All 64 gameplay-path loads require geometry, companion, local events and applicable presentation sets.
  These checks do not require every referenced image to exist and do not execute every event branch.
- All 128 geometry/companion payloads match the original source bytes; all 7,883 ZIP entries pass CRC.
- The app import path retains its 377-file source copy, atomically selects the converted record, and
  publishes packages identical to direct conversion. Failure cleanup and existing-output protection pass.
- 25 outdoor/event import tests and 10 app import tests pass. Focused native terrain and TFT checks pass.
- Normal Chinese character creation, a separate-process reload, existing format-83 combat save reload,
  and format-84 indoor position/camera/actor-state reload plus return to town pass with the expanded package.
- The original three-dungeon shared entrance/door/chest/return event checks still pass.
- Generic unsigned iOS Release build passes. Nothing was installed, launched or demonstrated on iPad.

Reports preserve unresolved source references: 10 maps reference absent surface names (eight are the
literal `pending` placeholder; the other two names are `wtrtyl` and `sginn`), 17 reference missing sprite
frames and palettes, and four contain unresolved decoration name `0`. Hive declares `DemonQueen A/B/C`
encounters that do not resolve in the supplied monster statistics; their actual population behavior is
still unverified. The three missing original MM8 base archives and missing music remain unresolved.
Missing references are not replaced with invented artwork. A scene-load pass is not proof of visual
completeness, encounter correctness, navigability or quest completion.

Evidence lives in ../local-data/validation/map-expansion-047/. result.json identifies the published
runtime and checks; scene-gaps.json and reports/scene/scene_conversion.json preserve exact gaps.
Long play sessions, map-to-map travel coverage, complete quests/MMExtension overlays, all indoor combat,
import recovery/cancellation/storage management and later physical acceptance remain open.
