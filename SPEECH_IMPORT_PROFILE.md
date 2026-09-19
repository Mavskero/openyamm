# Character speech event import profile

`speech_import SOURCE BASE_ASSETS NEW_DESTINATION` produces the shared
`engine/data_tables/character_speech_events.txt`. It reads the owned MM8
executable and the already converted face, voice and sound tables. No original
program runs, and no audio samples or replacement portraits are generated.
Output must be fresh and outside both inputs; files use lowercase names and
mode 0644. A failed conversion removes its new output.

## Event and voice semantics

The converter requires the 1,155,072-byte executable with SHA-256
`97be86a7597fbd9763f9b4b0ed1f4a7e47c960703fa30857c32daa056b7d403b`.
Its face-event table starts at raw offset `0xfd660`, with eight bytes per record:
two one-based sound group choices, one reserved byte, and five expression IDs.
This is the same source used by `FX_IMPORT_PROFILE.md` for face animations.

An explicit semantic binding covers all 87 shared SpeechId events. Most bind to
the corresponding original face event. Yes, ThankYou and Indignant instead bind
directly to voice groups 42, 43 and 44, validated against original pair labels.
These three events do not acquire an invented face animation. Silent events
retain their expressions; Dying, SelectCharacter and DeathBlow retain the
original absence of expression choices. These bindings do not establish parity
of every event caller, trigger, or Windows patch.

The output marker is `#SpeechReactionMode<TAB>MM8VoiceGroupsV1`, followed by
Event, Name, VoiceGroups, FaceAnimation and SourceFaceEvent columns. Source IDs
remain audit metadata; runtime dispatch uses the engine's named event IDs.
Source group order, duplicate choices and expression associations are retained.
The converter validates expression lists against the imported face table and
checks that all 100 voice slots are present.

The shared voice selector chooses a group before choosing one of its two slots.
If the chosen slot is zero, only the other slot in that group is tried. A group
with two zeros remains silent. Flattening all nonzero variants would change
group probabilities when groups have different numbers of available variants.
For two groups A=[100,101] and B=[0,200], four equally weighted selections now
produce [100,200,101,200]. A missing sound file does not trigger another group.

The original `CharacterOutfits.lua` hook around `0x492c8c` establishes the pair
addressing and within-pair zero fallback. `Scripts/Modules/Faces.lua` documents
group-before-variant selection. OpenYAMM retains its seed-based selection, so
the original RNG stream, custom module loading and patched Windows execution
are not claimed equivalent. Legacy unmarked speech tables continue to use
their existing sound-type-name matching.

## Verification checkpoint, 2026-09-09

- Native output: `../local-data/speech-tables-001`, combined as
  `../local-data/gameplay-base-026`.
- 87 events, 81 face bindings, 45 voiced events and three direct voice events.
  All 7,900 original voice cells independently match the prerequisite table.
- 10,744 event/voice/group/variant checks, including 3,344 silent selections,
  agree with the source. The active bindings select 5,799 distinct nonzero IDs.
- 76 distinct selected IDs lack Sounds.txt records: two for SelectCharacter,
  62 for ThankYou, and twelve shared by Indignant and ShopRude. The full list
  and per-event breakdown are recorded; no IDs or payloads are fabricated.
- 16 speech checks and the full 480 resource checks pass. Mac game/converters
  and the iPadOS library/app compile and link. No iPad was connected or updated.
- Actual GameApplication now passes the previous missing-table exit and runs
  its Metal main loop. A bounded 25-second run ended cleanly when stopped.
  A second run of the same binary in a local Mac app bundle visibly renders
  terrain and exits normally through its window close button.
- Complete startup/playability is still unaccepted: HUD layouts are absent,
  the initial position needs checking, and `plansky1`, `m529`, music and sound
  payloads are missing from this assembled runtime. Optional house animation
  data is also absent. No NPC dialogue, task, battle or player-save workflow
  has been visually accepted by this checkpoint.

Evidence: `../local-data/validation/speech-source-comparison.json`,
`speech-import-source-research.json`, `resource-tests-speech.log`,
`speech-mac-build.log`, `speech-ios-build.log`, `speech-tools-build.log` and
`gameplay-app-start-026/`. The latter includes actual application logs and
`actual-game.png`; the local bundle contains the same built executable, without
an alternative preview entry point. Overall remains a low-confidence 35%–40%.
