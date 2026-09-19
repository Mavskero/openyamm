# Inspected MMMerge combat import profile

This profile targets the user's supplied 2023/2024 MM8-based merged installation.
It describes fixed metadata used by the native converter; it is not a Windows
execution or memory-hook layer. Original files remain unchanged. The converter
does not need or copy EXE/DLL files on the iPad.

## Evidence inspected locally

- `mm8.exe` SHA-256:
  `97be86a7597fbd9763f9b4b0ed1f4a7e47c960703fa30857c32daa056b7d403b`
- `MM8patch.dll` SHA-256:
  `f89f52281b042d3e421c3ff1d4e7c70589538a90876359c1f4f57e967c27b05d`
- The supplied `Scripts/Structs/00 structs.lua` declares `SpellObjId` at virtual
  address `0x4F4648` (signed 16-bit value, four-byte stride) and `SpellSounds` at
  `0x4FE12A` (signed 16-bit value, two-byte stride), each for spell IDs 1–132.
- Read-only PE section translation and comparison checked every one of the 132
  slots. For source ID `s`, object slot is
  `1000 + ((s - 1) / 11) * 1000 + ((s - 1) % 11) * 10` using integer division;
  sound ID is object slot + 9000. The local evidence report is
  `../local-data/validation/combat-profile-verification.json`.
- `Spells2.txt` supplies the effective mana/recovery/damage overrides. Its zero
  slot is retained as an index offset, and all 132 names must match base spells.
  Twenty-one `Unused!!!` slots are explicitly reported and omitted from active
  spell output. Neither localized placeholders nor blank text create new spells.

## Missile names and object identities

The original MM8 missile-name parser is at `0x45264D`. The supplied patch replaces
the comparison call at `0x452670` with the wrapper at `0x33051A4`, calling the
comparison helper at `0x3305130`. Inspection confirms `FireAr` leaves missile kind
2, `Rock` leaves kind 6, and `Dagger` leaves kind 0. Consequently `Rock` uses the
Earth missile, not the separate Rock Blast spell or Cyclops Projectile object.

| Tokens | Object ID / source ObjList name | Impact object |
| --- | --- | --- |
| Arrow | 545 / Arrow | None |
| ArrowF, FireAr | 550 / FireArrow | None |
| Air, Earth, Fire, Water | 500 / Air, 505 / Earth, 510 / Fire, 515 / Water | None |
| Body, Mind, Spirit, Light, Dark | 520, 525, 530, 535, 540, matching source names | None |
| Rock | 505 / Earth | None |
| Ener | 555 / Laser | 556 / explosion |
| Dagger | No projectile, as in this legacy parser | None |

The converter verifies names, IDs, required projectile flags, positive speed and
impact references against the converted/source object registry. Unrecognized
future missile tokens fail conversion. Dagger descriptors for monsters 637–639
remain unchanged; the production monster loader confirms they are not ranged.
Spell impacts use the adjacent slot only where an actual `explosion` object is
present. Utility/buff spell slots often have no world object and are reported,
without inventing one. Rendering, spell-specific impact behavior and audio still
require gameplay verification; this data profile alone does not establish them.

## Localization and limits

The appended spell column 24, `Base Name`, holds its source semantic name.
`SpellTable` derives its normalized name and compact lookup key from this field,
keeping the display name Chinese. Tables without the extension keep their English
behavior. The duplicate source name `Charm` at IDs 60 and 113 retains both IDs and
the same name-lookup outcome as the English table; conversion does not silently
choose a different spell. Permission/damage flags in spell column 10 are mechanics,
not translatable text.

The current source contains four names already listed as unsupported by the shared
`MonsterSpellSupport`: day-o-gods, finger of death, mass curse and paralyze. They
remain in monster data and are enumerated in the manifest. Finger of death and
mass curse have no corresponding current spell-table row. Unknown missing spells
outside the engine's explicit unsupported list fail validation.

Item names, unidentified names and descriptions use the supplied loose ZHCN
overlay. Mechanical columns and treasure weights remain unchanged. Four hundred
fourteen random-item icon labels differ from the base item table; the runtime
joins by item ID and retains the authoritative base icon. Item icon conversion and
inventory footprints are incomplete, especially with the missing MM8 base icons.
The output therefore remains `runtime_ready=false` and
`inventory_footprints_ready=false`. Other installations that change these fixed
MM8 metadata contracts need a separately verified profile.

## Enchantment profile (2026-09-07)

`EnchantmentResources.cpp` reads the base and optional Chinese `stditems.txt` and
`spcitems.txt` through the native LOD/text decoder. The profile has exactly 25
standard and 73 special positional slots. Headers and trailing treasure-level
instructions are not entries. Since these source tables have no explicit IDs,
conversion keeps their order and verifies the first 24/72 source effect keys
against the production enum ordinals. Incomplete, reordered or unknown profiles,
invalid weights/prices/rarities and populated unexpected columns fail conversion
before an output directory is created. This profile is specific to the supplied
language/base pair; equal-length arbitrary translations cannot prove semantic
row correspondence by themselves.

Runtime column indices are zero based:

| Table | Original columns | Added key | Localization |
| --- | --- | --- | --- |
| standard | 0–10; empty padding through 15 | 16: base stat name | Only 0 (visible stat) and 1 (suffix) |
| special | 0–16; empty padding through 19 | 20: base suffix | 0 (visible bonus text), 1 (suffix), 16 (description from locale column 0) |

The language package's special column 16 contains English design notes, not its
actual translated bonus description. Source weights, price expressions such as
`X 2`, and rarity stay unchanged even if the older language pack differs. Native
typed validation compares all localized effects, weights, prices, rarity and
prefix/suffix classifications with the original English loader results. Display
names cannot redirect an effect to a different enchant. Existing tables lacking
the additional keys retain their original interpretation. This does not yet adapt
every inventory/tooltip screen to Unicode or change its word-order formatting.

Standard slot 25 (`Great might`) and special slot 73 (`of Sparking`) have no shared
runtime effect implementation. Both have zero random generation weights in this
source. The converter preserves their slots and reports them as unsupported; any
new nonzero generation weight rejects this profile. Fixed/scripted items using
these slots still need behavior work. This is not a claim that every enchant works
in a playable session. Treasure-level footer ranges also remain unimported and are
explicitly recorded as such.

Actual output `../local-data/combat-tables-003/` contains all 98 localized rows and
restores 119,399 glyphs across the six combat tables. Source mechanical comparison
evidence is `../local-data/validation/enchant-source-comparison.json`. All four
earlier combat tables are byte-identical to the previous output. The combined
production startup advances to missing `chest_data.txt`. Nineteen focused combat
tests / 92 total resource tests pass; the library builds for iPadOS arm64 offline.
