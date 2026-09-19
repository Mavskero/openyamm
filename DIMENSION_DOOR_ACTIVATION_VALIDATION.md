# Dimension Door activation validation — checkpoint 116

Accepted on 2026-09-13 for the scope below. This is controlled real-resource integration and compile evidence;
it is not a natural cross-world playthrough or iPad acceptance.

The importer now translates seven recognized original MMerge map scripts into explicit supplemental Lua files.
Town Portal uses the resulting activation state instead of the previous cast-time location shortcuts. Activation
alone does not open the destination UI. It records the destination day, appends an eight-minute reversion timer,
and remains active off the trigger until reversion or a map/session reset.

## Implemented behavior

| Map | Supported original condition |
| --- | --- |
| out01 | Terrain step on tile 63,59 |
| out07 | Terrain tile 43,98 or event 500 |
| outb3 | Terrain tile 89,47 or replacement event 105; removed the old invented cast-time radius |
| 7out04 | Terrain tile 84,94 or event 504; every ClL1_W model face routes to 504 |
| out09 | Event 6, or a strict 1500-unit radius checked by the original anchored one-minute timer |
| oute3 | Replacement event 140 only while MapVar50 is less than 1 |
| out02 | Final-quest flag plus strict 600-unit range; blocks Town Portal and queues BrAlvar/Breach by completion flag |

Repeated activation appends timers without extending the earliest deadline. Reversion while an Info screen is open
is deferred another eight minutes; closing the screen does not immediately expire activation. Ordered removal
preserves the original first-matching-timer behavior when a later alarm fires before an older deferred alarm.
Map change, clear, and save restore reset activation; a same-map rebind preserves it. The separate checkpoint115
four-minute Verdant contact policy remains intact.

The out09 timer begins at absolute minute 3 with a one-minute period. It uses the previous poll tick, including the
original one-tick boundary, and may catch up on successive polls after a long absence. Identical ticks do not fire
twice. The continuous native clock is floored to an original tick so 2.999 minutes does not consume the 3-minute poll.
Legacy and relative native timer behavior is covered by the existing timer regression cases.

## Verification

Evidence root: `../local-data/validation/dimension-door-116/`.

- `event-import-result-002.json`: seven overlays emitted; all eight generated base scripts (Global plus seven maps)
  match runtime085 byte for byte. Original source files and the complete runtime085 packages were not modified.
- `town-portal-tests-001.log`: 7 cases, 488 assertions passed through actual imported local/global events, shared
  spell/dialog services, and original Chinese introduction data.
- `dimension-door-tests-001.log`: 22 cases, 484 assertions passed, comprising 10 new activation/metadata cases and
  12 existing timer cases. Includes actual loaded model faces, fractional strict-radius probes, MapVar50 values
  0/1/2, queued final-area destinations, Lloyd/journal deferral, and session boundaries.
- `import-profile-002/result.json`: 6 cases, 58 assertions passed for recognized source hashes, missing optional map,
  missing controlling script, modified controlling/core/map scripts, unrelated scripts, and escaping symlink.
- `ios-build-result-002.json`: final unsigned generic iOS Release passed; 21 requested source hashes stayed unchanged.
  The importer recompiled with the corrected embedded event support and the native timer recompiled for arm64.
- Total: **35 cases, 1030 assertions**, all passed. This total is not a game-content coverage percentage.

## Failures found and resolved

The first resource import was stopped after process sampling showed an infinite Lua table insertion loop. Existing
`SetMapMetadata` appended lifecycle arrays to themselves when an overlay supplied only unrelated metadata. Astra
corrected identity checks in both map and global metadata helpers. The failed import, sample, and partial output
remain under `events-001` and `event-import-sample-001.txt`; the accepted import is `events-002`. Regression tests
now cover partial updates and merging replacement arrays without duplicating the existing arrays.

Source review also corrected the initial single-catch-up interpretation of the native interval timer and the
rounding issue described above before runtime acceptance. These production fixes were owned by Astra xhigh.

The first native test build used a variadic CAPTURE call unsupported by the bundled doctest version. Sol high's
first repair passed compilation; no runtime repair was needed. Shell-only wrapper failures from assigning zsh's
read-only `status` variable are retained. No bug reached three unsuccessful Sol repair attempts.

## Limits and next work

Terrain hooks currently run on actual horizontal grounded Terrain movement, independently of walking audio. This
bridge is not a claim of identical cadence to every MMExtension terrain-sound query, and the headless tests invoke
the shared hook rather than physically walking the rendered application onto each tile.

The final-quest flags in out02 are controlled fixtures. Its test verifies the pending map request, not completion of
the original final quest or arrival in those special areas. The remaining cross-continent quest, scroll greeting
wrapper, and BrBase/BrAlvar/Breach-specific scripts remain incomplete. Some native status/UI text remains English.

No destination was selected for an actual cross-world scene transition in this checkpoint. Cross-world party,
quest, map-state persistence and independent reload/return are the next acceptance milestone. No iPad or simulator
was operated. Runtime085 remains the complete baseline; the new development overlay is `events-002` and must be
included in a derived runtime or a new import for application use. The normal continuation save remains checkpoint113.

Overall estimate remains **50–55%, low confidence**, based on capability and remaining engineering risk.
