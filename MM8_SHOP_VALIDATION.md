# MM8 normal weapon shop transactions: checkpoint 084

2026-09-11. Evidence: `../local-data/validation/mm8-shop-084/`.
Runtime remains `equipment-083/runtime-001`; all four ZIPs and conversion receipt retain their hashes.
Save format remains 85. This checkpoint accepts a normal weapon purchase/sale and stock persistence,
plus two shared production fixes. It does not accept complete shop visuals, services or the MM8 starter loop.
Overall remains 40–45%, low confidence. No iPad or simulator was queried or operated.

## Production fixes

`HouseServiceRuntime::trySellInventoryItem` previously used the pointer into the backpack after removing
that item. Compaction could replace its identification/enchantment state with another item's state,
or leave an invalid last-element reference. The result now uses the removed item returned by the same
inventory operation. No item value, sale formula or source data changes.
The focused test sells identified/unidentified items with oppositely identified following items:
two assertions fail before the fix; one case / 15 assertions pass afterward.

An actual shelf purchase exposed a second issue: one Escape both closed the shelf and exited the house.
The shared hotkey path set `closeOverlayLatch`, then inactive-loot cleanup cleared it before dialogue
input saw the same key. Loot cleanup no longer owns that shared latch; the shared hotkey path releases
it when Escape is released. Actual checks require shelf-to-dialogue return, no further dismissal while
the key remains held, release reset, and separate key presses through the sell/equipment/root layers.

The save inspector now emits the house stock seed, stable house ordering, refresh time/sequence/version
and all item fields for standard, special and spellbook stock. This changes diagnostic output only.

## Normal route and independent checks

Final `route-007` starts from the unchanged four-member
`mm8-inn-081/route-green-002/inn-route-complete.oysav`, walks 14,947 units using normal Forward input,
collision and actor simulation, and enters original out01 event 171 / weapon shop 1 with Trigger.
The recessed doorway is approached from the west; map coordinates guide the test's camera direction,
and no gameplay positions, attributes, money, items or quest flags are injected.

Only actions actually offered by the dialogue are selected. A normal press/release clicks the selected
weapon's actual rendered opaque pixels; the sell click uses its occupied backpack cell.

- Stock has six standard and six special slots. The shared serialized schema also keeps twelve empty
  spellbook placeholders, which are not offered as a weapon-shop service.
- The party's original 200 gold buys item 825, 大斧, for 41 gold. Exactly one stock slot clears and the
  purchased item's complete inspected state matches stock except its assigned backpack coordinates.
- A second click on that empty stock location changes neither gold nor item count.
- The normal equipment/sell menu sells that same axe for 10 gold: 200 → 159 → 169.
  Every inspected original character/item field and original backpack grid position is restored.
- Other party/quest/NPC/food/death fields remain equal. Five actor projectiles generated during the
  approach remain identical during the transactions; world time/projectiles advance again outside.
- `reload-bought-001` and `reload-final-001` each cold-load their actual phase save, reload it in the same
  session, reopen the shop with Trigger and exit normally. All 33 inspected fields except the save name
  match the respective input at both immediate restore points, including stock, actors and projectiles.
  Re-entry preserves the empty sold slot, every remaining standard/special/book record and refresh data.
- `verify-saves.py`, `save-comparison.json` and `verify-saves-003.log` preserve independent comparisons.
  Source audit verifies all cells in the 426-row original house-rules table. Export adds TSV quoting,
  so byte equality with the unquoted source is not claimed. The source weapon shop has multiplier 1.5,
  seven-day restock and hours 06:00–18:00. The axe's source value is 30; the unchanged recruited party
  has novice Merchant 1. These inputs support this observed 41/10 transaction, not full original economy parity.

Final native `build-008.log`, focused `sale-green-001.log` and generic unsigned iOS
`ios-release-002.log` pass. The full unit target is disabled and was not run.
The full route renders 928 actual Mac Metal frames; each reload renders 181 frames and exits successfully.
Final shelf, before-sale and after-sale PNGs were viewed. No long-session or physical accessory claim.

## Retained failures and open work

- Build 001 called an internal-only grid helper; capture coordinates now use the actual grid dimensions.
- Routes 001–003 hit buildings using invalid approach candidates. Original building/door geometry led
  to the accepted western approach; no movement rule was weakened.
- Route 004 completed the purchase but exposed the real double-Escape bug. Route 005 reproduced it
  directly from the actual pre-purchase door save, with before/after input-state evidence.
- Route 006 completed buying/selling and saved, but repeated close steps queued duplicate screenshot
  labels. Its owned process was terminated after verifying that condition; it is not counted as a pass.
  The final capture state machine advances to a separate close phase, and route 007 passes all captures.
- Save verifier 001 incorrectly assumed an empty book vector rather than twelve empty placeholders.
  Verifier 002 incorrectly compared world projectiles before and after the walk. Both failed logs remain.
  Final verification retains exact cold/warm comparisons and compares projectiles across shop operations;
  moving world state is explicitly outside initial-versus-final route equality.

**Visible source-transparency defect remains:** `hammer1` from `Data/mm6.icons.lod` has palette entry zero
RGB (252, 0, 216), used by 5,392 pixels. `LodResources.cpp::decode` expands indexed art to 24-bit BMP,
discarding index-zero identity. The current generic magenta color key does not identify this source color,
so the actual shelf displays its purple background. See `remaining-transparency.json`.
Next fix should preserve source palette/index-zero information for item icons and audit all imported
pixels/alpha behavior; do not widen arbitrary RGB thresholds or change original artwork.

English service/transaction text, hover/inspection wording, special purchases, identification, repairs,
insufficient gold/full inventory, restocking and other shops remain unaccepted. Existing 083 equipment
seams/clipping and missing source artwork remain. Then continue normal original MM8 portals/d05,
combat/loot, return/quest delivery and restart continuation. The 079 navigation and 074 candle work remain open.
Full starter-loop desktop acceptance plus iPad revalidation could support 55–60%; it has not been reached.
