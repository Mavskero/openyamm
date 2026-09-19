# MM8 temple revisit, central chamber and stair puzzle — 090

Actual Mac Metal application, 2026-09-12. Runtime085/runtime-003 and save format85 unchanged.
Only ApplicationSceneCapture.cpp and validation/documentation changed. No iPad or simulator operations.
Final native capture build013 passes (final whitespace wrap). Production iOS build remains088/ios-release-002; no new production
build or disabled full unit suite is claimed. Overall remains40–45%, low confidence.

## Accepted normal route

Input: 089/resupply-004/resupply-letter-complete.oysav, five normally recruited and resupplied members.
Camera aim and normal Forward/Trigger/combat inputs guide the party. No coordinates, stats, items,
quest bits, damage, event results or elapsed game time are injected.

- revisit001: original portals451/453/466, normal temple entry, then original stairs down to Z944.
  44491walkedunits,2292Metalframes. Letter741, Q3/Q221,179gold,14food and all character fields retained.
  Previously killed twelve bats and searched corpses51/55 persist; consumed stones are not recreated.
- chamber005: descend the winding passage into the central hall, trigger original floor12 and wait for
  door2 to finish opening. Normal shared combat, Fire Bolt and healing defeat eleven original bats0–10.
  XP increases[68,68,72,72,68]; HP[30,35,28,46,63], MP[0,23,12,24,0], no conditions.
  7050walkedunits,2152Metalframes. These eleven corpses were not searched.
- stairs001: ordinary Trigger presses original buttons513/514/515/516, MapVar31 advances0→1→2→3→4.
  Moving mechanisms restore the staircase, and the party physically walks from floor0 to floor160.
  8485.7walkedunits,811Metalframes. Lamentia regenerates46→48HP through her existing skill.
- independent reloads revisit001/chamber001 each render181frames, retaining the completed segment.
  reload-stairs001 also passes; final reload-stairs002 explicitly saves at the lower floor before
  ascending again. Restored physical movement is160→0→160,1285.55walkedunits,181Metalframes.
  No button is pressed again. All seven runs exit0; capture counts are in save-comparison.json.

These are normal saved segments, not one uninterrupted starter run. No boat departure or letter
hand-in has occurred. Final continuation save is stairs001/stair-puzzle-complete.oysav.

## Independent evidence and limits

`local-data/validation/mm8-delivery-090/verify-temple.py` and verify-temple003 pass:
seven inputs each preserve all33inspected fields except name across cold/warm restoration;
fifteen static party/quest/NPC/stock/inventory fields remain unchanged across the normal route.
Original items and character fields other than combat resources/XP and ordinary recovery remain.
All23defeated temple bats remain dead, with the original two searched corpses still consumed.
All four button increments and stair mechanisms are inspected, including the final restored state.

Intermediate fourth-button animation is legitimately still closing when the shorter final stair
finishes opening: button48/100=0.48s versus stair32/75≈0.427s. The initial audit assumed both had
finished together; corrected to check the original motion rate and final settled states. A second
audit attempt incorrectly assumed removed corpses always had optional materialization fields.
Both failed audit logs are retained. Neither is a production gameplay bug.

Viewed actual central-hall completion, fourth button, completed stairs and reloaded ground captures.
Chinese “按钮” is visible. Close wall/stair views and existing English Menu remain; these images do
not establish whole-dungeon visual, media or localization acceptance.

Retained driver failures: chamber001 wrong cave edge;002 attempted a raised platform;003 did not wait
for the opening side gate;004 killed all eleven but included combat time in an inapplicable gate
wait timeout. Final005 corrects these driver paths/checks without changing collision or game rules.
Build007 lacked the cmake PATH,009 named the wrong target,011 used an undeclared capture variable;
final012 builds. Source/runtime hashes and source-preservation.json record bounded unchanged paths.

## Next route

The boat is on a different island. Village portal472 requires Q227, which this party has not unlocked;
original island-side471 sets it. For the current normal walking route, traverse the temple to exit502
(position12672,2432,floor384), arriving outdoors at(21519,21106,97), then reach boat house479.
Earlier static searches across the sea did not yield a path; they are not evidence of a collision bug.

Current party is at(57,1344,160), at the north end of the solved central staircase.
The north corridor floor runs x-64..192,y1408..2432,Z160, then turns east down stairs toZ-352.
Door15/door5 guards the next passage atx2112..2176,y2176..2432. Event11 is farther away in room22,
not the central hall north exit. Read original geometry and events before guiding normal movement.
Next: remaining temple passage/encounters, exit502, original boat travel, Ravenshore NPC3/topic28,
verify consumed letter/reward/quest changes and restart. Full starter and subsequent iPad acceptance
remain required; MM6/MM7/cross-world progression, media, Chinese coverage and long play remain open.
