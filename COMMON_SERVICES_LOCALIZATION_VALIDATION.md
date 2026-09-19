# Common service localization validation

Checkpoint 126 is accepted. It extends the checkpoint119 typed `house_service_text.txt` path from 18 Temple rows to
72 rows without using localized display strings for service dispatch. Tavern lodging/food, level training, shop
buy/sell/identify/repair and transport destination/cost/days now use typed Chinese or English templates for their
scoped labels and result states.

The source inventory is
`../local-data/validation/common-services-126/source-inventory.md`. Forty-one required original GlobalTxt IDs are
read from the base English archive or Chinese overlay. Where the original table has no complete native-runtime
sentence, the importer supplies a clearly bounded Chinese template and an English fallback. The loader rejects
missing keys, duplicate keys, undeclared/missing placeholders and unsafe controls.

## Rule preservation

Production behavior still selects actions by `HouseActionId` and original house/service data. Localization receives
the already calculated member, item, destination, open/close time, price, days, level, experience, points and food
values. Tests exercise refusal before mutation and success after funding for all four groups, including shop
equipment identification/repair state, training level/skill points and travel time/pending destination. Existing
Temple heal/donate/learn checks were rerun against the new complete runtime.

The focused native results are 2 cases / 9 assertions for catalog schema, 5 expanded cases / 250 assertions for the
localized runtime, 3 / 27 for dialogue layout, 1 / 15 for shop sale and 4 / 18 for travel prompts. The importer suite
passes 19 tests across English and Chinese. The house import gameplay save/reload check also passes. Details and
commands are in `../local-data/validation/common-services-126/commands.md`.

## Actual Mac evidence

Four normal Mac Metal house entries produced ten final 2420×1668 BMP captures. Ordinary pointer input bought a shop
item, bought tavern food, trained one level and exercised an insufficient-gold transport route; corresponding gold,
inventory, food, level/skill-point or travel state was checked. All scoped labels/results and close paths pass. The
visual review is recorded in `../local-data/validation/common-services-126/visual-inspection.md`.

The failed first transport fixture is retained because it assumed a house dialog persisted in a save. The accepted
second run enters New Sorpigal stable house 470 through the normal Trigger path. No same bug accumulated three failed
repair attempts; see `../local-data/validation/common-services-126/repair-attempts.md`.

## Complete runtime and boundary

The complete runtime is `../local-data/validation/common-services-126/runtime-001`. Its four ZIPs pass CRC checks,
all three world archives are identical to checkpoint119, the engine member set stays at 16,976, and only
`data_tables/house_service_text.txt` differs. It retains Temple text, events and images. Exact hashes and derivation
are in `runtime-001/derivation.json`; the aggregate result is `result.json`.

This checkpoint does not accept NPC recruitment, promotions, Arcomage, general Learn Skills text, complete quest
prose, iPad/simulator behavior or all house layouts. Overall port progress remains **55–60%, low confidence**: this
chapter closes a shared localization gap but does not expand natural campaign or device coverage.
