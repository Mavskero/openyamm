# MM8 full normal starter — 096

2026-09-12. The Mac application's normal new-game starter route and final restart pass.
No iPad or simulator was queried or operated. All 41 previously recorded source paths and both
validation binaries matched checkpoint 095 before documentation. Native 095/build-011 and production
iOS 095/ios-release-001 are reused; there was no new production change, build or resource import.
Runtime 085/runtime-003, save format 85 and the disabled full-unit-suite status remain unchanged.
Full runtime content hashes were last verified at 090.

## One normal new game, all sixteen stages

`application_scene_capture --check-mm8-replay` starts through normal character creation and the
application's normal creation callback. It creates 中文骑士, a level-one Knight with 0 XP, 200 gold,
five food and the original starter items. Normal bonus points and two optional skills are allocated;
God Lich and incomplete creation are disabled. Intermediate saves are observations, never loaded
back into this run. The run log confirms normal-new-game origin and no saved-session load/reset.

The same session completes crystal quest/recruitment, inn recruitment, shop trade, two stone portals,
Simon recruitment, lower dungeon, potion recovery/loot, return, lodging/letter pickup, dungeon revisit,
central chamber, four-button stairs, northern passage, eight-button retracting floors, temple exit,
boat travel and letter delivery. These are the sixteen registered test stages, using normal movement,
combat, inventory, service and dialogue handlers. No coordinate, party, item, reward or QBit injection.

The route rendered **24,645 Metal frames**, produced **94 captures**, walked approximately **261,585
game coordinate units** and exited 0. The five-member roster is [0,2,1,7,3]. This generated encounter
contained seventeen lower-floor winged serpents (fifteen defeated during descent), eleven central
enemies and fifteen exit-route enemies. The driver reads this run's actual actors and shop stock.

The Knight fell to -8 HP during the lower encounter. Normal original healing/mana potions and Heal
recover the party; two healing and two mana potions are consumed, while an unnecessary antidote remains.
The two actual searched corpses give 7 and 8 gold. Gold progresses 200 → 195 after shop trade → 210
after corpse searches → 206 after food/lodging → 137 after the 69-gold boat fare → 2637 after delivery.
The ship follows the original four-day route and arrives at out02 (8609,-15609,265).

Letter 741 is consumed once, Q3/Q221 cleared, Q4 set, autonote 494 added and NPC 3's topic slot 0 changed
to 29. The reward is exactly 2500 gold and 7500 XP for each member. Final out02 position is
(12863.9736,-12237.9375,257), game minute 7705.09961, 2637 gold, 14 food and zero party deaths.
HP [35,45,28,51,75], MP [0,51,52,24,0], all conditions clear. Earlier poisoned/rescue variants remain
separate accepted evidence in 095; this successful whole run does not replace those regression cases.

## Independent save audit and restart

The 095 replay auditor accepts an absolute run prefix and passes for this complete new-game run.
It checks ordered stage completion, absence of intermediate save loads, original quest/item progression,
consumed-corpse persistence, boat fare/time, exact reward and retained dungeon state. It explicitly
reports `new_game_continuous_desktop_pass=true` and `physical_ipad_accepted=false`.

The additional 096 auditor preserves nineteen identity/base-stat/skill fields for all five characters,
compared with their actual creation/recruitment saves. Original unused Knight items retain properties
and grid positions; original sword 1 and armor 84 are normally equipped. There is no automatic training
or character replacement hidden in the progression.

A separate process cold-loads the final save, reloads it within that process, then normally reopens the
recipient's front door. Both restores preserve all 34 inspected fields except save name. Twenty-seven
static fields remain exact through the reopened dialogue, including characters, items, quests,
inventory, prior dungeon actors/corpses, chests, mechanisms and presentation. Topic 28 is absent;
there is no repeated reward. The restart run renders 181 frames, captures four images and exits 0.

## Scope and next work

Viewed actual recruitment, solved-puzzle and final-delivery-area images. The puzzle capture faces a
wall and does not establish full floor/lighting visual quality. English recruitment/status/Menu labels,
prior scene/enemy-bar/media gaps and visual polish remain open. This is one complete normal starter
route, not every optional room, every random start or MM8 campaign completion. Frame counts are not
a performance or long-session stability benchmark.

Overall rough assessment moves to **45%–50%, low confidence**, based on the new continuous desktop
starter and restart evidence. The conditional 55%–60% stage still requires the current flow on iPad,
including relevant import/input/background checks, without a new foundational blocker. Current work
continues without devices: MM7/MM6 representative progression and cross-world persistence, remaining
Chinese/media/input/import recovery and stability remain in scope. The full development goal is active.

Evidence: `local-data/validation/mm8-full-starter-096/result.json` and the associated audits/logs.
Current complete starter save: `local-data/validation/mm8-full-starter-096/replay-001/full-starter-complete.oysav`.
