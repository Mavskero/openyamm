# Arcomage import profile

`arcomage_import SOURCE BASE_ASSETS NEW_DESTINATION` reads the owned MM8 executable
as inert data and the previously converted house/map registries. It exports all
102 card definitions, the original Chinese card atlas when present, and the
original table background. No Windows executable, DLL or original script runs.
Output must be fresh and outside both inputs; failures remove the staging output.
Files retain lowercase names and mode 0644.

## Binary data contract

The supplied `mm8.exe` is 1,155,072 bytes, SHA-256
`97be86a7597fbd9763f9b4b0ed1f4a7e47c960703fa30857c32daa056b7d403b`.
The 102-record card table starts at raw offset `0xf0228` (VA `0x4f0228`) with
108-byte records; its SHA-256 is
`3310fae18cbba8c381dc47792279bbbaceb322e9811fcea2039be40d72d6156e`.
Unknown executable fingerprints fail until a separately verified profile exists.

| Offset | Original data |
| --- | --- |
| 0–31 | NUL-padded ASCII mechanical name |
| 32–35 | Little-endian atlas slot |
| 36 | Resource type: bricks, gems, recruits |
| 37–42 | Quarry, magic, dungeon, bricks, gems and recruits requirements |
| 43 | Discardable flag |
| 44–47 | Original condition number |
| 48–76 | Primary effect: 29 signed bytes |
| 77–105 | Secondary effect: 29 signed bytes |
| 106–107 | Zero padding |

The export adds ordinal IDs 0–101, keeps the existing 70-column card schema and
preserves every original field, including condition 25 and effect sentinels 98/99.
Names and atlas slots are unique; each resource type has 34 cards. Atlas slots are
not contiguous card IDs: they occupy 0–33, 40–73 and 80–113. The renderer already
uses `x = 96 * (slot % 10)`, `y = 220 + 128 * (slot / 10)`.

## Explicit runtime interpretation

The first row is `#ArcomageCardMode<TAB>MM8BinaryV1`. The shared loader validates
this version and all 70 fields before accepting its cards. Unmarked legacy tables
retain their existing normalized-effect convention.

- Original condition 25 compares player tower > enemy wall. The original selector
  at `0x40cc6d` reads those values and branches with a strict greater comparison.
  It maps explicitly to OpenYAMM's legacy enum value 21. Unknown binary conditions
  21–24 and 26 are rejected by this profile; they are not silently made unconditional.
- Player-wall effect 98 becomes a typed `swapWalls` operation, with scalar wall
  delta zero. The serialized source value remains 98. The original wall exchange
  is visible at `0x40d55b`–`0x40d572`. Shift no longer adds 98 walls or swaps twice.
- Marked cards execute their complete primary/secondary bundle without also
  invoking the legacy name-based Flood Water / Shift handlers.
- Flood Water condition 15 selects the player only when their wall is lower;
  otherwise it selects the enemy. Equal walls therefore affect the enemy in this
  unpatched executable profile. This differs from the legacy handler's both-sides
  tie behavior. GrayFace/MMMerge DLL patch parity has not been established.
- Value 99 retains the engine's existing copy-other-value operation. Copping the
  Tech and both Parity branches are checked with unequal and equal production.

The original executable was inspected for data layout and branch behavior only.
No OpenEnroth or mm_mapview2 implementation code was copied. This is not a claim
that every Windows card edge case, patched behavior or AI decision is equivalent.

## Images and house rules

Archive priority is `mm7.icons.lod`, `mm8.D.lod`, `mmmerge.D.lod`, then optional
`10 LocZHCN.EnglishD.lod`. Only `sprites.pcx` and `layout.pcx` are selected.
The supplied source resolves to:

- `engine/icons/sprites.pcx`: Chinese overlay, 960×2500, 3,953,074 bytes.
- `engine/icons/layout.pcx`: MM7 icon archive, 640×480, 524,818 bytes.

Both are shared Arcomage presentation assets, not world NPC portraits. Wrapped
PCX payloads are preserved byte for byte. The engine image decoder must accept
them; its BGRA checksum is recorded and independently compared against Pillow.
The Chinese-art flag reflects the actual selected atlas owner, not just the
presence of a localization archive. No new layout YAML is needed: ArcomageScreen
resolves the existing logical texture names Sprites and Layout case insensitively.

The existing HouseTable applies the imported House rules and resolves 53 tavern
rules. The converter uses these actual rules to initialize matches. It does not
invent separate tavern settings or duplicate the original Lua memory hooks.

## Verification checkpoint, 2026-09-09

- Output `../local-data/arcomage-tables-002`, merged into `gameplay-base-024`.
- 7,140 exported card fields and 5,916 raw effect bytes independently compared.
- 306 real-deck card plays across lower/equal/higher states; selected card costs,
  conditional damage, wall swaps and production copies independently asserted.
- 53 automatic tavern matches finished, 4,672 actions total, maximum 254 per match.
  This proves legal actions and termination for these seeds, not original AI parity.
- Standalone shared-rule checks cover raw and legacy effects plus malformed rows.
  21 new resource checks and all 448 resource checks pass; Mac and iPadOS builds pass.
- Full-map diagnostic for `oute3.odm` now exits 0, with missing sprite family
  `m529` and optional house animation warnings. Its entry point now sets the
  requested initial map, avoiding an unrelated default-map load before profiling.
- Actual GameApplication startup still exits 1: `sounds.txt` and
  `character_speech_events.txt` are absent. Missing-audio-table failures now name
  the files instead of silently returning false. No required loader was bypassed.

Evidence lives in `../local-data/validation/arcomage-source-comparison.json`,
`resource-tests-arcomage.log`, `gameplay-start-024/`, `gameplay-app-start-024/`,
`arcomage-mac-final-build.log`, `arcomage-ios-final-build.log` and the
`arcomage-audio-diagnostic-{mac,ios}-build.log` files.

Rendered card interaction, full Chinese status text, iPad touch/keyboard/mouse
handling, audio, tavern reward/QBit integration, patched Windows behavior and AI
strategy remain unvalidated. This checkpoint neither enables the iPad Start Game
button nor claims a complete playable MM678 flow. Overall progress remains the
low-confidence 35%–40% estimate until application/playability milestones pass.
