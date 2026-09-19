# MM8 normal starter replay — 095

Follow-up: checkpoint 096 passes a fresh sixteen-stage normal new game and final restart using this
checkpoint's final code. See `MM8_FULL_STARTER_VALIDATION.md`. The partial/failed 095 runs below remain
their original regression evidence and are not relabelled as full new-game passes.

2026-09-12. Mac Metal validation only; no iPad or simulator queried or operated.
Runtime 085/runtime-003 and save format 85 remain unchanged. Full runtime content hashes were last
checked at 090. Production iOS Release 095/ios-release-001 passed without signing or device deployment.
The full unit suite remains disabled and was not run.

## Production fix and actual regression

An unconscious member cannot become the active gameplay actor. The shared portrait input controller
previously returned after a failed selection before remembering the first click, so a second click
could never open that member's inventory. It now validates the member and remembers the first click
before returning. A single click still leaves the active actor and closed inventory unchanged;
a double click opens the selected unconscious member's backpack without selecting them for an action.

Replay 003 reproduced the failure with the original Necromancer at -3 HP and the healer active.
Continuation 001 verified inventory opening but exposed an automation assumption: unconscious inventory
icons are rendered without inspectable-item metadata. The normal grid input still supports taking
items. The driver now clicks the actual item's backpack grid position, using the normal shared input.
No rendering or item-use rule was changed to accommodate the driver.

Continuations 002 and 003 both verify the actual original four potions: owner 2's healing potion
revives member 0 from -1 to 9 HP, its mana potion restores member 1 from 1 to 11 MP; owner 3's healing
potion revives member 2 from -3 to 7 HP, and its mana potion restores member 1 from 11 to 21 MP.
Each original item is consumed once. Unused item properties, grid positions, equipment, skills, levels,
quests, food and gold are preserved. The unnecessary antidote remains. Both runs also verify that
opening the unconscious owner leaves active member 1 unchanged. `verify-supplies.py` passes for both.
The first audit compared compact inventory summaries with detailed entries; the corrected projection
retains the detailed per-item checks. This was an audit mismatch, not a production defect.

## Continuous route driver

`--check-mm8-replay` starts through the application's normal character creation screen: Jadame,
level-one Knight, normal bonus allocation and optional skills, committed Chinese name 中文骑士.
It uses the normal creation callback and never reloads an intermediate save into that continuous run.
The sixteen stages cover crystal/recruitment, inn recruitment, shop, portals, Simon, lower dungeon,
supplies/loot, return, resupply/letter pickup, revisit, central chamber, stairs, northern passage,
retracting floors, temple exit and boat/delivery. Saves and screenshots record stage boundaries.

`--check-mm8-replay-from=N` is a separate diagnostic continuation from an existing normal save.
Its result is named `starter-continuation-complete`, never `full-starter-complete`. It does not establish
a continuous new-game pass. The driver preserves the original Simon greeting between entry and
recruitment, reads actual generated shop stock and identifies the current region's actual enemies
and corpses. Runtime actor indices and random loot are not assumed equal across different new games.
All movement, combat, item operations, services and dialogue effects use normal gameplay handlers;
there is no coordinate, party, inventory, reward or quest-state injection.

Continuation 003 passed stages 6–14 continuously: it defeated the two remaining lower-floor enemies
(seventeen were already dead in its input), searched two corpses, returned/resupplied, defeated eight
central enemies, completed both puzzles and defeated twenty-one exit-route enemies.
Its two searched corpses each gave 7 gold: 200 → 214; food and room cost 4, leaving 210 and 14 food.
Its final boat assertion incorrectly expected healthy recovery despite two severely poisoned members.
Delivery 001 resumes its normally reached temple exit and completes boarding and delivery: 69 gold
fare, four-day voyage, 2500 gold and 7500 XP per member, one letter removed, Q3/Q221 cleared, Q4 set,
autonote 494 and NPC 3's next topic. Independent reward/state audit passes. Final gold is 2641;
members 2/3 retain severe poison, with HP/MP recovery reduced accordingly. Poison is not silently cured.

