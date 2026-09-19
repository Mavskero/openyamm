# Temple localization validation

Checkpoint119 adds a dedicated, typed `house_service_text.txt` instead of mixing service text into combat text.
The core importer selects the supplied Chinese archive when present and otherwise retains English. Stable house
types and action IDs remain mechanical; translated names and labels never decide service behavior.

The table requires 18 semantic rows exactly once and validates `{member}`, `{skill}`, and `{price}` signatures.
`GameDataLoader` rejects a missing or invalid table with an explicit rebuild/reimport diagnostic. All normal
gameplay dialogue contexts receive the localized table and localized skill display names; direct test/tool callers
retain the previous English fallback when they do not supply one.

Evidence is in `local-data/validation/temple-localization-119/result.json`. The new complete runtime is
`local-data/validation/temple-localization-119/runtime-001`. Native tests cover labels, names, amounts, success,
already-well, unsupported condition, insufficient gold, no wrong deduction, donation reputation/counters/buffs,
and stable action IDs. Synthetic core imports cover Chinese and English sources and strict rejection behavior.

Checkpoint119 does not claim visual fit. Actual Chinese screenshots, long-text layout, selection, clicking, and
return navigation are intentionally assigned to checkpoint120. It also does not add or alter temple gameplay rules.
