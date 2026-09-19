# MM8 Adventurer's Inn — checkpoint 081

The normal MM8 island party can enter the Chinese Adventurer's Inn and recruit the two originally
unlocked companions through the actual shared portrait and hire-button handlers. Partial and final
recruitment saves survive cold and warm loading. This is a Mac Metal gameplay result, with a passing
generic unsigned iOS Release build; no iPad or simulator was queried or operated.

## Fixed behavior

The original merged house 1607 has service type `The Adventurer's Inn` and Chinese display name
`冒险者的旅店`. The engine checked only its English display name, so normal map event 199 opened
a generic house without the roster. Both inn identification and default-inn lookup now use the
original service type. The existing English `House`/`The Adventurer's Inn` representation remains
supported, while other service types with the same display name are excluded.

After the first hire, the next companion moves to the same portrait index. Clicking it quickly
was incorrectly interpreted as a double-click on the previous person, opening inventory and making
the hire button unavailable. An intervening non-portrait activation now ends that double-click
sequence. This fix lives in the shared party-overlay input controller.

The actual inn biography also had overlapping Chinese lines: legacy line spacing subtracted two
pixels and did not include the Apple text raster height. Spacing now covers the measured rendered
line and its shadow. Both available companions' complete biographies fit without overlapping in
the inspected captures. Font size itself is unchanged.

## Normal route and persistence

`route-green-002` continues the unchanged checkpoint 080 quest-complete save. Normal movement and
collision walk approximately 9268 units to the original inn door; normal Trigger opens event 199.
Actual viewport portrait and button clicks recruit roster 1 (阿肯那斯) and roster 7 (拉曼提亚).
The run renders 576 Metal frames. There are no injected coordinates, stats, items or quest flags.

| Checkpoint | Active roster IDs | Waiting at inn |
| --- | --- | --- |
| Input | 0, 2 | None loaded yet |
| First inn visit | 0, 2 | 1, 7 |
| First hire | 0, 2, 1 | 7 |
| Second hire | 0, 2, 1, 7 | Empty |

Independent save inspection verifies the two existing characters are unchanged. Both new companions'
names and ten numeric fields match the original roster table, and all ten starting items per recruit
remain in their equipment/backpack. The inspected character records transfer unchanged from the
waiting roster to the active party. Gold remains 200, food five, deaths zero; quest bits, autonotes,
held items and NPC availability/topic/house state remain unchanged.

`reload-partial-001` starts a new process with three active members and one waiting companion.
`reload-final-001` starts another with four active members and an empty inn. Each performs cold and
same-session warm restoration: all **31 inspected top-level fields except save name** match its
input, including waiting-roster character records and portrait IDs. After normal door interaction,
the partial case can hire the remaining companion; the final case reports the inn is empty and
does not regenerate either recruit. Each continuation renders 181 frames.

The inspector now reports waiting-roster data; the serialized save format remains version 85.
Resources remain checkpoint 079/runtime-002 and were not reimported or modified.

## Verification and scope

Evidence: `../local-data/validation/mm8-inn-081/`, including `result.json`, `verify-saves.py`,
`save-comparison.json`, original BMP captures, lossless PNG copies, source hashes and `changes.patch`.
Three identity regression cases / nine assertions pass. The identity red run failed seven assertions;
the original actual route also reproduced the generic-house failure. The first run after identity
repair reproduced the separate double-click bug and overlapping biography. Final native build 004
and generic unsigned iOS Release 003 pass. Earlier environment/header/build failures are retained.
The full house-dialogue regression suite was not run: the existing Mac configuration disables that
unit-test target, so only the focused identity executable and actual application routes are claimed.

Remaining visible gaps include English labels/status messages and missing paperdoll body/background
layers (some equipment is visible without the body). No fully polished inn screen is claimed.
Dismissal, scrolling, full-party limits, later unlocks, cross-world inns, companion combat and long
sessions are not covered. The converted active event audit currently finds inn 1607 only in MM8
out01/out02; original loose scripts for MM6/MM7 inns still need integration. A no-missing-map-texture
count is not evidence that character UI artwork is complete.

Continue from `route-green-002/inn-route-complete.oysav`: address the original paperdoll assets,
then normal shops/equipment, stone portals and d05 temple exploration. The complete MM8 starting-area
milestone remains open. Overall remains **40–45%, low confidence**, with the previously stated
conditional 55–60% reassessment only after that complete playable sequence passes.
