# Dimension Door cycle validation

Checkpoint121 binds the original GenDimDoor tables and Lua-double formulas to an independent 240-row fixture.
The production resolver matched day0–239 and the exact repeated day240–479 cycle for all three stable target IDs,
map IDs through real MapStats, icon suffixes, zero coordinates and direction values.

All 21 map/icon combinations were also opened through the real Dimension Door layout and selected with ordinary
pointer input. Every selection preserved the 20-SP learned-spell context, queued the expected target once, used
the literal zero transform for same-map out01 targets, and substituted Map Start only for cross-map targets.
The three TPGlobal targets remained separate despite overlapping source artwork, and an ordinary Town Portal
load before and after TPGlobal was not polluted.

Activation immediately before midnight kept the frozen day after the pointer-close path; explicit reactivation
after midnight updated it. The checkpoint118 item-scroll and natural-location timer implementation was unchanged.

Evidence is in local-data/validation/dimension-door-cycle-121/result.json. The 480 resolver inputs are not 480
played days, and the 17 unique maps were not all loaded. The 21-combination/17-map list is available in
unique-destinations.tsv for checkpoint122 route selection.
