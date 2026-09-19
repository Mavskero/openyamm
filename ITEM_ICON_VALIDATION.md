# Shared item images and actual chest UI — checkpoint 077

The inventory import stage now packages the images referenced by the existing merged item table,
including the three gold heap images used by the shared chest renderer. Missing item artwork caused
the chest in checkpoint 076 to appear empty despite containing loot. The actual Mac Metal chest now
shows its original candlestick, berries and gold, and pointer transfers and Chinese inspection work.

## Import contract

`ItemIconResources.cpp` runs within the existing inventory converter's staging/rollback boundary.
It uses the already validated merged item registry, selects image members from the source icon and
image LOD archives, decodes original pixels and writes lowercase, mode-0644 assets to `engine/icons`.
It records each name, item IDs, selected archive, output path and dimensions in
`item_icon_conversion.json`. Missing images and blank item icon IDs are explicitly reported.
Item mechanics, text, IDs and source archives remain unchanged.

The supplied source has one owner for every available referenced image. The importer rejects any
competing selected owner, including unknown or other-world archives, instead of assuming arbitrary
mod precedence. Source escapes, unsafe member names and damaged images fail the entire inventory
stage and remove its partial output. Additional UI packs are not searched as replacement item art.
This strict unique-owner profile may reject another installation with overlapping item-image mods;
such a profile needs explicit precedence before it can be supported.

## Evidence

All files are under `../local-data/validation/item-icons-077/` on NVMeSSD.

| Check | Result |
| --- | --- |
| Item-table references | 911 image names; 910 source images imported; literal `null` remains missing; item 0 has no icon |
| Source ownership | 314 original MM8, 258 MM6, 301 MM7, 17 prefixed MM8, 20 Merge images |
| Independent pixel comparison | All 910 images, 5,002,938 RGB pixels match source decoding |
| Package comparison | 901 new engine entries; nine images already existed identically; every previous engine entry unchanged |
| World packages | All three world ZIPs byte-identical to checkpoint 076 |
| Complete conversion | All 140 supported MM6/MM7 maps load; MM8 scene expansion remains pending |
| Focused tests | 30 passed: inventory behavior plus item image import, ownership, source boundaries and rollback |
| Native builds | Final import tools build 005 and capture build 004 passed |
| Generic unsigned iOS Release | Build 002 passed; build 001 failed after the Xcode build graph regenerated to add the new source |

`inventory-002` was produced with the final functional importer. Its image report and every image
match the complete runtime's outputs. The only later importer edit wraps a long source line.
The initial item test fixture changed localized mechanics without updating its original table;
`item-icons-red.log` is a fixture failure, not valid before-fix evidence. Corrected tests pass in
`tests-002.log/xml`. The actual before-fix evidence is the old-runtime application run below.

## Actual application validation

- `chest-red-001` uses runtime 076 and fails the new rendered-item check: active chest contents have
  no rendered images. `chest-green-002` with the new runtime shows all three entries and passes loot.
- `chest-inspect-004` checks the normal temple save without changing stats, items or position. It
  clicks the candlestick using a mouse input frame, then clicks the first character portrait to store
  it; mouse input collects 188 gold; a touch-marked input frame picks up berries and stores them via
  the portrait. Exactly one of each original item is gained, gold changes 200→388, and reopen is empty.
- Inspection uses right-button SDL events through the normal game input system, targets an opaque
  candlestick pixel and checks the active item ID. The actual screenshot shows the Chinese name and
  description. `Type` and `Value` labels remain English; full UI localization is still incomplete.
- `chest-reload-005` cold/warm restore comparisons match 25 inspected top-level fields; only save
  name differs. Reopening preserves chest state, characters, gold, quest bits, turn state and clock.
  Trap damage remains 12 per character; these checks do not change the original trap or reward.

The first inspection diagnostic, 003, unnecessarily required keyboard focus. The final diagnostic
addresses the single SDL capture window by its window ID and does not require bringing it forward.
The production renderer and input handlers were not modified. Actual screenshot BMPs are retained;
PNG copies are only lossless format conversions for review.

Mouse/touch-marked input frames establish shared gameplay behavior; they do not establish physical
iPad input, long-press recognition, drag/drop gestures or every inventory/shop layout. The window
event check establishes Mac SDL inspection and rendering, not a physical mouse event on an iPad.
All 910 images are decoded and source-verified; only the inspected chest is visually accepted here.
No iPad or simulator UI operations occurred. Save format remains 85.

Current complete runtime: `../local-data/validation/item-icons-077/runtime-001`. Preserve 076 and
the original sources. Verification scripts, source hashes, diffs, reports, build/test logs and save
comparisons are retained beside it. Further work includes the remaining UI labels, inventory/shop
visual coverage, MM8 scenes, candlestick quest completion and physical iPad acceptance. Overall
project estimate remains **40–45%, low confidence**.
