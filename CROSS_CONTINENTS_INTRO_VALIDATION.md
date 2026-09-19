# Original Merge cross-continent introduction — checkpoint 114

Status: focused checkpoint114 acceptance passed. Normal gameplay continuation remains the accepted save from 113.
All work and evidence are on NVMeSSD. No iPad, simulator or natural encounter was operated in this section.

## Scope and implementation

The native event importer previously exported the merged EVT/STR programs but omitted the original
MMExtension cross-continent quest. This section adds an explicit, limited translation of NPC803
(维尔丹) introduction behavior, gated by the supplied `Quest_CrossContinents.lua` SHA-256
`0790af9b1b387b7db901f0b568593df725516e823a86fa0edb2d1fde459bce49`.

- `CrossContinentsResources.cpp` recognizes the supported original script without executing it.
  A changed script is rejected for compatibility review; absence emits only the existing base events.
- `EventResources.cpp` validates the required imported NPC/topic/text/greeting rows, emits
  `engine/events/Global_mmmerge_cross_continents_intro.lua`, and records the selective profile.
  The general `mmextension_lua_overlays_applied` flag remains false because other source Lua is unsupported.
- The authored overlay uses the shared global NPC-enter hook and imported NPCText2149–2152.
  Its intro occupies slot1 and uses topics1778,1779,1779,1780 in order. It persists `MetVerdant`,
  `GotIntro1`, `GotIntro2`, and `GotMainQuest` under the existing global save namespace.
- Leaving and reentering NPC803 before acceptance resets the two intermediate flags. Once met,
  the conversation resumes at text2150. Completing the intro changes greeting328 and hides the
  intro without adding experience, gold or items. Later continent and hint topics are not implemented here.
- The source overlay is embedded in native/iOS importers at CMake configuration; its file is registered
  as a configuration dependency. Regeneration does not erase the change.
- `openyamm_save_inspect` now exposes sorted `named_global_vars` alongside existing map variables.
  Save format and gameplay session serialization are unchanged.

## Evidence and acceptance

Evidence root: `../local-data/validation/cross-world-114/`.

- `audit/original-behavior.md`: Sol high comparison against the original supplied script and tables;
  includes differences from the older OpenYAMM-authored English summary script.
- `events-001/event_conversion.json`, `import-audit-001.json`: original script/profile accepted,
  eight referenced Chinese NPC/topic/text rows present; shared Global and three world-map programs
  byte-identical to their 085 package versions. Changed-source rejection and absent-script behavior passed.
- `events-002`, `greeting-audit-002.json`: final import emits identical bytes and rejects a missing
  completion greeting328 without leaving an output directory.
- `intro-test-result.json`, `intro-tests-005.log`: **4 cases / 273 assertions passed** with actual
  imported Chinese rows, an MM6 headless GameDataLoader overlay-discovery check, and partial/completed
  cross-world cached-state binary save fixtures. No experience/gold/food/item/Q1104/Q1105/house relocation side effects.
- `global-inspector-audit.json`: a separate save-inspector process confirms the partial/completed global flags.
  `normal-save-inspector-audit.json` checks the unchanged normal113 save with the added inspection field.
- `ios-build-result-003.json`, `ios-release-003.log`: unsigned generic iOS Release **PASS**, arm64 object
  for the new importer present, generated Lua payload byte-identical to the authored source.

The initial test expected accumulated dialogue messages to be cleared on every call. Astra identified the
existing message-history cursor contract; Sol's first fixture repair now requires exactly one NEW message per
click. Diagnostics001–004 remain preserved and do not establish a production double-execution bug.

The first iOS build linked an old object list after ZERO_CHECK regenerated the project. A separate physical-path
configure failed because cached dependencies use the preserved Documents alias. Regenerating with the exact
cached source/build alias and then building succeeded; all files still resolve to NVMe. Original failed logs/receipts
remain intact. No C++ gameplay or save-format fix was needed for either verification issue.

## Explicit limits and next work

These fixtures invoke ordinary event/session APIs at controlled boundaries. They are not a natural
playthrough, a real travel route, or iPad acceptance. Runtime085 and normal continuation save113 remain unchanged.

Natural first contact, scroll generation/acquisition, Town Portal interception, timed catches, Verdant placement,
Enroth letter delivery, post-intro hints, connector behavior, continent rewards and final quest remain outside this
profile. The original `MoveVerdant` helper is never called in its source script; do not treat its nearby comment as
proof of natural initial placement. The next milestone must connect and validate actual contact/travel conditions.

Overall project estimate remains 50–55%, low confidence; this is not a task coverage percentage.

## Delegation

Astra xhigh owns implementation, scope and review. Sol high agents audit original behavior and implement/run
focused acceptance tests; Sol medium executes the iOS build. Same-bug repair failures are counted across Sol agents:
after three unsuccessful
repair/verification cycles, Astra takes over with the full evidence. No production bug has yet required that escalation.
