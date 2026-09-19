# MMMerge race and character selection import profile

The native `race_import` reads the supplied installation's loose tables directly
and validates their associations against previously converted class assets. It
exports three shared tables without running the original mod or requiring an
iPad. This is data integration, not acceptance of the character creation screen.

## Source and output contracts

| Source under `Data/Tables` | Output under `engine/data_tables` | Preserved identity |
| --- | --- | --- |
| `Class Starting Stats.txt` | `class_starting_stats.txt` | Eleven race IDs 0–10 and seven attributes per race |
| `Race Skills.txt` | `race_skills.yml` | Race ID, skill key, optional class kind and original rule token |
| `Character selection.txt` | `character_selection.yml` | Race/class matrix and ordered continent selection lists |

`Continent settings.txt` supplies continent IDs/names for association checks only;
its remaining mechanics are not imported here. The four class source tables and
their converted outputs must agree on IDs, kinds, HP/SP, mana modes, caps and
starting skills. All 51 active classes retain actual merged IDs; unused class
slots 49/52 cannot enter selection lists. No class tables are rewritten.

## Race attributes and point costs

Despite its class-oriented filename, the starting-stat source is indexed by race:
Human 0, Vampire 1, DarkElf 2, Minotaur 3, Troll 4, Dragon 5, Undead 6, Elf 7,
Goblin 8, Dwarf 9 and Zombie 10. Seven trailing `n/u` columns have no active race
identity. They are validated and reported as inactive IDs 11–17, then omitted.

Each attribute has a base/maximum pair followed by a `+ add` pair. The supplied
`Scripts/Structs/After/DataTablesSupport.lua`, `DataTables.StartingStats`, defaults
the denominator to one when it is omitted. The converter therefore writes an
explicit denominator: source `2` becomes `2/1`. Otherwise the existing
`RaceStartingStatsTable` would infer a different dropped-point cost for some base
values. Explicit `1/2` remains `1/2`; base and maximum values stay unchanged.

Both production starting-stat loaders agree on all 77 records, including race
IDs, base values, maxima and both step values. Malformed pairs, zero steps/costs,
unknown headers and values outside the byte-sized source range are rejected.

## Skill pools and an unresolved legacy parser difference

The source contains 39 skills and 187 rule columns: 11 races for the general pool,
followed by 16 class-kind pools of 11 races each. Headers must match these exact
targets. The actual data has 18 active rules and one blank Troll/DisarmTraps cell;
the blank and `-` cells produce no override. Raw active tokens remain in YAML.

The existing OpenYAMM contract interprets each token as minimum mastery / mastery
increment / exception. Masteries are `0 B E M G`; exception numbers identify
class kinds, and `I P IP S W` identify mana-mode conditions. General pools apply
the exception; class-specific pools apply within their kind without consulting
the exception. An increment only applies when the class already has a positive
base cap. Effective caps are clamped to Grandmaster. A racial grant can make a
previously unavailable starting skill selectable, but does not make it a default.

There is a confirmed parsing difference in the supplied original
`Scripts/Structs/RacialSkills.lua`: its pattern's first capture accepts `BEMG`
but not numeric `0`. An isolated standard Lua `string.match` probe, using the
pattern read from that file, produces these captures:

| Token | Supplied Lua captures | OpenYAMM three-field interpretation |
| --- | --- | --- |
| `0/1/0` | empty, `0`, `1` | `0`, `1`, `0` |
| `E/1/2` | `E`, `1`, `2` | `E`, `1`, `2` |
| `E/0/W` | `E`, `0`, `W` | `E`, `0`, `W` |

Nine actual rules use `0/1/0`: Human Learning, Vampire Perception, DarkElf
Merchant, Minotaur Bodybuilding, Troll Regeneration, Dragon IdentifyMonster,
Elf Meditation, Goblin DisarmTraps and Dwarf RepairItem. Consequently this profile
retains OpenYAMM's existing three-field interpretation and records
`legacy_lua_parity=false`; it does not claim equivalent behavior to the supplied
Windows game. The whole mod was not executed. A future compatibility decision
needs to check effective original-game behavior and any later overrides before
changing shared mechanics or deliberately reproducing that parser behavior.

Native validation compares production effective caps and starting availability
against the explicit three-field formula for all 51 × 11 × 39 = 21,879
class/race/skill combinations, plus every resulting default skill's name, level
and mastery. This validates that stated interpretation, not legacy Lua parity.
Class 46's source default Dark magic still has cap zero for all 11 races;
production effective creation filtering suppresses those 11 assignments. The
class-only source table retains its original values and reported inconsistency.

## Character selection

The complete race/class matrix retains 68 allowed pairs, including Peasant 48.
Undead 6 and Zombie 10 have empty allowed-class lists. These authored values are
preserved; rules from a different source profile are not substituted.

Four continent records retain their source IDs, names and ordered class/race
lists: Jadam 1, Antagarich 2, Enroth 3 and Between time 4. Existing menu keys are
`jadame`, `antagarich` and `enroth`; the fourth metadata key is `between_time`.
The shared screen currently exposes three continent buttons. Retaining the
fourth record does not add or validate a fourth playable start. Portrait exception
tokens are preserved, but all four actual lists are empty; portrait artwork and
character creation visuals remain unvalidated.

## Reproduction and evidence

```sh
cmake --build build-apple --target race_import
build-apple/race_import /path/to/MMMerge /path/to/class-assets /path/to/new-race-output
python -m pytest tests/test_race_resources.py -q
cmake --build build-ios --target openyamm_race_import
```

Conversion requires a fresh destination outside both inputs. Canonical source
and class-asset symlink escapes are rejected; failed output writes are rolled
back. File modes are 0644. Twenty tests cover source values and preservation,
stat-cost normalization, conditional and class-specific rules, original IDs,
Peasant selection, empty race lists, portrait tokens, malformed inputs, stale
class assets and directory boundaries. All 179 resource tests pass. Mac native
linking and iPadOS arm64 library compilation pass; no device was used or updated.

Actual outputs: `../local-data/race-tables-002/`. Combined assets:
`../local-data/gameplay-base-009/`. Evidence under `../local-data/validation/`:

- `race-source-comparison.json`: independent direct-source comparison of all
  stat fields, active raw rules and selection lists; combined files byte-identical.
- `race-legacy-token-verification.json`: isolated Lua capture comparison.
- `resource-tests-race.log`, `race-mac-build.log`, `race-ios-build.log`.
- `gameplay-start-009/startup.log`: formal startup passes starting-stat loading
  and stops at required `engine/data_tables/english/scroll.txt`.

Optional `house_animations.txt` also remains missing. Formal startup has not yet
reached its later merged YAML-loading phase; those two YAML tables are validated
through their production loaders in the converter. All outputs remain
`runtime_ready=false`, `character_creation_ready=false` and
`full_gameplay_loop=false`. No portrait selection, player party, combat or game
save is accepted by this data check.
