# Original fitted equipment: checkpoint 083

2026-09-11. Current runtime: `../local-data/validation/equipment-083/runtime-001`.
Evidence and retained failures: `../local-data/validation/equipment-083/`.
This checkpoint accepts original fitted image import and a limited normal Mac equipment round trip.
Overall remains a low-confidence 40–45%; complete equipment, campaigns and iPad acceptance remain open.

## Production changes

The native equipment stage selects 1,107 typed image references from the unchanged 157 equipment rows:
54 armors, 40 helms, 18 belts, 21 boots and 24 cloaks. It imports 1,062 available original images.
Import and runtime share `EquipmentPictureVariants.h`: armor uses the open/holding poses, complex flags
select original helm/belt variants, and each permitted doll type retains its own cloak variant.
Back cloak, collar and complex front collar are separate layers. Missing fitted art no longer silently
uses backpack icons. Source doll equipment permissions still apply; no source table or save format changed.

The only competing image owner rules for this stage are `icons.lod` followed by `mm8.icons.lod`, and
`mm6.icons.lod` followed by `mmmerge.icons.lod`. Enumeration order does not determine precedence.
Other collisions, corrupt images and escaping archives fail and roll back the fresh stage.
Outputs remain lowercase, mode 0644. Body/collar coordinates retain all 2,010 point checks.

The shield anchor now follows original open-left-hand coordinates plus the offhand offset, then shield
offset minus equipment grip. Cached paperdoll composites no longer draw body/equipment layers twice.
Both renderer and alpha-aware shared hit testing resolve the same collar/body parts and coordinates.
No collar-specific clicking or complete race/layer correctness is claimed.

Source behavior was inspected read-only in composed input `mm8-runtime-076/source-001`, including
`CharacterOutfits.lua`, `RemoveItemsLimits.lua` and static `mm8.exe` disassembly. Windows code was not executed.
No OpenEnroth code was copied. Source audits are `source-variants.json` and the two disassembly text files.

## Accepted evidence

- `verify-resources.py` independently rederives source selections and verifies all 19,051,614 RGB pixels,
  owners, item references, dimensions and file modes. `resource-verification.json` records the result.
- Full conversion completes 32 stages plus packaging/validation and 201 map loads (64 MM6, 76 MM7, 61 MM8).
  The four ZIPs have 43,445 entries and pass CRC. All 42,382 other old payloads are byte-identical;
  only old `engine/ui/gameplay/character.yml` intentionally changes for collar layers and helmet order.
- Import tests: 51 pass (26 doll, 25 equipment), with three existing Pillow deprecation warnings.
  The old equipment binary fails nine new cases; 16 existing cases pass. Pure logic: six cases,
  54 assertions pass. Full unit target is disabled in this Mac configuration and was not run.
- Native production/import build and final `build-capture-006.log` pass. Generic unsigned iOS Release
  `ios-release-002.log` succeeds. No physical iPad or simulator was queried or operated.
- Actual Mac `capture-004` exits successfully after 302 Metal frames. It starts from the unchanged normal
  four-member 081 save, opens all four inventory pages through shared portrait clicks, and requires actual
  fitted body/collar textures for each worn item. Character IDs are 1, 5, 9 and 14.
- Normal pointer input equips Knight armor 84 and sword 1, returns the sword to its original backpack
  position, then returns the armor. No item, statistic, position or quest state is injected.
  Five phase saves satisfy the independent expected-state checks. Final 25 compared fields match the
  incoming save after canonicalizing inventory array order only; every item property and grid cell remains.
  Gold 200, food 5 and deaths 0 remain unchanged. Time and world actor animation are excluded.
- All four initial/cached panel images match exactly. Removing the sword restores the armor's open-arm
  pixels; removing the armor restores the original unequipped panel. Panel bounds come from the runtime.
  `verify-saves-visuals.py` and `save-visual-verification.json` preserve these checks.
- All four final character PNGs were viewed. Open-arm and holding-sword captures were also viewed.
  This turn does not add a cold/warm reload or long-session claim.

## Retained failures and limits

`capture-001` selected an opaque armor pixel above the actual inventory grid, where the sprite overhangs.
The test now requires the clicked grid cell to contain the expected item. `capture-002` completed equipping
and sword return but selected an out-of-grid armor return origin; the test now clamps the normal return
click inside the grid. `capture-003` completed the round trip but lacked eight expected screenshot tokens.
Its capture list was corrected; final `capture-004` passes. Production gameplay rules were not weakened.

Forty-five source collar-b images are missing: `6cape1..7v1..5b`, `6nwcapev1..5b`, `7item105v1..5b`.
No replacement art is fabricated. Current four views do not validate every item or all doll types.
Member 2 (character 9) still shows a left-arm seam when holding his two-handed staff. The long sword tip
can clip at the panel top, and some shields meet the panel edge. These remain visual follow-up items.
Actual minotaur/troll/dwarf views, complex front collar wearing, every helmet/belt/boot combination,
jewelry and collar-specific clicks remain unaccepted. Original body missing references from 082 remain.

## Next acceptance milestone

Continue normal MM8 gameplay from `mm8-inn-081/route-green-002/inn-route-complete.oysav` using runtime 083.
Prioritize shop transactions, original stone portals and d05, then combat/loot, quest completion,
map return and restart/reload continuation. The 079 navigation and 074 candle work remain open.
Desktop completion plus iPad revalidation without new foundational blockers could support an estimated
55–60%. Representative three-world progression and cross-world persistence could support 70–80% later.
These are conditional scope estimates, not measured percentages or delivery-date promises.
