# MM7 scene and application validation — checkpoint 048

Follow-up: checkpoint 049 fixes the observed shoreline-water artifact by importing original hardware
materials and animation frames. The wide-HUD issue remains. See HARDWARE_WATER_VALIDATION.md;
the original checkpoint 048 failures and captures below are retained as before-fix evidence.

2026-09-11. Mac-only development, generic unsigned iOS Release compilation, no iPad operations.
The native pipeline now includes 140 scenes: 64 MM6 scenes and all 76 registered MM7 scenes.
This is an incremental port with known visual and gameplay gaps, not complete campaign acceptance.

## Content coverage

MM7 adds 13 outdoor ODM/DDM pairs and 63 indoor BLV/DLV pairs. World discovery uses the shared registry;
conversion retains each map's explicit owner. Package validation switches AssetFileSystem and
GameDataLoader to the correct world, reloads shared gameplay data, and checks each selected map's owner,
geometry, companion, local program and presentation sets. The previous MM6 exclusions remain unchanged.

Global plus 140 local programs contain 7,100 source events and 10,330 local STR slots. All 280 geometry/
companion payloads match the original decompressed archive bytes, including the 7d37 patch override.
All 17,040 ZIP entries pass CRC. Both CLI and the app's native OYResourceImport path perform the conversion.
This does not execute all event branches, establish terrain/actor visual correctness or validate quests.
MM8 and Breach scenes remain outside the current runtime profile. No unsupported source instruction
was turned into a no-op to expand coverage.

The launcher now derives its suggested continents from the imported map list: an old MM6-only record
still suggests Enroth, while a package containing 7out01 also suggests Antagarich. The default menu world
stays MM6; choosing MM7 follows the normal application's existing world activation path.

## Actual gameplay paths checked on Mac

NewGameCreationCheck now supports --world=mm7 while retaining its prior MM6 usage. The test creates the
character through NewGameScreen and calls the actual GameApplication startup path without debug loadout,
immortality or preseeded party. It starts from an MM6-mounted menu so MM7 activation must really occur.

- Chinese Knight, level 1, 200 gold, five food, seven starter inventory entries.
- Emerald Island / 7out01.odm, position (12552, 800, 193).
- Original map event 100 sets all six scavenger-hunt QBits 513–518. QBit 519 stays unset.
- No MM6 letter 2125 or MM6 starter QBits 1104/1105 are inherited.
- Save and separate-process reload preserve the map, world activation, quest flags, items, coordinates
  and camera orientation checked by the existing normal-session verifier.
- MM6 normal creation still passes with its original letter and quest state.

The existing IndoorSessionCheck adds the Moon Temple route, reusing the shared transition/dialog code:
7out01 event 101 enters 7d06; 7d06 event 100 returns to 7out01. No quest-key fixture is used on this route.
The actual scene creates 68 actors. Entry floor/sector are valid; twelve simulation steps move the party
about 118.75 units. Format-84 save/reload and a separate-process reload retain the checked actor fields,
position, sector and deliberately changed camera orientation. The real application returns to Emerald
Island and saves again. This does not test walking from the town spawn to the doorway, dungeon combat,
all corridors, physical input, or completing the scavenger hunt.

## Metal captures and visual failures

ApplicationSceneCapture runs the actual EngineApplication/GameApplication update and render callbacks
with the Metal backend. It loads an existing test save through the same loadSessionFromPath method used
by the load screen, captures frames 30 and 150, runs approximately 181 frames and saves to a new directory.
The input save is never overwritten. Captures are native BMPs; PNGs are format conversions for inspection.

- Emerald Island: terrain, buildings, sky and NPC sprites render.
- Moon Temple: the loaded corridor geometry and surface textures render; this view is dark and does not
  establish lighting parity or visibility in other rooms.
- **Visual acceptance remains failed/incomplete:** Emerald Island water shows magenta artifacts, and
  the current default widescreen HUD lacks its party basebar and related artwork in both scenes.
- The UI import report already lists missing authored wide-HUD resources such as basebar_cut, status_bar,
  scorebg_gold and scorebg_food. This is not evidence of a successful complete HUD.
- The shared HW-water table is imported, but its mapping is not consumed by current scene/render paths;
  this is a concrete next investigation for the water artifact, not a confirmed complete diagnosis.

The first capture attempt called quickLoadFromPath directly and left the menu visible. That artifact is
not accepted as gameplay evidence. The diagnostic was corrected to use the actual load-screen callback
and now rejects captures while a menu/cutscene remains active. No production menu behavior was altered.

## Remaining evidence gaps

MM7 reports absent surface names in 31 maps, sprites in 15, palettes in 27, and unresolved decoration names
in 12. Those are source-reference counts and include placeholders; they are not counts of visible defects.
Its declared encounters all resolve, but their actual population has not been verified on every map.
Missing music, original MM8 base resources, full MMExtension overlays, video playback, travel/quest chains,
long sessions, importer recovery/cancellation/storage, and later physical acceptance remain open.

Evidence: ../local-data/validation/mm7-scenes-048/result.json, package/geometry validation reports,
app-converted.json/log, mm7-start.log, mm7-cold.log, moon-temple.log, moon-cold.log,
mm7-metal-002/scene-first.png, moon-metal-001/scene-first.png and ios-release.log.
