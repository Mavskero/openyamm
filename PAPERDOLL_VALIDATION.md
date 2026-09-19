# Original paperdoll images and coordinates — checkpoint 082

Four normally recruited MM8 companions now show their original bodies, backgrounds and hands in
the actual Mac Metal inventory screen. The shared loader preserves the original Merge screen
coordinate direction. Posed clothing and some equipment placement are still visibly incomplete;
this checkpoint does not accept the complete equipment screen or the MM8 starting-area milestone.
No physical iPad or simulator was queried or operated.

## Original resources

The existing native doll stage selects nine typed body/background/head/hand fields from all 77
original character rows. It preserves all source cells and character identities, leaves face-frame
selection with its existing converter, and does not invent meanings for the two unknown columns.
There are 502 unique image references and 491 available original images. They are written to shared
`engine/icons` in lowercase with 0644 permissions. No replacement artwork is generated.

The original `icons.lod` to `mmmerge.icons.lod` patch is the only accepted collision for this
profile; actual `pc22rhd` selects the Merge owner independent of directory order. Other competing
owners, corrupt selected images and source escapes fail and roll back the complete new doll stage.
Each image report records its owner, dimensions and referencing character IDs.

Independent decoding compares **4,035,869 RGB pixels**, all 491 images, source references and package
permissions. The final runtime has **42,383 ZIP entries**: all 41,893 previous payloads remain
byte-identical and 490 images are new entries; one selected background already existed identically.
Every ZIP passes CRC. Full native conversion validated 201 map loads before the later coordinate
loader fix; that fix changes no map geometry or serialized table cells. Standalone conversion
afterward produced the same 494 files byte for byte, including both tables and the image report.

The eleven missing source references are four MM7 beard overlays (`7pc15brd`, `7pc16brd`, `7pc21brd`,
`7pc22brd`) and seven `pc30` body/hand images. The latter belong to an unavailable class/race -1
placeholder. Two beard-bearing rows are selectable, so the missing overlays are not universally
irrelevant. All nine typed fields for the currently inspected four companions have their images.

## Coordinate repair

The original `CharacterOutfits.lua` reads body coordinates and all twenty doll coordinates as
signed screen offsets without reversing Y. `PaperDoll.lua` documents their screen-relative origin.
The shared C++ table loader had negated Y unconditionally. Actual imported bodies consequently
appeared too high, and hands/weapon anchors were outside the visible panel.

The original 27-column portrait schema and named LHo/LHc doll schema now retain signed Y directly.
The pre-existing converted schemas still perform their original Y conversion and preserve their
different hand order. The importer validates the original coordinate values and both weapon
anchors. Source tables, saves, rendering rules and original game scripts are not rewritten.

Three focused regression cases cover positive coordinates, negative offsets, original hand order
and legacy behavior: the red run fails 14 of 36 assertions; all 36 pass after the fix. These tests
are registered in the unit target and were also built/run as a focused executable. The full unit
suite is disabled in the current Mac configuration and was not run.

## Actual application and persistence

`capture-green-002` starts with the checkpoint 081 normal four-member save. Shared portrait clicks
open each companion's inventory, using the normal right-button pointer hold while in world view.
No position, item, attribute or quest injection is used. Character IDs are 1, 5, 9 and 14. The run
renders 202 Metal frames, then writes the inspection and final saves.

All four original captures were viewed. Body/head clipping caused by the reversed coordinates is
resolved and hands are visible; the staff/sword/mace now reach their respective hand area. Armor
and boots still show inventory icons instead of fitted pose variants, some shields extend beyond
the panel, and limb/layer overlap still needs verification with the correct equipment artwork.
This is not pixel-perfect alignment acceptance for every race or equipment combination.

Independent inspection confirms **24 gameplay fields** remain equal to the incoming save,
including all characters, inventory, quests, NPC states, position, camera and projectiles. Only
selection intentionally ends on member 3; initial world frames can advance time and actor animation,
which are outside this comparison. This check does not repeat or claim the earlier 31-field cold/
warm reload validation, and it does not perform an equipment transaction.

## Evidence and next work

Evidence is in `../local-data/validation/paperdolls-082/`: `result.json`, source and changes hashes,
`verify-resources.py`, `resource-verification.json`, `coordinate-import-verification.json`,
`verify-saves.py`, `save-verification.json`, original BMPs and lossless PNG copies.

The final 26 Python import tests, focused 36 coordinate assertions, native build and generic
unsigned iOS Release build pass. Earlier failures are retained: missing image tests, incorrect
capture input assumptions, and the pre-fix coordinate assertions. The initial image-presence run
passed mechanically while showing incorrect coordinates; its captures remain as contrary visual
evidence. Three Pillow deprecation warnings do not affect the pixel comparison.

Current runtime is `runtime-001`; previous complete runtime is checkpoint 079/runtime-002. Continue
normal gameplay from the unchanged 081 `route-green-002/inn-route-complete.oysav` with the new
runtime. Next import fitted equipment variants and check shared layer/anchor behavior before normal
shops, portals and d05 progression. The original `RemoveItemsLimits.lua` also gives minotaurs/trolls
their own cloak variants, whereas the current runtime still forces type 2/3 to variant 1; that
separate mismatch remains open. Overall remains **40–45%, low confidence**.
