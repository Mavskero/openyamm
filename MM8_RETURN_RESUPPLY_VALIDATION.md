# MM8 temple return, tavern resupply and Dadeross letter — 089

Date: 2026-09-12. Actual Mac Metal application only. No iPad or simulator queried or operated.
Runtime remains 085/runtime-003; all four ZIPs and conversion.json match checkpoint 088.
Save format remains 85. Only the Mac application capture tool changed; production code is unchanged.
Overall **40–45%, low confidence**. Delivery in Ravenshore and the complete starter/iPad milestone remain open.

## Normal gameplay evidence

The input is the unchanged normally recovered five-member save from
088/continuation-002/dungeon-loot-complete.oysav. Each accepted segment uses shared gameplay controls,
real collision and original map/NPC actions. Camera aim and route waypoints guide ordinary movement;
there is no coordinate, stat, item, quest, damage, reward or time injection. Paid rest is completed with
the ordinary Escape control; the shared rest handler advances the stay and applies its normal recovery.
Returned dialogue actions are selected semantically, not claimed as physical iPad taps.

1. **route-001** reverses the original temple stairs from foot Z944 to the upper exit at Z2464,
   opens and confirms the original door, and uses activated return pads467/465 and the free pad452.
   All stones remain consumed, and original Q1/Q2/Q8 remain set. The party returns to the village
   landing around(10123,4488), walking42588.5units; 2233Metalframes, eight captures, exit0.
   Gold183, food5, all twelve defeated bats remain dead and both searched corpses remain consumed.
   Lamentia's HP increases32→41 while walking; she retains original level-one Regeneration.
   Other inspected character fields remain equal. An initial blanket invariant that every HP must stay
   unchanged was rejected as an audit assumption, not treated as a gameplay defect.
2. **resupply-004** continues that unchanged village save. The party follows a normal path to
   original tavern228, chooses proprietor 土伯瑞克 and buys food: gold183→182, food5→14.
   Renting a room costs another3gold, matching the displayed price. Ordinary rest completion advances
   approximately1060game minutes to the next morning; food remains14. All five members' HP and MP
   reach their normal maxima: HP[35,45,28,51,75], MP[0,51,52,24,0], conditions empty.
   The party returns to original chief hall212, chooses Dadeross/NPC2 and original topic20.
   The offered original letter741 is put in the first member's backpack using its rendered portrait.
   Q3/Q221 are set; one identified letter is present. Gold179, food14, experience unchanged.
   26239.5walkedunits, 1546Metalframes, six captures, exit0.
3. **reload-return-001** independently loads the village return save and preserves the return state;
   181Metalframes, three captures, exit0.
4. **reload-letter-001** independently loads the accepted-letter save, re-enters the chief hall and
   repeats original NPC2/topic20. The original Chinese reminder appears, with exactly one letter,
   unchanged quest flags, gold, food and character/item state. 181Metalframes, three captures, exit0.

These extend the earlier normal saved chain. They do not turn the whole multi-checkpoint starter route
into one uninterrupted accepted run, and they do not complete the letter's delivery or award its reward.

## Independent checks and limits

Evidence: `local-data/validation/mm8-return-089/`.

- `verify-return.py` / `verify-return-001.log`: five return checkpoints retain15static inspected fields,
  original character/item fields except observed regeneration, and defeated/searched actor state.
  The source cold/warm loads each match all33inspected fields except save name.
- `verify-resupply.py` / `verify-resupply-001.log`: food purchase changes only gold/food/name, costs match
  displayed options, paid rest restores resources, unused original item properties and grid positions
  remain exact. The letter is the only added item; only original Q3/Q221 are added, with no XP reward.
  Resupply and both reload inputs each cold/warm match all33inspected fields except name.
  Normal NPC re-entry after restart does not duplicate the letter.
- Final native capture build `native-build-006.log` passes; new code lines satisfy120columns.
  The other21previously recorded source paths were unchanged before root documentation updates.
  No new production/iOS/unit build was needed for capture-only edits; the final production iOS result
  remains088/ios-release-002.log. The disabled full unit suite is not claimed.
- Viewed actual screenshots: return-stairs, return-exited, return-town-complete; tavern service options,
  rest overlay, restored party, original letter dialogue, and reload reminder. Chinese letter text fits.
  English service/rest/status labels remain. Rest's upper animation area is black; return-stairs looks
  into a black exit-facing area and distant outdoor terrain contains black regions. These images do not
  establish complete media/visual acceptance. Existing083paperdoll seams/clipping remain open.

Retained failed attempts: resupply001 crossed free portal451 again and then hit tent model48/face46;
resupply002 entered coastal low terrain and could not climb the slope; resupply003 reached the tavern
but the driver attempted food before choosing its proprietor. Resupply004 uses the correct original
route and dialogue sequence. These are driver failures; no speculative collision or service fix was made.
All processes are terminal. The turn made verified progress, and the full goal remains active.

## Continue

Use `local-data/validation/mm8-return-089/resupply-004/resupply-letter-complete.oysav`.
Next: walk normally to boat house479/out01event183, select its original available travel service,
travel to Ravenshore, find NPC3 and deliver using topic28. The original event should consume741,
clearQ3/Q221, setQ4, award2500gold and7500XP per member, and unlock autonote494. Verify scheduling,
payment, map arrival and subsequent restart rather than patching any state to bypass the trip.

Boat door guidance is read from original ODM model127/face42, x17200, y21490–21618, z192–448;
`house-geometry.json` records its vertices. A safe approach has not yet been walked.
Avoid the village pad rectangle x9856–10368/y4608–5120 when leaving the return landing.
The accepted tavern path stays on village high ground via(4000,6400)→(4000,8500)→(2700,8500)
→(2700,10000)→(2700,11264). The western direct coastal route is not walkable by this party.
Deeper dungeon doors/traps/chests, full starter continuation, MM6/MM7/cross-world campaigns, complete
Chinese/media coverage, import/keyboard/mouse/touch/background behavior on iPad and long play remain open.
