# Dimension Door destination and cast validation — checkpoint 117 accepted

The controlled Mac roundtrip, ordinary Temple healing and independent reload are accepted.
Work is paused at the user’s request. iPad validation is separate and outstanding.
The accepted checkpoint 116 activation evidence is unchanged and sealed in `dimension-door-116`.

## Source contract and changes

The supplied `Scripts/General/2_OutdoorTravels.lua` selects daily destination MapStats IDs and matching icons.
The three rotations are Jadame `{1,2,6,1,2,7,7,8}`, Antagarich `{64,67,67,68,70,65,66,72}`, and
Enroth `{144,151,150,143,147}`. Antagarich uses `ceil(day/1.2)`; the others use the day modulo array length.
The UI now uses the activation day already recorded by checkpoint 116, preserving the selected rows across midnight
until reactivation. The former fixed death/new-game landing rules depending on QBits 4 and 519 have been removed.

The supplied executable copies TPGlobal zero transforms literally for same-map travel. Cross-map loading first
selects the authored Map Start, then replaces only nonzero pending components. The native destination request now
uses Map Start with no direction override for a different map, and literal zero XYZ/yaw/look on the same map.
An optional pending look angle was added to the shared spell/map-move path so the latter can reset pitch as well.
Other map moves keep their existing angle behavior when the optional field is absent.

Ordinary Town Portal selection now preserves the original caster request and uses the common spell-table mana and
recovery rules on completed selection. Explicit direct scroll/event casts retain their own GM10/free context;
this is not acceptance of all scroll/NPC acquisition or timing behavior. In particular, the original item-190
one-minute reversion still needs a dedicated check against the current native activation path.

The source audit found no quest consumer requiring the temporary MMExtension portal-switch aliases. Native explicit
activation state and unlocked DD controls remain authoritative, preserving the party's ordinary portal discovery bits.

## Evidence under local-data/validation/cross-world-travel-117

- `audit/original-destination-parity.md`: supplied script/table contract and source hashes.
- `audit/zero-transform-and-qbits.md`: executable addresses, reproducible disassembly and bounded QBit-consumer audit.
- `runtime-001` and `package-result-001.json`: complete cloned runtime containing the checkpoint-116 overlays;
  all four ZIP CRCs validated, previous runtime 085 and normal save 113 unchanged.
- `native-build-001.log` and `native-build-002.log`: successful native builds.
- `activation-regression-001.log`: 22 cases / 484 assertions passed.
- `contact-regression-001.log`: 7 cases / 488 assertions passed.
- `destination-tests-001.log`: failed because the fixture omitted normal HUD layout initialization.
- `destination-tests-002.log` and `destination-crash-002.ips`: first repair then exposed missing settings binding
  in the full input bootstrap; a subsequent scene-adapter omission was also repaired. These failures are retained, not counted as passing.
- `destination-assets-001.json`: negative check — the prior runtime lacks the DD background and 21 dynamic icons.
  Standard UI importer support is now verified: 27 synthetic cases pass and the original source supplies all 22 assets.

The old fixed-route capture draft and its plan were superseded before execution. The source is preserved in
`capture-tool-source-001.cpp`; the save-audit script now checks daily outbound cost and has passed the fixture and initial cold-load boundaries.

Overall engineering estimate is now 55–60%, low confidence, after the actual roundtrip milestone.
This is not a percentage of quests or maps verified.

## Completed bounded checks

`destination-tests-004.log` passes 7 cases / 444 assertions after the third fixture repair. The first two repairs
were unsuccessful; no production crash was established by these incomplete harness bootstraps.
`destination-test-result-001.json` records commands, source hashes and failure history.

`ui-tests-001.log` passes 27 native importer tests; `ui-001/ui_conversion.json` confirms all 22 original DD assets
with source archive provenance. `runtime-002` adds exactly those images and preserves all prior archive entries;
`package-result-002.json` records full CRC validation of all four ZIPs.

On runtime002, destination-tests005, activation-regression002 and contact-regression002 pass 36 cases and
1,416 assertions. iOS Release001 compiles all changed native production units, with ten source hashes unchanged.

