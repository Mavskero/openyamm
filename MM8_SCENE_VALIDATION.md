# MM8 scenes and normal entry — checkpoint 078

Update 079: the 13 exact image-loading gaps below are fixed by the importer, and actual MM8 clouds
render. Current runtime is dynamic-presentation-079/runtime-002. See DYNAMIC_PRESENTATION_VALIDATION.md
for source-pixel evidence, the failed navigation recheck and remaining full-playthrough milestone.
The remainder records checkpoint 078's observations rather than current defect status.

The native importer now includes 61 MM8 scenes: 14 outdoor and 47 indoor maps. The complete package
at `../local-data/validation/mm8-scenes-078/runtime-004` loads 201 scenes through GameDataLoader:
64 MM6, 76 MM7 and 61 MM8. Retain checkpoint 077 as the previous package. This establishes resource
loading and a normal MM8 entry, not a completed campaign or visually complete MM8 support.

All work and outputs remain on NVMeSSD. No iPad or simulator UI was queried or operated.

## Import changes

- RuntimeResources includes MM8 in discovery, event export, scene conversion and package validation.
  `out01.odm` is required as its normal starting map. The existing MM6 exclusions for unsupported
  instructions 20/27/28 remain; `mm6:zddb05.blv` is still absent. Breach expansion remains pending.
- For MM8 minimaps only, a known MM8/Merge/patch icon layer may replace original `icons.lod` artwork.
  A second overlay owner or an unrelated competing owner still fails. In the supplied source,
  `out02` and `out13` select the revised `mmmerge.icons.lod` images. Source ownership is reported.
- A scene selecting no sprite groups now emits `sprites: []`. Source row structure, names, flags,
  timing and requested identities still undergo validation. Empty contributions do not require
  an independent nonempty runtime table or remove existing shared frames. The production global
  SpriteFrameTable loader remains unchanged. This fixes sprite-free `out15.odm` conversion.
- Runtime import validation requires building textures when geometry actually references them.
  `out15.odm` has no building faces and legitimately has no building texture set. Its terrain,
  geometry, companion and actor presentation still undergo the other mandatory checks.

No shared gameplay table, generated event script, source archive, save format, renderer or game
rule was hand-edited. Existing converters regenerate the MM8 local event programs.

## Resource and build evidence

| Check | Observed result |
| --- | --- |
| MM8 registered source geometry and companions | All 61 pairs present; all 122 converted payloads match source bytes |
| MM8 event exports | All 61 map programs convert and load as Lua |
| Complete runtime map validation | All 201 selected scenes load in their registered world |
| Original minimaps and arrows | 14 maps plus 8 arrows; all 3,663,240 RGB pixels match recorded source owners |
| Prior package payloads | All 31,302 previous entries unchanged; 9,623 new entries only in the MM8 package |
| Package integrity | All 40,925 entries pass ZIP CRC checks |
| Focused tests | 25 event/outdoor tests; native empty-sprite/terrain/surface checks pass |
| Native builds | Import build 007 and final creation diagnostic build 010 pass |
| Generic unsigned iOS Release | `ios-release-003.log`: BUILD SUCCEEDED |

The 61-map source-reference report has no missing textures, sprites, palettes or encounter identities;
`out07.odm` retains one unresolved decoration name `0`. That report covers collected map references,
not all images requested dynamically while playing. The latter have outstanding gaps below.

## Normal entry, reload and Mac Metal captures

The diagnostic invokes the actual NewGameScreen and GameApplication creation/load/save paths.
Single-character and four-character parties enter `out01.odm` at `(3560, 7696, 545)` with level-one
characters, ordinary starting items, 200 gold and 5 food. Original OnLoad event 3 sets exactly
QBits 185, 226, 401 and 407. The later 10:00 event has not fired, and no MM6 letter/quest or MM7
scavenger-hunt flags are inherited. No character, item, position or quest overrides were used.

The four-person diagnostic uses the imported Jadame classes Knight, Priest, Necromancer and Vampire,
selecting eligible candidates through the normal screen. Earlier attempts incorrectly reused MM6's
Cleric/Sorcerer/Archer assumptions and failed; the source's class availability was preserved. The
first three selected candidates share portrait 1; the Vampire uses portrait 13. These are legitimate
screen choices, not four distinct portrait samples. Chinese names survive creation and reload.

Independent-process reloads preserve all 26 fields exposed by openyamm_save_inspect for both party
sizes, including members, inventory, quests, time, position/camera and the saved actor/turn records.
This is limited to the inspector's fields, not proof of every gameplay state or full serialization.

`navigation-001` renders 261 actual Mac Metal frames. All eight original arrows load at the requested
camera directions; HUD zoom reaches both limits, then returns to normal. Twenty normal scene-movement
steps move 762 world units, and the minimap UV follows by 762/65536. Directions are set by the test,
zoom uses the shared HUD handler, and movement uses OutdoorSceneRuntime. This does not test physical
keyboard, mouse or touch delivery. `party-capture-002` renders another 181 frames with the four-person
party. Source BMP captures are retained; PNGs are lossless format copies for inspection.

## Remaining defects and next work

Actual rendering requests 13 images absent from the package: `sunsetclouds`, `arrowa0`–`arrowa4`,
`obj187`, and `splash1`–`splash6`. Every exact name exists in the original base: clouds in `bitmaps.lod`,
the other 12 in `sprites.lod`. The first actual scene shows a flat sky where clouds should render.
Next extend authoritative sky and dynamic object/effect reference collection, then verify source
pixels and repeat the affected actual capture. Do not invent replacement artwork or suppress warnings.

MM8 houses, recruitment, dungeon traversal, quests, cross-world progression, complete localization
(`Menu` and `palm tree` remain English in these captures), audible output, long sessions and physical
iPad acceptance remain open. Dummy audio permits decoder/cache activity only. This short capture
does not prove combat, the full starting island, a completed campaign or broad stability.

Evidence lives in `../local-data/validation/mm8-scenes-078/`: `result.json`, `resource-verification.json`,
`save-comparison.json`, `runtime-presentation-gaps.json`, `mm8-scene-gaps.json`, source hashes/patch,
test and build logs, all earlier failed logs and the actual captures. Overall progress remains
**40–45%, low confidence**; map counts do not establish a campaign completion percentage.
