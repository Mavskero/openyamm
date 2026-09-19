# MM8 temple retracting-floor puzzle — 092

2026-09-12. Actual Mac Metal application, without querying or operating an iPad or simulator.
Runtime 085/runtime-003 and save format 85 are unchanged; content hashes were last verified at 090.
Native build 002 and generic unsigned iOS Release 001 pass. The disabled full unit suite was not run.

## Normal puzzle route

From 091/route-003/passage-complete.oysav, the original five-member party descends the southeastern
stairs, opens entrance door 108 using event 62, walks inside and triggers event 63 at the exit door.
That original event closes doors 108/109, starts four retracting floors and enables eight button lights.
Normal movement and Trigger actions press events 105,104,103,102,109,108,107,106. Original timer 110
then stops floors 110–113 and opens both doors. The party walks out onto the room 12 landing.
No party, inventory, coordinate, quest, event-state or time injection is used.

Route 001 completes this normal segment in 7,491.1 walked units, 1,109 Metal frames and 14 captures,
exit 0. Final position is (7516.084,2431.855,-640). Gold 179, food 14, letter 741 and quest flags 3/221
remain. There are no new kills or XP awards. Character fields remain unchanged except Lamentia's
original Regeneration, 50→51 HP; final HP is [30,35,28,51,63], MP [0,23,12,24,0].

The original puzzle counter progresses 0→1→2…9→15; 15 is the completion sentinel, not a button count.
Each button's map variable, texture and light changes match the original event rules. Completed
floors correctly stop partway through their opening animation: state 3, moving false, distance
568.16626 and elapsed 7083.37549 ms. They must not be treated as ordinary fully opened doors.

## Saved-runtime restoration fix

Reload-mid-001 exposed an actual defect: cold and warm restoration reset a half-finished puzzle's
counter from 5 to 0 and changed its mechanisms, while retaining its half-completed textures/lights.
GameApplication restored the saved runtime, then replayed map-entry/refill effects over that state.

The shared application now skips those entry/refill effects only when loading an existing saved
runtime snapshot. Ordinary map entry and initialization without a snapshot retain their original
behavior. Indoor and outdoor loading use the same condition; timer preparation and application of
the restored event state still run. No original event script or save format was changed.

Final-production checks pass:

- Reload-mid-002 restores the same halfway save, retains the first four buttons and normally
  completes the remaining four: 2,778.24 walked units, 559 Metal frames, 8 captures, exit 0.
- Reload-final-001 restores the completed puzzle and normally walks on its exit landing:
  181 Metal frames, 3 captures, exit 0. Stopped floors, opened doors and presentation state remain.
- Map-entry-001 restores the original outdoor letter save, normally travels through the portals,
  enters the temple and descends its entrance stairs: 2,286 Metal frames, 4 captures, exit 0.
  Original d05 OnLoad event 2 still executes after the normal map-transition confirmation.

The uninterrupted full puzzle route used pre-fix production; its mid/final restores and the normal
map-entry regression used the final production build. This is not one uninterrupted starter run.
No general legacy-save or complete MMExtension compatibility claim follows from these checks.

## Independent audit and visual scope

SaveGameInspect additionally exposes already-serialized indoor texture and light overrides; there
are now 34 inspected fields except the name. Four inputs each restore all 34 fields identically
in cold and warm loading. The audit checks every main-route button checkpoint, 18 static party
fields, original character data, previous dead/searched actors and the partial stopped-floor state.

Verify-puzzle-001 initially assumed event light references were array index + 1. Original MM8 BLV
records instead carry explicit IDs: references 3 and 4 resolve to runtime indices 3 and 2. The raw
60-light source records, positions, IDs and source/parser hashes are retained in
original-light-references.json. Verify-puzzle-002 uses that original mapping and passes. No lighting
production change was needed. Keep this audit mistake separate from the real pre-fix reload defect.

Actual button, solved-button and exit-corridor images were viewed. Chinese button text and the
five-member HUD are visible; these close views do not establish complete room lighting, floor-motion,
trap or visual acceptance. English Menu and earlier media/visual/localization gaps remain open.

## Continue

Canonical continuation: local-data/validation/mm8-floor-puzzle-092/route-001/floor-puzzle-complete.oysav.
Continue through room 12 toward room 13 and the remaining deeper temple route to original exit 502,
then island boat 479, Ravenshore NPC 3/topic 28 letter delivery and restart. The route beyond this
landing, complete normal starter run and iPad revalidation are not yet accepted.

Overall remains approximately 40%–45%, low confidence. The first substantial reassessment to about
55%–60% requires the full normal MM8 starter chain and subsequent iPad validation without a new
foundation-blocking defect. MM6/MM7, cross-world progression, import/input/background behavior,
complete Chinese/media coverage and long play remain required. The development goal stays active.
