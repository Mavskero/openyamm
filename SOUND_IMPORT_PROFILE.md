# Sound catalog import profile

Checkpoint 076 adds original EnglishD.lod below prefixed MM8 and Chinese layers. The combined source
exports8574WAVs; all decode and all7886previous payloads remain byte-identical. Catalog sample6359 and
631voice lookup IDs remain unresolved. See MM8_BASE_RUNTIME_VALIDATION.md; file/decoder validation
does not establish physicaliPad audio acceptance or Windows cache-policy parity.

Checkpoint 055 adds an engine-managed unused WAV cache budget and verifies the map/shared preload
lifecycle without changing source tables or payloads. Original cache-flag emulation remains pending;
see AUDIO_LIFECYCLE_VALIDATION.md. The last resource conversion is checkpoint 054 below.

Checkpoint 054 also imports original WAV payloads through this same native stage. See
SOUND_PAYLOAD_VALIDATION.md for the 7,886 byte-identical selected sounds, Chinese precedence,
partial ADPCM block fix, actual decoder/mixer checks and remaining missing samples. No iPad operations.

`sound_import SOURCE BASE_ASSETS NEW_DESTINATION` converts the original
`Data/Tables/Sounds.txt` into `engine/data_tables/sounds.txt`. It reads an owned
MM8 executable only to verify the behavior profile, and checks references from
the previously imported character voice table. No original executable runs and
no substitute sound samples or placeholder files are generated. Original wrapped WAVs are exported
to engine/audio without transcoding. Output must be fresh and
outside both inputs, with lowercase names and mode 0644; failed output is removed.

## Source lookup and format

The source has 8,546 records, all four columns wide: Name, Id, Type and Is3D.
All original fields, order and duplicate rows are preserved after the explicit
`#SoundCatalogMode<TAB>MM8OrderedV1` marker. Literal source tabs and lines are
converted to the engine's quoted TSV serialization without treating name quotes
as original CSV syntax.

- Is3D `x` means positional audio; `-` means nonpositional. The legacy unmarked
  engine format continues to recognize `3d`.
- Source Type values 0, system, swap and lock correspond to 0, 1, 2 and 4.
  They are retained as `SoundCatalogEntry.loadType`. The engine's existing audio
  cache is not yet changed to emulate original preload/swap/lock lifetimes.
- Repeated nonzero IDs select the first row in marked MM8 tables. Unmarked
  tables continue to reject nonzero duplicate IDs. The merged profile is
  engine-owned and cannot be used as a world-local registry.
- Eleven rows use ID zero. They remain in the file, while the marked lookup
  keeps the original null sentinel. Zero is not a playable sound ID.

This lookup rule is directly visible in the supplied executable. At
`0x4a7f4e` the sound loader compares the requested ID and exits its forward scan
on the first match. The sound player repeats the forward scan at `0x4a883c`.
Both use 152-byte records with IDs at offset 64. The executable fingerprint is
`97be86a7597fbd9763f9b4b0ed1f4a7e47c960703fa30857c32daa056b7d403b`;
unknown fingerprints require another verified profile. Instruction evidence is
saved in `../local-data/validation/sound-lookup-binary-evidence.json`.

The duplicate nonzero source IDs are 125 and 31111–31114. ID 125 repeats the same
title record. The other four choose the earlier elf magic shop names, retaining
the later dwarf weapon shop rows as shadowed original data. No ID reassignment
or guessed name substitution is performed. Original MMExtension table aliases
and sound struct fields independently establish the Type and Is3D semantics.

## Voice references and acceptance limits

All 6,946 nonzero voice-table cells are checked against the catalog. The source
voice table contains 631 distinct IDs absent from Sounds.txt; the report retains
their full list. This includes unused voice slots and must not be interpreted as
631 missing active speech events. The 41 groups referenced by original face
events use 5,550 distinct nonzero sound IDs, of which 14 are absent from the
catalog. These are recorded in `speech-import-source-research.json`; additional
direct voice-only reactions are not included in that subset. The event-to-voice
mapping is a separate task.

Checkpoint 053 corrected the earlier payload audit: no standalone SND/WAV files are present, but
five D.lod archives contain 8,732 WAV member records. Checkpoint 054 imports 7,886 unique originals
with the 846 named Chinese overlays taking precedence. All selected bytes match the source and all
files decode through the shared SDL path. The report now includes exported counts/bytes and 723
missing nonzero catalog sample IDs. Actual sound/voice requests have limited diagnostic mixer checks;
full gameplay dispatch, audio lifetime rules and patched Windows parity remain pending.
The separate media stage imports 49 original MP3s; see MUSIC_SOURCE_VALIDATION.md.

Speech-reaction table integration is documented separately in SPEECH_IMPORT_PROFILE.md; the sound
catalog by itself is not complete effect or voice playback acceptance.
No speech table is replaced with a dummy and no required loader is bypassed.

## Verification checkpoint, 2026-09-09

- Native output: `../local-data/sound-tables-001`, merged into `gameplay-base-025`.
- All 34,184 source fields independently compared. Original order, duplicate
  rows, quote characters and all 643 spatial-sound flags remain intact.
- Production SoundCatalog queries match all 8,531 first-occurrence lookup IDs,
  including load type and position flags. Legacy duplicate rejection and `3d`
  handling have separate synthetic checks.
- 16 new checks and all 464 resource checks pass. Mac converter/game and iPadOS
  library/app builds pass. No device was connected or updated.
- Actual GameApplication still exits 1, now reporting only missing
  `character_speech_events.txt`. It resolves the sound-table file but returns at
  the missing-speech guard before constructing audio playback; native typed
  catalog checks do not substitute for full application startup or sound output.

Evidence: `../local-data/validation/sound-source-comparison.json`,
`sound-lookup-binary-evidence.json`, `resource-tests-sound.log`,
`sound-mac-build.log`, `sound-ios-build.log`, `sound-tools-build.log` and
`gameplay-app-start-025/`. Overall remains a low-confidence 35%–40% estimate.
