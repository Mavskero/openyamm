# Device acceptance and routine regression

Checkpoint 059 adds Mac evidence for enemy turns, normal projectile damage, one kill/loot and
cold reload, plus a visible shared turn-phase/movement label. MM7 turn controls and Chinese
dialogue spacing pass again. No iPad operation or physical acceptance was added. Later device
regression must verify the phase label and combat input/readability; missing effect sources and
oversized near-enemy health bars remain open. See HOSTILE_TURN_COMBAT_VALIDATION.md.

Checkpoint 058 repairs shared turn queue membership and Chinese dialogue body spacing, with a
0.9 body scale. Mac checks cover normal turn controls, Torch Light, five movement steps, guide
modal pause/resume and reload. Later physical acceptance must verify readable dialogue and turn
input on the user’s screen; no iPad operation was performed. Complete hostile turn combat and
visible turn indicators remain pending. See TURN_COMBAT_VALIDATION.md.

Checkpoint 057 adds the authored text spellbook and fixes stale double-click state after other
controls. Mac input-frame checks cover touch-origin/mouse-origin selection, details, quick/attack
bindings, school changes, normal casts and target selection; cold reload preserves the bindings.
These are not physical iPad/accessory checks. Later device work must verify the new spellbook,
right-click/long-press behavior and available audio; missing source icons/samples remain documented.
See SPELLBOOK_CONTROLS_VALIDATION.md.

Checkpoint 056 fixes normal starting spell knowledge/book allocation. Mac MM6/MM7 caster rules,
learning, mana/recovery, one MM6 projectile impact, cold reloads and formal scene-adapter casting
pass. Actual spellbook captures expose missing source icons; visible spell selection and the missing
common cast/open-book sounds are not accepted. No new physical acceptance or device operations.
See NORMAL_SPELL_VALIDATION.md.

Checkpoint 055 adds bounded unused on-demand WAV caching and Mac checks for active/paused sounds,
preload/music lifetime and the formal gameplay party audio queue. It adds no physical input/audio
acceptance. Later device tests must cover actual action triggers, concurrent audio, interruption and
long-session behavior; total app RAM is not limited by the 64 MiB transient PCM budget.
See AUDIO_LIFECYCLE_VALIDATION.md.

Checkpoint 054 adds 7,886 original WAVs, Chinese overlay selection and SDL partial ADPCM block support.
Mac decoder and dummy postmix checks pass; this adds no physical iPad/audio/input acceptance. Future
device work should use the new sound-containing package and verify actual UI/voice/effect triggers,
speaker/headphone routing, interruptions and sustained mixed audio. See SOUND_PAYLOAD_VALIDATION.md.

Checkpoint 053 imports 49 original MP3s and fixes paused music transitions/resampler tail. All tracks
decode on Mac; shared mixer tests and formal MM6/MM7 Mac application starts pass. Generic unsigned iOS
Release builds. No physical audio or iPad tests were added. Later device acceptance must cover actual
speaker/headphone/Bluetooth output, interruptions, app backgrounding and sustained playback using
the new music-containing resource package. See MUSIC_SOURCE_VALIDATION.md.

Checkpoint 052 adds Mac-only arrow, zoom and short movement acceptance on MM6/MM7, plus corrected
world-qualified ZIP mounting. Final generic unsigned iOS Release compiles. No iPad or physical input
checks are added. Later device regression must use the new package bindings and verify actual zoom
clicks, direction/movement and cross-world asset behavior. See MINIMAP_NAVIGATION_VALIDATION.md.

Checkpoint 051 restores outdoor minimap imagery on Mac using 28 original world-owned images.
Actual MM6/MM7 Metal captures and a generic unsigned iOS Release build pass. This adds no iPad or
physical input acceptance. Party arrows, map controls and navigation behavior still need work;
see MINIMAP_SOURCE_VALIDATION.md. Subsequent device regression must use the updated runtime package.

Checkpoint 050 adds only Mac evidence: original-source wide-HUD regions and available character
faces render in explicit single/five-person fixtures; hit geometry and side-panel separation pass.
Default/MM8 portraits and outdoor minimap imagery remain incomplete. No device or physical input
acceptance is added. See HUD_SOURCE_VALIDATION.md for the test-fixture limits.

Checkpoint 049 is also Mac-only. Source hardware-water bindings/frames fix the observed
Emerald Island shoreline artifact in real host Metal captures; the wide-HUD defect remains.
Generic unsigned iOS Release compiles. No physical acceptance is added or requested.
See HARDWARE_WATER_VALIDATION.md. Historical checkpoint 048 visual failures remain below.

Checkpoint 048 remains Mac-only: 140 MM6/MM7 map loads, MM7 creation/cold reload and
Moon Temple transitions/movement/save pass. Actual Mac Metal frames expose water and
wide-HUD defects, so these are not visual acceptance passes. Generic iOS Release compiles.
No device query, connection, installation, launch or demonstration occurred. Physical
acceptance remains paused. See MM7_GAMEPLAY_VALIDATION.md for exact host evidence.

