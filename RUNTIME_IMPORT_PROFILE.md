# Native runtime import — checkpoints 048–079

Checkpoint 079 adds global object presentation and fixes the sky package directory. Current complete
runtime: local-data/validation/dynamic-presentation-079/runtime-002; retain 078 as previous. All 201 maps
load. Source-exact sprites/palettes and world skies clear the 13 image-loading gaps in the MM8 entry;
actual Mac Metal clouds are visible. This is not a complete quest or effect-behavior acceptance.
See DYNAMIC_PRESENTATION_VALIDATION.md, including the failed navigation recheck and next milestone.

Historical checkpoint 078 adds all 61 MM8 scenes, for 201 validated MM6/MM7/MM8 map loads. Normal MM8 single/four-person
creation and cold reload pass; actual Mac Metal captures verify entry and limited minimap navigation.
The current complete package is local-data/validation/mm8-scenes-078/runtime-004; retain 077 as previous.
All previous package entries remain unchanged. Dynamic sky/projectile/splash images are still missing
despite existing in the supplied base; visual completeness and physical iPad acceptance remain pending.
See MM8_SCENE_VALIDATION.md. The following checkpoint sections preserve their historical scope.

Checkpoint 077 adds910original item images in the existing inventory stage, including901new engine
entries. All existing engine entries and all three worldZIPs retain their076bytes; all140supported
maps stillload. Actual Mac chest contents, shared mouse/touch transfer, Chinese inspection and
cold/warm save reload pass. 30focusedtests and genericunsignediOSRelease pass. Current complete
runtime is local-data/validation/item-icons-077/runtime-001; retain076as previous. No iPadoperations
orMM8scene expansion. See ITEM_ICON_VALIDATION.md for pixel/source evidence and acceptance limits.

Checkpoint 076 combines the GOG MM8 base with the existing Merge/Chinese source. Original sound/video
layers and explicit original-to-Merge frame precedence now convert; runtime-002 loads140supported
MM6/MM7maps and clears the limited base/archive and chest-background missing lists. All8574WAVs
decode; prior7886WAVs and118shared text/script/layout files are unchanged. 91focusedtests and unsigned
iOSRelease pass. Actual Mac chest art appears, but item icons are still unpackaged and visually absent.
MM8 scenes and physicaliPad acceptance remainpending. See MM8_BASE_RUNTIME_VALIDATION.md.
Use local-data/validation/mm8-runtime-076/runtime-002 for subsequent resource work; retain063unchanged.

Checkpoint075 obtains the actual GOG MM8 base through verified Mac extraction:175files match installer
checksums, including3baseLODs/8chestbackgrounds/2videos and EnglishD.lod with3250WAVs. No separateSounds
folder is required by this edition. See MM8_BASE_EXTRACTION_VALIDATION.md; sound-layer integration,
combined-source conversion and actual media/chest/MM8scene acceptance remainpending. Runtime063unchanged.


Checkpoint 073 preserves known base-file/chest-background gaps through native conversion and
portable reimport, and keeps them visible in the launcher. Unknown reports are not treated as complete.
40 native tests and unsigned iOS Release pass; a real conversion loads140maps and reproduces all4ZIP
hashes, while both native and portable imports retain the3base/8background gaps. Original069selection
and377source files are unchanged; new duplicates were pruned. See IMPORT_RESOURCE_STATUS_VALIDATION.md.
The assessment covers two limited file-presence categories; complete MM8/gameplay remains pending.

Checkpoint 070 adds launcher cancellation, 64 KiB streamed copying, cooperative conversion checks
and an atomic cancellation/publication boundary. 26 native tests pass. Actual copy, scene, package
and validation cancellation preserve prior selections and remove incomplete output; a full retry
loads 140 maps and reproduces all four previous ZIP hashes. The 069 fixture is restored and its new
duplicate runtime is pruned. Generic unsigned iOS Release passes; no iPad/simulator UI operations.
See IMPORT_CANCELLATION_VALIDATION.md for evidence and latency/provider limits. Content stays at 063.
MM8 base-source requirements were also verified: see MM8_SOURCE_REQUIREMENTS.md.

Checkpoint 069 adds journaled copy/conversion transactions and startup cleanup of interrupted output,
with a cross-process lock protecting live work and atomic publication preserving prior selections.
20 native cases pass; a real source conversion was killed, recovered and successfully retried. All
377 copied source files and four generated ZIPs retain their original/063 hashes. All 140 maps load,
and the renamed published runtime starts in the actual Mac Metal application. Source/runtime app
fixture: `../local-data/validation/import-recovery-069/app-support-001`. No new content migration or
physical iPad work. See IMPORT_RECOVERY_VALIDATION.md for boundaries, evidence and remaining controls.

