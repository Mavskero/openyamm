# MMMerge bolster source-table import

`bolster_import SOURCE BASE_ASSETS NEW_DESTINATION` preserves the three original
Bolster tables under shared engine/data_tables. It reads loose files by literal
tab-separated lines, matching AdaptiveMonstersStats.lua. In particular, quotes
inside explanatory notes remain literal source characters. Export uses quoted
UTF-8 TSV, mode 0644, to a fresh directory outside both inputs.

The supplied source contains 208 map rows (including placeholder 0), 652 monster
rows (including placeholder 0), 11 active formulas and 18 comment rows. Ten
formulas target `def`; one HP formula targets monster family 76. Formula text and
notes are preserved. Lua performs syntax compilation only, with no libraries
opened and no chunk executed. This does not validate formula variables, results
or integration with combat. The current shared GameplayBolsterRuntime uses
internal calculations and has no imported-formula input; loading the required
formula table must not be described as original bolstering behavior acceptance.

Every active map ID and continent resolves. Applying the production map table
sets the original continent IDs on the 207 map registry entries. Forty-three maps
have positive random-profession rarity limits. All flags, custom sky strings,
optional rarity fields and source notes are retained. Actual skies/weather and
random NPC population behavior remain pending.

All source monster IDs, taxonomy strings, gender, creed, style, magic preferences,
optional numeric fields and 29 summon references round-trip through the production
loader. Existing supported category flags are applied and checked. Nine taxonomy
labels remain represented by table strings without a dedicated engine MonsterKind
flag: Construct, Creature, DarkElf, Dwarf, Goblin, Human, Lizardman, Minotaur and
Troll. The import does not invent new flag semantics for those labels.

The base monster registry additionally contains 652 Reactor; the source bolster
table ends at 651 and has no row for it. This gap is reported without creating a
replacement configuration. Both placeholder rows stay in the exported source
tables but do not create active map/monster records. The profile accepts the
supplied named categories, numeric zero sentinels and nonnegative optional shifts;
unsupported numeric category aliases, negative shifts and empty active formulas
are rejected rather than silently changing their source semantics.

`bolster-tables-001` independently matches every raw source cell. Fifteen new
tests and the full 372-test resource suite pass; Mac game/converter and iPadOS
arm64 library/app builds pass. `gameplay-base-020` passes these tables during
formal startup and next requires `character_voices.txt`. Optional house
animations remain missing. No iPad was connected or updated. Original formula
execution, complete category behavior and full gameplay remain unvalidated.
