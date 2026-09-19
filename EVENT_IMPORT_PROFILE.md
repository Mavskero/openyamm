# Native merged event import

2026-09-11 scene expansion (045): native conversion now selects a list of map stems,
exports Global once and emits OUTE3/6D01/6D02/6D18 in the assembled runtime. Requested
missing or duplicate maps fail the whole event stage. The four maps plus Global contain
1,525 events and 83 map-local string slots. Dynamic texture/sprite metadata feeds indoor
resource collection. Shared runtime checks cover entrance key gates, entry handlers,
door/chest targets and exits, without claiming walking, combat or rendered transitions.
See INDOOR_IMPORT_PROFILE.md. Earlier narrower evidence below is retained for history.


2026-09-11 runtime correction (042): testing actual normal character creation
found that the full New Sorpigal on-load chain set QBits 1104/1105 but lost item
2125. The previous isolated event-100 check could not reveal a later handler
clearing the reward. Shared event preparation now retains pending output queues
through on-load/timer batches that request preservation; independent events
still clear prior outputs. No generated Lua or conversion output changed.
Focused checks cover ordered rewards across two entry handlers, independent-event
reset and the complete imported entry chain. Actual application startup, shared
item placement, new-process reload and one-letter persistence pass on Mac.
House reward/repeat/save checks remain passing.
Evidence: ../local-data/validation/new-game-042/. Physical acceptance is paused.

Checkpoint: 2026-09-10. This is an incomplete gameplay import, not acceptance of
all quests or MMExtension behavior. Only shared Global plus New Sorpigal is
currently assembled in `../local-data/gameplay-base-030`, now including house
presentation metadata from `SERVICE_IMPORT_PROFILE.md`.

## Inputs and ownership

`event_import SOURCE CONVERTED_BASE NEW_DESTINATION [MAP_STEM]` reads binary EVT
and NUL-separated STR directly from the original LODs. `SOURCE` uses the same
layout as the native iPad file import. The already converted base supplies house,
NPC, topic, quest and explicit map/world lookup tables; no Python-extracted event
files or diagnostic reports are used as inputs.

Archive priority is `mm6.T.lod`, `mm7.T.lod`, `mmmerge.T.lod`, then optional
`10 LocZHCN.EnglishT.lod`. The language archive may replace STR, not event
mechanics. Chinese legacy text is decoded per slot; empty slots and embedded
newlines stay in place. Localized/base slot counts must agree. Shared Global is
written once under `engine/events/Global.lua`; map events go under
`worlds/<world>/events/maps/<stem>.lua`.

All supplied merged EVT files use MM8 field widths, including the MM6 and MM7
archive contents. Keep merged QBits/item/house IDs unchanged, including 16-bit
MoveToMap house IDs. Validate instruction payload lengths and inline string
terminators before export. Unknown/unimplemented opcodes (including 20, 27, 28)
fail explicitly. Bad imports remove their new partial output; existing output
and both input roots are protected.

The converter embeds the repository's own
`assets_dev/engine/scripts/common/event_support.lua`. This tracked file was
absent only because of sparse checkout; it was restored unchanged from HEAD.
Generated event Lua is never manually patched. The exporter now emits
`evt.CanPlayerAct` for opcode 68: the original MM8 command checks an actionable
roster member, not an NPC follower with a coincidentally equal ID.

## Verified evidence

- `events-resources-003`: 1,283 global source events / 12,079 instructions and
  99 OUTE3 source events / 417 instructions. All 34 local STR slots survive,
  including 188 restored Chinese glyphs. Output is byte-identical to
  `events-resources-002` after stricter payload validation.
- `event_import_runtime_check` loads generated Lua with the real event runtime:
  1,283 global handlers, 99 local handlers and 157 topic-condition handlers bind.
  Event 100 is registered for map load, sets QBits 1104/1105, requests one item
  2125, and issues no second request when repeated.
- The same check converts a synthetic binary opcode-68 branch and executes it
  against ready, unconscious, zero-health, missing-roster/same-ID-follower and
  recovered-roster parties. Both gold outcomes are checked.
- Formal Mac Metal game: actual UI-created saves before/after loading prove
  QBits 1104/1105 and exactly one letter 2125. Map, position, camera, gold, food,
  member count, all quest bits and member inventory match. This checks initial
  event execution and save/load, not handing in the quest.
- Evidence lives in `../local-data/validation/event-gameplay-001/`,
  `event-runtime-check-003.json`, `event-reexport-comparison.json`,
  `event-import-tests.log`, `event-resource-suite.log`,
  `event-import-integration-tests.log` and `event-import-ios-build.log`.
  Native Mac tools and the iPadOS converter library/app build pass. The library
  is not yet wired into the iPad import interface. No iPad was updated.

## Outstanding behavior

Metadata/Lua binding checks do not execute every branch. `runtime_ready` and
`mmextension_lua_overlays_applied` remain false in the manifest. The current
package omits authored map/global supplements, cutscenes, cross-world quests and
the MMExtension hook behavior. Upstream OpenYAMM already contains some authored
supplements; review them against the user's version before mounting them.

Global riddles 164–166 currently have localized question text but unresolved
answer strings 100–105. The existing exporter can collect the first same-numbered
STR entry from any map; the new converter deliberately does not use that ambiguous
lookup. The runtime currently receives no answer strings for these questions.
They require explicit map-context resolution and correct/incorrect-answer tests
before they are playable.

Importing all registered maps currently stops at `breach.odm`, whose original
package has `Scripts/Maps/Breach.lua` but no EVT member in the selected archives.
This is a custom-Lua porting gap, not proof that the user's Breach content is
missing. Other maps also need validation; unsupported opcodes must be understood
instead of silently omitted. House animation/resident mappings now load; the
shared dialog controller passes the initial letter reward, topic replacement
and save/reload check. Full rendered NPC conversations, shops and the first
complete playable quest loop are pending.
