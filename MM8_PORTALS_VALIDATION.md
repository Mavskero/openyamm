# MM8 portals, combat and temple entry — 086

Date: 2026-09-12. Actual Mac Metal application; no iPad or simulator operations.
Runtime stays 085/runtime-003; save format stays 85. Overall estimate: **40–45%, low confidence**.

## Accepted evidence

The validation tool uses normal portrait/inventory pointer actions, shared movement/attack/Combat/Trigger input,
returned dialogue actions and the existing spell service with validated targets and original mana costs.
Camera aiming and terrain-derived waypoints guide the route; real collision remains authoritative.
No character stats, coordinates, items, quest flags, enemy HP, generated Lua or original content were edited.

The evidence is a chain of normal saved continuations, not one uninterrupted successful full-route execution:

1. Unchanged 084 shop-complete → route-006/route-combat.oysav. The Knight equips original sword 1 and armor 84.
   Free town portal 451 retains both stones. Original 10 AM NPC 27 dialogue is displayed and normally closed.
   Portal 453 consumes stone 617, sets QBit 1, and retains stone 618 and completed crystal quest QBit 8.
2. That combat-before save → route-007/portal-second-landed.oysav. Normal turn combat defeats original actor
   indices 109,110,111,142,144,145,146; each starts at 20 HP and ends dead at 0 HP. Twelve Fire Bolts and ten Heals
   consume original mana. Experience gains are [126,126,133,133], final HP [32,36,28,51], mana [0,31,28,24].
   Portal 466 consumes stone 618 and sets QBit 2. Corpses are **not looted**.
3. That second-portal save → route-011/temple-entry.oysav. The route aligns with the narrow original ramp at
   x=-12800 before climbing from z=704 to z=1856. Trigger hits model 21 / face 25 / event 501. The returned
   confirmation enters d05.blv at its original spawn. Original on-load event 1 presents NPC 264,
   赛蒙・谭普勒; his greeting is displayed and normally closed, retaining the four-member party.
   This final entry segment completes **372 Metal frames**, six screenshots, exit 0.
4. Reload-001 loads the final temple save in a fresh application, reloads it in the same session and renders
   the interior: **181 Metal frames**, exit 0.

For route-011's second-portal input and reload-001's temple input, both cold and warm snapshots match all
**33 inspected fields except the save name**. This covers inspected characters, equipment/inventory, quests,
NPC state, actors, shops, time, view, turn state and projectiles. It is bounded by the inspector schema,
not binary identity or long-session stability.

The independent save audit checks original identities/base stats/skills/spells, 169 gold, five food,
zero party deaths, equipment, item-count conservation except the two stones, and quest additions limited
to bits 1/2/232. The seven defeated actors persist into the indoor save. A far-away actor 88 also dies during
normal world simulation, over 22,000 units from the combat start; this is recorded separately and not
attributed to the player. Audit 001 incorrectly treated all world deaths as player kills; corrected
verify-saves-002.log passes and retains that failed assumption.

## Reproduction and scope

Evidence: ../local-data/validation/mm8-portals-086/, including result.json, save-comparison.json,
verify-route.py, changes.patch and hash manifests. Final native build: native-build-013.log.

Only tools/ApplicationSceneCapture.cpp changes executable code. Root instructions/progress also update;
the other 13 files in 085's hash manifest and all four ZIPs plus conversion.json remain identical. No new iOS/unit run was needed for this
Mac-only validation tool change. Earlier iOS/test results are historical, not new executions.

Actually viewed images: route-004/route-equipped.png; route-007/route-first-kill.png;
route-009/temple-confirm.png; final route-011/temple-npc.png and temple-entry.png.
The original transition art and Chinese paragraph are visible. Simon's three-line greeting does not overlap.
The interior renders with textures and HUD. The first-kill screenshot shows the ongoing crowded encounter;
kill acceptance comes from action observations and saved HP/XP. Confirmation used the shared dialogue action;
physical pointer activation of its icon is not claimed. Optional OK-label skipping and English Cast/Menu remain.

Retained failures: route-001 cliff approach; 002 unhandled 10 AM NPC; 003 tree collision; 004 hostile blocker;
005 continuous realtime melee ended in party defeat; 006 tried Heal from a companion without Body Magic;
007 approached the temple ramp from its side; 008 missed transition confirmation; 009 entered but lost its
diagnostic entry screenshot while entry preview work was active; 010 exposed the original on-load Simon
dialogue. Passing 011 handles the dialogues and waits for entry work before requesting its final screenshot.
Partial checkpoints from 006/007 above are explicitly identified, not relabeled as whole-run passes.
Native builds 001–012 and geometry/planner exploration are retained. All handles are terminal.

## Next

Continue from **route-011/temple-entry.oysav**. Verify normal dungeon exploration, doors/traps/nonempty loot,
Simon joining if appropriate, return through activated portals without replacement stones, and corpse loot.
Dadeross's letter is not yet accepted: original NPC 2 / topic 20 in chief house 212 gives item 741 and
QBits 3/221; delivery is NPC 3 / topic 28 in Ravenshore. Do not substitute the already accepted crystal
quest for the full starter campaign loop.

Keep 083 equipment seam/clipping, 079 navigation, 074 candle, remaining shops/localization/import edge cases,
and device/long-play acceptance open. Complete starter gameplay on Mac plus iPad revalidation could support
55–60%; this checkpoint does not meet that milestone.

