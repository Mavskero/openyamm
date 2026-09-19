# MMMerge class mechanics import profile

The native `class_import` converts four authoritative loose TXT tables from the
supplied 2023/2024 installation. It runs offline on Apple platforms, reads original
bytes directly, and requires neither audit exports nor Windows/Lua execution.
This phase imports class mechanics; Chinese character inspection text, race rules
and the character creation interface remain pending.

## Source and runtime schemas

| Source under `Data/Tables` | Shared output under `engine/data_tables` | Conversion |
| --- | --- | --- |
| `Class HP SP.txt` | `class_multipliers.txt` | Retain four HP/SP values per active class; translate SPStats tokens into runtime semantic names |
| `Class Extra.txt` | `class_extra.txt` | Retain explicit merged ID, Kind, Step and source name; omit two declared inactive slots |
| `Class Skills.txt` | `class_skills.txt` | Retain 39 skill rows and every active class column; validate canonical skill order |
| `Class Starting Skills.txt` | `class_starting_skills.txt` | Transpose skill rows to class rows, expanding positional class pairs by `floor(classId / 2)` |

The supported source has 53 class slots, 39 skills and 30 starting-skill groups.
Class names agree across the HP/SP rows, cap columns and explicit metadata after
the shared canonical-name conversion. Metadata IDs must match the source row
positions. Numeric resource values must fit the original unsigned byte fields;
mastery tokens are restricted to `- B E M G`, starting tokens to `- C F`.
Headers, shapes, numeric parsing and all positional joins are checked before output.

Slots 49 and 52 are both explicitly `n/u`, Kind 0, Step 0. Their duplicate names
would collide in the runtime's name-keyed tables, so both remain outside the active
registry and are recorded as inactive. Activating either slot requires a reviewed
profile. The remaining 51 classes keep their original IDs: High Priest remains
50 and Master Wizard remains 51. `class_extra.txt` supplies explicit identities
after the multiplier loader reads the reduced row set. Native round-trip checks
verify every final ID and that neither unused slot became an active class.

## Mana attribute tokens

`Scripts/Structs/After/DataTablesSupport.lua`, function `DataTables.HPSP`, assigns
source `-`, `I`, `P`, `IP` / `PI` to mana modes 0, 1, 2 and 3. The existing shared
loader expects words, so conversion explicitly maps these to `None`, `Intellect`,
`Personality` and `Mixed`. Copying the source tokens unchanged would silently
select None in the shared loader. All four numeric HP/SP columns remain unchanged.
English class/skill identifiers select mechanics and are not localized here.

Every class is probed through production HP/SP calculations at levels 1, 10 and
25, with no equipment or skills. Fixed endurance 14, intellect 25 and personality
30 have bonuses 0, 5 and 6, allowing separate checks of each mana mode and growth
factor. The table is passed explicitly; no global mechanics pointer is changed.
This validates base formulas, not equipped characters, leveling interactions or
combat on iPad.

## Starting skills and promotion families are different indices

`DataTables.StartingSkills` reads positional columns into `Game.ClassKinds`.
The supplied `Scripts/General/MenuChooseCharacter.lua:218` and
`Scripts/General/NPCMercenaries.lua:249` both select the group using
`floor(classId / 2)`. Source header labels are descriptive comments and sometimes
misleading: Archer 0 and WarriorMage 1 both use group 0; MasterArcher 2 and Sniper
3 use group 1, whose label is WarriorMage. This lookup is also distinct from
`Class Extra`'s Kind field used for promotion families.

High Priest 50 and Master Wizard 51 share group 25 as authored. Source groups
26–29 have no active class consumers in this profile and are recorded as unused.
No promoted class is assigned a guessed base-class default. The resulting
51-by-39 starting matrix retains every source `-`, `C` and `F` selection. All
102 default assignments in the real source produce level-1 Normal skills through
the production loader.

Kind/Step metadata drives native checks of all class-pair promotion comparisons
and the highest cap among reachable higher promotions for all 39 skills. The
converter validates legacy fallback promotion edges before traversing them,
rejecting cycles or family crossings in malformed profiles. No promotion rules
or event scripts are rewritten.

The source has one explicit disagreement: class 46 ArchMage receives default
Dark magic from starting group 23, but its class cap is `-`. Both source values
are retained and recorded in `starting_without_class_cap`. Race skill overrides,
class selection constraints and actual creation/recruitment still need integration
before deciding how the complete game should treat this combination. The native
converter does not remove that skill or invent a mastery cap.

## Reproduction, validation and remaining work

```sh
cmake --build build-apple --target class_import
build-apple/class_import /path/to/MMMerge /path/to/new-class-output
python -m pytest tests/test_class_resources.py -q
cmake --build build-ios --target openyamm_class_import
```

Conversion uses a fresh destination, refuses existing outputs and overlapping
source/output paths, rejects source symlink escapes, and rolls back a failed new
output. Files use mode 0644. Sixteen tests cover all positional cells, unchanged
source bytes, IDs after unused slots, mixed mana spellings, default skill levels,
promotion behavior, invalid schemas/tokens/numbers and directory boundaries.
All 138 resource tests pass. Mac compilation/linking and iPadOS arm64 library
compilation pass without a connected iPad.

Latest tables: `../local-data/class-tables-002/`; combined assets:
`../local-data/gameplay-base-007/`. Independent direct-source comparison verifies
all 51 original active IDs, 204 numeric resource cells, 1,989 cap cells and
1,989 starting-skill cells. The four combined files are byte-identical to the
conversion output. Evidence:

- `../local-data/validation/class-source-comparison.json`
- `../local-data/validation/resource-tests-class.log`
- `../local-data/validation/class-mac-build.log`
- `../local-data/validation/class-ios-build.log`
- `../local-data/validation/gameplay-start-007/`

Formal headless startup now passes class multiplier and skill loading and stops
at required `engine/data_tables/english/stats.txt`. The existing optional
`house_animations.txt` warning remains. Character inspection/localization, race
starting stats/overrides, selection restrictions, portraits and the playable loop
remain incomplete. Output readiness flags remain false. The installed iPad app
has not been updated by this offline round.
