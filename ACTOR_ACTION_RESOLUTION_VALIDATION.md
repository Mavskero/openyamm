# Monster action resolution and temple entrance — checkpoint 071

The shared turn controller now distinguishes the regular monster phase from the period that finishes
attacks already in progress. Previously both periods ran full AI: an actor could start another attack
while the controller was waiting for the previous attacks to finish. The real Mac application reproduced
this in `6d02.blv`: actor 126 began attacking with decision count 90→91 during resolution
(`encounter-006.log`). Attempt 005 counted decisions too broadly, including pursuit; 006 also checked
that the actor actually entered the attacking state.

`GameSession` passes `ActorAiUpdateMode::FinishCurrentActions` through both world implementations into
the shared actor AI while `waitingForActorActionResolution` is set. Normal animation, impact, spell
effect, death and paralysis handling continue; no new attack or pursuit decision is made in that period.
Realtime updates and the initial monster wait still use full AI. The mode derives from the existing
saved turn state, so the save format remains version 85. This does not constitute a complete audit of
monster initiative rules or the existing action-resolution timeout.

## Regression evidence

Evidence directory: `../local-data/validation/temple-encounter-071/`. Start with `result.json`,
`save-comparison.json`, and `source-hashes.json`.

- Before the fix, the four new resolution cases produced 18 failed assertions in two cases:
  `resolution-red-001.log`.
- Final checks: 25 cases / 349 assertions passed, `resolution-tests-003.log`. Coverage includes ready
  melee/ranged actors, pursuit, already-delivered impacts, one pending melee/ranged impact, unfinished
  swings, death/paralysis cancellation, normal full AI, derived mode after restore, and the earlier
  turn persistence, recovery and movement tests.
- Generic unsigned iOS Release passed: `ios-release-001.log`. No device was queried or operated.
- Both indoor and outdoor hooks compile; the new real-world capture below is indoor. An actual outdoor
  resolution capture remains separate work.

## Normal four-member entrance encounter

`encounter-008` loads the original normal four-member level-one temple-entry save from checkpoint 062,
with 212 materialized map actors. It equips existing weapons, armor and the archer's original bow using
shared equipment rules, and consumes the sorcerer's original book 401 to learn spell 2. It does not
grant items, change attributes/HP/mana, weaken enemies, edit quests or teleport the party.

The capture sends normal movement/attack/pass/combat actions and uses the normal targeted-spell and
corpse interfaces. Camera aiming and input scheduling are diagnostic automation, not physical input
acceptance. The initial encounter is explicitly actors 124–128, selected within 2500 horizontal units
of the entrance; additional enemies can approach and the rest of the dungeon is still populated.

The final run defeats and searches all five initial enemies by frame 1076. Four members remain alive
with HP `[12, 13, 8, 12]`, experience `[60, 60, 60, 60]`, and 200 gold. It completes 14 action-resolution
periods over 327 rendered updates with 40 existing attack impacts and zero fresh combat decisions.
The original experience sharing is preserved: each ordinary bat's 39 XP yields 9 per member, and the
big bat's 96 XP yields 24, totaling 60 per member.

All five corpses happen to be empty. Ordinary bats have no treasure, and the big bat has only a 5%
item chance in the imported original table. This accepts empty-corpse consumption, not nonempty indoor
loot. Original treasure rules are recorded in `original-monster-rules.json`.

The actual Metal application renders 1079 frames. `encounter-008/indoor-loot.png` is a lossless conversion
of its captured bitmap and was visually inspected: dungeon geometry, four portraits, supplies and
the return to realtime mode render. Completed-state screenshot readbacks use zero simulation delta;
this is not a long-session test. Dummy audio is not listening acceptance.

Earlier attempts 001–004 failed and remain evidence. Attempt 007 passed 12 resolution periods but
timed out at corpse collection because the diagnostic repeatedly sent the mode key after its first
tap was refused during the enemy phase. The final diagnostic waits for `canToggleOff` before tapping.
This input timing correction is confined to the capture tool.

## Reload and corpse persistence

`resolution-reload-009` cold-loads and then warm-loads an actual save made during attack resolution.
Both immediate restores match all 23 inspected fields except the intentional save name, including
characters, turn state, projectiles, all indoor actor records and viewpoint. Preview byte count is
included in those fields; this is not a preview pixel comparison or an exhaustive binary comparison.

After restore, four pending actor impacts finish over 32 updates without a fresh combat decision.
The party receives its action; member 0 passes normally to member 1 with recovery initiative 29.
The capture renders 181 frames and stops simulation after the continuation assertions pass.

`encounter-reload-010` uses the real headless application to cold-load and warm-load the final
encounter save. All 212 actors and four members are retained; the five consumed corpses cannot
regenerate loot when searched again. Its saved restore matches the same 23 inspected fields.
The tool subsequently invokes the exit event directly as an existing transition smoke check; that
does not prove walking back to the entrance.

## Remaining scope

Nonempty indoor loot, chest interactions, a complete dungeon/quest traversal, further spell/status/
death/roster cases, and MM8 base assets remain open. There is no new physical iPad, accessory, audio,
full campaign or extended-session acceptance. Original data and runtime packages were not rewritten;
the active complete package remains `combat-text-063/runtime-001`. Overall estimate remains
**40–45%, low confidence**.
