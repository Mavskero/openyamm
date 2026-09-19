# Original item transparency — checkpoint 085

The MM8 weapon shop's hammer no longer displays its purple rectangular background in the actual
Mac Metal application. Native import now preserves the original 8-bit palette and pixel indices
for typed inventory images and IFT icon animation frames. Other LOD image callers retain their
existing RGB output; wrapped PCX remains unchanged. The shared engine already supports palette-zero
transparency, so no RGB-key thresholds, original artwork or gameplay rules were changed.

This is limited visual/import acceptance, not completion of the starting-area gameplay milestone.
Overall remains **40–45%, low confidence**. A complete normal MM8 starter loop, followed by iPad
revalidation, is the next proposed milestone that could support 55–60%. It is not yet met.

Evidence is in `../local-data/validation/item-transparency-085/`. Current complete runtime is
`runtime-003`; the preceding complete runtime is `../equipment-083/runtime-001`.
The normal continuation save remains `../mm8-shop-084/route-007/shop-route-complete.oysav`.

## Source and package checks

- All 910 item icons retain their original base-level indices, complete 256-entry palettes and
  5,002,938 RGB pixels. Their 2,626,577 zero indices are no longer discarded during conversion.
- IFT frames can share inventory images. Both import paths now use indexed output, so duplicate
  resource checking still requires byte equality. Portrait frames keep their previous RGB format.
- Exactly 1,151 packaged images change encoding: 910 item images plus 241 additional IFT frames.
  All changed images retain identical RGB pixels; the extra frames also match original source
  indices and palettes. The other 42,294 payloads remain byte-identical.
- All four ZIP CRC checks pass, with the same 43,445 entries. All three world ZIP payload sets,
  gameplay tables, layout files, source IDs and save format 85 remain unchanged.
- Full native conversion completes all 32 stages, packaging and validation, including 201 map loads.
  Missing source resources, unsupported event maps and `runtime_ready=false` remain explicit.

`verify-resources.py`, `verify-resources-002.log`, `resource-comparison.json` and
`source-pixel-audit.json` contain the independent LOD/Pillow/package comparison.

## Actual shared runtime check

`capture-001` cold-loads the unchanged four-member shop save, also checks a same-session reload,
then uses normal Trigger and the offered shop action to reopen the shelf. No party coordinates,
stats, items or quest flags are injected. The application exits successfully after 181 Metal frames.

The actual cached hammer texture has 5,392 transparent original purple pixels and 2,309 opaque pixels.
Shared pointer hit testing agrees with the rendered texture's alpha for all 7,701 pixel centers.
The final `capture-001/shop-stock.png` was viewed: the background rectangle is gone, while the hammer,
other offered weapons, Chinese proprietor text and party portraits remain visible.

Both cold and warm restores match all 33 inspected fields except the save name. Normal shop entry
and exit preserve 21 checked gameplay fields, including every character/item property, stock and
refresh data, quest state and gold 169. The purchase-removed slot remains empty.
See `verify-capture.py`, `verify-capture-001.log` and `capture-comparison.json`.

## Build and regression evidence

- Four new synthetic import cases fail against the old importer, then pass after the fix. They cover
  both LOD directory formats, compressed/uncompressed data, odd row padding, mip chains, full palettes
  and duplicate RGB colors at zero/nonzero indices.
- Final related import suite: 68 tests pass; five existing Pillow deprecation warnings remain.
- Shared indexed-image decoder: three cases / 25 assertions pass, including retaining a same-colored
  nonzero pixel while making index zero transparent, and keeping an opaque background opaque.
- The initial complete four-case decoder invocation fails because an existing PNG mockup fixture
  is absent. `decoder-indexed-001.log` explicitly selects the three relevant cases; the PNG case and
  full disabled unit-test target are not claimed as passing.
- Final native build004 and generic unsigned iOS Release build002 pass. Capture build003 contains the
  final alpha/hit check; build004 subsequently changes only the frame importer and its linked targets.
- Retain runtime001's real inventory/frame format conflict and runtime002's incorrect layout-directory
  invocation. Both outputs roll back; runtime003 is the accepted build. The first resource audit covered
  only engine.zip; audit002 additionally verifies the three nested world ZIPs.

No physical iPad or simulator was queried or operated. Shop identification, repairs, special buying,
restocking, limits, English labels, full equipment/race visuals and long play remain unaccepted.
Next: normal MM8 portals and d05, combat, loot, return/quest delivery and restart continuation.
