# Indoor application sessions and camera saves — checkpoint 046

2026-09-11, Mac-only runtime checks and generic unsigned iOS Release compilation.
No iPad query, installation, launch, screenshot or accessory operation was performed.

## Change

Indoor camera yaw and pitch were absent from saved game state. GameApplication captured
outdoor camera angles but only the indoor world/party snapshot. A loaded dungeon could
restore its coordinates while resetting its view direction.

GameSession now captures the indoor renderer's yaw/pitch alongside the scene snapshot.
Save format 84 stores both angles, and GameApplication reapplies them after loading an
indoor session, including when no graphics view is initialized. Version 83 and earlier
remain readable; without stored orientation they retain the old default yaw 0 and
pitch 0.15 radians. New version-84 saves require a reader supporting that version.
Existing saves were not overwritten.

The save inspector now selects position and camera from the active scene kind. Having a
remembered outdoor state no longer makes an indoor save appear to have outdoor coordinates.

processPendingMapMove accepts the existing initializeView choice already used by loading
and new-session methods. Normal gameplay defaults to true. The headless checker passes
false through both destination loading and failed-transition recovery, allowing the actual
application's map-change algorithm to execute without constructing a graphics view.

## Evidence

IndoorSessionCheck uses the app-generated 045 runtime and the normal Chinese Knight save.
It invokes imported entrance events with the actual party and scene context, constructs
the shared transition dialog model, executes its real confirm action, and processes the
queued move through GameApplication. Goblinwatch uses an explicit key-2109 fixture; earning
that quest prerequisite through gameplay is not claimed.

Movement uses IndoorPartyRuntime with the runtime's actor, decoration and object colliders,
plus scene simulation and actor AI. Twelve 50-ms steps request a modest forward velocity.
This exercises short movement and floor placement, not the hardware input/renderer path or
all corridor, stair, wall and door collision cases.

| Dungeon | Materialized actors in this run | Movement distance | Final sector | Saved yaw / pitch (radians) |
| --- | ---: | ---: | ---: | --- |
| 6d01 / Goblinwatch | 217 | 117.740 | 1 | -1.61968 / -0.23 |
| 6d02 / Abandoned Temple | 223 | 117.766 | 45 | 1.88844 / -0.23 |
| 6d18 / Gharik's Forge | 117 | 118.750 | 6 | -1.20080 / -0.23 |

Each route saves through GameApplication, reloads in the same process, then uses its actual
exit event to return to the town runtime. Separate processes also load all three indoor saves,
resave, reload and return to town. The checks compare map identity, party XYZ/sector and camera,
actor count and every actor's monster_info_id, HP, XYZ, attributes and group, plus party health,
gold and food. Inspector comparisons additionally cover the exposed party/item fields.
These are selected invariants, not a byte-for-byte assertion of every runtime state field.

The old 043 version-83 outdoor combat save also reloads under the updated reader, retaining
281 actor identities/HP/dead/visibility and its consumed corpse. A synthetic version-83
indoor compatibility fixture removes only the new eight-byte camera pair and changes the
version header of a test copy; it reads with default camera angles and unchanged other
inspector fields, and passes a cold application reload. This fixture is not an old user
playthrough save. The original source/runtime packages and prior saves remain intact.

The generic iOS Release build succeeds with signing disabled. No visual correctness or
device save/reload acceptance follows from this build.

## Remaining work

Full dungeon traversal, combat/loot, rendered doors and transitions, indoor UI and hardware
input remain unaccepted. Source campfire artwork and music remain missing. Source actor
placement can trigger the runtime's existing floor-recovery diagnostic; this short run
does not certify every authored spawn position. Additional regions, worlds, full quests,
MMExtension behavior, import recovery/storage management and longer stability checks remain.

Overall project progress remains the low-confidence 35%–40% estimate.

Run: indoor_session_check RUNTIME_ROOT INPUT_SAVE NEW_OUTPUT_DIRECTORY MAP.blv.
Accepted routes are 6d01.blv, 6d02.blv and 6d18.blv. With an outdoor input the checker enters
the selected dungeon; with an indoor input it checks restoration before resaving and exiting.
No render/input loop or synthetic attack/loot success is used.

Evidence: ../local-data/validation/indoor-session-046/result.json and adjacent logs/saves.
