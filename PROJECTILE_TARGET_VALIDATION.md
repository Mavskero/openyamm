# Projectile target identity and encounter evidence — checkpoint 064

This checkpoint diagnoses the outdoor failure from 063 and accepts one normal Mac encounter:
original equipment, movement, Fire Bolt hit/kill, corpse loot and independent cold reload.
It changes diagnostic tools only. Production collision, rendering and save schemas are unchanged.
All work and evidence are on NVMeSSD; no physical iPad operations occurred. Overall estimate
remains 40–45%, with low confidence. This is not a completed dungeon or campaign.

## Actual projectile target

The original solo level-one Sorcerer save has 16 HP, 30 mana, 200 gold and existing leather armor
(item 84) in inventory, with the armor slot empty. Selected actor 226 starts with 4 HP.
In `trace-001`, production collision records identify party spell 2 projectiles 9/10 hitting
actor 226 and projectile 11 hitting actor 225 first. Independent save inspection confirms
225: 6→4 HP, 226: 4→1 HP, mana 30→24; every other actor's HP is unchanged.
The two actors have the same displayed name, so a status message alone cannot identify the victim.

`OutdoorWorldRuntime::buildProjectileCollisionFacts` considers the earliest eligible collision.
Selecting actor 226 does not exempt actor 225 from intercepting the projectile. Collision rules
were not changed to force a hit on the selected actor. The diagnostic now snapshots actor identities
and HP immediately before the cast, requires a single live hostile victim to lose HP, and reports
selected/actual identities separately. It may continue against that actual blocker. The accepted
run below hits 226 both times and does not itself exercise this retarget branch; the separate trace
establishes the interception diagnosis.

## Normal encounter and persistence

`combat-006` equips existing item 84 via the same equipment/skill checks used by gameplay. It adds
no item, skill or stat. Two normal movement steps cover 311.994 world units, consuming movement
points 130→104→78. The caster takes incoming damage and casts twice: actor 226 HP 4→3→0, with
mana 30→28→26. The production combat trace independently identifies the same victim and applied
damage 1/3. Pending projectiles block actions until resolved, the dead actor leaves the turn queue,
and ordinary corpse interaction grants 5 gold without duplicate loot.

The completed state has 7 HP, 26 mana, 24 experience, 205 gold, 5 food and consumed corpse 226.
It is saved after returning to realtime, then rendered with zero delta only while remaining
screenshots are collected. Fighting was not paused. A new process loads and saves the checkpoint.
All compared character contents, equipment, inventory, quest bits, game time, location/view and
260 retained outdoor actor records match both the post-capture save and cold-reloaded save.
These checks do not establish active-turn save behavior or survival after normal time resumes.

The save inspector now exposes outdoor snapshots, including actor identity, position, HP/death,
visibility and materialized/consumed corpse contents. It reads through the normal save loader;
it does not change the save format or rewrite test input.

## Screenshot request correction

bgfx accepts only one screenshot request per framebuffer per frame. Both a status-change label and
an encounter milestone sometimes requested that same frame. A temporary callback queue and a
three-request smoke probe did not solve this: both probe runs received only one image and failed
with exit 4. Those unsupported-usage probes are retained, not counted as a red/green renderer test.
The temporary engine and smoke-tool changes were reverted to their pre-064 contents.

The capture tool now gathers labels per frame, issues one bgfx request and writes that actual
readback under each applicable label. No frame is synthesized or relabeled from a different time.
The accepted run renders 240 frames and obtains all 13 BMP artifacts; three pairs of same-frame
aliases are byte-identical. Actual 2420×1660 Metal screenshots show the readable original Chinese
kill line `中文法师造成3点伤害，杀死低级魔法师` and the post-loot state. PNGs are lossless conversions
of those captures. The final few zero-delta readback frames are not a long-running stability test.

## Failures, builds and limits

Earlier runs are retained: the old selected-target assertion fails in trace-001; unarmored routes
can die before loot or while collecting screenshots; combat-004 completes and cold-reloads but
its duplicate screenshot request times out; combat-005 still fails before loot. These are separate
runs, not combined into one successful encounter. Combat-006 is the accepted complete route.

Final Mac targets and generic unsigned iOS Release build pass. The earlier iOS build using the
rejected temporary callback queue is superseded by `ios-release-final.log`. The normal Metal smoke
target was rebuilt, not rerun after restoring its source. One final initializer line wrap is
whitespace-only. No full resource conversion or broad unit suite was repeated for tool-only edits.
Current required runtime remains `../local-data/validation/combat-text-063/runtime-001`.

Target-selection, cast, evasion and realtime-mode text remain partly English. Actual stun/paralysis,
nonempty indoor loot, complete quests/dungeons/travel, MM8 source coverage, missing original portraits
and audio, import recovery and long-term behavior remain open. Dummy audio is not listening acceptance.

Evidence: `../local-data/validation/projectile-targets-064/result.json`,
`interception-diagnosis.json`, `interception-save-delta.json`, `combat-006-verification.json`,
`combat-006.log`, `reload-002.log`, `mac-final-build-002.log` and `ios-release-final.log`.
Next: active-turn save semantics, remaining shared spell text and complete gameplay progression.