Original presentation geometry is now audited in `audit/portal-presentation.md`. The shared resolver uses inclusive
image bounds, reverse slot priority and the latched pressed owner during drag/release. The renderer already had
hover-only drawing; the change makes rendering and input agree on a single winning preview. The CPU metadata path
uses the existing original image loader when a renderer is not initialized.

`hotspot-result-001.json` records the final 39 cases / 1,475 assertions: destination-tests007 (10/503),
activation-regression003 (22/484) and contact-regression003 (7/488). iOS Release002 passes with 13 stable source hashes,
independently matched to the workspace. These results do not prove actual cross-world loading.

Mac `jadame-001` ran the real application and waited normally until day24. All inspected character fields remained
unchanged during waiting. The declared controlled caster fixture and its cold restore each pass 42 independent
save-inspector checks. The run then stalled on its ordinary Forward route at waypoint6 before reaching the fountain;
no destination preview or travel occurred. The failed log and saves remain preserved.
Later runs below repaired the capture route and completed ordinary healing and return.

## Accepted actual outbound and reload

`outbound-result-001.json` records the bounded acceptance. Mac jadame006 reaches original out01 Map Start
(3560,7696,545), spends20SP, starts recovery and retains the party's HP/conditions/inventory/resources.
Independent process jadame-reload002 passes cold, warm and final save-inspector boundaries (42 checks each).
The three original daily destination previews and actual arrival were visually reviewed by Astra.

The first strict outbound audit rejected two normal changes. `audit/outbound-state-differences.md` documents
GameSession's automatic ready-member2→3 switch and the supplied global NPCMercenaries first-map/month refill
which relocates the unhired candidate NPC303 from house264 to out01's tavern228. The revised audit permits only
those exact fixture changes and holds the remaining prior checks; jadame006 and final jadame007 each pass56.
The original failed audit remains available.

The UI review found pink margins and exposed HUD around the4:3 portal canvas. The shared renderer now draws
an opaque full-screen backdrop for portal modes. Jadame007 passes the complete first leg (581frames/5BMP),
and all pixels in both outside strips are black for each of the three previews. iOSRelease003 compiles the final
renderer. The initial build008 never started CMake because the shell lacked the project tool PATH; corrected009
passes and the failure log is retained. No production NPC change or gameplay-state edits were needed.

## Accepted ordinary return and final pause

`capture-return-result-001.json` records Sol high’s complete return module. From the saved arrival, ordinary frames
first expire the caster’s remaining recovery. Normal movement and house input reach Temple303 and purchase Heal
for 10 gold: caster HP9→24, SP0→39 and poison64→0, with no game-time advance. The other party members remain
unchanged, including the archer’s poison. Astra independently compared the before/after saves: 44 checks passed.

Ordinary movement then activates terrain tile63,59. The spellbook and original Enroth preview select day24
map147/outd2, spending20SP and applying recovery. Actual loading reaches authored Map Start (6880,-22208,97),
with yaw0 and pitch−0.15. Saved active member2→3 follows ordinary readiness selection; NPC303 moves228→266
under the original un-hired mercenary refill. No quest bits change during this return. Astra’s bounded return
audit passes60 checks. No extra HP/SP/condition/position fixture edits were made for the return.

Enroth002 completes1476 frames and5 screenshots. A separate reload process passes cold/warm/final audits,
42 checks each, retaining the healed caster’s HP24/SP19/condition0 and saved active member3.
Astra reviewed the Temple, terrain activation, preview and arrival images. Some house service strings remain
English and house canvas margins still need polish. Save checks cover inspector-exposed fields, not every binary
field or condition timestamp. These results validate the controlled route, not natural spell acquisition or a full campaign.

`result.json` is the final bounded checkpoint acceptance; `seal.json` hashes its selected evidence. The earlier
`outbound-result-001.json` remains an immutable first-leg receipt whose then-pending return is superseded here.
Production hashes match generic iOSRelease003. All three agents and project build/capture processes finished.
The user requested a pause after this section; no further work is scheduled by this checkpoint.
`audit/scroll-timing.md` records the separate item190 one-minute timer backlog for a later explicit resume.
