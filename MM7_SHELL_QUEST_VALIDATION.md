# Emerald Island shell purchase and delivery — 098

2026-09-12. The Mac application passes normal shell purchase, delivery and independent restart.
This completes **one of the six scavenger items**, not the contest or the MM7 starter campaign.
Overall remains **45–50%, low confidence**. No iPad or simulator was queried or operated.

## Accepted gameplay

`shell-route-002` starts from unchanged 097/scene-001/after-render.oysav: a normally created single
level-one 中文骑士 with 200 gold, five food, zero XP, seven original items and quest flags513–518.
The game follows ordinary movement and collision from the starting area around the eastern shore,
approaches actual NPC344 (塞莉), selects original topic765 and pays100gold through topic766.
One original shell1437 is placed in the backpack using the normal portrait click handler.

The same session walks back to headquarters1163, opens its original front door, selects judge341
and topic756. The shell is consumed, Q514 is cleared, Q521 is set and exactly500XP is awarded.
Repeating topic756 gives no extra item or reward. No intermediate save is loaded and no coordinates,
character stats, items or quest flags are assigned to bypass gameplay. Tutorial dialogues open normally
and are closed through ordinary Escape. No guide-completion flag is forced.

The route exits0 after **2319 Metal frames, eight captures and33018.5 walked units**. Final position
is7out01(16079.7217,8558.26562,129),100gold, five food,500XP,35HP and no conditions or deaths.
All other five scavenger tasks remain outstanding. The only NPC topic override is344/slot1→766.

`reload-001` independently starts from the completed save, reopens the headquarters and repeats
the judge dialogue. It exits0 after181Metalframes/four captures, preserving the single reward.
The Chinese reply correctly says no currently carried items satisfy the remaining requests.

`verify-shell.py` / `verify-shell-001.log` independently inspect purchase, delivery, completion and
restart observations. The original inventory is unchanged after delivery; all26other character fields
are preserved except XP. Both route and restart cold/warm restores match34fields except save name;
17other static fields remain unchanged. The other five tasks remain and contest-complete519 is absent.
Viewed actual delivery/reopened PNGs show readable Chinese dialogue and the headquarters scene.
The delivery portrait has a temporary patterned effect; these captures do not establish complete
visual, animation, sound or Chinese coverage.

## Reliable route generation and following

Only Mac tools change. `openyamm_route_plan --party` uses OutdoorMovementController to simulate each
proposed edge, with the normal body, full map geometry, decoration/sprite collision and water mask.
It searches a128-unit grid, rejects water/burning support and excessive falls, and never loads its
calculated state into a game. Moving NPCs, events and actual application execution remain separate.
The grid search is a diagnostic proposal and can fail on narrow passages; it is not a complete navigator.

Final forward proposal003 starts at(12552,3600,97), with125points; final return002 has63points.
The already observed initial straight segment is walked normally by the application before the first
proposal point. The first proposal from the original spawn could not find a path; this is retained.

Full gameplay loading is required because lightweight headless loading omits the land mask and
decoration collision. Earlier commentary inferred from height0 that the coastal proposal crossed water.
That inference was premature: original MM7 rows140/141/148 are shoreline transition tiles with flags768,
without the water bit2. The full-data forward proposal is identical to the lightweight proposal and
the actual application walks it without damage. Height0 alone is not evidence of water entry.

The capture driver reads `mm7-shell-forward.tsv` and `mm7-shell-return.tsv` beside its new output directory.
It uses five-unit waypoint tolerance and at least one1/128s input tick. Previous25-unit corner-cutting
entered a real steep triangle; the first new route attempt reached waypoint56 but its sub-tick pulse
rounded to no movement about3.03units from the waypoint. Final005 fixes the test input pulse, not game rules.
Route001 failure and all planning/build logs remain available. Build002 mistakenly placed route fields in
the adjacent MM8 test struct; build003 corrects this. Native004 planner and005 capture pass.

Reproduction uses the existing runtime and normal input save with a new output directory in098:

```sh
./build-apple/application_scene_capture RUNTIME NORMAL_MM7_SAVE NEW_OUTPUT --check-mm7-shell
./build-apple/application_scene_capture RUNTIME COMPLETED_SAVE NEW_RELOAD --check-mm7-shell-reload
```

The two TSV files are required beside NEW_OUTPUT for the first command. The restart mode does not need them.
The independent audit accepts the main/reload directory names under098.

## Preservation and remaining work

Evidence: `local-data/validation/mm7-shell-098/result.json`. Before documentation,47of49recorded097
source paths match; only ApplicationSceneCapture and OutdoorRoutePlan change. Production code, original
game source and runtime085/runtime003/save85 are unchanged. The production iOS build remains
097/ios-release-001; no new iOS build or full unit suite was run for these Mac-tool changes.
Full runtime hashes were last verified at090. All process handles are terminal and reaped.

Next continue from098/shell-route-002/mm7-shell-complete.oysav. Outstanding scavenger objects are
red potion222, longbow845, tile1438, instrument1434 and hat1433. The original starter reagent210
maps to potion224, not222; do not assume it can make the required red potion. Investigate normal
alchemist116 stock/reagents and tavern239 recruitment, then the dungeon and contest/travel sequence.
Full MM7 progression, cross-world persistence, current device acceptance and long play remain open.
