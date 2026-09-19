# MM7 starter investigation and outdoor slope correction — 097

2026-09-12. The party terrain correction passes focused regression tests, the actual Mac MM8
temple-return route, and a generic unsigned iOS Release build. MM7's shell transaction and delivery
remain unaccepted. No iPad or simulator was queried or operated. Overall remains **45–50%, low confidence**.

## Production correction

`game/outdoor/OutdoorMovementController.cpp` previously selected the slope triangle using the opposite
terrain diagonal from `sampleOutdoorRenderedTerrainHeight`. A high corner outside the supporting
triangle could therefore reject movement up a gentle slope. The party check now selects exactly the
same three vertices as the floor sampler, using `fractionX + fractionY <= 1`.

The existing height-spread limit of 512, normal step/jump behavior, collision handling, original map
data and actor pathfinding rules are retained. No party, item, quest or coordinate state is injected.
Save format remains 85. Runtime 085/runtime-003 is reused without reimport; its full content hashes
were last checked at 090.

`tests/OutdoorStepUpTests.cpp` adds two gentle-triangle variants and a genuinely steep ascent case.
The first flat-ground diagnostic did not reproduce the defect (`terrain-red-001.log` actually passes).
The improved ascending cases fail before the fix: 9 cases, 36 assertions, four failed assertions in
one case (`terrain-red-002.log`). After the fix, **all 9 cases and 36 assertions pass**, including the
seven existing step cases (`terrain-green-001.log`). This is focused coverage, not the disabled full suite.

Native build014 includes the production fix and tests; build015 updates only the MM7 automation route.
`ios-release-001.log` confirms recompilation of OutdoorMovementController for arm64 and BUILD SUCCEEDED.
This unsigned build is not a device installation or runtime acceptance.

## Actual Mac regression

The unchanged normal 096/06-loot save is loaded through the application and follows the existing
`--check-mm8-return` route: original temple stairs, exit confirmation, two island return pads and
the village pad. `mm8-return-regression-001` exits 0 after **2226 Metal frames, eight captures and
42487 walked units**. It returns to out01 with 210 gold, five food and the same five-member party.

`verify-return-regression.py` / `verify-return-regression-002.log` independently audit all five
observations and both cold/warm restores. Each restore matches 34 inspected fields except name;
19 static fields remain unchanged along the route. All 17 defeated actors44–60 and searched corpses
53/54 retain their state. Character identity, inventory, skills and other fields remain unchanged
except normal recovery timers and Lamentia's original Regeneration, which raises HP44→51.
Other HP remain [29,35,21,56]. No rewards or new items are introduced.

The first audit attempt assumed every actor had a corpse record; SaveInspect intentionally omits
corpse fields where no record exists. Audit002 checks both presence and value rather than inventing
missing records. The failed audit log is retained. The actual town PNG was inspected: Chinese shop
label, scene and party HUD are visible; English Menu and previously recorded visual/media gaps remain.
The complete sixteen-stage MM8 run from 096 was not repeated with this production change.

## MM7 findings and unfinished route

Normal creation and actual rendered load give a single level-one 中文骑士, 0 XP, 200 gold, five food,
seven original inventory items and the six Emerald Island scavenger flags513–518. Actual map load
also puts mercenary NPC303 into original tavern239; the initial headless observation did not establish
a missing-recruitment defect.

Original imported events offer shell1437 through NPC344: topic765 exposes purchase766 for 100 gold.
Judge NPC341 in headquarters1163 accepts it via topic756 for 500 XP, clears514 and sets521. These
event facts are inspected; the full normal purchase/delivery/reload sequence has **not** passed.
The other five items, contest completion, Moon Temple combat and subsequent travel are also open.

The Mac capture tool adds pending `--check-mm7-shell` and `--check-mm7-shell-reload` modes. Walking uses
normal controls and collision; tutorial NPC358–376 dialogues are allowed to open and closed through
ordinary Escape. They are not disabled by setting the tutorial-complete flag. Eight main attempts
retain their failure saves/logs. Early stops exposed tutorial pauses and invalid paths through buildings
or steep terrain. The latest western attempt walks through town but stops near (6152,10248,507),
before reaching the shell seller. This does not establish that the full quest is broken or passed.

The read-only `openyamm_route_plan` tool asks the existing actor pathfinder for navigation proposals.
Its proposed route did not pass actual party movement. The local terrain-only Python proposals also
do not account for all collision or route-following behavior. Neither is accepted as a playable route.
`verify-shell.py` is prepared but unrun; no shell audit success is claimed. Next work should resolve
normal party navigation and then verify the actual purchase, delivery and persistent single reward.

## Evidence and scope

Evidence root: `local-data/validation/mm7-starter-097/`; authoritative summary: `result.json`.
The 096 source manifest contains43 paths: before documentation, CMakeLists and ApplicationSceneCapture
changed and41 matched. It did not cover OutdoorMovementController or OutdoorStepUpTests; both changes
are explicitly declared and added to the new manifest. This comparison is not a whole-repository audit.
All processes are finished and their handles reaped. Original game source files and runtime packages
were not rewritten. Device import/input/background acceptance, cross-world persistence and long play
remain separate milestones.
