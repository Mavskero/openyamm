# MM8 original Simon recruitment and temple round trip — 087

Date: 2026-09-12. Actual Mac Metal application; no iPad or simulator operations.
Runtime stays 085/runtime-003 and save format stays 85. Overall: **40–45%, low confidence**.

## Accepted scope

From the unchanged 086 route-011/temple-entry.oysav, the actual application handles the original
NPC 264 greeting and roster invitation, accepts Simon through the returned shared dialogue action,
closes the result with normal Escape, exits d05 through the original door confirmation and re-enters.
It then opens the fifth member's inventory through normal portrait pointer input and closes it.
No character stats, items, coordinates, quest bits, original game files or generated Lua were changed.
Camera aiming selects the actual exit; this short entrance round trip does not claim dungeon movement.
Dialogue acceptance invokes the shared returned action; physical pointer or touchscreen selection is not claimed.

Final route-005 exits successfully after **257 Metal frames and all 11 expected captures**.
The normal continuation is **route-005/temple-roster-complete.oysav**.
A separate fresh process, reload-001, restores this five-member save, reloads it in-session, then
opens/closes the fifth member inventory normally: **181 Metal frames, four captures, exit 0**.

The original roster row 3 supplies 赛蒙-谭普勒, a level-five Knight with 10,000 experience and
attributes might/intellect/personality/endurance/speed/accuracy/luck = 20/9/9/17/15/15/10.
His ten original starting items are 2,41,61,94,99,110,117,127,132,122. Nothing was granted by the tool.
The four existing members remain exactly equal in every character field exposed by the save inspector.
Gold stays 169, food five and deaths zero. NPC 264 becomes unavailable after joining, and the
already-recruited Simon does not offer recruitment again on normal temple re-entry.

Independent verify-roster.py passes (verify-saves-001.log, save-comparison.json):

- Joined, outdoor, re-entered and final checkpoints each preserve the original four character records
  and 12 static party/quest/shop/inn fields; Simon's 12 identity/stat fields and all ten items match roster.txt.
- Unavailable NPCs equal the original set plus 264; the exit is out01.odm and indoor checkpoints are d05.blv.
- Both input saves restore all **33 inspected fields except name**, during cold and warm loading.
- After normal interaction with the reloaded fifth portrait, 15 compared party/character/inventory/NPC
  fields still match the original five-member final save. Time/world animation are not asserted static here.

QBit 403 is not added merely because Simon joined. Original NPCMercenaries.lua assigns the relevant
availability bit in DismissCharacter; that is not evidence that recruitment should set it. The observed
round trip already suppresses duplicate recruitment. Decline, dismissal and later rehire remain untested.

## Presentation and implementation

Viewed route-005 PNGs: temple-simon-offer, temple-simon-sheet and temple-five-party.
The Chinese three-line invitation fits and all five portraits fit the HUD. Simon's original equipment
is visible. His large shield is clipped at the inventory panel's right edge; the existing equipment
clipping issue remains open. Yes/No and other labels remain English. These are not complete UI acceptance.

Only executable source changed: tools/ApplicationSceneCapture.cpp, adding --check-mm8-temple.
Native build native-build-005.log passes. No production changes or new iOS/unit execution; earlier results
are historical. All four ZIPs/conversion.json rehash identically to 086. The other 14 recorded 086 paths
were unchanged before updating root instructions/progress. Evidence includes source/runtime/evidence hashes.

Reproduce from openyamm/ after sourcing ../external-storage/env.sh:

```sh
build-apple/application_scene_capture \
  ../local-data/validation/item-transparency-085/runtime-003 \
  ../local-data/validation/mm8-portals-086/route-011/temple-entry.oysav \
  ../local-data/validation/mm8-temple-087/route-new --check-mm8-temple
```

Use the resulting temple-roster-complete.oysav as input with the same flag for the five-member reload path.
The evidence audit uses the retained route-005 and reload-001 directories.

Retained failures: routes 001–003 tried to exit while the original Simon greeting was still active;
the normal shared modal gate prevented it. These do not prove an engine exit bug. Route-004 completed
normal recruitment/roundtrip but missed the outdoor screenshot while normal map autosave preview work
was active. Final 005 waits for arrival work before capturing and passes all expected captures.
All process handles are terminal. The failures and builds 001–004 are preserved.

## Next milestone

Still outstanding on this normal MM8 chain: dungeon movement/doors/traps/nonempty loot, corpse loot,
return through activated portals without new stones, Dadeross letter acceptance and Ravenshore delivery,
and a complete starter continuation after restart. Earlier MM6 dungeon checks do not accept this MM8 route.
Keep equipment clipping, 079 navigation, 074 candle, remaining localization/services and long-play open.
A full starter loop on Mac plus iPad revalidation could support **55–60%**; this checkpoint does not meet it.
The goal remains active; continue local work without querying or operating an iPad or simulator.
