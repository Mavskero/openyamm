# Active-turn save persistence — checkpoint 065

Follow-up 072 fixes turn-mode door motion and chest Opened persistence after a trap. One nonempty
chest's normal transfer/cold-warm reload is accepted; its missing-background UI is not. See
`TEMPLE_CHEST_VALIDATION.md` for the exact evidence and limits.

Follow-up 071 fixes fresh monster attacks during action resolution and accepts an actual resolution
save's cold/warm restore and continued impacts. See `ACTOR_ACTION_RESOLUTION_VALIDATION.md`.

Follow-up 066 accepts actual four-member indoor turn saves, continued member actions and movement;
see the 066 section below. The original 065 scope and evidence remain historical.

Saving during combat now preserves the shared turn controller and pending projectile ownership.
Version 84 omitted both: `restoreFromSaveData` reset the turn controller, and the shared projectile
serializer omitted `turnBasedPendingAction`. The projectile could remain in the world while its
turn/action gating was lost. This checkpoint implements version 85 and accepts four outdoor
save/load states through the actual Mac application. No iPad operations occurred. All files,
caches and builds remain on NVMeSSD. Overall estimate stays at 40–45%, with low confidence.

## Persistence contract

Version 85 stores active mode, stage, ordered combatant queue, initiative, remaining enemy/action
timers, movement points, pending action count, completed turn count, movement completion, deferred
game-time advancement and all five tracked member recoveries. A snapshot preserves queue order
rather than starting a fresh round. Previous-frame movement latches and already-used movement-step
flags are cleared on restore; they are input state, not additional saved movement credit.

The existing shared projectile payload now includes its turn-pending marker for both indoor and
outdoor worlds. The save path derives the pending count from the captured world's actual live,
unsettled projectiles, including a save between creation and the next synchronization. Invalid
stages/timers, duplicate or unavailable combatants and pending-count mismatches are rejected before
writing. Truncated turn data fails to load. This does not claim complete corruption validation of
all preexisting save fields.

Map teardown/rebinding ends the old turn controller. Therefore `GameApplication::quickLoadFromPath`
restores the saved turn only after the destination scene, party and projectiles are applied.
`GameSession` seeds the already-synchronized world action count at that point, avoiding a second
registration on the first frame. Existing normal scene-transition behavior remains in place.

Version 84 saves still load and can be saved as 85. Earlier formats did not contain turn snapshots;
the loader explicitly preserves their historical realtime behavior and cannot reconstruct a lost
turn. Older binaries reject the newer version. The current 063 resource package is unchanged and
needs no additional conversion for 065; packages before 063 still need its combat-text reimport.

## Evidence

The focused turn target passes 20 cases / 278 assertions, including five new cases with subcases
for wait/recovery continuation, movement budget and input reset, pending-action gating, indoor and
outdoor projectile serialization, truncated data and invalid-state rejection without replacing a
valid save. These are focused checks, not the entire unit suite.

`capture-001` uses the original normal level-one Sorcerer, existing leather armor and normal spells,
movement and incoming damage. Four snapshots are written and read back equal to live turn state:

| Saved phase | Frame | Queue entries | Pending | Movement points |
| --- | ---: | ---: | ---: | ---: |
| Enemy wait | 5 | 8 | 0 | 0 |
| Player ready | 67 | 8 | 0 | 0 |
| Fire Bolt in flight | 72 | 8 | 1 | 130 |
| After one movement step | 77 | 8 | 0 | 104 |

That encounter later defeats the caster after the selected enemy dies and before loot. Its failure
is retained; generating valid checkpoints is not counted as a complete encounter pass.

Each checkpoint is loaded in a separate Mac process and then loaded again while an old active
scene exists. Both cold and warm saves match the input on all inspected gameplay keys: complete
turn snapshot, active member, character contents, inventory/equipment/recovery, quest bits,
game time, position/view, 260 current-map actor records (including retained aliases), and inspected
projectile identity, position/velocity, damage, age/lifetime and pending flags. Names/previews are
excluded because the diagnostic intentionally renames its output.

After restoring:
- Enemy wait advances to player attack after about 1.048 simulated seconds in the accepted run.
- A ready player stays ready while idle, with the same queue and recovery.
- Spent movement stays at 104 points; the actual Metal HUD reads `回合制 · 可移动 4 步`.
- The in-flight Fire Bolt blocks another action, hits actor 226 once (4→2 HP) and leaves mana at 28.
  It runs for 30 further normal frames without additional damage. Pending count becomes zero.
  Deferred game time advances once by approximately 0.83203125 minutes, matching the saved budget.

The final screenshot readbacks use zero delta after the behavior check completes. Their 180/181
rendered frames are not all advancing world time and do not constitute long-session stability.
Actual movement-HUD image: `reload-movement-003/scene-first.png` (2420×1664, lossless BMP conversion).
Known default portrait and original sprite/audio gaps remain visible or logged.

