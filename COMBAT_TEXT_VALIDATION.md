# Shared combat text — checkpoint 063 (2026-09-11)

Follow-up 064: `PROJECTILE_TARGET_VALIDATION.md` establishes that the outdoor third projectile
hit actor 225 before selected actor 226. A subsequent normal outdoor kill/loot/cold-reload route
passes and actual Chinese projectile-kill rendering is accepted. The historical 063 failures
below remain unchanged; the indoor route and actual stun/paralysis are still open.

Five combat messages now come from imported original text instead of English literals in the
shared combat controller. This checkpoint accepts the text import/formatting implementation and
observed Mac rendering, not complete encounter regression or complete Chinese localization.
No physical iPad operations occurred. All output remains on NVMeSSD; overall progress stays at
40–45% with low confidence.

## Content and runtime contract

The core converter reads `global.txt` from the supplied `10 LocZHCN.EnglishT.lod` when present,
otherwise from `mmmerge.T.lod`. It exports these exact original templates without rewriting them:

| Source ID | Semantic key | Original Chinese template |
| --- | --- | --- |
| 164 | `combat.melee_hit` | `%s攻击%s，造成%lu点破坏` |
| 189 | `combat.projectile_hit` | `%s射击%s，造成%lu点伤害` |
| 175 | `combat.kill` | `%s造成%lu点伤害，杀死%s` |
| 635 | `combat.stun` | `%s打昏了%s` |
| 636 | `combat.paralyze` | `%s麻痹了%s` |

`engine/data_tables/combat_text.txt` is shared base content. `CombatTextTable` validates all five
keys and the exact original argument signatures, then substitutes literal names and nonnegative
damage. It never executes imported printf strings. Kill arguments are attacker/damage/target;
hit arguments are attacker/target/damage. Percent characters inside names remain literal. The
shared melee presentation and delayed projectile-impact branches use the same table. Actual
applied damage and existing kill/paralysis/stun priority are preserved; combat rules are unchanged.

This is an explicit content migration: older converted packages lack the new required table and
must be regenerated from the original input before using this build. The common loader rejects
missing/invalid tables with a reimport diagnostic. It does not silently restore hardcoded English.
The development asset includes the original English templates. Save schema is unchanged.

## Accepted checks

- Six focused C++ cases / 57 assertions pass for original Chinese/English text, argument order,
  applied damage, status priority, weapon modes, percent-containing names and invalid catalogs.
  The tests are also registered in the standard unit-test target; that whole suite was not run.
- Nineteen native core-import tests pass, including missing/duplicate IDs, incorrect signatures,
  incomplete Chinese catalogs, original input preservation and existing-output protection.
- Full native conversion validates all 140 supported MM6/MM7 maps. Four ZIPs contain 27,528
  CRC-checked entries. Compared with 059, the only addition is `combat_text.txt`; no previous
  payload changes or disappears. The five Chinese rows exactly match the supplied archive.
- A previous 062 four-member save loads with the new package in a separate process. All 212
  actors, four members and one consumed corpse pass the existing persistence assertions, then
  the normal exit returns to town. An old 059 package fails explicitly on the missing table.
- Mac targets and generic unsigned iOS Release build pass. The iOS build uses external DerivedData;
  nothing was installed or launched on iPad.

## Actual rendering and encounter limits

Reviewed 2420×1660 Mac Metal screenshots show complete, readable status lines above the portraits:
`探路骑士攻击蝙蝠，造成8点破坏`, `远行弓手造成11点伤害，杀死蝙蝠`, and
`中文法师射击低级魔法师，造成1点伤害`. Font sizes/layout were not changed.
The diagnostic records status text and captures the rendered frame when it changes.

The indoor run reaches melee hit and kill messages but does not finish the selected-target route:
actor 125 remains at 1 HP, moves away, and the party eventually falls. Its kill message therefore
must not be presented as killing that selected actor or as a successful dungeon encounter.

Both outdoor runs reach actual projectile-hit messages and two verified hits on actor 226. Their
third projectile produces damage feedback but does not lower that selected actor's HP, so the
existing route assertion fails. A hit on another actor is plausible, but exact target identity
was not recorded and is not established. Both failures and saves are retained. Projectile-kill
rendering, actual stun/paralysis encounters and complete encounter regression remain unaccepted.
These failures were not hidden by weakening assertions, changing stats or repeating until a pass.

Target-selection, casting and evasion messages still include English. This checkpoint does not
import every Global.txt entry or complete all combat/spell localization. Known original sprite,
audio and MM8/base source gaps persist; dummy audio is not listening acceptance.

## Evidence and build recovery

Evidence: `../local-data/validation/combat-text-063/result.json`, `source-comparison.json`,
`package-comparison.json`, focused test logs, `cold-reload-001.log`, and `ios-release-003.log`.
Reviewed images are `indoor-001/combat-status-1.png`, `indoor-001/combat-status-2.png` and
`projectile-002/combat-status-4.png` in that directory.

Initial Mac linking failed because the new source was absent from the explicit game CMake list.
The first iOS build was interrupted to correct that list; the next used the prior loaded Xcode
build graph while regenerating the project and failed linking. A fresh invocation compiles the
new source and succeeds. One capture rebuild omitted the external CMake PATH; its corrected run
passes. These logs remain available and are not counted as successful builds.

Next: identify actual projectile targets in the failed route, finish remaining shared spell text,
define active-turn save behavior, and continue nonempty indoor loot and complete quest/dungeon play.
