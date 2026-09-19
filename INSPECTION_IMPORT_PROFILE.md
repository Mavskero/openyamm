# MMMerge character inspection import profile

The native `inspection_import` reads the original wrapped LOD text members and
loose class metadata directly. It produces three shared UTF-8 TSV tables for
26 attributes, 39 skills and 51 active classes. Chinese titles and descriptions
come from the supplied ZHCN package. The implementation does not execute Windows
code or Lua and does not depend on the audit's extracted files.

## Identity and display fields

| Source member in `mmmerge.T.lod` | Output under `engine/data_tables/english` | Schema (zero-based columns) |
| --- | --- | --- |
| `stats.txt` | `stats.txt` | Original semantic label 0, description 1, appended Display Name 2 |
| `skilldes.txt` | `skill_des.txt` | Original semantic label 0, description/mastery text 1–5, appended Display Name 6 |
| `class.txt`, `Data/Tables/Class Extra.txt` for identity | `class.txt` | Canonical class key 0, description 1, original family token 2, appended Display Name 3 and Class ID 4 |

The existing `CharacterInspectTable` previously used display text as its lookup
key. Replacing English names with Chinese would make attributes unrecognizable
and leave skills/classes indexed under names that gameplay never queries.
The shared loader now reads the optional display-name columns while retaining
column zero for semantic lookup. Unextended legacy tables keep their previous
English naming rules. Skill/class header rows are explicitly skipped, preventing
phantom entries named Skill or Class name.

Existing inspection overlays already use the entry's `name` as their title; no
parallel iPad lookup system is introduced. This round does not migrate every
character-sheet label, mastery prefix, dynamic tooltip supplement or text drawing
path. Those include English literals and behavior-specific descriptions that
still require localization and rendered UI verification.

Physical checkpoints 037–039 now bind the 26 attribute labels and skill-points
headings to these display names. CharacterSkillRows owns the shared canonical
skill groups for drawing, upgrade clicks and inspect popups; labels use the skill
entries while identifiers and levels remain unchanged. Extra skills sort by their
canonical key so translation does not reorder them. Large lists use a shared
scroll viewport with readable row spacing. Actual iPad Chinese labels, scrolling
and short Home/return pass; complete localization/mastery prose/mechanics remain
outside this acceptance. See APPLE_PORT.md and the checkpoint 039 evidence.

## Positional source profiles and paragraph handling

These original tables have no numeric ID column. Their row order defines the
identity. Base stat labels must match the supported 26-entry order, and base skill
labels must match the first 39 canonical skill IDs. Class rows join the 53 explicit
merged IDs in `Class Extra.txt`, with name agreement and inactive-slot checks.

The optional archive is `Data/10 LocZHCN.EnglishT.lod`. A present archive must supply
the supported complete tables. Conversion checks all 26/39/53 translated labels
against the supplied language profile, plus exact headers, reconstructed row
counts and unchanged class family tokens. This detects reordered translations
even when their row counts or family labels agree. The profile is intentionally
specific to this supplied ZHCN label order; a language pack with different labels
requires an explicit profile review rather than an unchecked positional overlay.
Absent localization retains the base English display text and descriptions.

Quoted multiline fields remain intact. The supplied Chinese Peasant description
is unquoted across four physical lines: an unfinished name/body row is followed
by body continuations, ending with a family-token column. The reader reconstructs
the paragraph before assigning class IDs. Stray text, premature full records,
unfinished paragraphs and extra columns fail conversion. The three actual
continuation lines must not shift High Priest 50 or Master Wizard 51.

## Class 47 and inactive slots

The supplied class table displays class 47 as Master Necromancer (Chinese
死灵大法师), with family token Wiz. The authoritative numeric metadata uses
Dark ArchMage for that same ID. The native converter validates this exact source
association and exports internal key `DarkArchMage`, retaining the original
display name in its separate field. It does not create a second class or alter
the previously imported class mechanics.

Source slots 49 and 52 remain `n/u`, Kind 0, Step 0, and are excluded from active
inspection entries. Their IDs are recorded; later IDs remain unchanged. The
original Peasant 48 description is retained. Class metadata is read for validation
only; this converter does not rewrite `class_extra.txt` or the HP/SP/skill tables.

## Verification and limitations

All three outputs are serialized, parsed again and loaded through production
`CharacterInspectTable` functions. Every title, description and all 156 mastery
descriptions are checked through their English/canonical lookup keys. Chinese
labels must not become additional mechanism keys. Legacy naming and header
behavior are checked through the same loader. The native run restores 10,238
legacy glyphs and localizes 116 active entries.

The original language package's prose is preserved; it is not treated as an
authoritative description of current mechanics. For example, the base Stealing
and Learning grandmaster descriptions say quintuple effect, while the supplied
Chinese text says fourfold. This round does not alter either gameplay formulas
or the user's translation by inference. `descriptions_match_mechanics_validated`
remains false, alongside `full_gameplay_ui`, `full_gameplay_loop` and `runtime_ready`.

Twenty-one synthetic tests cover English/Chinese output, all display and lookup
fields, actual class IDs, the class 47 alias, equivalent paragraph encodings,
shifted translations, empty descriptions, malformed metadata/text, original input
preservation and canonical path boundaries. Output creation requires a fresh
directory and rolls back on failure; existing outputs and overlapping source
paths are refused. Source symlink escapes are rejected; files use mode 0644.

```sh
cmake --build build-apple --target inspection_import openyamm
build-apple/inspection_import /path/to/MMMerge /path/to/new-inspection-output
python -m pytest tests/test_inspection_resources.py -q
cmake --build build-ios --target openyamm_inspection_import
```

All 159 resource tests pass. The Mac game links with the shared loader change;
the loader and conversion library also compile for iPadOS arm64 without a device.
Latest output: `../local-data/inspection-tables-001/`; combined assets:
`../local-data/gameplay-base-008/`.

Independent comparison against audited source text verifies every exported cell,
all 116 display names, 195 skill-description cells and the original active class
IDs. The combined files are byte-identical, and earlier class mechanics are
unchanged. Evidence:

- `../local-data/validation/inspection-source-comparison.json`
- `../local-data/validation/resource-tests-inspection.log`
- `../local-data/validation/inspection-mac-build.log`
- `../local-data/validation/inspection-ios-build.log`
- `../local-data/validation/gameplay-start-008/`

Formal headless startup now passes character inspection loading and stops at
required `engine/data_tables/class_starting_stats.txt`. The optional house
animation warning remains. Race starting attributes, race overrides, character
selection restrictions and actual gameplay/UI integration are still pending.
The iPad was neither connected nor updated during this round.