Checkpoint 065 changes the save schema to 85 for active turns and projectile pending flags. The
063 content package below is unchanged and needs no new reimport. Four actual outdoor cold/warm
turn reloads and a version-84 save migration pass; actual indoor/multi-member turns remain open.
See TURN_SAVE_VALIDATION.md.

Checkpoint 064 reuses the unchanged 063 runtime below. Actual outdoor projectile kill, loot and
cold reload pass; the earlier target mismatch is diagnosed as interception by a different actor.
Only diagnostic tools change; no full conversion or content migration was performed in 064.
See PROJECTILE_TARGET_VALIDATION.md. Complete encounter/campaign coverage remains open.

Checkpoint 063 adds required shared `combat_text.txt`, preserving five original English/Chinese
Global.txt templates. Full conversion loads all 140 maps; 27,528 ZIP entries pass CRC. Every
previous 059 payload remains byte-identical. Older packages require regeneration; save schema is
unchanged and an existing four-member save passes cold reload with the new package. Mac text
rendering and unsigned iOS Release build pass, while complete encounter regressions remain open.
See COMBAT_TEXT_VALIDATION.md. Current package: `../local-data/validation/combat-text-063/runtime-001`.

Checkpoint 059 packages the authored shared turn-phase text label in gameplay.yml. Native import
loads all 140 maps; 27,527 ZIP entries pass CRC and every other payload is byte-identical to 058.
The prior incomplete assembly was rolled back; runtime-002 is the accepted output. Existing
source gaps remain explicit. See HOSTILE_TURN_COMBAT_VALIDATION.md.

Checkpoint 058 changes only the authored dialogue body scale in dialogue.yml. Native import
loads all 140 maps; all 27,527 ZIP entries pass CRC and other payloads remain byte-identical to 057.
Chinese dialogue rendering uses actual shaped line bounds. See TURN_COMBAT_VALIDATION.md.

Checkpoint 057 packages the authored text spellbook through the normal UI stage. All 140 maps
load. Four ZIPs contain 27,527 CRC-checked entries: only spellbook.yml changes relative to 054 and
two now-unreferenced attack-button images are omitted; every other payload is byte-identical.
Mac controls and cold reload pass; original icon/audio gaps and later device work remain open.
See SPELLBOOK_CONTROLS_VALIDATION.md.

Checkpoint 054 adds 7,886 original WAVs through the existing sound stage, preserving 846 Chinese
overlays and all 053 payloads. All 140 maps load; 27,529 ZIP entries pass CRC and source-byte checks.
All WAVs decode with the shared SDL partial-block fix. Generic unsigned iOS Release passes, with no
device operations. Missing common samples and full audio gameplay remain; see SOUND_PAYLOAD_VALIDATION.md.

Checkpoint 053 adds 49 byte-identical original MP3s through the existing native media stage. The final
runtime has 19,643 CRC-checked ZIP entries, preserving every checkpoint-052 payload; all 140 maps load.
Shared FFmpeg/SDL music decoding, pause/transition and looping checks pass on Mac. Generic unsigned
iOS Release builds; no device operations. See MUSIC_SOURCE_VALIDATION.md for precise audio scope.

Checkpoint 052 imports 16 original directional arrows plus two world presentation bindings, for 19,594
CRC-checked package entries. World-qualified ZIP mounting now uses independent streamed I/O rather than
an ignored duplicate mount of the same filename. Existing 140-map loading, Mac minimap navigation and
generic unsigned iOS Release checks pass; physical input and full navigation/gameplay remain unaccepted.
See MINIMAP_NAVIGATION_VALIDATION.md for precise evidence and limits.

Checkpoint 051 imports 28 original outdoor minimaps under world-local icons. All 140 map loads pass;
19,576 ZIP entries pass CRC, with every 050 payload unchanged. MM6/MM7 images render through the
existing shared HUD on Mac; party direction arrows and navigation behavior remain pending.
Generic unsigned iOS Release passes; no iPad operations. See MINIMAP_SOURCE_VALIDATION.md.

Checkpoint 050 adds runtime source regions for five wide-HUD assets, original character pose images
and a shared wide-HUD scale that separates five portraits from side panels. Native map coverage stays
at 140; the final package has 19,548 ZIP entries. Missing default/MM8 portraits and outdoor minimap
imagery remain. See HUD_SOURCE_VALIDATION.md for exact visual-fixture and build evidence.

Checkpoint 049 adds source-derived hardware-water materials and referenced frames. The new direct
conversion has 17,083 ZIP entries and still validates all 140 maps. Actual Mac Metal captures confirm
the observed Emerald Island magenta-water defect is fixed; the wide-HUD artwork gap remains.
Generic iOS Release compiles. App publication measurements below refer to checkpoint 048;
see HARDWARE_WATER_VALIDATION.md for the exact newer validation scope.

