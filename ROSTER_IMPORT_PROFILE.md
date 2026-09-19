# MMMerge roster import profile

`roster_import SOURCE BASE_ASSETS NEW_DESTINATION` reads the original archive
`mmmerge.T.lod/roster.txt`, optional ZHCN overlay, the declarative roster class
mapping in `Scripts/General/MiscTweaks.lua`, and original character portrait IDs.
It validates against converted class, multiplier, item and NPC topic/text tables.
It never executes Lua or Windows code and publishes only to a fresh destination
outside both inputs. Invalid input leaves no partial output. Files are mode 0644.

## Identity and mechanics

The supported roster has two headers and 125 columns. The second header defines
39 skill mastery/level pairs, 12 spell-school counts, an unlock bit, ten equipment
slots and biography text. IDs are preserved. Only the explicitly identified
zero Placeholder row is omitted. The supplied roster has 50 active entries,
including creation/random/service slots 36–50; they are not discarded as duplicates.

The native converter narrowly parses `local ClassConns` integer assignments and
rejects expressions, duplicate aliases or unavailable class IDs. Class tokens are
resolved to numeric merged IDs before loading. In this source `cleric2` means 6
(Priest of the Light), whereas the engine's historical generic alias means 5
(Priest). Roster IDs 18 and 26 therefore use 6; the source's explicit numeric 5
for roster ID 2 remains 5. The global legacy class alias is left unchanged.

Equipment cells retain the documented `item ID + 1000 * enchantment level`
encoding. The supplied 350 nonempty slots resolve to existing items. This format
is also confirmed by the owned MM8 executable's roster-reading divisions at
VA 0x494d42 and 0x494d5a. Values with no item remainder or unavailable items fail;
merged item IDs above 999 are not silently interpreted as direct roster IDs.
Original random enchantment generation is a separate, unvalidated behavior.

Roster skill fields carry a level with optional E/M/G mastery bits. Original
MM8 reading at VA 0x494c0c–0x494c9c compares those three strings and combines the
level with the mastery bits. A positive level with N or an empty marker means
Basic. The shared loader now retains these skills; in the supplied roster this
recovers ID 4's Bow N/1. Skill parsing is bounded to columns 22–99. No source cell
is rewritten for this correction. The independent binary inspection used the
same owned EXE fingerprint documented in `FX_IMPORT_PROFILE.md`.

## Chinese overlay

Only name and biography fields are taken from matching translated IDs. Every
other source field wins over older localization, and differences are counted.
Unknown localization IDs are ignored and reported. The supplied source restores
2,683 legacy glyphs and applies 50 names and 50 nonempty biography fields.
Recruitment topics use numeric IDs, preserving association through translated
names; 49 existing join offers resolve. This does not establish that all those
slots are accessible through actual gameplay events.

## Validation and limits

All converted rows round-trip, load through `RosterTable`, and enter the shared
Party recruitment path. Checks compare identity, numeric attributes, every skill
and spell count, known spells, and equipped plus carried item quantities. Both
copies of repeated items must survive. An independent archive decoder compares
every output field against the source and text overlay, including class mapping.
Synthetic integration tests cover Chinese controls/multiline text, patched class
identity, old mechanics, template slots, unusual skill markers, invalid input and
path boundaries. Actual recruit UI, all unlock conditions, portrait art, voice,
original enchantment rolls and a full gameplay loop are not yet validated.

Checkpoint `roster-tables-003` is byte-identical to the roster in
`gameplay-base-015`. All 24 roster tests and the complete 286-test resource suite
pass. Mac game/converter and iPadOS arm64 builds pass. Formal startup now reaches
required `character_data.txt`; optional house animations remain absent. No iPad
was connected or updated during this phase.
