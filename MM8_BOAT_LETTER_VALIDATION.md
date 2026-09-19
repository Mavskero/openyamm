# MM8 normal boat travel and letter delivery — 094

2026-09-12, actual Mac Metal application. No iPad or simulator queried or operated.
Only the Mac capture tool and records changed. Native build 004 passes; 33 other previously recorded
source paths are unchanged before documentation. Production iOS remains 092/ios-release-001.
Runtime 085/runtime-003 and save format 85 are unchanged; full runtime hashes were last checked at 090.
The disabled full unit suite was not run.

## Accepted route

Input: 093/route-002/temple-exit-complete.oysav. Route 004 continuously walks from the normally reached
temple exit, along the original pier and boarding ramp, to boat 479 (顺风号). Normal Trigger activates
original door event 183. The returned boat service takes original location 19 to Ravenshore/out02.odm.
The table retains the filename Out02.odm; the active map uses its normalized lowercase name.
The fare is 69 gold with the existing Merchant skill, leaving 110. Normal travel advances 5760 minutes
(four days) and restores all surviving members' HP/MP. Arrival is the original (8609,-15609,265).
The later arrival save includes about 0.2 minute of normal updates after the exact travel advance.

The party then walks east along the destination pier, reaches land, goes around the merchant building,
climbs its original side ramp from ground to the 256-unit platform and approaches the front door.
Normal Trigger activates original event 503, which speaks to NPC 3 while Q59 is unset. Selecting its
returned topic 28 delivers letter 741: +2500 gold, +7500 XP to each of five members, clear Q3/Q221,
set Q4, add autonote 494 and replace NPC 3/topic slot 0 with 29. Other inventory entries, their grids
and properties, equipment, skills and levels remain unchanged. No subsequent topic 29 was selected.

This uninterrupted segment walks 14,939.8 units, renders 994 Metal frames and produces seven captures,
exit 0. No coordinate, character, inventory, quest, reward or time injection is used. The driver uses
normal movement/Trigger and actual returned dialogue actions; normal shared gameplay owns all effects.
Final: out02.odm (12863.9746,-12237.8428,257), game minute 7719.07617, 2610 gold, 14 food.
HP [35,45,28,51,75], MP [0,51,52,24,0], no conditions. The previous dungeon's actor/corpse records,
chests, mechanisms and presentation overrides are unchanged, including both completed puzzles.

Original out02 OnLoad sets Q93 and clears Q212/Q213 (only Q212 was set). Existing map mercenary
recruitment relocates placeholder NPC 303 from tavern 228 to tavern 229. These normal map-entry changes
are recorded separately from letter rewards; no character joins or leaves this party.

## Restart and audit

Reload 001 cold-loads the completed delivery and also reloads it within the same application, then
normally reopens the same front door. The completed letter topic is absent, the next task topic remains,
and gold/experience/items/quests are unchanged. 181 Metal frames, four captures, exit 0.
All five run inputs (including earlier failed routes) preserve 34 inspected fields except the save name
in both cold and warm loads. These inspected fields are not a claim of every binary field's equivalence.

Verify-delivery-002 passes against actual snapshots and original imported transport/global quest data.
Seventeen static top-level fields remain exact through all six route checkpoints. It verifies exact
letter reward deltas, original letter removal only, no repeated reward, original dungeon state and
travel recovery. Audit 001 incorrectly expected a generic global-event trace from NPC dialogue;
that path records topic_clicked instead. Final audit checks the actual topic action once and all
state deltas; retain 001 as an audit assumption error, not a production failure.

## Earlier attempts and presentation limits

Route 001 opened the boat correctly but the driver unnecessarily requested a proprietor action; this
boat directly offers its service. Route 002 checked the raw table filename against lowercase and failed.
Route 003 traveled correctly but approached the merchant platform's 256-unit wall from below. The final
route uses its original side ramp. All are retained; no production collision/dialogue change was needed.
Native builds 001–004 pass. Only the final build/route establish the complete accepted segment.

Viewed actual boat-service, Chinese delivery-response and reopened-dialogue captures. The full delivery
text fits the dialogue region. Boat route wording is still partly English and wraps the destination
name; earlier health-bar/scene/media issues remain open. A rendered boat video frame is not complete
animation/audio acceptance. The dialogue backdrop is the outdoor merchant doorway for this SpeakNPC
interaction. No claim of complete visual fidelity or long-session stability is made.

## Continue

Current completed save: local-data/validation/mm8-letter-094/route-004/mm8-delivery-complete.oysav.
The earlier MM8 starter path is still a series of normal validated segments. Next replay the complete
path from normal new-game creation using the current code, then later revalidate on iPad when authorized.
Keep current no-device work scope. This checkpoint completes boat/delivery, not the full starter milestone.
Overall remains roughly 40%–45%, low confidence; 55%–60% remains conditional on full starter/iPad acceptance.
MM6/MM7/cross-world progression, remaining Chinese/media/import/input/background and long play stay open.
