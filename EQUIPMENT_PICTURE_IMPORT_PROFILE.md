# Equipment picture import profile

## Current checkpoint 083, 2026-09-11

The converter now also exports 1,062 original fitted images from 1,107 typed references, with a shared
runtime/import variant resolver. Armor poses, complex flags, doll permissions and separate cloak collar
parts select the original art. Source tables and 2,010 coordinate checks remain unchanged. All 19,051,614
RGB pixels match independent original decoding; 45 source collar-b references remain missing.
Four Mac character pages and a normal armor/sword equip/unequip round trip pass limited acceptance.
See `EQUIPMENT_VISUAL_VALIDATION.md` for the owner rules, verification and remaining visual/gameplay gaps.
The original metadata-only checkpoint and its acceptance limits below are historical.

`equipment_picture_import SOURCE BASE_ASSETS NEW_DESTINATION` reads the original
`Complex item pictures.txt` and `Complex item pictures offsets.txt` tables from
`Data/Tables`. It joins them against the imported item and character/doll tables.
No original Lua or Windows code executes, and Python audit exports are not inputs.
Output must be fresh and outside both inputs. Failed publication is removed;
files use lowercase names and mode 0644.

## Source interpretation

`Scripts/Structs/RemoveItemsLimits.lua` defines five ordered equipment sections:
Armors, Helms, Belts, Boots and Cloaks. Printed IDs restart within each section;
the global lookup key is the item ID in column two. The supplied table covers all
157 registered items in those categories: 54 armors, 40 helms, 18 belts, 21 boots
and 24 cloaks. Source fields, section headers, literal quotes in notes and blank
rows are preserved in the exported file.

Each item has six X/Y pairs for doll types 0–5. Cloaks additionally have six
independent collar pairs. Empty or absent trailing coordinates mean zero in the
source script. The profile accepts the supplied 14/16-column ordinary records
and 26/28-column cloak records, while checking each present coordinate as signed
16-bit. It rejects changed section order, duplicate item keys, displaced local
IDs, unsupported flags, missing registry references and category mismatches.

Column four marks complex helms, belts or collars. The source has 36 marked
helms, 18 marked belts and one marked cloak. These flags are retained by the
shared typed loader; they do not by themselves implement special layer drawing.

One authored offset applies to original portrait 26 and item 908: X +4, Y -3.
This is a cloak, and the offset applies to both body and collar coordinates.
Original portrait 26 is engine character 27. The offset stays keyed by original
portrait ID; it must not alter another portrait wearing the same item.

## Shared runtime changes

`MergedComplexItemPictureTable` now retains complex flags and collar coordinates.
Its coordinate resolver combines the selected body/collar point with any exact
portrait/item offset. An absent item or out-of-range doll type returns no point.
Signed additions use checked 64-bit intermediates before returning 32-bit values.
`GameDataRepository` exposes the offset table, and `GameplayScreenRuntime` uses
the resolver for body equipment rectangles. Both rendering and hit testing
already call that shared rectangle function. Existing portrait helmet offsets
remain applied afterward.

The imported item table contains four lowercase body-equipment categories:
1306/1307 use `armor`, and 2030/2045 use `cloak`. Several existing engine
equipment/enchantment branches require canonical case. `ItemTable` now normalizes
the five known body-equipment type names when loading, without changing exported
source cells. The converter validates the production enchantment-category lookup
for every referenced equipment item. This is category validation, not proof of
full item use, equipment restrictions or enchantment gameplay.

## Acceptance limits

The phase exports coordinate data, not images. Collar points and complex flags
are now available to runtime consumers, but separate collar layers and complete
paperdoll layering are not yet rendered by this change. The source-image variant
selection, final screen scaling, alignment, visual hit testing and iPad input
still need actual artwork and gameplay acceptance. Missing MM8 base image
archives remain a separate resource gap. A successful conversion or build does
not establish a playable game.

## Offline checkpoint, 2026-09-09

`equipment-picture-tables-002` is the accepted output, including the shared
item-category fix. Independent comparison in
`validation/equipment-picture-source-comparison.json` verifies every original
cell, all 157 item keys, the four lowercase source types and the portrait offset.
Native validation performs 2,010 body/collar/portrait point checks. Sixteen focused
tests and the full 404-test resource suite pass. Mac game/native tools and iPadOS
arm64 app builds pass.

`gameplay-base-022` combines this output with the previous checkpoint. Formal
startup passes the equipment tables and next requires `continent_settings.txt`;
optional `house_animations.txt` is still absent. No iPad was connected or updated.