The native launcher conversion now includes 140 scenes: 64 MM6 (15 outdoor, 49 indoor) and
76 MM7 (13 outdoor, 63 indoor).
The current profile excludes one source-missing map and two maps whose legacy event commands are not
implemented. Complete MM6/7/8 gameplay remains unfinished. See SCENE_IMPORT_PROFILE.md for exact coverage,
source gaps and validation; INDOOR_IMPORT_PROFILE.md and INDOOR_SESSION_VALIDATION.md record earlier work.

## Source and publication

The user selects their owned MMMerge directory. OYResourceImport copies permitted resource folders into
an isolated source record. The root mm8.exe is retained as non-executable data for fixed-profile table
readers; Windows code is not loaded or executed. The user-owned source is not modified.

“转换并开始游戏” performs conversion on a worker queue, reporting stage progress. A new destination receives
all generated data. After common data and converted-map loading pass, active.json atomically selects the
runtime record. The source copy remains available. Exceptions remove the new conversion destination and
leave the selected source intact; publication rejects a source selection that changed during conversion.
Checkpoint 069 now cleans journaled process-kill output and recognized legacy staging safely.
Checkpoint 070 adds cooperative cancellation. Byte/stage resume and in-app import history/storage remain unfinished;
unmarked old UUID directories are preserved because their completion cannot be inferred.

RuntimeResources.cpp executes 32 converters in dependency order: menu, core, story, combat, service,
journal, class, inspection, race, inventory, frame, FX, transition, roster, doll, teacher, NPC, support,
bolster, voice, equipment pictures, world rules, Arcomage, sound, speech, UI, decoration, object presentation,
events, scenes, houses and media. Packaging and validation are progress steps 33/34 and 34/34.

Scene discovery uses the shared registry and source geometry/companion availability. The explicit MM6
profile exclusions are 6d09.blv (commands 27/28) and znwc.blv (command 20). zddb05.blv is source-missing.
These remain registered and are reported separately; their original event semantics are not discarded.
MM8 scenes are included since 078; Breach integration remains pending. The launcher suggests Enroth/MM6 and, when 7out01
is present in the imported record, Antagarich/MM7. Earlier MM6-only records retain Enroth guidance.

Each converter receives a new stage directory. Reports remain under reports/STAGE, outside engine/world
packages. Identical overlaps are accepted; differing overlaps fail except for the selected UI replacing
generic interface art, and the existing full-SFT-over-preview rule. The current scene pipeline no longer
runs the old single-map actor preview converter.

## Runtime packages and limits

RuntimeZip streams stored UTF-8 ZIP entries in 64 KiB blocks with CRCs, producing engine.zip and
worlds/mm6.zip, mm7.zip and mm8.zip. ZIP32 limits reject oversized output. Geometry and presentation remain
world-owned; shared tables/layouts/rules remain engine-owned. The app bundles 26 gameplay layouts.

Before publication, the real game data loader mounts these packages, loads common gameplay data, the
MM6 starting scene, and each selected map with its companion, local program and presentation sets.
Validation switches both the mounted world and loader context at world boundaries and checks map ownership.
Missing individual artwork is reported by the scene converter; these load checks do not prove visual
completeness or campaign completion. The manifest therefore keeps runtime_ready/full_gameplay_validated
false, while common_data_validated, starting_map_load_validated and converted_maps_load_validated are true.
OYResourceImport reads the manifest's actual map list and keeps runtime_validated=false.

Checkpoint 048 verifies both direct and app-API conversion on Mac. They publish identical packages with
17,040 entries, each passing ZIP CRC checks. All 280 geometry/companion payloads match original data.
Global plus 140 local programs contain 7,100 source events and 10,330 local STR slots. The copied source
remains selectable. Normal Chinese MM7 creation and cold reload preserve the six starting quest flags
without MM6 rewards; Moon Temple entry, movement, save/cold reload and return pass. MM6 creation still
passes. Actual Mac Metal captures show the town and temple, but expose magenta water and missing wide-HUD
artwork. Visual acceptance remains incomplete. See MM7_GAMEPLAY_VALIDATION.md for exact limits.
Generic unsigned iOS Release compilation passes; this checkpoint performs no iPad operation.

Run from the repository:

```
build-apple/runtime_import SOURCE assets_dev/engine/ui/gameplay NEW_OUTPUT
build-apple/apple_import SOURCE NEW_SUPPORT_DIRECTORY
build-apple/apple_import --convert NEW_SUPPORT_DIRECTORY assets_dev/engine/ui/gameplay
build-apple/scene_import SOURCE CONVERTED_TABLE_EVENT_BASE NEW_OUTPUT MAP.odm MAP.blv
```

The last command requires an unpacked converted table/event base. It does not independently convert all
shared gameplay tables or publish a complete package. The existing indoor_import target is a CLI alias.
Full table conversion and complete gameplay have separate acceptance requirements. No subprocesses,
Python, archive executables or Windows execution are required by the native application importer.
