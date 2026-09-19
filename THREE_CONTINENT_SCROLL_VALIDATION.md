# Three-continent Dimension Door scroll validation

Checkpoint 122 is accepted for the bounded controlled route described here. One Mac Metal process loaded the
ordinary checkpoint113 MM6 save, used three real item190 objects through inventory/held-item/character input,
selected three destinations by pointer, and completed MM6 → MM7 → MM8 → MM6. This is compatibility
evidence, not natural acquisition, natural Verdant contact, or campaign-progression evidence.

## Controlled fixture

The source save remained read-only:
`local-data/validation/mm6-full-starter-113/replay-003/mm6-candlestick-return-complete.oysav`.
The new fixture changed only the audited boundary recorded in `continuous-005/fixture-diff.tsv`: game time advanced
1440 minutes to day 24, active member changed from 3 to healthy Knight 0 with zero SP and zero recovery,
`MMerge.CrossContinents.MetVerdant=1` was set, and three independent item190 objects were placed at inventory grids
`4,0`, `5,0`, and `6,0` with the matching ever-owned record. Equipment, skills, quests, other inventory, party
resources, and prior map state were retained.

## Continuous actual-Mac route

| Leg | Day-24 selection | Actual world/map | Exact first-frame MapStart | Ordinary proof after load |
| --- | --- | --- | --- | --- |
| MM6 → MM7 | `TPGlobal:2`, `tpantag5` | MM7 `out11.odm` | `(0,0,2241)`, yaw `-pi/2`; original center-height fallback | 8 Forward inputs, 150 units |
| MM7 → MM8 | `TPGlobal:0`, `tpjadam1` | MM8 `out01.odm` | `(3560,7696,545)`, yaw `0`; Party Start decoration | 8 Forward inputs, 150 units |
| MM8 → MM6 | `TPGlobal:4`, `tpenroth5` | MM6 `outd2.odm` | `(6880,-22208,97)`, yaw `0`; Party Start decoration | 8 Forward inputs, 150 units |

The item count was exactly `3 → 2 → 1 → 0`. The active Knight stayed at zero SP and every member stayed at
zero recovery, proving the scroll's free direct-cast context rather than learned Town Portal. The process completed
in 196 normal Metal frames with 12 acknowledged BMP captures and exit code 0. The 267-check structural audit covers
characters, resources, equipment, skills, non-scroll inventories, quests, global/NPC state, earlier map state,
actors, event variables, original OnLoad QBits, and exact monthly NPC placement changes.

The exact original-script exceptions are scoped rather than ignored: the first MM7 load publishes history events
1/2 to continent history, MM8 `out01` sets QBits 185/226/401/407, and NPC303's monthly free-mercenary house sequence
is `264 → 247 → 228 → 266` across the three arrivals. All other audited fields in those classes remain subject
to the structural comparisons.

The MM7 `out11` first frame legitimately uses the original zero-XY fallback and steep terrain; the delayed image
contains untextured distant geometry. The MM6 `outd2` view points into its close steep slope. These authentic results
were preserved rather than hidden or altered.

## Independent persistence

Each accepted arrival save was opened by a separate game process, cold-saved, reloaded inside that process, rendered,
and saved again. Accepted runs are `reload-mm7-003`, `reload-mm8-001`, and `reload-mm6-001`; each produced three
acknowledged BMPs, completed in 44 frames, and exited 0. The 172-check audit requires exact cold inspected state and
exact static warm/final state. Live actor position and normal time may advance while actor core and archived actors
remain exact. MM7 additionally requires exact XY and monotonic Z descent on the authored slope while HP, SP,
conditions, and recovery remain unchanged.

All 21 accepted BMPs were opened and visually reviewed through lossless PNG previews; BMP files are authoritative.
Failed harness runs remain in the checkpoint directory and are not counted as accepted evidence.

## Final gates

- Final scoped native regression on the complete checkpoint119 runtime: Dimension Door activation `26/519`,
  destination/cycle `14/15187`, and Town Portal contact `7/488`; all passed.
- The capture tool containing `--scroll-loop` and `--scroll-reload` was built natively and supplied all accepted
  actual-Mac runs above.
- The cached-alias Xcode project was regenerated, then scheme `openyamm_ipados` built as unsigned Release for
  `generic/platform=iOS` with DerivedData on NVMe. The arm64 Mach-O build succeeded. This is compile evidence only;
  no iPad or simulator was operated.

Machine-readable receipts are in
`local-data/validation/three-continent-scroll-122/result.json`, `audit/continuous-audit.json`,
`audit/reload-audit.json`, and `source-runtime-hashes.json`.

## Coverage boundary

Still excluded are natural scroll acquisition, natural first Verdant contact, Merge campaign progression, broad
unrelated quests, live traversal of every destination map, non-Temple service localization, and current iPad
import/control/lifecycle/media/performance validation. Checkpoint 122 and batch 118–122 do not claim the full port is
complete.
