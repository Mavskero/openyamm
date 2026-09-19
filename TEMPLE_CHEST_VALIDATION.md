# Temple door, trap, loot and persistence — checkpoint 072

2026-09-11. Actual Mac Metal gameplay exposed two state bugs, now fixed: indoor doors stopped moving
during turn selection, and a successfully reopened chest lost its Opened flag after a trap discharge.
The latter fix is shared by indoor and outdoor chest activation. This checkpoint accepts the normal
loot transfer and persistence of one temple chest; its visual interface is still incomplete.

All evidence is under `../local-data/validation/temple-chest-072/`. No physical iPad or simulator UI
was queried or operated. All work stays on NVMeSSD. Overall estimate remains 40–45%, low confidence.

## Door motion during turn selection

The original event 3 on face 2819 executes the original door action (raw action 1). The mechanism
entered a moving state, but IndoorRenderer skipped the entire scene simulation while turn mode
was active. It therefore never advanced the door mesh or collision geometry.

IndoorSceneRuntime now separates mechanism motion from world-clock advancement. IndoorRenderer
advances only mechanisms during active turns; normal realtime simulation still invokes the same
mechanism implementation. The existing modal/pending-target pause gate is retained. Moving-platform
support and mechanism audio/collision updates remain in the mechanism step, but actual lift gameplay
has not been accepted by this test.

- Input: `route-002/door-pressed.oysav`, acquired by normal movement and interaction.
- Pre-fix `door-red-003`: after 150 rendered updates, distance remained 0 and the door remained moving.
- Fixed `door-green-004`: distance reached 252, motion completed, and the opening was visually checked.
- Game time 559.478638, the complete inspected turn snapshot and party members did not change.
- Existing actor presentation/death motion/projectile expiry continues in both runs. All 212 actors'
  inspected identity/HP/position/hostility fields match between red and green; not all actor fields
  match their original input. See `door-comparison.json`.

## Normal route and loot

The input chain is the original four-member level-one party's accepted checkpoint 071 encounter save,
then `route-006/door-pressed.oysav`, then `route-009/before-chest.oysav`. Movement uses ordinary shared
Forward input, turn movement budgets, actor collision and original geometry. No coordinates, items,
HP, enemy values, quest flags or rewards were assigned to force an outcome.

The diagnostic equips the existing shield/mace, returns displaced original equipment to inventory,
and learns Heal using the existing book 467. The successful route after the door save travels about
3011.49 horizontal units, uses four normal Heal casts and one potion made from an original berry 200
and bottle 220. That potion heals the knight from 24 to 34. These are gameplay service calls, not proof
of physical touch or keyboard input.

Original event 26 opens chest 2. The first attempt discharges its trap, dealing 12 damage to each
member in the retained successful run; the second attempt opens it. MapVar 3 becomes 1 and QBit 1058
is set by the original event. Party HP is then [22, 12, 6, 8]. Chest contents are:

| Content | Transferred |
| --- | --- |
| Item 2069, 蜡烛台 | 1 |
| Item 1764, 柳条浆果 | 1 |
| Gold | 188; party gold 200 → 388 |

The normal Return-key transfer path removes each selected item and verifies inventory/gold
conservation. Both visible and hidden item lists end empty. This is not a cleared dungeon or
completion of the candle quest.

`route-009` reached and looted the chest, but its capture runner lacked the resumed-door screenshot
token and was stopped after loot completion. It is not a fully passing capture. Final runs start
from its legitimately reached pre-chest save, not a generated position fixture.

## Persistent chest flags

A discharged trap recorded a persistent clear mask for Opened. A later successful open changed the
map flag without clearing that old override. Replaying overrides on reload cleared Opened again;
reopening a cached view also retained stale flags.

`markChestOpened` now publishes a successful open to the map and persistent event overrides, removes
the obsolete Opened clear mask, and preserves unrelated flags. Both world activation paths refresh
the cached chest flags before displaying the view. Failed trap attempts still do not open a chest.

- `chest-flags-red-012` fails the actual reopen check: cached Opened flag is lost.
- `chest-flags-green-013` passes trap/open/loot/Escape/reopen, rendering 181 frames.
- `chest-flags-reload-014` cold-loads the resulting save and loads it again over an existing scene.
  Each save matches 25 inspected top-level fields; only the save name differs. The inspector now
  includes full materialized/active chest item metadata, hidden items, seed, flags and map variables.
- Normal reopening after reload preserves all inspected chest state, characters, gold, quest bits,
  game time and turn state. Actor presentation and projectile lifetime fields can advance during
  rendering. The reload capture also renders 181 frames.
- Two focused cases / eight assertions pass for override replay, repeated opening, unrelated flag
  preservation and a missing event runtime. Actual outdoor trapped-chest interaction remains untested.
- Generic unsigned iOS Release passes in `ios-release-002.log`. Later edits only format diagnostics.

Save format remains version 85. Old inconsistent diagnostic chest saves are retained as failure
evidence, not migrated or used as final acceptance fixtures.

## Visual limitation and remaining work

The actual `route-009/chest-open.png` shows a black chest area. All eight unprefixed MM8 chest
backgrounds are absent from the source, as already recorded by the service converter. Neither the
background nor visible item interaction is accepted here. Import the separately supplied complete
MM8 base, then verify the original background, item rendering, mouse/touch selection and Chinese
inspection text. Do not silently substitute prefixed MM6/MM7 chest art into the MM8 layout.

Earlier failed strategies and diagnostic mistakes are retained: route 001 equipment handling,
route 002 frozen door, routes 005/008 party defeat, route 006 failed after the trapped interaction,
route 007 corner stall, and route 009 missing capture token. Subsequent routing follows actual
room bounds and uses normal healing/spell/item actions; game rules were not relaxed.

Reproduce the saved-data audit from the repository after sourcing `../external-storage/env.sh`:

```sh
../.tools-venv/bin/python ../local-data/validation/temple-chest-072/verify.py
./build-apple/chest_open_state_check
```

`result.json`, `save-comparison.json` and `source-hashes.json` record the evidence and its limits.
No source game data was changed or reimported. The checkpoint 063 runtime is still in use.
