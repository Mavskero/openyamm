# Character voice import profile

The native `voice_import SOURCE BASE_ASSETS NEW_DESTINATION` converter reads
`Data/Tables/Character voices.txt` from the user's imported installation and
checks portrait defaults against the previously converted `character_data.txt`.
It does not execute the Windows game or original Lua, and does not use Python
audit exports. Output must be fresh and outside both inputs; failed publication
is removed. Exported files use lowercase names and mode 0644.

## Source contract

`Scripts/Structs/CharacterOutfits.lua` defines 100 sound slots and six availability
categories per voice. It reads sound slots by position. The supplied table has
79 contiguous voice IDs, 100 rows of 16-bit sound IDs, two empty separator rows,
six ordered availability rows, and a Notes footer: 110 rows total.

The profile accepts 1–256 contiguous voice sets with the same 100 positional
labels and footer layout. Every source cell is retained. Literal source tabs and
newlines are decoded before serialization into the engine's quoted TSV format;
quote characters in Notes remain literal. Invalid widths, displaced categories,
changed positional labels and out-of-range sound IDs fail before publication.

The supplied 7,900 sound cells include 954 zero/silent slots and 6,554 distinct
nonzero IDs. Repeated labels and repeated sound IDs preserve their original
order and weight. Notes and availability are metadata, never audio reactions.

## Shared runtime changes

The export prepends `#CharacterVoiceMode<TAB>Complete`. The shared
`MergedCharacterVoiceTable` reads exactly 100 sound rows in this mode, then
parses all six availability categories. Unmarked legacy engine tables retain
their prior reaction-list behavior and have no explicit availability override.
An explicitly empty category returns an empty list rather than selecting voices
from an unrelated category.

`GameDataRepository` now exposes this table to `NewGameScreen`. Candidate voice
cycling follows original categories and priority:

1. Original Face 67 or 68 selects Undead Dragon, overriding race and sex.
2. Race 5 selects Dragon.
3. Races 6 and 10 select Undead Man/Woman by sex.
4. Remaining portraits select Man/Woman by sex.

The engine uses one-based character IDs, so the original Face is `id - 1`.
Ordinary male/female voices have 32/24 choices; Dragon has 2; undead male/female/
dragon have 12/4/4. Voice 30 is disabled in every category. Original portraits
28 and 29 are nonselectable special outfits with race -1 and default voices
outside their ordinary category. Their authored defaults remain unchanged and
are reported, not replaced. All 77 portrait defaults are within the voice array.

## Acceptance limits

Typed checks compare every sound cell, all named reaction queries, zero handling,
metadata exclusion, 40 category/portrait/sex combinations, and portrait default
references. This proves indexing and selection rules, not rendered character
creation or audio playback.

The supplied original directory and native import contain 49 MP3 music files,
but no `.snd` archives or `.wav` files. The native importer already permits the
Sounds directory; this is a source-resource gap. This phase exports no audio and
does not create substitute voices. The sound catalog, sound extraction, original
face-event sound bindings and actual playback remain unvalidated. Full gameplay
and device input acceptance also remain pending.

## Offline checkpoint, 2026-09-09

`voice-tables-001` contains the converted source and JSON report. Independent
comparison in `validation/voice-source-comparison.json` verifies every source
cell, availability counts and the original/native audio-file gap. The converter
performs 7,189 named reaction lookups and 40 category/portrait/sex checks.
Sixteen focused tests and the full 388-test resource suite pass. The Mac game,
native converters and iPadOS arm64 app build successfully.

`gameplay-base-021` combines these assets with the preceding checkpoint. Formal
startup passes this table and next requires `complex_item_picture_offsets.txt`.
Optional `house_animations.txt` is also absent. No connected iPad was needed or
updated. These results do not establish audio playback or a complete game.
