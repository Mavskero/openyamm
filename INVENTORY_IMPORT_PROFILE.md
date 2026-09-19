# MMMerge scroll and alchemy import profile

Checkpoint 077 extends this same native inventory stage with the already validated item table's
referenced image resources and the shared chest gold images. 910 original images are packaged under
engine/icons with explicit unique-owner validation, lowercase/0644 outputs and per-image provenance;
missing references are reported and corrupt/ambiguous images roll back the stage. Original tables
and mechanics are unchanged. All images match source pixels and actual Mac chest pointer transfer,
Chinese inspection and cold/warm save restore pass. See ITEM_ICON_VALIDATION.md for limits and evidence.

The native `inventory_import` reads original LOD members and loose potion settings,
then validates references against the previously imported shared items and journal.
It does not require Windows, mod execution, an audit export, or a connected iPad.

| Source | Shared output | Identity and scope |
| --- | --- | --- |
| Base/localized `scroll.txt` | `engine/data_tables/english/scroll.txt` | Original item IDs, body, location and item-name comment |
| Base `potion.txt` | `engine/data_tables/english/potion.txt` | Ordered matrix of registered potion operands |
| Base `potnotes.txt` | `engine/data_tables/english/potnotes.txt` | Ordered matrix of existing potion autonote IDs |
| `Data/Tables/Potion settings.txt` | `engine/data_tables/potion_settings.txt` | Potion ID, actual item ID, mastery, drinkable/usable flags and notes |

All original inputs are read-only. Converted item mechanics must agree with the
base archive on every field except the three previously localized display fields.
Referenced potion notes must resolve to active Potion-category journal entries.
The importer does not create extra items or notes to satisfy stale references.

## Scroll records and Chinese text

The base table has 245 item IDs, including 194 nonempty bodies and 51 inactive
empty records. The supplied ZHCN table provides 193 usable translated bodies.
Its ID 1574 is still an empty placeholder, whereas the newer base has the
Philosopher stone hint. That entry retains the newer English body and is reported
as untranslated. Nonempty translations require matching ID, location and item-name
comment; old translations cannot activate empty base records or create new IDs.

Source bodies include quoted and unquoted paragraphs. Records are reconstructed
from the initial ID/body and final location/name columns before processing outer
quotes. This also isolates five truncated item-name comments at IDs 1520, 1521,
1524, 1526 and 1527. Each has one opening quote without a closing quote; a strict
profile validates the exact original comment before removing that stray marker.
The repair occurs in both archives, ten times total. Other unmatched leading
quotes, duplicate IDs and interrupted paragraphs fail conversion.

The real conversion reconstructs 397 continuation lines and restores 16,861 legacy
Chinese glyphs. It preserves text spacing, paragraph breaks, internal quotations
and metadata; output is properly escaped UTF-8 TSV. The shared readable-scroll
loader reopens the serialized data and validates every field by item ID.

All 194 active entries are also passed through the production
`InventoryItemUseRuntime::useItemOnMember` using a diagnostic party. They return
the exact expected body and the item's display name without consuming the scroll.
Titles therefore use the previously imported Chinese item names; the source
English name comments are not substituted for those titles. This is a headless
item-use check, not acceptance of the rendered scroll overlay or player inventory.

## Registered potion identities and matrix orientation

The authoritative loose settings contain 71 IDs 0–70, mapped to actual items
220–290: bottle, catalyst and 69 matrix operands. Every setting is checked through
`MergedPotionSettingTable`, including mastery, flags and original notes. The table
preserves note padding; the production loader trims it for lookup/display.

Both source matrices still contain a 70th operand labelled Strange potion, ID 806.
The current base item registry assigns 806 to Graceful Sword, a Weapon, and the
authoritative potion settings do not register it. This explicit profile excludes
its operand row and column, recording the exclusion. It does not renumber any
remaining potion or invent an item for the obsolete label.

The resulting matrices each preserve all 69 × 69 = 4,761 active ordered cells.
Mixing uses row = held potion and column = inventory target. The note loader's API
uses row = target and column = held, as called by the existing mixing runtime.
The source has four asymmetric active mixing cells and two asymmetric note cells;
neither matrix is symmetrized. There are 202 nonzero note associations. Every
combination and note cell is verified through the production loaders.

Rows 282 and 283 have a split effect/recipe layout; each is reconstructed in both
matrices, four continuations total. Empty padding, reagent descriptions, color
labels and footer notes are not recipe rows. Reagent conversion rules and extra
potion script effects remain outside this phase.

## Invalid recipe output and shared runtime protection

One active source recipe still produces the obsolete ID: held potion 262 (Stone
to Flesh), target 223 (Magic Potion), result 806. The reverse cell is E4. The
original numeric result is preserved and reported as an unsupported recipe;
there is insufficient evidence to select a replacement result or damage level.

Previously `InventoryItemMixingRuntime` would accept this non-potion result and
replace the target with a sword. The shared runtime now requires a non-explosive
recipe result to exist in both the item registry and potion settings before any
mutation or consumption. Invalid results return an unhandled mix, leaving both
inputs unchanged. The fix applies to the shared game, including both indoor and
outdoor inventory use; it does not add a separate Apple mixing implementation.

Two native runtime checks use the converted tables: a valid 222/223 mix yields the
authored result, average power and correctly oriented note ID; the invalid
262/223 mix leaves both potions, their quantities and powers intact and produces
no bottle, note or consumption request. Original Windows behavior for the invalid
recipe is not established. Full alchemy effects, failure damage and UI remain
unaccepted; `alchemy_gameplay_validated=false` and `runtime_ready=false`.

## Verification and reproduction

```sh
cmake --build build-apple --target inventory_import openyamm
inventory_import=/path/to/build-apple/inventory_import
"$inventory_import" /path/to/MMMerge /path/to/combined-assets /path/to/new-inventory-output
python -m pytest tests/test_inventory_resources.py -q
cmake --build build-ios --target openyamm_inventory_import
```

Twenty-four tests cover Chinese/English text, paragraph encodings, source identity,
all ordered matrix cells, original fields and files, truncated comments, valid and
invalid runtime mixing, absent translations, invalid values, stale references and
canonical path boundaries. Fresh outputs are required; input overlap, existing
outputs and escaping symlinks are refused. Failed writes roll back; files use 0644.
All 203 resource tests pass. The Mac game/converter link, and the modified shared
game library plus importer compile for iPadOS arm64. No iPad app was updated.

Actual output: `../local-data/inventory-tables-001/`; combined assets:
`../local-data/gameplay-base-010/`. Evidence under `../local-data/validation/`:

- `inventory-source-comparison.json`: independent original-text comparison;
  all four combined files match their converted output byte for byte.
- `resource-tests-inventory.log`, `inventory-mac-build.log`, `inventory-ios-build.log`.
- `gameplay-start-010/startup.log`: formal startup passes scrolls, potion mixing
  and potion notes, then fails at required `portrait_frame_data.txt`.

Optional `house_animations.txt` remains missing. Formal startup has not reached
the later merged-settings phase; the settings table is validated directly through
its production loader here. Actual character creation, gameplay, art, player saves
and input acceptance remain incomplete. This phase leaves the overall
low-confidence 35%–40% estimate unchanged.
