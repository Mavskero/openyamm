# MM8 crystal quest and Talimere recruitment — checkpoint 080

The first normal MM8 crystal-delivery quest and companion recruitment now pass in the actual Mac
Metal application. The complete resource package remains checkpoint 079/runtime-002. This checkpoint
changes only the capture and save-inspection tools; gameplay, original content and save format 85
are unchanged. No iPad or simulator was queried or operated. All outputs remain on NVMeSSD.

## Normal route and source results

`route-005` starts from the unchanged checkpoint 078 single-character MM8 creation save: level-one
Chinese Knight, 200 gold, five food, original inventory and QBits 185/226/401/407. Door coordinates
come from the original ODM. Waypoints supply camera directions; every position change uses normal
session Forward inputs, terrain/building collision and actor simulation. Normal Trigger activates
the original house events. Dialogue selection uses actions offered by the actual dialogue runtime.
No coordinates, attributes, inventory, event programs or quest flags are injected.

The route walks approximately 14,173 world units, enters the chieftain's house 212, talks to NPC 1,
selects portal topic 8 and quest topic 21, then walks to Talimere's hut 866. The shared house builder
opens its lone resident NPC 28 directly. Original topic 12 completes the delivery; original roster
offer 602 and its accept action recruit Talimere. The run renders 929 actual Metal frames.

| Action | Independently checked result |
| --- | --- |
| Accept crystal quest | One item 617, QBits 7 and 212 set, no XP or gold reward |
| Store reward | Original event places it on the pointer; a normal portrait click stores it |
| Repeat chief topic | Still exactly one crystal |
| Deliver crystal | QBit 7 cleared, 8 set; item 617 retained; original member gains 1500 XP |
| Quest notebook | Original autonote 492 stored with value 492; visible-runtime query succeeds |
| Delivery topic | Reward topic 12 is replaced by roster offer 602 |
| Accept companion | Roster ID 2, Chinese name, level 5, 10000 XP; NPC 28 becomes unavailable |
| Original recruit data | Name and ten numeric fields match roster source; ten starting items retained |
| Item placement | Six recruit items equipped, four in inventory, including energy stone 618 |
| Existing character | All inspected character fields except XP/inventory match the normal initial save |

Gold remains 200, food five, deaths zero. The original Knight remains at 35 HP. Source item checks
count both equipment and backpack contents. Complete Chinese delivery and invitation text is visible
in actual captures. `Yes`/`No`, the internal-looking roster topic label and some status text remain
unlocalized; those are not accepted as finished product wording.

## Save, reload and continued interaction

`reload-001` launches a separate Mac application process with the completed normal route save. It
saves immediately after cold restoration, loads again within the active session and saves again.
All **30 inspected top-level fields other than save name** match, including full inspected character
records, roster identities, items/equipment, autonotes, NPC topics/availability, quests, camera/time,
actor records and turn/projectile state. This is not a claim about every serialized field.

Normal Trigger reopens the now-unoccupied hut. Its original house dialogue is active and NPC 28 is
absent; two party members remain, with unchanged quests, items and rewards. Actual captures show the
empty resident panel and both party portraits. This continuation renders 181 frames; it does not
establish a long session, cross-map travel or the companion's combat behavior.

## Verification and retained failures

Evidence is in `../local-data/validation/mm8-start-080/`: `result.json`, `verify-saves.py`,
`save-comparison.json`, original event/door audits, original BMP captures and lossless PNG copies,
source hashes, patch and build logs. Native capture build 007 and inspector build 003 pass. The
production application and resources did not change, so checkpoint 079's passing iOS Release and
focused importer checks remain applicable without another build or full reimport.

Earlier failures are retained: the first route counted only backpack items while the crystal was
on the pointer; the second walked into original building 30; the third expected a multi-resident
house menu instead of the shared single-resident dialogue. These were test/route assumptions, and
no production collision or dialogue rules were weakened. The first compile used a nonexistent
Escape action, corrected to the normal Escape scancode. The initial inspector omitted the Lua
autonote tag, and the first independent comparison incorrectly expected boolean 1 rather than
the original stored note ID 492. Final inspection uses the existing variable decoder.

## Next work

Checkpoint 081 now validates normal island inn recruitment and partial/final roster persistence;
see `MM8_INN_VALIDATION.md`. Its four-member continuation save supersedes the two-member starting
point below. Shops, portals and the temple remain open.

Continue from `route-005/mm8-route-complete.oysav`: use shops/inn, assemble a suitable normal party,
activate the original stone portals and explore the abandoned temple. Original local events
453/465 consume item 617 and set QBit 1; 466/467 consume 618 and set QBit 2. Their scripted travel
is legitimate gameplay and must be reached through actual interactions, not direct event calls.
Main-island temple entry 501 reaches d05; its full dungeon, combat/loot, main quest and travel back
remain unverified. The full MM8 starting-area milestone is still open. Overall remains **40–45%,
low confidence**; this completed short quest is one component of that milestone.
