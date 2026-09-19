# Normal MM6 quest routes

## 067: first letter, tavern stage

The normal four-member creation save from 062 walks from New Sorpigal's original start to
the Lonely Knight tavern through shared session Forward input and ordinary collision/actor
simulation. Waypoints only choose camera directions; no position, inventory, stats, quest
bits, NPC topics or rewards are assigned by the driver. The walked path is about 3141.87
world units. Normal Trigger opens house 260; the returned dialogue actions select resident
786 and topic 1314. These are programmatic inputs, not physical accessory or touch acceptance.

The imported original Global.lua specifies this stage's result: 1000 gold, clear QBit 1105,
set QBit 1106 and change NPC 786's first topic to 1315. The letter (item 2125) remains in
inventory and no XP is awarded here. Actual Mac Metal execution matches those rules: gold
200 to 1200, quest bits [1104,1105] to [1104,1106], the original four complete inspected
character/inventory records unchanged, one letter retained, and no reward on topic 1315.
The Chinese response fits the captured dialogue panel with the original four portraits.

A separate application process loads the resulting save, immediately resaves before advancing
gameplay, re-enters the tavern and selects the follow-up topic twice. The repeat gives no gold.
All 23 inspected top-level fields except the intentionally changed save name match on cold
restore, including NPC topic overrides, quest bits, four characters, clock, position, camera,
turns, projectiles and both stored outdoor snapshots. Those snapshots have keys `oute3.odm`
and `world.mm6.map.oute3`, with 274 actor records each; they are not 548 distinct town actors.
The pre-existing alias duplication is retained, not silently migrated in this tool-only check.

`application_scene_capture` accepts `--check-first-letter` from the normal 062 start and
`--check-first-letter-reload` from `letter-001/letter-complete.oysav`. Both require independent
captures and a new output directory. `openyamm_save_inspect` additionally exposes sorted
character awards and global NPC topic overrides for quest persistence checks.

Mac diagnostic builds pass. Production and test sources hash-match the pre-checkpoint state;
063 resources and save version 85 are unchanged. The passing 065 generic unsigned iOS Release
and focused 20-case/278-assertion checks still apply and were not repeated for tool additions.
No physical iPad operations were made. Dummy audio is not listening acceptance; missing
`runstone`, `enter`, `gold01` and `chimes` cues remain visible in the logs.

Evidence: `../local-data/validation/first-quest-067/result.json`, `save-comparison.json`,
`source-rules.json`, the run logs and `letter-001/letter-reward.png`. The original run renders
245 frames, and the cold reload run 181; post-checkpoint screenshot waiting uses zero delta.
The first reload run also passed; a second was needed to add the immediate pre-frame save.

## 068: coach travel, castle delivery and stair movement

Checkpoint 068 continues the actual 067 tavern save. The normal level-1 Knight, Cleric,
Sorcerer and Archer retain their original inventory, equipment, HP and mana. Shared Forward
inputs walk to house 470 and select the returned transport service. Original stable row 19
uses route 60 on Monday/Wednesday/Friday: two days to `outd3.odm`, with destination
(14317,2696,96). This run pays 25 gold, advances exactly 2880 minutes in the service action,
and arrives at foot height 97 after ground snapping. Saved checkpoints differ by 2880.038815
minutes because an ordinary frame runs before the arrival save. Fare modifiers, other weekdays
and other destinations are not covered by this one trip.

Normal walking then follows terrain and original collision geometry to house 222. Early
navigation candidates hit buildings, steep terrain and an original tree; these are retained
as failed route diagnostics. The final approach exposed an actual movement defect: the
32-unit first riser of model 57 `IrHall` (face 53, y16264, z2080..2112) blocked the party at
about y16227,z2081. The body radius is 37, but terrain-map step floor queries only checked
five units beyond the body center, so they could not see the tread where collision stopped
movement. A standalone regression reproduced the block before the fix.

The shared outdoor controller now gives horizontal party step probes the body footprint and
existing collision separation. It resolves final support using the newly reached face, with
its polygon and height checked at the final position. Retained party support includes the
same small collision separation; actor conditional probes retain their original radius.
Ordinary discovery of new floors is not expanded. Existing step-height, notAStep and headroom
checks remain. Seven cases / 26 assertions pass: ascending five steps, stopping at the edge,
descending, height limits, prohibited steps, insufficient headroom, tall walls, idle terrain
and releasing support beyond the footprint. These fixtures need no game assets or devices.

The final Mac Metal run (`route-final-010`) walks about 18411.7 units across both maps and
reaches (17132.8,16755,2241). Camera directions guide movement; no position is assigned.
Normal Trigger opens house 222. Returned resident 789/topic 1322 actions deliver the letter:
gold 1175 to 6175, 3000 XP and Award 100 to each member, item 2125 removed, QBits 1105/1106
cleared and NPC 789's first topic changed to 1323. Every other inspected character/inventory
field is identical to the normal input. Selecting 1323 starts QBit 1110 and changes the topic
to 1324; two further selections without the shield give no additional reward. The Kilburn
quest is only accepted, not completed.

A new process (`cold-reload-011`) loads the pre-Kilburn delivery save and resaves immediately
before advancing gameplay. All 23 inspected top-level fields except the intentional save name
match, including complete inspected character and actor records, quests and NPC topic overrides.
This save has 419 Ironfist and 274 New Sorpigal actor records, each under both a legacy and a
namespaced map key; those aliases are not additional distinct actors. The restored party can
re-enter the throne room, accept QBit 1110 and select 1324 twice without duplicate rewards.
Original spawning is not deterministic across independent coach trips, so actor counts from
separate route attempts must not be substituted for these cold-restore counts.

Actual 2420x1660 Metal screenshots show the seven-line Chinese delivery response and five-line
follow-up fully within the dialogue area. The route renders 631 frames (checks finish at 630),
and cold reload 181 (checks finish at 75). Completed checks freeze world time for remaining
screenshot readbacks. This is not long-session or physical-input acceptance.

Mac builds and generic unsigned iOS Release pass; the iOS build uses external DerivedData.
The only post-build production edit reflows one function call without changing non-whitespace
text. No redundant build was run for formatting. The full unit suite was not run. Save version
85 and 063 resources remain unchanged; no new import is needed. Temporary geometry-dump code
and collision tracing were removed from the final capture driver. A failed repeat-check run
009 reset its own phase timer; that driver error was corrected before final runs 010/011.

Evidence: `../local-data/validation/castle-letter-068/result.json`, `save-comparison.json`,
`step-before-001.log`, `step-final-004.log`, `ios-release-001.log`, `castle-source-rules.json`,
and `route-final-010/castle-reward.png` / `castle-followup.png`.

## Current acceptance boundary

The first complete MM6 letter chain is accepted on Mac through checkpoints 067 and 068:
normal creation state, tavern stage, normal coach travel, castle approach/delivery, original
rewards and cold persistence. It does not establish general quest or full-game compatibility.
No physical iPad operations were made. Missing footstep/enter/reward audio remains; dummy
audio is not listening acceptance. The coach sentence remains partly English. More complete
quests, dungeons and nonempty indoor loot, spell/status/death/roster cases, original MM8/rifts,
remaining media, import recovery, long sessions and later device validation remain open.
Overall game progress remains a low-confidence 40–45% milestone estimate.
