# MM8 northern temple passage — 091

2026-09-12, actual Mac Metal application. No iPad or simulator queried or operated.
Runtime085/runtime-003 and save format85 remain unchanged. Only the Mac capture tool and records changed.
Final native build004 passes; production/iOS remains088/ios-release-002. No full unit run is claimed.

## Normal passage and restore

Input: 090/stairs-001/stair-puzzle-complete.oysav. The five-member party follows the original north
corridor, descends from floor160 to-352, presses the original door15 action with normal Trigger, waits
for door5 to finish its254-unit opening, and follows the curved walkable route across room8.
No coordinate, party, quest, item, reward, event or time injection is used.

Final route003 is one uninterrupted normal passage segment:7359.01walkedunits,709Metalframes,
five captures,exit0. It ends on original flat face702 at(4346.077,2364.132,-352) in room10.
Gold179,food14,letter741 and original quest flags remain; there are no new kills or XP rewards.
All inspected character fields stay unchanged except Lamentia's original Regeneration,48→50HP.
Current HP[30,35,28,50,63],MP[0,23,12,24,0]; the undelivered letter remains in its original inventory cell.

Independent reload001 loads that completed passage and continues normal movement on the landing;
181Metalframes,four captures,exit0. The opened door, party, items and quest state remain intact.
verify-passage002 passes: main input, diagnostic input and completed-passage input each cold/warm
restore all33inspected fields except name. Twenty-four top-level fields remain unchanged across
normal passage; only door5 changes among mechanisms. All existing dead/searched temple actors persist.
Live actor movement during elapsed gameplay is not treated as a persistence defect.

## Diagnosed endpoint assumption

Routes001/002 reached the eastern passage but failed a flat-floor-height assertion at their original
endpoint. Waiting did not change this. Actual collision tracing from that unchanged saved position
showed grounded support on original hidden ramp2208, normalZ0.883452, atZ-340.534, with no jump.
Normal movement about54units onto flat face702 reachesZ-352. Diagnostic landing001 renders181frames
and exits0; no collision behavior or source geometry was changed. The accepted full route ends on
that flat landing. These captures do not establish exact original-engine parity for every ramp edge.
Temporary detailed collision tracing was removed after diagnosis; its original log remains evidence.

Viewed the actual opened doorway and final landing images. The room appears as a full stone floor;
its original non-walkable floor faces have Untouchable, while the winding path is walkable. This is
not a visual acceptance of every hidden floor, trap, light, or dungeon asset. Existing English Menu
and earlier visual/localization/media gaps remain open.

## Continue

Use `local-data/validation/mm8-passage-091/route-003/passage-complete.oysav`.
Original room10 stairs descend southeast through floor centers(4448,2272,-352),(4512,2208,-400),
(4576,2144,-448),(4640,2080,-496),(4704,2016,-544),(4768,1952,-592) to(4843,1877,-640).
Room11 lies beyond(4979,1946,-528), toward the central mechanism area around(6656,2432).
Event62 opens door108;63 initializes the mechanism and closes doors108/109. Eight buttons102–109
change MapVar12 and textures/lights. Inspect their full original logic before attempting the puzzle;
its surrounding floor101 touches the Eradicated variable and must not be treated as safe ground.
Original geometry/events remain in090/d05-face-guidance.json and d05-events-reference.lua.

Next: complete the deeper route to original exit502, reach the island boat479, travel to Ravenshore,
deliver via NPC3/topic28, then restart. Complete starter and later iPad revalidation are not yet passed.
MM6/MM7/cross-world progression, complete Chinese/media coverage, import/input/background and long play
remain required. Overall stays40–45%,low confidence; the goal remains active.
