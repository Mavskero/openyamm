# Normal four-member party and indoor combat — checkpoint 062

Follow-up 072 fixes turn-mode door motion and chest Opened persistence after a trap. One nonempty
chest's normal transfer/cold-warm reload is accepted; its missing-background UI is not. See
`TEMPLE_CHEST_VALIDATION.md` for the exact evidence and limits.

Follow-up 071 accepts the first five-enemy temple encounter and persistence of its five empty corpses;
see `ACTOR_ACTION_RESOLUTION_VALIDATION.md`. The original single-enemy scope below remains historical.

Normal creation, initial inventory, indoor combat, empty-corpse persistence and cold reload now have
Mac evidence for four members. This is one enemy kill, not a cleared dungeon or full campaign.
The overall port remains an estimated 40–45% complete with low confidence. No physical iPad was
queried, connected, installed or operated. All outputs and temporary files remain on NVMeSSD.

## Normal creation and session

`openyamm_creation_check --four-members` drives the existing creation-screen methods to add four
slots, choose allowed original candidates, cycle classes, allocate the normal bonus pool and choose
two allowed optional skills. Committed SDL text events supply the Chinese names. Switching between
slots preserves each name, class, points and skills. It is controller-level creation validation,
not an on-screen touch/keyboard acceptance test.

The resulting MM6 party has four level-one characters, no experience or skill points, and original
MM6 portraits offered by the normal character list:

| Name | Class | Character ID | HP | Mana |
| --- | --- | ---: | ---: | ---: |
| 探路骑士 | Knight | 38 | 35 | 0 |
| 晨光牧师 | Cleric | 39 | 30 | 33 |
| 火焰法师 | Sorcerer | 40 | 20 | 33 |
| 远行弓手 | Archer | 41 | 30 | 5 |

The actual new-game application supplies the original starting inventory, 200 gold, five food,
the single MM6 letter and its two quest flags. A separate process reloads the starting save without
changing any inspected character field. The ordinary entrance event brings this party into
`6d02.blv`; it does not prove walking from town to the entrance. This new session materializes 212
actors. It is a fresh session, not a claim that the prior 223-actor save lost eleven actors.

## Actual Metal combat

The diagnostic equips usable weapons and armor from each member's existing inventory using the
normal equip policy, without granting items or changing character/monster stats. Equipped main-hand
item IDs are 41, 79, 21 and 31; available armor 84 is equipped on members zero and two. The previous
solo check used sword 1 explicitly; this route selects the first usable existing weapon.

Camera yaw and pitch aim at the selected enemy's body. Complete SDL movement and Attack taps pass
through normal application input and gameplay frames. The final route attacks as soon as the
target is in range, without requiring the party to wait for a hit first.

`combat-004` renders 214 frames, kills actor 125 (ordinary Bat, monster 481, 9 initial HP), searches
its empty corpse and saves. Each member receives nine experience; the party retains all 115 HP and
200 gold. The imported original monster row specifies zero loot for this ordinary bat, so an empty
search is the correct result. The inspector records a dead, invisible actor and a materialized,
consumed corpse with zero items. A repeated search cannot create a loot view or grant gold.
This does not validate transferring nonempty indoor loot, a cleared room, healing or every class's
attack mode. Incoming party damage occurred in failed earlier runs, not the accepted final run.

The actual capture shows four distinct original portraits, visible health/mana bars, an indoor
minimap and the bat scene. The hit/kill status still contains hardcoded English around Chinese
names. That remains a localization gap; this checkpoint does not claim to fix it.

## Cold reload and return

The enhanced indoor reload check compares every member's identity, HP, mana, conditions and
experience, plus every actor's identity, HP, coordinates, flags and group. It separately verifies
that stored consumed-corpse markers survive and cannot reopen.

A new process loads `combat-004/after-render.oysav`, preserves all four characters and 212 actors,
saves/reloads, then follows the ordinary exit event back to New Sorpigal. Independent save JSON
comparisons preserve equipment, inventory, skills, position/view, gold, food and quest bits. The
dead bat remains empty and each member still has nine experience after reload and town return.

## Unsuccessful attempts and diagnostic corrections

- `combat-001` kills the bat, but the old diagnostic insists on a nonempty loot view, waits and
  eventually loses the party. Its actual save proves that the empty corpse had already been
  searched. It is not a successful whole-route capture.
- `combat-002` understands empty corpses but still waits for incoming damage and aims horizontally.
  The party loses. The diagnostic's aim did not consistently point at the chosen bat's body.
- `combat-003` corrects aim and allows prompt attacks, completing the kill and empty search. Its
  optional evidence save uses a bare relative filename, which the save writer cannot create;
  that run ends in an error. The diagnostic now supplies an absolute external path. No save-writer
  production behavior or combat values were changed to make the test pass.
- `combat-004` is the accepted complete capture after those diagnostic corrections. Earlier failed
  saves/logs remain available and are not reported as full passes.

## Build and scope

Only validation tools changed in this checkpoint: creation, capture, save inspection and indoor
reload. Their Mac builds pass. Runtime packages were reused from checkpoint 059 without conversion
or duplication. Production application code did not change, so the latest generic unsigned iOS
Release build remains checkpoint 061; an additional identical iOS build was not run or installed.

Evidence: `../local-data/validation/multi-party-combat-062/result.json`, `save-comparison.json` and
the named run directories/logs. All original game files are unchanged.

Next work remains full combat/quests and travel, Chinese combat feedback, active-turn save behavior,
nonempty indoor loot, MM8/base source gaps, import recovery and longer sessions. The combat controller
currently constructs English messages in `GameplayCombatController::formatPartyAttackStatusText`
and separately in projectile impact presentation; both paths need the same content-owned text
policy. No global text catalog was found in the current imported engine package.