Scene expansion checkpoint 047 (2026-09-11, Mac only): the native app importer now packages
64 MM6 scenes (15 outdoor, 49 indoor) and validates all through the gameplay data loader.
One source-missing map and two scenes with unimplemented event commands remain explicit
exclusions. This is data/package coverage, not full campaign or visual acceptance.
Normal new-game/cold reload, prior combat-save state, indoor camera/state restoration and
return to town pass. Generic unsigned iOS Release compiles. No iPad was queried, installed,
launched or demonstrated. See SCENE_IMPORT_PROFILE.md and map-expansion-047/result.json.

Checkpoint 046 (Mac only) verifies actual application entrance/exit transitions,
short indoor movement, save/reload and cold-process restoration in all three imported
dungeons. Save format 84 fixes indoor camera persistence and remains able to read old
saves. The new version and camera restoration have not been installed or tested on iPad.
Indoor combat, rendered corridors/doors/UI and accessory behavior remain pending.
See INDOOR_SESSION_VALIDATION.md for the exact checked invariants and compatibility limits.


Checkpoint 045 (Mac only) adds three dungeon resources/events and shared TFT/SFT loading.
Four converted map loads, entrance/key/exit event requests, on-load handlers and one
mechanism/chest target per dungeon pass in the shared runtime. Actor previews have no
missing actor textures, while campfire art remains missing in two dungeons. These results
are not physical indoor play acceptance: transitions, corridors, combat, save/reload,
visuals/audio and accessories still need later verification. No device was used or updated.


Status: 2026-09-11, M5 iPad Pro 11-inch, iPadOS 27 beta 24A5408d.
Physical demonstrations are paused at the owner's latest request. Checkpoint 042
after that instruction uses Mac application/runtime checks and generic iOS builds
only. Default Knight creation, Enroth startup and a new-process save reload pass
on Mac; they do not satisfy the pending device gameplay or IME/visual checks.
Checkpoint 043 additionally covers normal starter equipment, movement to a real
hostile, incoming damage, melee/recovery, a kill, corpse gold and a new-process
reload using the shared runtime on Mac. This runs without graphics or UI input
and does not satisfy physical combat, effects, sound or accessory acceptance.
Checkpoint 044 wires the launcher's source-conversion button to the same native
pipeline tested on Mac: source copy, 31 conversion stages, package/data/map
validation, publication and failure/retry. It does not establish physical folder
picking, iPad conversion resource usage, or a complete world-content import.

Six device acceptance **themes** remain; this is not a count of test cases or an
estimate of six user operations. Passing them also does not complete missing
gameplay, assets, original-folder conversion or MM6/7/8 world content.

| Theme | Current physical evidence | Remaining device checks |
| --- | --- | --- |
| Keyboard, mouse and trackpad | W movement, R rest, single click advances five minutes, pointer lock prevents mouse Dock | Held and combined keys, right click, wheel, dragging across inventory/dialogue, accessory reconnect |
| Touch and Chinese input | Menu/dialogue clicks, fast double tap, software pinyin selection, Chinese save/load, edited row above keyboard, append/delete, duplicate-name rejection and cancel; touch movement/turn save deltas; larger Chinese stat/skill labels and skill scrolling with all eight saves unchanged | Movement/camera/multi-touch together, inventory dragging, other text editors |
| Lifecycle and windows | Touch Dock and return preserves full screen and controls; keyboard dismissal restores frame; game status bar hides; short Home/return passes in menu, town active Chinese save editor; 45-second Home/return preserves scrolled skills | Lock/unlock, longer background intervals, window changes during play/media/editing, audio interruptions |
| Rendering and sustained performance | Metal outdoor town and one tavern animation | Indoor world, combat effects, repeated transitions, sustained session, memory and thermal behavior |
| Audio | Host decoding checks exist; no complete listening acceptance | Speaker/Bluetooth output, synchronization and interruption/reconnect behavior |
| Player import | Development imports and failure protection partly exercised | Real folder picker and permissions, cancellation, full native conversion, large imports, restart availability |

Routine gameplay rules, dialogue outcomes, reward idempotency, save fields and
error handling belong in Mac headless tests and repeatable UI scenarios where
possible. The simulator can exercise iPad UI, sandbox behavior and much of Metal
rendering, but its Mac hardware and simulated environment do not establish iPad
accessories, system gestures, thermal behavior, audio routing or performance.

The existing arm64 simulator build/runtime is available. It has not run the new
032 gameplay scenarios. Those earlier scenarios used a physical iPad; the latest
instruction now pauses device use. The standalone runner is currently
configured for iphoneos; simulator support must be added explicitly before its
tests are claimed as simulator results.

Automation sends actual touch/IME operations and preserves screenshots, recordings
and save copies. Physical E still needs an accessory operation: synthesized UIKit
presses are ignored by SDL when a physical keyboard is handled via GCKeyboard.
An XCTest API call returning successfully is not proof that the game reacted.
Likewise, a label visible both before and after an action is not a scene-transition
assertion. Use distinct scene evidence and read actual saves for persistent state.

Sources: [Apple device and Simulator testing](https://developer.apple.com/documentation/xcode/running-your-app-on-simulated-or-physical-devices),
[Metal in Simulator](https://developer.apple.com/documentation/metal/developing-metal-apps-that-run-in-simulator).
