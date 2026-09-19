# MMMerge award and journal import profile

The native `journal_import` targets the supplied 2023/2024 merged installation
and optional Chinese language archive. It exports five shared UTF-8 TSV tables
under `engine/data_tables/english`, the existing runtime lookup path. Conversion
does not execute Windows programs or Lua and does not require the audit exports.
The output remains diagnostic: `runtime_ready=false`, `full_gameplay_loop=false`.

## Table identity and localization

Base members come from `Data/mmmerge.T.lod`; optional matching members in
`Data/10 LocZHCN.EnglishT.lod` provide display text by numeric ID.

| Source member | Output file | Real-source result |
| --- | --- | --- |
| `awards.txt` | `awards.txt` | 104 original character awards, all localized |
| `autonote.txt` | `autonote.txt` | 696 original slots, 692 nonempty localized notes |
| `history.txt` | `history.txt` | 28 original MM8 slots, 18 active localized entries |
| `mm7history.txt` | `mm7_history.txt` | 28 active localized MM7 entries |
| Verified party-state associations below | `award_display_aliases.txt` | 31 display aliases |

Only award/note body text and history body/title fields are localized. Source
award priority, notes, autonote category and history time tokens are preserved.
Time tokens are retained as data; actual chapter unlocking has not been exercised.
IDs, column counts and headers are validated; duplicate/out-of-range IDs and
translations that erase active text are rejected. Missing translations retain
base text and are reported, while translation-only IDs cannot create entries.

Blank base autonotes 110, 111, 112 and 400 and MM8 history slots 19–28 remain
inactive regardless of translated text. Autonote 696 contains the literal text
`0`, which is preserved. The source `Award` category for notes 394–398 currently
uses the production loader's existing Misc classification. These five IDs are
explicitly reported; this round does not implement a new category or general
unknown-category fallback.

The reader restores legacy glyph runs with the shared strict CP1252/GB2312
decoder. Both quoted multiline fields and the supplied unquoted paragraph format
are supported. Only an unfinished body cell can accept continuation lines; its
final tabs complete the remaining category/time/title fields. Premature numeric
records, extra columns, stray continuations and unfinished bodies are rejected.
Real conversion restores 37,252 glyphs and handles 282 unquoted continuation lines.

## Party award aliases and provenance

The upstream `tests/AwardRuntimeTests.cpp` contract identifies 13 party autonotes
492–504 and 18 party QBits 1537–1554 as merged MM8 award display sources. Native
conversion preserves those actual state IDs and uses these explicit display keys:

- `mm8.award.autonote.<mergedNoteId>` for autonotes 492–504;
- `mm8.award.qbit.<mergedQBitId>` for QBits 1537–1554.

These are new display identities, not a reconstruction of unavailable original
MM8 award numbers. They do not renumber saved QBits, autonotes or character awards.
Display priority is 3, matching the upstream regression contract and the supplied
`LocalizationAndQuests.lua` AutoAward default. No original script is executed.

Autonote aliases use their localized note text directly. Each QBit alias uses note
`qbit - 1032` (505–522). The converter reads base `quests.txt` solely to validate
this association: the QBit's quest display text must be blank and its source Notes
cell must exactly equal the corresponding base autonote body. All 18 associations
are checked; a missing row or mismatch rejects this profile. The newer quest rows
1712–1719 omit optional Owner cells, which are padded without treating them as
unfinished paragraphs. The earlier exported quest table is not rewritten here.

The native production `AwardTable` and `buildVisibleAwards` validate all 135
character/party entries. Each starts hidden, becomes the sole visible entry after
its own state is set, and disappears after that state is cleared. The check verifies
text, key and priority, including counter substitutions. Party aliases do not
mutate `Character::awards`. This is diagnostic state manipulation, not evidence
that a playable quest or event currently sets those flags.

`JournalHistoryTable` validates MM8 and MM7 records with their respective continent
IDs. `JournalAutonoteTable` validates original note IDs, categories, body text and
the existing `(id << 16) | 0xE1` event-variable representation. All validation runs
before the fresh output is committed.

## Reproduction and evidence

```sh
cmake --build build-apple --target journal_import
build-apple/journal_import /path/to/MMMerge /path/to/new-journal-output
python -m pytest tests/test_journal_resources.py -q
cmake --build build-ios --target openyamm_journal_import
```

Existing outputs, original inputs and overlapping source/output paths are
protected; canonical archive paths must remain inside the source directory.
New output creation is transactional and files use mode 0644. Fifteen tests
cover bilingual fields, state visibility, multiline equivalence, inactive slots,
bad identities/associations/categories/numbers, malformed text and path boundaries.
All 122 resource tests pass, and the library compiles for iPadOS arm64 offline.

Latest tables are `../local-data/journal-tables-002/`; the combined root is
`../local-data/gameplay-base-006/`. Independent comparison against audited source
text verifies every output cell, all 18 QBit joins and 13 autonote aliases. The
five files in the combined root are byte-identical to the journal conversion.

Evidence:

- `../local-data/validation/journal-source-comparison.json`
- `../local-data/validation/resource-tests-journal.log`
- `../local-data/validation/journal-mac-build.log`
- `../local-data/validation/journal-ios-build.log`
- `../local-data/validation/gameplay-start-006/`

Formal headless startup now reloads awards, aliases, quests, both histories and
autonotes before failing on required `engine/data_tables/class_multipliers.txt`.
The existing optional `house_animations.txt` warning remains. No required loader
was weakened. Class data, events, complete gameplay UI and a playable game loop
remain pending; this round did not connect or update the iPad.
