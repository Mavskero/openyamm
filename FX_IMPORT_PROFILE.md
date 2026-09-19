# MMMerge spell and face association import profile

The native `fx_import SOURCE BASE_ASSETS NEW_DESTINATION` exports three shared
tables from the supplied MM8 executable profile and previously converted gameplay
data. It never executes, loads or patches Windows code. This phase is not a full
FX implementation; automatic face reactions and original runtime patch parity
remain pending.

## Supported source and provenance

The exact supported `mm8.exe` is 1,155,072 bytes, SHA-256
`97be86a7597fbd9763f9b4b0ed1f4a7e47c960703fa30857c32daa056b7d403b`.
The full hash is checked before using any fixed offsets. Other binaries require
an explicit profile; a changed byte fails conversion. No proprietary executable
or instruction body is bundled with the port.

The shared native file importer now preserves a root `mm8.exe` as data, normalizes
its name and sets permissions to 0644. Setup programs, DLLs and nested EXEs remain
excluded. The original source permissions and contents are unchanged. A fresh
real import contains 377 files and 1,303,024,352 bytes, with runtime validation
still false. Earlier 376-file imports do not contain this new input.

The standard face array is identified by the supplied MMExtension definitions:
`Scripts/Structs/00 structs.lua` exposes `StandardFaceAnimations` at virtual address
0x4fd660 with 110 records. `FaceAnimationInfo` in `01 common structs.lua` describes
two sound bytes, a reserved byte and five ordered expression bytes per record.
The fingerprinted file maps this array to file offset 0xfd660.

The source `Scripts/Core/ConstAndBits.lua` supplies numeric face-event names.
A narrow declarative parser preserves those labels, including original spelling,
and handles CRLF. Numeric IDs remain authoritative. Unnamed active slots retain
their numeric label, matching the source module's export convention. A loose
`Data/Tables/Face Animations.txt` override is currently refused explicitly rather
than silently replaced by the binary default.

## Spell associations

`engine/data_tables/spell_fx.txt` contains 37 spell-to-animation associations.
The associations were obtained by inspecting the supplied selector at virtual
address 0x4a6fce, and independently checked by bounded branch analysis for all
149 ordinary selector inputs. Only comparison/arithmetic branches and the final
resource-name selection are interpreted by the offline diagnostic; it makes no
native calls and performs no game execution.

The importer contains the observed resource associations as a fixed profile.
Each spell must exist in the production `SpellTable`; its current localized name
is retained. Every animation must resolve to an active group in `IconFrameTable`.
For example, spell 8 selects `spell03`, and spell 27 selects `spboost2` despite the
presence of an independently preloaded `spell27` group. Resource names or preload
lists alone are therefore not used to infer bindings.

Generic selector values 150–153 map to `spboost1`, `spboost2`, `spboost3`, `zapp`.
They are not spell IDs and are not exported as spells.

## Portrait events and operation order

`engine/data_tables/portrait_fx_events.txt` defines seven existing event kinds.
The first four columns preserve kind/name/default-animation/optional-face.
Columns 4–6 supply Set/Add/Subtract variants. Older three/four-column tables still
select their original animation and optional face choice.

The original call sites are independently verified: Set at 0x448453 supplies
selector 150, Add at 0x448df4 supplies 151, Subtract at 0x449096 supplies 152,
and the monster-special call at 0x48d548 supplies 153.

Shared `EventRuntime` requests now retain the operation separately from their
semantic kind; `GameplayFxService` selects the corresponding table entry. Negative
Add still uses Add, and a decreasing Set still uses Set. ClassId Add preserves its
operation when delegating to Set. Only adjacent requests of the same kind/action
merge their member lists, preserving Set/Add/Set order and its final effect.

A native two-member diagnostic checks that sequence, Subtract, negative Add,
unchanged arithmetic, member targeting, adjacent coalescing, unchanged-value
suppression and old table behavior. All 28 kind/action bindings resolve to IFT
groups. No automatic facial reaction is guessed from the broad semantic kinds.

Direct OpenYAMM notifications without a variable operation default to Add for
positive gains, Subtract for decreases and 153 for monster special effects. This
is an explicit profile policy, not proof that every trigger, sound and reaction
matches patched Windows behavior. `automatic_face_reactions_validated=false`
and `runtime_patch_parity=false` remain in the report.

## Face choices and unresolved source data

`engine/data_tables/face_animations.txt` contains 101 active face events and 146
ordered expression choices. The original table has eight rows with no expression:
0, 33, 35, 90, 94, 95, 102 and 109. These remain absent from the active expression
registry; a sound-only event is not converted into an invented facial expression.
The 47 nonzero sound slots are counted but deferred to voice integration.

Event 64, `LookDown`, contains expression 32. The supplied loose PFT does not
define group 32, so the event is recorded as unsupported and omitted. Its original
reference remains documented in the manifest and source comparison. Another
portrait is not substituted. Missing portrait references in other records fail
conversion. Every supported choice is compared through `FaceAnimationTable`,
including order, count, event ID and source name.

## Verification and current limit

Eighteen FX integration tests use an owned binary fixture (set `MM8_EXE_FIXTURE`)
plus synthetic prerequisite tables. The binary is not distributed with the tests;
fixture-dependent checks skip when it is unavailable. Cases cover source-choice
conservation, Chinese names, aliases, missing prerequisites, profile changes,
loose overrides, CRLF, unsupported enumeration syntax, output preservation and
canonical path escapes. One added native file-import test verifies EXE selection,
byte preservation, permission removal and installer exclusion.

All 242 resource tests pass on the development Mac. The Mac game and converters
link; the FX library and modified iPadOS app compile/link for arm64. No app was
installed on the disconnected iPad. All output files use lowercase names and 0644;
fresh destinations are required and failed writes roll back.

Artifacts:

- Latest `../local-data/fx-tables-002/`, combined `../local-data/gameplay-base-013/`.
- `../local-data/native-import-002/`, including the new executable data input.
- `../local-data/validation/fx-source-comparison.json`: independent original
  branch outcomes, face bytes, names and combined-file equivalence.
- `../local-data/validation/fx-events-source-comparison.json`: original operator
  call sites and bindings; the first two tables remain byte-for-byte unchanged.
- `../local-data/validation/resource-tests-fx-events.log`, `fx-events-mac-build.log`,
  `fx-events-tools-build.log` and `fx-events-ios-build.log` in the same directory.
- `../local-data/validation/gameplay-start-013/startup.log`: formal startup passes
  all three FX/face tables, then stops at required `engine/data_tables/english/trans.txt`.
  Optional house animations remain missing.

No new art is imported. Missing images, portrait rendering, sound choices, patched
Windows behavior and full gameplay remain unaccepted. `runtime_ready=false` and
the low-confidence overall 35%–40% estimate remain unchanged.