## New-game variation and lodging

Fresh replay 004 passed stages 0–7 continuously. It generated a different lower encounter and the two
searched corpses gave 3 and 5 gold. The Knight became severely poisoned before the first lodging visit.
That exposed the same healthy-only assumption in the inn driver; the game correctly retained poison
and restored a quarter of maximum resources. The test expectations for lodging and travel now cover
the observed healthy/severe-poison states and explicitly reject unmodelled conditions.

`inn-poison-001` loads that exact return save and passes normal paid lodging and letter pickup. Its
cold/warm loads preserve all 34 inspected fields except the save name. Gold 199 → 198 → 195, food
5 → 14; the poisoned Knight rests to 8/35 HP while healthy members recover fully. Character properties,
inventory, previous dungeon/corpse/mechanism state and poison remain intact. One letter is received.
`verify-inn-poison.py` passes. Gameplay rest rules were not altered.

## Current status and limits

Fresh replay 005 stopped in stage 6 with the healer unconscious at 0 HP. The old fixed potion recipients
left him unconscious, so requesting Heal correctly failed. The normal replay driver now gives healing
potions to an unconscious healer first and directs the antidote to an actually poisoned member.
Continuation 004 passed from that exact stage-5 save using native build 011. Four members began at
HP [-6,0,-2,0]; the healer received the first healing potion and the Knight the antidote. The healer then
normally cast Heal to recover the remaining party. The original five potions were consumed once,
including items taken from both unconscious owners; the expanded independent supply audit passes.

All ten remaining stages (6–15) passed continuously: 178,039 walked units, 17,111 Metal frames,
60 captures, exit 0. This fresh encounter had five central enemies and twenty-two exit-route enemies.
Final 2641 gold/14 food, HP [35,45,7,51,75], MP [0,51,13,24,0]; only the Necromancer remains severely
poisoned. The independent replay audit verifies corpse persistence, quest/item progression, fare/time,
exact letter reward and previous dungeon state. Its first version assumed every actor had optional
corpse fields; the corrected audit follows the inspector's explicit omission for unmaterialized records.

Reload 001 passes cold and warm load (34 fields except name exact) and normally reopens the recipient.
Twenty-seven static fields, including characters, items, poison and the previous dungeon, remain exact;
the completed topic is absent and the reward does not repeat. 181 frames, four captures, exit 0.
`verify-reload.py` passes. The current continuation save is
`local-data/validation/mm8-replay-095/continue-004/starter-continuation-complete.oysav`.

No full continuous new-game milestone is accepted yet. Earlier failures remain separate evidence:
replay 001 input-path handling;
002 Simon dialogue handoff; 003 unconscious portrait inventory; 004 healthy-only inn expectation.
Continuation 002 also rejected never-opened corpses because their loot records were not materialized;
the corrected driver treats an absent record as unsearched, as the actual game does.

Native builds 007–011 pass. Build 006 was a capture-tool field-name error; build 004 followed a failed
edit and reported no work, so it is not acceptance of that intended edit. The current production change
is the shared portrait controller only; other route edits are confined to the Mac validation tool.
Viewed actual unconscious inventory, recovered-dungeon and full Chinese delivery screenshots. Earlier English text,
HUD/scene/media defects and long-play coverage remain open. Desktop input calls do not establish
physical iPad touch, keyboard, pointer, background recovery or import acceptance.

Next run a fresh complete new game with the corrected normal rescue/rest strategy, then reload its final save.
Overall remains roughly 40%–45%, low confidence. Full normal starter plus later iPad revalidation is
the conditional 55%–60% milestone. MM6/MM7/cross-world progression, remaining Chinese/media/import/
input/background and stability work stay in scope; the active development goal remains unfinished.
