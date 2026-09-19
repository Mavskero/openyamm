# MMMerge portrait, reagent and interface support tables

`support_import SOURCE BASE_ASSETS NEW_DESTINATION` reads three original loose
tables: MonPortraits, Reagent settings and Additional UI. It preserves every
source cell and writes UTF-8 shared engine tables to a fresh destination outside
both inputs, mode 0644. It requires the converted monster, item and potion
registries. Invalid source schemas or references leave no partial output.

## Monster portrait associations

The source has 217 groups. Printed IDs must match one-based row order, as used
by `RemoveMonPortraitsLimits.lua`; each group covers three consecutive monster
IDs. All three monster IDs must exist. There are 837 nonzero ordered choices,
including repeated values, and 77 groups containing the original zero sentinel.
The shared loader represents these zero groups as no portrait, matching the
source's null name pointer. Zero is not a missing texture to replace.

All choices and zero groups pass 2,742 production selection checks across the
three monster tiers. There are 362 distinct nonzero portrait IDs. No portrait
images are exported here, and their availability/rendering has not been accepted.
Notes and the original table remain unchanged; icon ownership is unchanged.

## Reagent mixtures

All 43 source material IDs map to existing registered potion items. Both Reagent
and the merged Herb item kinds are supported; English source notes remain
metadata while item display uses the existing Chinese item registry. Source IDs,
item IDs, result IDs and notes are checked with the production typed loader.

Eighty-six production mixing checks cover each material held over a bottle and
the reverse orientation. They verify the resulting item, identification, quantity,
Alchemy skill plus reagent power, and the held-item-consumed signal. The mixing
service leaves held cursor removal to its caller; this check verifies that
contract, without claiming an inventory UI interaction or cursor removal occurred.
Original-game power parity and full alchemy UI acceptance remain pending.

## Additional interface metadata

Three original UI sets retain archive filenames, indicator/selection-ring flags,
signed offsets and notes. IDs must match the source script's row order. The
three declared non-default archive files exist under Data/Additional UI and path
containment is checked. Existence is not archive or artwork validation: no skin
images are decoded/exported here. A missing declared archive is reported and
leaves readiness false rather than creating replacement art.

## Verification

`support-tables-001` independently matches all source cells and is merged into
`gameplay-base-019`. Fifteen new tests and the complete 357-test resource suite
pass. Mac game/converter and iPadOS arm64 library/app builds pass. Formal startup
passes the three tables and next requires `bolster_formulas.txt`; optional house
animations remain missing. No iPad was connected or updated. Full gameplay,
portrait rendering, alternate UI skins and alchemy interaction remain unaccepted.
