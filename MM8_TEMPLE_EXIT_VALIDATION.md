# MM8 deeper temple route and island exit — 093

2026-09-12, actual Mac Metal application. No iPad or simulator queried or operated.
Only the Mac capture tool and records changed. Final native build 003 passes; production iOS remains
092/ios-release-001. Runtime 085/runtime-003 and save format 85 are unchanged. Runtime content hashes
were last verified at 090. The disabled full unit suite was not run.

## Accepted normal route

Input: 092/route-001/floor-puzzle-complete.oysav. The original five-member party walks from room 12
through the deeper halls, fights nearby original enemies, climbs two original flights from floor -640
through -128 to 384, activates exit 502 and confirms the normal map transition. The original exit
lands at out01.odm (21519,21106,97), on the island with the boat. No character, inventory, coordinate,
quest, event, reward or time injection is used.

Final route 002 completes this uninterrupted segment in 11,545.8 walked units, 3,285 Metal frames,
8 captures, exit 0. It defeats original actors 23–43: sixteen winged serpents and five serpentmen.
All five members survive without conditions. Final HP is [27,37,28,45,58], MP [0,1,0,24,0].
Gold remains 179, food 14; the undelivered original letter and quest flags 3/221 remain.
No new corpses were searched. Previously searched corpses and earlier puzzle states are preserved.

Six normal Fire Bolts and eleven Heals consume two mana each. Independent original monster-table
reward calculation gives gains [200,200,215,215,200], including existing Learning skills. Character
equipment, inventory, skills, level and other inspected static fields stay unchanged. Plate 1318
fires original event 115 during the route; this proves activation, not all trap effects or their
visual/audio fidelity. The optional side rooms and all remaining dungeon enemies were not cleared.

Reload 001 cold-loads the final outdoor save and also reloads it within the same session, then
continues normal frame updates: 181 Metal frames, 3 captures, exit 0. Characters, letter, quests,
completed mechanisms and indoor defeated/searched actors remain intact. No boat travel has occurred.

## Tool change and retained earlier run

The capture tool adds --check-mm8-exit and shares its existing normal dungeon combat driver with
the prior dungeon/chamber modes. Gameplay rules remain in the existing shared game systems.
The final driver preserves centered pointer input and uses target-edge distance when deciding
whether to attempt a normal melee action; the game still decides reach, hits, damage and recovery.

Route 001 also exited successfully: 5,517 Metal frames, 8 captures, exit 0, but its conservative
300-unit center-distance threshold wasted melee opportunities. Three characters were unconscious
at exit and another was weakened. Preserve that successful but unhealthy route as an earlier result;
use route 002 as the continuation. No production combat change was made to make either run pass.
Native build 001 failed because the driver used an unavailable generic eye-height method; build 002
fixed access to the actual indoor camera, and build 003 includes the improved input strategy.

## Independent audit and corrections

Verify-exit-002 passes: all three inputs restore 34 inspected fields except the save name identically
in cold and warm loads. Nineteen static top-level fields stay equal across hall/upper/exit checkpoints.
Actor IDs remain original, old dead/searched records persist, and the 21 new deaths match the actual
combat log. Original monster-table XP and Learning calculation agree with the final party gains.
The first audit incorrectly required a zero recovery timer immediately after combat; Simon's saved
0.279141486-second recovery at the upper landing is valid. Final cooldowns finish normally. Retain
verify-exit-001 as that audit assumption error, not as a gameplay failure.

Name correction: earlier MM8 notes and driver messages called monster IDs 7–9 "bats". The actual
merged monster table identifies them as 幼翼蛇 / 翼蛇 / 大翼蛇 (Couatl A/B/C). IDs 94–96 are serpentmen.
The rendered winged-serpent image agrees. This corrects report wording; original IDs, numerical
comparisons and saved game state were not changed. MM6 bat encounters are separate.

Viewed actual combat, exit confirmation and outdoor boat-island images. The outdoor image shows
the original boat and pier; it does not prove their interaction or travel. The transition has an
empty black left panel and English text mixed with the Chinese map name. Those presentation issues,
earlier dark geometry, health-bar clipping and media/localization gaps remain open.

## Continue

Use local-data/validation/mm8-exit-093/route-002/temple-exit-complete.oysav.
The party is at (21519,21106,97), game minute 1941.2395, with 179 gold and 14 food.
Original boat 479 is named 顺风号, open 06:00–18:00. Its door is original out01 model 127/face 42,
x=17200, y=21490–21618, z=192–448, event 183. Its route 19 travels four days, all week, to
Out02.odm (8609,-15609,265), with no required QBit. Approach, actual fare and travel remain untested.
Original door geometry is in 090/out01-door-geometry.json. The boat is now physically on this island.

Next: walk to the boat, take its normal route, reach Ravenshore NPC 3/topic 28, deliver the letter,
verify rewards and restart. Then replay the complete starter chain from new game and later validate
on iPad. Overall remains about 40%–45%, low confidence; the conditional 55%–60% milestone is not met.
Keep the full development goal active, including three-world progression, Chinese, user imports,
touch/external input, background recovery, media and long play.