The prior version-84 064 completed-encounter save also passes independent cold reload, preserving
all inspected gameplay keys and its realtime mode; its new output is version 85. The 260 actor count
is for the current scene, not the sum of canonical/file-name aliases of the same retained scene.

## Build results and retained diagnostics

Mac targets and generic unsigned iOS Release build pass. One diagnostic compile used a nonexistent
actor accessor; it was corrected to inspect the actual world snapshot. The initial enemy-wait
check assumed completion by frame 45; that failed before normal enemy actions resolved. It now
uses simulated time and the existing wait/action timer budget, without changing gameplay timing.
An initial comparison counted two saved aliases of the same map as two damage victims; current-map
counting and equality of both aliases establish the one-victim result. All failure logs remain.
The final iOS build is `ios-release-001.log`; a later production edit only wrapped a long line.
Subsequent functional changes are diagnostic tools. No new resource conversion or device build/install.

Evidence directory: `../local-data/validation/turn-save-065/`. Start with `result.json`,
`all-phases-comparison.json`, `pending-and-legacy-comparison.json` and `turn-tests-001.log`.
Accepted application runs: `reload-pending-002`, `reload-wait-003`, `reload-ready-003`,
`reload-movement-003`; old-save check: `legacy-reload-001`.

## Remaining scope

Actual indoor and multi-member active-turn saves still need application-level acceptance. The
focused tests cover indoor serialization and two-member timing, not an indoor battle. No RNG state
was added to saves, so future damage/AI results are not claimed to be deterministic across loads.
Uncommitted targeting UI, audio playback and transient visual effects are outside this turn snapshot.
Remaining English combat/spell/mode strings, nonempty indoor loot, complete quest/dungeon/travel,
MM8 source gaps, import recovery, long-running behavior and eventual real-device validation remain.


## Checkpoint 066 — actual four-member indoor turns

The existing normal 062 four-member dungeon-entry save is used without changing attributes,
health, mana or enemy rules. Original inventory equipment is equipped through normal checks;
the third member uses the original item 401 to learn Fire Bolt and consumes that book exactly once.
The party enters turn mode through normal Combat input. Members one/two pass; member three casts
Fire Bolt for 2 mana, hitting original actor 125 (9→7 HP); member four passes. One movement step
advances 156.019 units through ordinary indoor collision and spends 130→104 movement points.
No kill, corpse loot, complete dungeon or quest is claimed from this sequence.

Four live-equal snapshots are saved in a 27-entry queue: enemy wait, second member ready, third
member's projectile in flight while the fourth is selected but blocked, and after one movement step.
Each is cold-loaded in a new actual Mac Metal application, then loaded again with an active old
scene. Independent inspection finds no differences in gameplay keys excluding output names/previews:
all four character contents, active member, equipment/items/spells, HP/mana/recovery, quest state,
time and viewpoint, complete turn snapshot, inspected projectile state and 212 indoor actor records.
The inspector now includes actor IDs, precise positions, velocities, sector/grounding, animation,
recovery/cooldown and attack-impact state, in addition to the previous map-delta records.

Continuation passes in all four runs. A restored enemy wait finishes normally; the first member's
normal Pass advances to member two. The saved second member remains selected after reload, then
its normal Pass adds recovery and advances to member three. The saved third-member projectile hits
actor 125 exactly once (9→7), leaves mana `[0, 33, 31, 5]`, and runs 30 further normal frames without
additional damage; member four is still selected. A restored 104-point movement budget supports
another normal collision-checked move of approximately 156.019 units and becomes 78 points.

Actual images show four original portraits, the fourth selection highlight, the Chinese Fire Bolt
hit line and the remaining-three-steps HUD. The source sequence completes at frame 51. Each capture
continues to 181 rendered frames, using zero delta only after its specific behavior checkpoint for
screenshot readback. These are short functional checks, not long-running survival/stability proof.
No physical device or accessory input was used; dummy audio is not listening acceptance.

This checkpoint changes only the capture/inspection tools. Production code, save version 85 and
063 resources are unchanged. Mac diagnostic builds pass. The unchanged 065 production sources
retain their passing generic unsigned iOS Release and 20-case/278-assertion focused checks; these
were not rerun merely for tool additions. Three final capture edits wrap long lines only.

Evidence: `../local-data/validation/indoor-turn-save-066/result.json`, `phase-comparison.json`,
`capture-001.log` and the four `reload-*-001.log` files. Actual images:
`reload-pending-001/scene-first.png` and `reload-movement-001/scene-later.png`.

Remaining: multi-projectile/status/death/roster-change combinations, complete normal quests,
nonempty indoor loot and full dungeon/travel routes, remaining English text, original source gaps
and eventual device/long-session acceptance. Next prioritize a full normal MM6 quest with reward
and persistence checks over repeating these now-accepted basic turn-save cases.
