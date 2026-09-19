# MM8 dungeon battle, supplies and negative-health persistence — 088

Date: 2026-09-12. Actual Mac Metal application; no iPad/simulator operations.
Runtime stays 085/runtime-003, four ZIPs/conversion.json unchanged; save format stays 85.
Overall **40–45%, low confidence**; the full starter/iPad milestone remains open.

## Production fixes and their evidence

The actual saved Knight had -1 HP and the Unconscious condition. Both cold and warm application loads
changed HP to zero. Party's bonus refresh and GameMechanics' base-resource refresh independently clamped
HP to a nonnegative range. Both now preserve negative HP while retaining their maximum-health cap.
Party::healMember also keeps the exact signed healing result: healing -6 HP by two now yields -4,
not zero. A wider intermediate protects the addition; nonpositive healing and the upper cap are retained.
No saved HP, condition, gameplay stat, enemy or original resource is patched by the validation route.

The first focused regression run has 10 failed assertions; after the Party fix, 41 assertions pass but
actual cold/warm loading still fails. Expanding coverage to base-resource refresh exposes six failures.
The final three cases / **44 assertions pass**, and the unchanged actual -1 HP save restores all
**33 inspected fields except name** in both modes. Negative-health-actual.json records both failed fixes
and final successful application evidence. The full unit target is disabled and is not claimed.
Final production native build: native-build-010.log; final capture build: native-build-013.log.
Generic unsigned iOS Release: ios-release-002.log passes. Earlier ios001 lacks the second refresh fix.

## Actual normal saved continuation

This is a chain of normal saved continuations, not one uninterrupted successful whole-route run:

1. Unchanged 087 route005/temple-roster-complete → **route-001/dungeon-landing-complete**.
   Ordinary Forward input follows the original switchback stairs from footZ2464 to944, walking7431.03units.
   Normal turn input, existing weapons, Fire Bolt and Heal defeat actor indices45,47,48,49,50,51,52,54,55,56.
   This segment predates the health fixes; it is retained as a historical normal navigation/battle result.
   2858Metalframes, six captures, exit0. Knight ends at-1HP/unconscious, Simon has severe poison.
2. That unchanged injured save → **recovery-005/recovery-first-kill** with final production health fixes.
   Normal rendered backpack/portrait clicks consume two original healing potions222, two mana potions223,
   and one antidote226. The Knight wakes and Simon's poison clears. Normal combat defeats actor53.
   The later direct approach to the last bat times out; only this earlier valid checkpoint is accepted.
3. That checkpoint → **continuation-002/dungeon-loot-complete** with final capture tool.
   Normal movement flanks the original stair wall and combat defeats actor46. The route heals normally,
   returns along the corridor, and uses Trigger to search actual corpse51 then55. Each yields seven gold:
   **169→176→183**. Both searches are gold-only; no new inventory item is claimed for this chain.
   A normal portrait double-click opens Acken's remaining inventory, then Escape closes it.
   595Metalframes, seven captures, exit0.
4. **reload-001** loads the final save in a fresh application and again in-session, opens/closes the
   normal inventory and saves continuation. 181Metalframes, four captures, exit0.

Final HP: [27,34,21,32,48]; mana: [0,1,0,24,0]; all five condition bitsets empty, food5, party deaths0.
XP gains relative to087: [42,88,74,93,88]. Twelve original actors45–56 are dead; only corpse51/55 searched.
The two nearby bat searches are nonempty gold loot, not acceptance of every loot/item generation branch.

Independent verify-dungeon.py / verify-saves-001.log passes. It checks every observed kill against saved
HP, identities/base stats/equipment, remaining item properties and grid positions, exact potion consumption,
quest/NPC/stock/inn state and the two gold transactions. Four input saves each cold/warm restore all33
inspected fields except name, including the negative-health and mid-combat saves. After final normal UI
interaction, character fields except recovery time stay equal; gold/inventory/static state and consumed
corpse flags remain. This is bounded by the inspector, not binary identity or long-session stability.

Viewed: route001 upper-stairs and landing PNGs; continuation002 loot-inventory and loot-complete PNGs.
The 183gold and five recovered portraits are visible. English pickup/status labels and the known staff
arm seam/panel clipping remain. No complete visual or localization acceptance.

## Reproduction, failures and next work

From openyamm/, source ../external-storage/env.sh and run:

```sh
build-apple/application_scene_capture \
  ../local-data/validation/item-transparency-085/runtime-003 \
  ../local-data/validation/mm8-dungeon-088/continuation-002/dungeon-loot-complete.oysav \
  ../local-data/validation/mm8-dungeon-088/reload-new --check-mm8-dungeon
```

The same flag accepts the original087 entrance, the injured landing, a normally saved post-potion combat
continuation, or the completed search. Route targets and camera aim guide ordinary shared input; they do
not teleport, grant items, set quests, call map events directly, or alter damage/experience/mana values.

Retained failures: recovery001 ignores spell recovery;002 heals while live bats approach;003 stalls on
an out-of-range bat;004 completes old-code gameplay but still fails negative-health loading;005 preserves
negative HP and defeats53 but its later direct chase cannot reach46. Continuation001's bounded diagnostic
shows partyx=-3915 sliding along the stairwall while bat46 is on the other side aroundx=-4241.
Continuation002 walks around the south end. This is an automation route error, not a proven collision bug.
Recovery004's alternate roll produced20gold and item178; it is not the final accepted14gold-only result.
Compiler failures001/003 and their fixes are retained. All process handles are terminal.

Next normal save: **continuation-002/dungeon-loot-complete.oysav**. Low caster mana makes return, resupply,
and continued exploration necessary. Verify the reverse stairs and original exit, activated return portals
without replacement stones, then Dadeross's letter741/Q3/Q221 and Ravenshore delivery. Deeper MM8 doors,
traps/chests, further physical item loot, full starter replay, iPad and long-play remain open. Keep083
visual issues,079 navigation,074 candle and other unaccepted services/localization open. Goal stays active.
