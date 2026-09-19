# MMMerge character and doll import profile

Checkpoint 082 extends the metadata stage with 491 original body/hand/head/background images
from nine typed columns. Images remain global, lowercase and 0644; the report records owners,
referencing characters and eleven missing source images. See PAPERDOLL_VALIDATION.md for independent
pixel verification, corrected original coordinates and actual four-member Mac captures. Fitted
equipment variants and complete visual acceptance remain open. Earlier metadata-only results below
are historical, not the current image count or startup boundary.

`doll_import SOURCE BASE_ASSETS NEW_DESTINATION` reads original loose
`Character portraits.txt` and `Character doll types.txt`. It exports shared
`character_data.txt` and `doll_types.txt` without replacing any source cells.
All files are published to a fresh destination outside both inputs with mode 0644;
invalid schemas, references, coordinates or paths leave no partial output.

## Source schemas and identity

The supported portrait header has 27 columns, including Helm X/Y, default attack
IDs and trailing notes. Doll types have 28 columns. Source IDs must equal their
zero-based row position because `CharacterOutfits.lua` populates arrays in row
order. The exported tables retain those printed IDs and all source fields.

The shared loader recognizes this original header and maps a Face index to the
engine's existing one-based characterDataId (source + 1). Zero remains the engine's
unset character identity. This explicit in-memory mapping preserves the first
source portrait and agrees with roster, party, new-game and Lich identity code.
Legacy 24-column engine tables keep their original one-based interpretation.

The supplied table has 77 portraits, six doll types and 56 portraits flagged for
initial selection. Three entries (source IDs 28, 29 and 30) have class/race -1 and
are not selectable; all three remain in the table, including the wetsuit entry.
Their class sentinel is represented by UINT32_MAX internally. Default class
references resolve through the already imported merged class metadata.

## Layout and shared behavior

Original LHo and LHc coordinates identify open and closed hands respectively;
the shared loader now uses their named header order. Legacy custom tables retain
the previous positional order. Checkpoint 082 preserves original downward-positive body/hand Y;
only the legacy converted schemas negate their upward-positive source Y. Helm offsets add directly to the screen position,
as in `RemoveItemsLimits.lua`'s helm placement hook. Dwarf portraits 61 and 62
retain respective offsets (2,1) and (1,2), and the shared equipment rectangle
applies them for both drawing and hit testing. Actual rendered alignment is pending.

The Party recruitment and replacement paths now apply doll race, sex, voice and
portrait information. An explicit voice is preserved. A character's numeric
identity remains authoritative when multiple characters share the same portrait
texture; parsing a reused pcNN filename must not change the Face index. Existing
portrait-only fallback initialization remains available when no character ID is set.

Default melee/ranged attack IDs are retained as metadata. Six portraits have a
nonzero special attack; executing those custom attacks is not connected by this
phase. Unknown image columns remain preserved in the exported source table.
Checkpoint 082 copies available original typed image resources without generating replacements.

## Verification and remaining work

Every source field independently matches the native output. Production loaders
check every portrait's numeric fields, assets and metadata, all doll flags and
20 coordinates per type, plus main/off-hand anchors. Seventy-four portraits with
valid class/race data pass shared Party recruitment and replacement checks,
including shared textures and explicit voice preservation. All roster portrait
references resolve. Legacy table checks preserve their old identity and layout.

The original metadata-only checkpoint reported 578 distinct direct image references after name normalization, including
neutral face names; this is not a count of all animated portrait frames. This
metadata phase exported zero images. The current typed image count is 502 references / 491 exports.
Four companions' body/hand visibility is now checked; complete equipment alignment, portrait
animations, asset availability, special attacks and a full gameplay loop remain unvalidated.

Historical checkpoint `doll-tables-001` was incorporated into `gameplay-base-016`. Formal
startup now passes both tables and stops at required `teacher_topics.txt`.
Optional `house_animations.txt` remains missing. Nineteen new doll tests and the
complete 305-test resource suite pass. Mac game/converters and iPadOS arm64 builds
pass. No iPad was connected or updated.
