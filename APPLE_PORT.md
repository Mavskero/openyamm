Storage maintenance (2026-09-11, user-authorized; gameplay development remains paused): obsolete
generated runtime ZIPs and byte-identical import-test source copies were deleted after audit.
Complete runtimes 059/runtime-002 and 058/runtime-001 remain; source, saves, screenshots, logs,
reports and build/dependency trees are preserved. Older runtime paths below are historical and
may no longer contain their ZIP payloads. See ../存储清理记录.md for scope and verification.

Checkpoint 059 (2026-09-11, Mac only): a normal MM6 caster survives enemy turns, resolves two
Fire Bolts, kills one enemy, gains experience and loots once. Cold reload preserves the kill,
resources and consumed corpse. A shared Chinese turn-phase/movement label is visually checked;
MM7 controls and the smaller, spaced Chinese dialogue pass again. Native import loads 140 maps,
23 UI checks and unsigned generic iOS Release pass. Missing effects/portraits/audio, oversized
near-enemy health bars and full gameplay remain open. No iPad work; overall remains 40%–45%,
low confidence. See HOSTILE_TURN_COMBAT_VALIDATION.md.

Checkpoint 058 (2026-09-11, Mac only): shared turn queues now include every eligible index,
remove invalid actors and add newly nearby hostiles. Fifteen logic tests, MM6/MM7 formal turn
controls, guide-dialogue pause/resume and cold reload pass. Per user feedback, dialogue body
text is 10% smaller and Chinese line spacing/frame sizing use actual shaped text bounds; the
original guide paragraph displays without overlap. Native import loads 140 maps, 23 UI checks
and unsigned generic iOS Release pass. No iPad work; full hostile combat and other campaign/
resource/lifecycle gaps remain. Overall stays 40%–45%, low confidence. See TURN_COMBAT_VALIDATION.md.

Spellbook checkpoint 057 (2026-09-11, Mac only): authored Chinese text buttons make learned
spells operable without the missing icon set. MM6/MM7 formal touch/mouse input-frame sequences,
details/dismissal, quick/attack bindings, school changes, casting/targeting and cold reload pass.
The native full import still loads 140 maps; generic unsigned iOS Release builds. Physical input,
all spell effects, source gaps and full campaigns remain open. Overall stays 40%–45%, low confidence.
See SPELLBOOK_CONTROLS_VALIDATION.md.

Spell checkpoint 056 (2026-09-11, Mac only): normal new characters now start with the original
basic spell knowledge and second-spell book. MM6/MM7 Sorcerer creation, learning, mana/recovery,
three cold reloads, one MM6 Fire Bolt impact and formal Mac casting pass; generic unsigned iOS
Release builds. Actual spellbook captures expose 250 missing source image references, so visible
spell selection is still unaccepted. No iPad operations. Overall stays 40%–45%, low confidence.
See NORMAL_SPELL_VALIDATION.md.

Audio lifecycle checkpoint 055 (2026-09-11, Mac only): shared WAV caching now evicts unused on-demand
PCM above a 64 MiB budget, protecting active/paused instances, explicit preloads and registered music.
The same 500 original requests retain 63.47 MiB instead of about 263 MiB. Eleven focused regressions,
map/music cache lifecycle, formal Mac party-queue speech and generic unsigned iOS Release pass.
No iPad operations. Overall remains 40%–45%, low confidence. See AUDIO_LIFECYCLE_VALIDATION.md.

Sound checkpoint 054 (2026-09-11, Mac only): native import preserves 7,886 WAVs and 846 Chinese overlays.
Shared SDL audio now keeps complete frames in partial IMA blocks; all samples decode, and explicit
sound/voice requests reach dummy postmix output. 36 focused checks and generic unsigned iOS Release
pass. No physical device/audio acceptance. Overall remains 40%–45%, low confidence.
See SOUND_PAYLOAD_VALIDATION.md.

Music checkpoint 053 (2026-09-11, Mac only): native import preserves 49 original MP3s. All decode through
the shared FFmpeg/SDL player; paused track replacement and resampler tail are fixed. MM6/MM7 Mac
application music and generic unsigned iOS Release pass. No device operations; effects/speech payload
integration and physical audio remain pending. Overall stays 40%–45%, low confidence.
See MUSIC_SOURCE_VALIDATION.md.

Navigation checkpoint 052 (2026-09-11, Mac only): original eight-direction MM6/MM7 minimap arrows,
explicit world bindings and original black transparency. Corrected packaged world namespace mounts;
11 binding/stream checks, five importer checks, actual Mac zoom/movement/direction captures and generic
unsigned iOS Release pass. No iPad operations. Overall stays 40%–45%, low confidence.
See MINIMAP_NAVIGATION_VALIDATION.md. Earlier pending-arrow statements below are historical.

Minimap checkpoint 051 (2026-09-11, Mac only): 28 original outdoor map images imported into
world-local icons and all source pixels checked. Actual MM6/MM7 Metal captures display the maps;
party arrow binding and navigation behavior remain pending. Generic unsigned iOS Release passes.
No iPad operations. Overall stays a low-confidence 40%–45%. See MINIMAP_SOURCE_VALIDATION.md.

HUD checkpoint 050 (2026-09-11, Mac only): original image regions restore wide-HUD art;
2,465 original portrait poses are imported. Available faces display in single/five-person visual
fixtures, with portrait hit geometry and panel nonoverlap checks. The default/MM8 face source
and outdoor minimap imagery remain incomplete. No iPad operations. See HUD_SOURCE_VALIDATION.md.

Hardware-water checkpoint 049 (2026-09-11, Mac only): import source bindings and original
animation frames for the existing shared material renderer. Emerald Island's captured magenta
shoreline is fixed and animated; the wide-HUD asset defect remains. All 140 map loads, focused
surface checks, MM7 cold reload and generic unsigned iOS Release compilation pass. No device
operation. See HARDWARE_WATER_VALIDATION.md for local visual evidence and remaining limits.

MM7 checkpoint 048 (2026-09-11, Mac only): 140 MM6/MM7 scenes pass package/data loads;
normal MM7 creation/cold reload and the Moon Temple entry/movement/save/return route pass.
Mac Metal captures show real town and indoor geometry but expose water and wide-HUD defects.
Generic unsigned iOS Release compiles. No iPad operation. See MM7_GAMEPLAY_VALIDATION.md
and ../local-data/validation/mm7-scenes-048/result.json for evidence and limits.

Scene expansion checkpoint 047 (2026-09-11, Mac only): the native app importer now packages
64 MM6 scenes (15 outdoor, 49 indoor) and validates all through the gameplay data loader.
One source-missing map and two scenes with unimplemented event commands remain explicit
exclusions. This is data/package coverage, not full campaign or visual acceptance.
Normal new-game/cold reload, prior combat-save state, indoor camera/state restoration and
return to town pass. Generic unsigned iOS Release compiles. No iPad was queried, installed,
launched or demonstrated. See SCENE_IMPORT_PROFILE.md and map-expansion-047/result.json.

Indoor session checkpoint 046 (2026-09-11, Mac only): actual application transitions,
short collision-aware movement, indoor save/reload, cold-process restoration and return
to town pass for 6d01/6d02/6d18. Materialized actor counts in these runs are 217/223/117.
Fixed missing indoor camera persistence: save version 84 adds yaw/pitch, while older
saves remain readable with the prior default indoor orientation. The inspector now
reports coordinates from the active scene. The existing outdoor combat save and a
synthetic version-83 indoor compatibility fixture load successfully. Generic unsigned
iOS Release compiles; no device was used or updated. These checks do not establish
full dungeon traversal, combat, rendered behavior or hardware input acceptance.
See INDOOR_SESSION_VALIDATION.md and ../local-data/validation/indoor-session-046/.
Overall remains the low-confidence 35%–40% estimate.

Indoor scene checkpoint 045 (2026-09-11, Mac only): the native import now assembles
New Sorpigal plus 6d01, 6d02 and 6d18, including BLV/DLV, map-local events and referenced
surface/actor/decoration resources. It adds 33 shared TFT rows and supports multi-map
EVT selection. Full common SFT now satisfies requested monster animations without
requiring redundant split files; both indoor and outdoor loaders use the fix.

The app-import API publishes four validated map identities, preserves the source copy,
and produces 3,526 CRC-checked ZIP entries. Shared event checks pass entrance/key gates,
entry handlers, one door/chest per dungeon and return targets. Dungeon actor previews
number 51/58/35 with no missing actor textures. Campfire frames/palette are still missing
in 6d01/6d02. Normal Chinese creation/start/reload and the earlier combat save remain usable.
Twenty-six event/importer tests, indoor staging cleanup/output protection and unsigned
generic iOS Release compilation pass. No device is used or updated. Indoor walking,
rendered transitions/animation, combat and indoor saves remain unaccepted.
See INDOOR_IMPORT_PROFILE.md and ../local-data/validation/indoor-import-045/.
Overall remains the low-confidence 35%–40% estimate.

Native import checkpoint 044 (2026-09-11, Mac only): the launcher and the Mac
import tool now share source-folder conversion, stage progress, package writing,
data/map validation and atomic selection. Thirty-one existing converters run
without manual assembly. New conversion starts use normal MM6/main-menu settings.
The supported scene remains New Sorpigal; full MM6/7/8 content is not complete.

The supplied source yields 2,813 package entries; all four ZIPs pass CRC checks
and match between direct and app-import execution. Ninety-one gameplay data files
match the prior proven assembly. Normal Chinese creation/start/reload passes,
and the prior 043 combat save is compatible. A fresh random-population combat
driver route fails and is not counted as a gameplay pass. Ten importer tests and
a real-source stage-12 failure/cleanup/retry check pass. Generic iOS Release
compiles with the bundled layouts; no device is used or updated.
See RUNTIME_IMPORT_PROFILE.md and ../local-data/validation/runtime-import-044/.
Overall remains the low-confidence 35%–40% estimate; full content and physical
source conversion have not been accepted.

Normal combat checkpoint 043 (2026-09-11, Mac only): added a focused application
driver that reloads the normal Chinese Knight from 042, equips the imported sword
and leather armor through shared inventory/equipment rules, and moves from the
town entrance to a real nearby hostile. No teleport, debug party, invulnerability,
manual damage or forged loot is used. Shared movement, actor AI, combat/recovery,
death-state timing and corpse-loot controllers run without rendering or UI input.
The driver explicitly binds the settings normally bound by the graphical view.

The final run produces attack damage 7–11 and AC 5, takes damage from enemies,
kills actor 241 with a 9-point hit, rejects a second attack during recovery, and
loots after the death animation reaches corpse state. It ends at HP 17, XP 24,
206 gold (6 looted). Incoming damage and loot use the runtime RNG, so earlier
successful runs have different HP/gold. These are observed outcomes, not balance
guarantees or a complete combat playthrough.

A separate process reloads the resulting save through GameApplication. Identity,
HP and death/visibility states match for all 281 actors, the one consumed corpse
cannot be looted again, and inspected party/save fields match after resaving.
The inspector now includes equipment IDs, condition bits and recovery time.
Equipment enchant/runtime fields and complete world snapshots are not compared.
Mac targets and generic iOS Release compile successfully; no device is used.
Evidence: ../local-data/validation/normal-combat-043/result.json. Overall remains
35%–40%, low confidence. Normal-party UI control, combat rendering/audio, spell
and turn-based battles, more maps, and native source-folder conversion remain.

Normal creation checkpoint 042 (2026-09-11): physical demonstrations are paused
at the owner's request. Work since that instruction uses Mac checks and generic
iOS builds only. NewGameScreen accepts validated UTF-8 names, limits by 15 code
points, deletes complete code points, resets repeat on focus loss, reports its
authored keyboard area and fits long names. Native checks cover editing and each
continent's default Knight allocation/skill constraints; this does not cover
every candidate/class or physical IME/visual acceptance.

Actual normal GameApplication startup exposed a lost first-entry letter: later
on-load handlers cleared pending item outputs while preserving quest bits.
Shared EventRuntime now preserves batch outputs until consumed; independent
events still reset outputs. The full imported entry chain and existing house
reward/repeat/save checks pass. Generated Lua and imported packages are unchanged.

The Mac driver disables immortal/unlimited mana in isolated settings and starts
the normally created Chinese character in New Sorpigal (level 1, HP 35, 200 gold,
5 food). Shared held-item controllers receive/place one letter. The application
saves and a new process reloads and resaves. The expanded inspector matches
identity, progression, stats, HP/MP, skills, known spells, all InventoryItem fields
and prior map/position/quest fields. A renderer-free reload initially lost yaw;
camera orientation now restores independently of renderer initialization.
Unreported character/world state remains outside scope.

Mac checks and the latest generic iOS Release build pass without using a device.
An earlier naming-only device install preceded the owner's instruction; its UI
runner failed during bootstrap. Later event/camera fixes have not been installed.
Combat, complete artwork/audio, MM7/MM8 application starts and full player import
remain pending. See ../local-data/validation/new-game-042/. Overall stays at the
low-confidence 35%–40% estimate.

Background checkpoint 040 (2026-09-11): signed app/runner builds and the physical
45-second Home/return plan pass. The scrolled last skill remains visible after
return, then dragging back to the top and Stats 2042 work. The immediate activation
capture shows a system transition; the subsequent OCR-checked capture establishes
the restored game. This does not establish lock/unlock, OS termination, sustained
performance or audio interruption. Launcher and preview-return guidance now
reflect converted-resource gameplay and Chinese saves. XCTest background actions
allow up to 60 seconds; gestures stay capped at 5 and whole plans at 180 seconds.
See ../local-data/validation/ipad-background-040/. Overall remains 35%–40%, low confidence.

Readable character skills checkpoint 039 (2026-09-11): following the owner's
font-size feedback, rows keep a minimum 18 logical units; crowded lists scroll
instead of shrinking the whole page. Shared CharacterSkillLayout places visible
rows/headers and supplies the same offset to rendering, upgrade clicks and inspect
popups. Wheel and touch dragging clamp to the content, excluding the tabs. Hidden
rows cannot receive skill upgrades, and dragging clears pending clicks. Ordinary
short lists keep the same row size with zero scroll.

Checkpoint 038 introduced shared canonical skill rows and existing translated names.
Its first physical build overlapped CJK lines; fitting CoreText image bounds fixed
the overlap but made all Chinese labels too small. Those failed/OCR runs remain.
The final 039 revision retains large stat labels and uses opt-in text.fit_height
for the constrained skill rows; CoreText measurement shares rasterization bounds,
including texture padding and shadow. Mastery/category headings and the unmapped
Throwing skill remain English. No conversion or imported package was changed.

Mac game/input/text and signed iPad builds pass. Native checks cover multiple sizes,
short/crowded lists, overscroll, the actual controller's wheel/drag, and upgrading
only the visible final VampireAbility row (its neighbor unchanged, 100->98 points).
An initial native fixture omitted its repository binding and crashed; that fixture
was corrected. Physical plan 001 passes 长剑 at top, 吸血鬼能力 after a drag, Home/
return preserving the scroll, drag back to top and Stats 2042. Screenshots confirm
separated larger text and complete numbers above the tabs; all eight saves remain
byte-identical. The owner confirms the new font size is suitable. Physical wheel acceptance remains pending.
See ../local-data/validation/ipad-readable-skills-039/. Overall stays 35%–40%, low confidence.

Physical Chinese attribute checkpoint 037 (2026-09-11): CharacterStatLayout now
owns the existing 26 attribute label/inspect bindings. Labels and both skill-points
headings read the already imported CharacterInspectTable display names; canonical
keys and numeric cells remain unchanged. Native checks and signed Release builds
pass. Device run 002 passes 力量/智力/2042/技能点数 and page switching; screenshots
confirm complete labels, values and a hidden status bar. Run 001 waited for unlock
and then found the app stopped; that failed evidence is retained. English condition
values and skill headings remain. See ../local-data/validation/ipad-character-chinese-037/.
Overall remains 35%–40%, low confidence.

Physical label/skill-layout checkpoint 036 (2026-09-11): the installed Release
fits full HUD labels to their pixel bounds, preserving numeric values. A failing
native regression reproduced scaled bitmap/CJK text overflow. An intermediate
strict-clipping build displayed 100 instead of 10000; the final fitting revision
preserves all digits. Tiny gold OCR still fails, so those failed runs are retained;
screenshots establish the complete 10000/11000 display, not a passing OCR assertion.

Shared CharacterSkillsLayout now derives row spacing from authored page/section
anchors and the tab boundary. Rendering, upgrade hit targets and inspect popups
use the same spacing; glyphs scale with the rows. Native checks exercise crowded
and short lists at 640x480, 1210x834 and 2420x1668, including repeated fitting.
The physical current party shows every skill above the tabs and full long names
separate from levels. Statistics (2042 HP), Home/return and Chinese save labels
pass follow-up checks; all eight saves remain byte-identical. Character labels
remain English, artwork is missing, and much larger mod skill sets may need
pagination. XCTest now passes the UIImage orientation explicitly to Vision;
this does not resolve all small bitmap-font OCR. See ../local-data/validation/ipad-label-layout-036/.
Overall stays 35%–40%, low confidence.

Physical touch-motion checkpoint 035 (2026-09-10): the installed 034 revision
passes a left-side backward drag and a right-side horizontal camera drag. A
separate eighth save, 移动, records a 69.42-unit XY displacement and -1.2274-radian
yaw delta, with unchanged height/pitch at inspector precision. Map, party count,
gold, food, QBits and ordered inventory IDs/quantities are unchanged; the earlier
seven save files remain byte-identical. This does not establish simultaneous
multi-touch or full combat controls. See ../local-data/validation/ipad-touch-motion-035/.

Physical status-bar checkpoint 034 (2026-09-10): iOS game windows now use SDL's
public borderless flag. The existing UIKit controller hides the status bar; the
separate native launcher retains it. Mac/iPad Release builds pass and the signed
build is installed. Physical screenshots confirm both states. Automatic Home/return
for three seconds in the menu and in town preserves the 1210x834-point window;
top Menu, load/save entries, and loading preserved save7 all work. This does not
replace lock/unlock, long background, or real pointer/Dock acceptance. Evidence:
../local-data/validation/ipad-statusbar-034/. Overall remains 35%–40%, low confidence.
The same installed build also passes Home/return with an active Chinese save-name
editor: actual keyboard/IME appends 测试, and uncommitted 输入测试 stays visible
above the keyboard. Dismissing the keyboard then tapping Cancel discards that
edit; reopening the list retains 输入. All seven copied save files are byte-identical.

Physical character-page checkpoint 033 (2026-09-10): a portrait double tap opens
the inventory, but missing item/paperdoll art prevents inventory drag acceptance.
A read-only audit of all provided LODs found 21 of 24 requested image names absent;
the remaining three are optional small-potion replacements, disabled by default
in the original Merge script. No substitute art was imported or enabled.
The first Inventory-to-Stats touch was intercepted by an invisible mobile inspect
zone. Shared runtime availability now requires the authored visible button/icon.
The failing native regression and a positive mobile-layout check pass. Mac/iPad
builds pass; installed ui-stats-fix-004 enters Stats on the first touch, returns to
Inventory, and enters Stats again. Screenshots and OCR retain both transitions.
English character labels, overflowing skill rows and missing artwork remain open.
See ../local-data/validation/ipad-inventory-033/result.json. Progress stays 35%–40%.

Previous physical input checkpoint 032 (2026-09-10): fast batched taps reproduced a
shared input failure before the fix. Ordered completed taps, releases, cancellation
and focus loss now pass native regression checks, including the actual SaveGame
controller. The signed iPad build passes fast double-tap entry to name editing.
Direct XCTest typeText still cannot target SDL's hidden field, but actual software
keyboard taps, Chinese IME candidate selection and Return save `测试` in save4.
After a second installed build/app restart, save4 loads successfully. Save-row
bounds are now supplied to SDL in window coordinates to keep `验证` visible above
the keyboard; Return saves it as save5 and restores the full game frame.
Actual save readbacks match rewarded save3 for the documented fields; previous
saves remain byte-identical. Mac/iPad builds and the input check pass. See
../local-data/validation/ipad-input-032/result.json and ios/UITests/README.md.
The owner then used E to re-enter the tavern. Automatic Andover letter selection
shows the already-paid reply and unchanged gold. A sixth save `测试版` confirms
unchanged QBits, inventory IDs/quantities and the other listed fields; earlier five
files are byte-identical. Actual software-keyboard append/delete also passes:
`测试版测试` followed by two Delete taps saves `测试版`. Direct entrance touch did
not enter. Complete import, combat, cross-world content and stability remain open.
Overall stays 35%–40%, low confidence.

A further native regression reproduced last-character loss when committed IME
text and Return arrived in one frame. The shared SaveGame controller now applies
committed text before name validation/saving. The regression and Mac/iPad builds
pass. The installed revision rejects a duplicate `测试` with a visible red error,
creates no seventh save and leaves earlier files unchanged. OCR misread that error,
so the failed run is retained; screenshot and file comparison establish the result.
Correcting the name to `输入` creates save7 with the expected fields and preserves
all six earlier saves. Same-frame timing is proven by the native regression, not
assumed from XCTest gesture timing.

Previous physical automation/quest checkpoint (2026-09-10): after the user entered
the tavern with E and confirmed ongoing animation/Chinese names, a standalone
XCTest runner now performs actual coordinate taps on the existing game. The
owner enabled system UI automation. It selects Andover 786 and letter topic 1314,
checks 10000 -> 11000 gold, repeats topic 1315 without more gold, exits, opens
Menu/save and creates a third save with the normal default map name. Actual
save readback confirms QBit 1105 removed, 1106 added, one letter 2125, and unchanged
inventory ID/quantity fields. Both earlier user save files are byte-identical.

Screenshots use XCUIScreen.main; coordinates use the main game window to avoid
the application proxy's incorrect screenshot crop. XCTest recordings are retained.
Custom text entry failed for absent keyboard focus; synthesized Escape had no
visible gameplay effect. Those and drag are not accepted. OCR also missed some
long/italic text, so screenshots, gameplay logs and actual saves remain necessary.
See ios/UITests/README.md and ../local-data/validation/ipad-gameplay-031/tavern-quest/.
Rewarded-save reload/repeat, combat, complete import and long stability remain
open. Overall remains a low-confidence 35%–40%.

Physical save checkpoint (2026-09-10): the user saved `真机验证`, restarted
the whole app, loaded it through the visible Menu, confirmed the saved position,
and saved `读档复验` without moving or turning. All these save/load/resave steps
ran without an attached debugger. Actual device save files have been copied back.
The native inspector confirms both UTF-8 names and matching map, party count,
position/view, gold/food, quest bits and ordered inventory item IDs/quantities.
Letter 2125 remains exactly once and the original save file is byte-identical.
Runtime tracing confirms the saved clock is applied on load; the second save
includes time elapsed after loading. Comparison is limited to the listed fields
and inspector precision, not every item property or all world state.

Physical Chinese save/restart/load/resave acceptance therefore passes. Menu
initially still failed after the touch-origin fix; later traced and untraced
operations succeeded. Broader first-tap reliability remains open. Tavern media,
quest rewards, combat, full raw import and long-session stability are unaccepted.
Evidence: ../local-data/validation/ipad-gameplay-031/player-save/comparison.json
and actual save copies. Overall remains a low-confidence 35%–40%.

Physical Dock retest (2026-09-10): the user confirms mouse motion at the
bottom no longer reveals Dock, with actual pointer state `locked=1`. Touch still
reveals Dock, but returning produces bad geometry and unresponsive controls.
The loop continues near 120 FPS and pointer lock is reacquired. LLDB reads a
1210x834-point key UIWindow containing an incorrectly sized 834x1210 Metal view.
Its scene orientation is landscape (3), while UIApplication.statusBarOrientation
is Unknown (0). SDL's legacy iOS 10 workaround swaps the authoritative bounds.

Correcting only the live view frame to its UIWindow bounds restores full-screen
display and W/R controls, confirmed by the user. LLDB is detached. The durable
SDL UIKit patch retains scene-backed window geometry, preserving legacy behavior
without a scene. Signed Release build and repeated CMake configuration pass;
the user confirms the installed build preserves full-screen display and controls
after touch Dock and return. No debugger correction was used in this retest. See
../local-data/validation/ipad-gameplay-031/window-recovery/diagnosis.json.
Overall remains a low-confidence 35%–40%.

The following rest-screen retest also passes by user confirmation: visible moving
native arrow, no mouse-triggered Dock, and exactly five minutes advanced by one
click. The Menu label revision builds for Mac/iPad and passes the existing input
check. It is installed with a separate Validation-base031-menu import, whose only
changed ZIP entry is ui/gameplay/gameplay.yml. Physical save/reload acceptance is
recorded above. The original selected placeholder images remain intact.

Earlier physical follow-up (2026-09-10): the user reports the key at Escape's position
returns to iPad home. The standard HUD menu hit area exists but its button art is
only 1x1 placeholders in the selected 6UI skin, so visible menu/save entry remains
unaccepted. These are present assets, not missing files. The user then reported a
stall when pointer motion revealed Dock; dock-stall.png confirms a partial,
mis-sized Chinese blacksmith screen. The process subsequently exited with SIGKILL;
no usable debugger stack or corresponding new crash report was obtained. The
termination cause remains unconfirmed.

At the user's request, ApplePointer now owns iPad game-window pointer lock across
camera and menu modes, with a native menu cursor. WindowPointer separates logical
camera input from physical confinement for shared input and both world views.
Touch system gestures are unchanged. Inactive iOS loops skip rendering and UIKit
owns window sizing. Lock request and actual UIPointerLockState are logged only
with performance tracing. System granting/revoking of lock must be verified.

The signed Release builds and installs; Mac build and the existing input check
pass. Automatic launch was refused because the iPad was locked. The user was
asked to unlock it; new pointer behavior, cursor click alignment, touch Dock and
resume are pending physical retest. See pointer-*.log and result.json in
../local-data/validation/ipad-gameplay-031/. Overall remains 35%–40%.

Earlier physical-device validation checkpoint (2026-09-10): the user reconnected
an M5 11-inch iPad Pro running iPadOS 27 Beta build 24A5408d, with external keyboard
and pointer. The current Release app is signed and installed. A regenerated Xcode
project rebuild resolved stale build-graph missing-object link errors without a
source patch. Four runtime packages from gameplay-base-031 pass ZIP integrity and
host shared player/dialog checks and import into a separate iPad record.

The real GameApplication renders the OUTE3 town entrance, buildings, decorations
and HUD through Metal. This uses a preseeded test party, not character creation.
Map entry sets QBits 1104/1105. The user's keyboard input moves the party from
(-9728,-11319,161) to (-9728,-9819,161); R opens rest. The user confirms pointer
activation of Wait 5 Minutes. Screenshots show 9:20 then 9:30; total click count is
pending clarification, so exactly-one-action-per-click is not yet accepted.

The user has been asked to create a Chinese-named player save. Save/reload,
tavern/media/quest interaction, touch and mouse camera, turn-based controls and
lifecycle tests are still pending. Known missing sky/player/item art and English
rest labels remain. This is partial physical acceptance, not a full playthrough
or raw-folder-to-runtime import acceptance. See
../local-data/validation/ipad-gameplay-031/. Overall stays at a low-confidence
35%–40% pending the remaining checks. The earlier pause below is historical.

# Apple port development status

Current offline checkpoint (2026-09-10): original video/NPC media is integrated
into `gameplay-base-031`. Native import preserves 297 SMK/BIK clips and decodes
1,646 original NPC portraits with explicit world/base ownership. Every video
byte and all 7,568,012 portrait RGB pixels independently match the source.
Static FFmpeg builds for Mac/iPadOS now decode Smacker and Bink video/audio.
Shared loop timing trims Smacker's excess audio tail and pads short tracks;
original SMK/BIK and existing OGV playback checks pass.

The formal Mac Metal window shows the tavern animation, NPC portraits, Chinese
names/topics and Andover's dialogue body. It entered using debug `event 11`, not
town navigation. External input occurred, so no controlled GUI reward claim is
made. Single-click inconsistencies and initial background-only startup remain
open. Of 392 house video references, 268 resolve; MM8 VID archives are absent.
All 163 proprietor portrait references resolve. This is not player face art.

Mac/iPadOS builds and 557 resource checks pass. Two optional generated-OGV tests
skip for unavailable desktop encoders; three independent OGV runtime checks pass.
See `MEDIA_IMPORT_PROFILE.md` and `media-gameplay-001/result.json`. No iPad was
connected or updated. User requested a pause after this section: development is
paused here. Overall remains a low-confidence **35%–40%**.

Previous offline checkpoint (2026-09-10): house presentation import now joins all
1,561 active houses, 488 movie records and 877 resident links into
`gameplay-base-030`. Original global IDs and Chinese house names survive. The
source merge uses a ten-ID house sound stride; explicit sound bases now avoid
the original engine's hundred-ID fallback. All greeting IDs resolve in the
catalog, while their actual audio remains unavailable.

The real imported game tables and OUTE3 scripts pass a shared dialog-controller
check: enter tavern 260, select Andover 786, show the letter via topic 1314, earn
1,000 gold, clear QBit 1105, set 1106 and replace the topic with 1315. The original
event retains the letter. Missing-letter, repeat conversation and native save/load
cases confirm that money is awarded only through the valid conversation once.
This uses an existing test world runtime, not rendered/physical tavern navigation.
Its saved fixture covers party/NPC state, not a full outdoor player save.

See `SERVICE_IMPORT_PROFILE.md`, `house-source-comparison.json` and
`house-quest-check-001.json` under `../local-data/validation/`. Source MM6/MM7 VID
archives and NPC portraits exist and still need native presentation integration.
House screens, shops, full battles, cross-world and iPad tests remain incomplete.
All 540 resource/import checks and final Mac/iPadOS builds pass; stronger raw
animation-ID validation leaves the three exported presentation files byte-identical.
Overall remains a low-confidence **35%–40%** estimate; no iPad was updated.

Previous offline checkpoint (2026-09-10): native binary EVT/STR import now supplies
shared Global and OUTE3 events to `gameplay-base-029`. In the formal Mac Metal
game, actual saves confirm that map entry sets QBits 1104/1105 and grants exactly
one letter 2125. Reloading and saving again preserves quest bits and inventory
without a duplicate letter. The window exited normally.

The native event runtime binds all 1,382 generated event handlers plus 157 topic
conditions. Focused execution checks cover starting-quest idempotence and the
opcode-68 roster/ability condition; the exporter previously emitted a follower
lookup for that opcode. Import validation now rejects truncated instruction
payloads and unterminated inline strings. Re-exported real event files remain
byte-identical after validation. Mac tools and the iPadOS converter library/app
build pass. No iPad was updated; the converter is not yet wired into its import UI.

See `EVENT_IMPORT_PROFILE.md` for source ownership and evidence. This is only the
initial quest event: full NPC/quest loops, custom Lua overlays and three global
riddles with unresolved answer strings remain incomplete. Whole-map import stops
at the custom Lua map Breach, which lacks a corresponding EVT in these archives.
The source Lua exists and needs porting. Overall remains **35%–40%**, low
confidence; passing current functionality on iPad would support roughly 40%–45%.

Previous offline checkpoint (2026-09-10): short key taps now reach shared rest,
always-run and combat hotkeys, indoor/outdoor movement and outdoor keyboard view
controls. Raw held/pressed states remain separate. Turn-based movement accepts a
tap only during its movement phase and still spends one action step; wait phase
and exhausted action points clear both held and pressed movement state.

Captured mouse motion reaches the camera only while its window has input focus.
Absolute/menu motion is drained without forwarding it. Focus changes clear old
relative motion; focus loss also clears pending keyboard taps, text and clicks.
Touch camera movement remains independent. The input check covers real rest and
hotkey controllers, Return/keypad Enter, blocked actions, five turn-based steps,
exhausted action points, focus cancellation and the previous touch/menu/UTF-8 cases.
Its first run found an unbound settings pointer in the new test fixture; binding
the existing settings fixed that test setup, and the check now passes.

Formal Mac UI evidence: ten forward taps moved the actual saved party from
(-9728, -11319, 161) to (-9728, -11187, 161). Native save readback confirms unchanged
camera, gold, food, member count and quest bits after rest/combat/menu operations.
Rest opened with R and its five-minute wait changed the display from 9:34 to 9:39.
Return entered/exited turn-based mode, which correctly refused rest. Keyboard
view controls and one mouse drag visibly changed the camera. Later external UI
interaction/focus changes limit the mouse comparison; this is not full capture
or lifecycle acceptance. The window exited normally.

Evidence: `../local-data/validation/movement-gameplay-001/` (real saves, native
readback, screenshots and `result.json`), `movement-input-check.log`,
`movement-input-mac-build.log` and `movement-input-ios-build.log`. Input check and
Mac/iPadOS builds pass. No resource data changed; the prior 515 resource checks
remain the latest full resource suite. Base remains `gameplay-base-028`.

English dynamic rest/combat labels, missing art/audio, full encounters, indoor
playthrough and iPad input/lifecycle remain outstanding. The earlier intermittent
background-only startup has not been conclusively reproduced/resolved. Overall
remains a low-confidence **35%–40%** estimate; no iPad was updated.

Previous offline checkpoint (2026-09-10): the formal Mac game now creates player
saves through the Chinese menu, displays a saved scene preview, and restores a
save from a previous process. Native readback of a subsequent UI-created save
matches map, position, camera, gold, food, party count and 18 quest bits. This
checks those fields, not all serialized gameplay state. Resource base remains
`gameplay-base-028`; all 515 resource checks pass.

Shared menu/HUD rendering now uses the existing Apple CoreText path for valid
multibyte UTF-8, with matching measurement, CJK wrapping and truncation. A bounded
text texture cache is shared by the two renderers; original bitmap rendering is
retained for ASCII and legacy single-byte text. The system text input lifecycle
now also covers Mac/iPad save editing. Save labels accept 30 Unicode code points,
reject malformed UTF-8/control characters and erase complete code points. This
is not full grapheme-cluster editing. Command-V pastes into active gameplay text
fields on Apple; other platforms use Control-V. New-character and event-prompt
editors still need the same Unicode editing review.

Actual Mac editing has displayed `中文甲`, erased it to `中文`, appended
`存档验证`, and committed `中文存档验证` with Return. The production save reader
confirms the exact UTF-8 name. The UI tool reports a clipboard-read timeout even
when the paste succeeds; inspect the resulting screen/file before retrying.
Direct simulated Chinese typing generated no SDL text events in this check, so
native IME composition/candidate-window behavior remains unvalidated.

Mouse button down/up edges now survive between frames with their event positions;
focus loss, blocking and screen transitions cancel pending clicks. High-DPI cursor
warps use window points. Mac builds now use the pinned static SDL 3.4.2 with
`cmake/patches/sdl-cocoa-button-position.patch`, which takes absolute button
coordinates from the Cocoa event even without preceding mouse motion. Relative
aiming retains SDL's captured cursor behavior. System SDL builds do not receive
this patch. Source patch application is checked and idempotent.

Evidence is under `../local-data/validation/mouse-input-002/`,
`save-chinese-001/restore-comparison.json`, `save-chinese-002/`,
`save-text-trace-002/` and `save-chinese-003/`. The clean production binary in
`save-chinese-003` displays `中文存档验证`, its preview, and loads the saved town
scene after process restart. Mac and iPadOS clean builds both pass. Trace bundles retain diagnostic evidence; temporary trace
statements are removed from the final source. Input/text checks and Apple builds
are recorded in `save-chinese-*-check.log` and `save-chinese-clean-*-build.log`.
Full gameplay, iPad input, map transitions and stability remain unaccepted. One
Mac run initially showed only the background, then recovered after loading a
save; startup camera/input needs investigation. No iPad was updated. Overall
remains a low-confidence 35%–40% estimate.

Previous offline checkpoint (2026-09-09): `gameplay-base-028` imports all 673
DecList records and 23,390 SFT frames, plus 53 original decoration sprites and
five palettes for New Sorpigal. The formal Mac game now visibly starts at the town
entrance with trees, flowers, fountain and MM6 HUD. Shared legacy orientation now
converts 2048-unit turns and the degree fallback correctly. All 515 resource
checks and Apple builds pass. Escape opens/closes the Chinese game menu; quick
mouse clicks still need event delivery before player save/load can be tested.
No iPad was updated and full gameplay remains unaccepted. See
`DECORATION_IMPORT_PROFILE.md` and the current screenshots/results under
`../local-data/validation/gameplay-app-start-028/`. Overall remains approximately
35%–40%, with low confidence.

Previous offline checkpoint (2026-09-09): `gameplay-base-027` adds 26 authored
gameplay layouts, 256 original interface images and seven original bitmap fonts.
The formal Mac application now visibly renders the MM6 standard HUD, bars and
food/gold digits. All 495 resource checks and Apple builds pass; the iPad bundle
includes the authored layouts. Complete gameplay remains unaccepted: the missing
decoration registry prevents entrance lookup, portraits and Chinese text need
work, and Escape/menu response has not been verified. See `UI_IMPORT_PROFILE.md`.

Previous offline checkpoint (2026-09-09): `gameplay-base-026` adds 87 speech event
bindings and shared group-before-variant voice selection. All 480 resource
checks and Apple builds pass. Actual GameApplication now reaches its Metal main
loop and visibly renders terrain, verified with the production binary in a Mac
app bundle. Complete startup/playability is still unaccepted: HUD layouts are
absent, the initial position needs checking, and sky/actor art and audio payloads
remain incomplete. See `SPEECH_IMPORT_PROFILE.md` and
`../local-data/validation/gameplay-app-start-026/`. No iPad was updated.

Previous offline checkpoint (2026-09-09): `gameplay-base-025` adds all 8,546 original
sound catalog records. Explicit ordered MM8 mode keeps first-occurrence lookup
for duplicate IDs, recognizes the original 643 spatial flags, and retains sound
load types. All 464 resource checks and Apple builds pass. Actual application
startup now reports only missing `character_speech_events.txt`; sound samples,
speech-group mapping and playback remain unvalidated. See `SOUND_IMPORT_PROFILE.md`.

Previous offline checkpoint (2026-09-09): `gameplay-base-024` adds all 102 original
Arcomage cards, Chinese card art and the shared table background. Explicit binary
card mode fixes wall-swap and conditional-card interpretation while retaining
legacy table behavior. All 448 resource checks and Apple builds pass; 53 automatic
tavern matches finish. The requested MM6 full-map diagnostic now passes, with
missing `m529` art, but actual GameApplication startup still fails because
`sounds.txt` and `character_speech_events.txt` are absent. See
`ARCOMAGE_IMPORT_PROFILE.md`; rendered gameplay and iPad input remain unaccepted.

Previous offline checkpoint (2026-09-09): `gameplay-base-023` adds nine continent,
house and travel rule tables. Production application generates 126 transport
routes and eight extra house exits. Later duplicate house definitions now win,
and mapped town portals use localized map names. All 427 resource tests and Apple
builds pass. Formal startup next requires `arcomage_cards.txt`. See
`WORLD_RULE_IMPORT_PROFILE.md`; actual travel, arrival-side parity and full
gameplay remain unvalidated.

Previous offline checkpoint (2026-09-09): `gameplay-base-022` adds both equipment
picture tables, covering 157 items, six doll types and separate cloak collar
coordinates. Shared HUD rectangles apply the original portrait/item override;
four lowercase source types now reach canonical equipment/enchantment branches.
All 404 resource tests and Apple builds pass. Formal startup next requires
`continent_settings.txt`. See `EQUIPMENT_PICTURE_IMPORT_PROFILE.md`; this stage
does not export images or validate final paperdoll layering/alignment.

Previous offline checkpoint (2026-09-09): `gameplay-base-021` adds the 79 character
voice sets and six availability categories. Shared candidate selection uses
original face/race/sex priorities. All 388 resource tests and Apple builds pass;
formal startup next requires `complex_item_picture_offsets.txt`. The supplied
original directory and native import have 49 MP3 files but no SND/WAV audio.
See `VOICE_IMPORT_PROFILE.md`; playback and rendered selection are unvalidated.

Previous offline checkpoint (2026-09-09): `gameplay-base-020` adds the three bolster
tables. Map continents and existing monster kind flags are applied; 11 formulas
are preserved and syntax-checked, but the current combat runtime does not consume
them. All 372 resource tests and Apple builds pass. Formal startup next requires
`character_voices.txt`. See `BOLSTER_IMPORT_PROFILE.md`; original bolstering parity
and complete gameplay remain unvalidated.

Previous offline checkpoint (2026-09-09): `gameplay-base-019` adds monster portrait
associations, 43 reagent recipes and three UI metadata sets. All 86 directional
reagent mixing checks and 357 resource tests pass, as do Apple builds. Formal
startup next requires `bolster_formulas.txt`. See `SUPPORT_IMPORT_PROFILE.md`;
this phase exports no portrait/skin art and does not accept full gameplay.

Previous offline checkpoint (2026-09-09): `gameplay-base-018` adds six NPC rule
tables and 77 Chinese profession names. Explicit NPCText news references survive
actor resolution and dialogue rebuilding without same-ID collisions. All 342
resource tests and Apple builds pass. Formal startup next requires
`monster_portraits.txt`. See `NPC_IMPORT_PROFILE.md`; full NPC interactions,
original news selection parity and complete gameplay remain unvalidated.

Previous offline checkpoint (2026-09-09): `gameplay-base-017` adds 178 teacher
topics and 253 NPC/journal bindings. Complete imported definitions no longer
reactivate removed legacy topics; two stale note bindings remain recorded.
All 326 resource tests, production paid-training checks and Apple builds pass. Formal startup now
requires `npc_professions.txt`. See `TEACHER_IMPORT_PROFILE.md`; rendered training,
note events and full gameplay remain unvalidated.

Previous offline checkpoint (2026-09-09): `gameplay-base-016` adds 77 character
portraits and six doll types as metadata. Shared loaders preserve original layout
and IDs; Party recruitment retains race/sex and identity across shared portraits.
All 305 resource tests and Apple builds pass. Required startup now stops at
`teacher_topics.txt`. See `DOLL_IMPORT_PROFILE.md`; doll images, rendering and
complete gameplay remain unvalidated.


Previous offline checkpoint (2026-09-09): `gameplay-base-015` adds 50 roster
entries, preserving Chinese text, merged class IDs and original skill/equipment
semantics. The resource suite passes 286 tests. Formal startup now stops at
`engine/data_tables/character_data.txt`; portrait/doll metadata is next.
See `ROSTER_IMPORT_PROFILE.md`. Complete gameplay remains unvalidated.


Earlier offline checkpoint (2026-09-09): `gameplay-base-014` adds 164 transition
texts, including 161 Chinese descriptions. The original notes remain metadata;
three newer English tunnel descriptions survive the older translation package.
The required startup blocker is now `engine/data_tables/roster.txt` (optional
house animations also remain missing). See `TRANSITION_IMPORT_PROFILE.md`.


Baseline: `pjasicek/openyamm` commit `db142fb3f38fb59be7f5a50a393dbe4377772bbe`.
Development branch: `codex/ipados`. This is an experimental port, not a playable iPad release.

## Verified platforms (2026-09-07)

- Native engine and desktop game compile and link with CMake/Ninja.
- bgfx's pinned native shader compiler builds with its official make target.
- Runtime shaders compile to Metal; SDL supplies a CAMetalLayer to bgfx.
- `openyamm_metal_smoke` renders a colored triangle through the actual engine context,
  reads back the Metal framebuffer and writes a PPM image (1280 x 960 in the first run).
- SDL absolute pointer coordinates are scaled from window points to drawable pixels.

- The full iOS arm64 engine, game library and UIKit app compile and link using
  Xcode 27 beta 6; device and simulator use separate builds and SDL 3.4.2.
- On the iPad Pro 13-inch (M5) iOS 27 simulator, the app installs and launches.
  `--metal-smoke` renders and reads back a 2752 x 2064 framebuffer, then returns
  to the Chinese launcher with a successful-render status.
- The Xcode Release device build completed with an Apple Development signature
  and installed successfully on the user's iPad Pro 11-inch (M5), iOS 27 build
  24A5408d. On the physical device, SDL/bgfx/Metal renders and reads back a
  2420 x 1668 color triangle, then returns to the Chinese launcher. Both the GPU
  readback and the launcher screenshot were visually inspected.
- The first unlocked device run exposed an upstream bgfx macOS-only Metal selector
  called on iOS. The reproducible patch under `cmake/patches/` corrects its platform
  guard; the device rendering result above was obtained after that correction.
- The native resource importer passes seven focused tests, including failed-import
  preservation, overlapping directories, symlinks and path aliases. It also copied
  the supplied installation: 376 files, 1,301,869,280 bytes.
- The same 376-file source import also completed on the physical iPad. A native
  C++ LOD decoder then converted the 13 original title/button images on the iPad,
  selected the Chinese overlay, and displayed them through OpenYAMM's own
  MainMenuScreen and Metal renderer. The 2420 x 1668 readback shows the Chinese
  title and buttons. Its conversion manifest matches the Mac output.
- The menu converter passes ten synthetic-archive tests (palette order, row padding,
  compressed/uncompressed images, both LOD header layouts, overlay priority,
  damaged archives and preservation of originals). Combined importer tests: 32 passed.
- Existing upstream scene exporters parsed 207 map pairs from the audit: 44 outdoor
  and 163 indoor candidates, zero errors. YAML syntax, geometry pairing and hashes
  were verified. These are candidate scenes with legacy IDs, not integrated worlds.
- The native outdoor converter now exports the actual MM6 `oute3.odm` geometry,
  199 terrain/building/sky images and the global 882-row terrain table. The Mac
  Metal preview displays New Sorpigal, walks 1536 world units on the ground, and
  verifies repeated blocking contact with building 14, face 27. Original sky
  `6sky05` is selected explicitly for this diagnostic; weather is not implemented.
- The updated signed app is installed on the physical M5 iPad. Its own native
  conversion manifest matches the Mac output exactly (199 images, 85 buildings).
  Metal readbacks at 2420 x 1668 show the town and sky before/after movement;
  the same 1536-unit ground route and building 14/face 27 collision probe pass.
  The before/after captures were retrieved and the final image visually inspected.
- Combined resource tests: 48 passed. The SDL input check also passes movement,
  simultaneous camera touch, cancellation, input blocking, menu taps and keyboard
  event handling. These are synthetic events, not physical accessory acceptance.
- Early render-setup failure now releases app resources before bgfx and drains
  queued uploads before shutdown. This fixes a reproduced Metal encoder assertion
  when an exception occurred before the first rendered frame.
- Actor presentation is now converted natively from the supplied resources:
  43 authored companion actors, seven monster types, 49 animation groups,
  174 indexed sprite images and seven palettes. The Mac preview reports all 43
  textured, zero missing, and visibly shows the original residents and guards.
- The final signed app is installed on the user's iPad. Its actor conversion
  manifest equals the Mac output, and retrieved 2420 x 1668 Metal readbacks show
  residents, guards and Chinese text. The 1536-unit ground route, wall probe and
  Unicode draw check pass. CoreText font lookup performance notes were resolved
  by using the reported PostScript font name and caching fixed HUD line breaks.
- The shared Apple UI text path uses CoreText for UTF-8 shaping and system font
  fallback, then bgfx/Metal for drawing. Chinese scene labels and controls render
  as text rather than prepainted menu art. Headless checks cover CJK line breaks,
  composed characters, supplementary code points, emoji sequences, truncation,
  invalid UTF-8 and raster output. This does not translate or migrate gameplay UI.

The macOS check used macOS 26.6.2, arm64, system SDL 3.4.14.
These checks establish the basic Apple rendering path. They do not establish
complete gameplay, Chinese dialogue/journal integration or physical input compatibility.

## Latest: original spell and face associations (2026-09-09)

The native `fx_import` exports 37 spell FX associations and 101 face events with
146 ordered expression choices. It checks the supplied MM8 executable fingerprint,
reads its data without execution, preserves source face labels and current Chinese
spell names, and validates production table lookups. LookDown event 64 references
missing PFT 32 and is recorded as unsupported; eight empty-expression slots stay
inactive. The 47 sound slots and automatic face reactions remain pending.
See `FX_IMPORT_PROFILE.md`.

The file importer now keeps only the root MM8 executable as nonexecutable data
(0644), with all setup programs, DLLs and nested EXEs still excluded. A fresh
377-file native import passes byte/permission checks. Outputs are in
`../local-data/fx-tables-002/`, combined assets in `../local-data/gameplay-base-013/`.
Independent binary decision/data analysis confirms the associations. All 242
resource tests pass. The Mac game and converters link, and the FX library plus
modified iPadOS app build for arm64; no disconnected device was updated.

Shared events now retain Set/Add/Subtract identity and sequence. Seven event kinds
have 28 validated action bindings; direct gains default to Add as an explicit
profile policy. Native checks cover Set/Add/Set, negative Add, Subtract, unchanged
arithmetic, member targeting and legacy table behavior. Full original trigger and
automatic face-reaction parity remain unaccepted.

Formal startup passes all three FX/face tables, then stops at required
`engine/data_tables/english/trans.txt`.
Optional house animations remain missing. Original runtime-patch parity, art,
complete startup and the playable loop are unaccepted. Overall progress remains
a low-confidence estimate of 35%–40%.

## Earlier: portrait and interface animation frames (2026-09-07)

The native `frame_import` exports the authoritative loose PFT/IFT overrides into
two shared tables: 76 portrait frames and 411 icon frames. It derives group totals
and end bits, preserves IDs and source blanks, and passes 1,306 frame-selection
checks through the production loaders. Original wall-clock timing parity and
character portrait artwork remain unverified. See `FRAME_IMPORT_PROFILE.md`.

Thirty-six available original icon images are exported to shared `engine/icons`;
all RGB pixels match independent source decoding. The manifest lists 371 missing
nonempty image references and three original blank cells. No substitute art or
animation aliases are invented. Spell and portrait FX bindings remain pending.

Latest output: `../local-data/frame-tables-001/`; combined assets:
`../local-data/gameplay-base-011/`. All 223 resource tests pass, including 20 frame
tests. The Mac converter links and the importer compiles for iPadOS arm64. Formal
startup passes both frame tables, then stops at required
`engine/data_tables/spell_fx.txt`; optional house animations are still missing.
No device app was updated; full startup and the playable loop remain unpassed.
The low-confidence overall estimate stays roughly 35%–40%.

## Earlier: readable items, potion data and invalid-result protection (2026-09-07)

The native `inventory_import` exports four shared tables: 245 scroll records,
4,761 ordered mixing cells, the corresponding note matrix with 202 active links,
and 71 potion settings. Of 194 nonempty scroll bodies, 193 use the supplied Chinese
translation; newer hint 1574 retains its base text because the old translation is
empty. All active bodies and Chinese item titles pass production item-use checks.

Five truncated metadata quotes in each scroll archive are handled by an explicit
profile without swallowing subsequent records. Split potion effect rows are
reconstructed. Settings and current item identity exclude the obsolete potion
operand 806, which is now Graceful Sword. One active recipe still outputs 806;
its original cell remains recorded, and the shared mixing runtime now rejects
unregistered results before consuming or changing inputs. A valid mix and this
rejected mix pass native runtime checks. See `INVENTORY_IMPORT_PROFILE.md`.

Latest output: `../local-data/inventory-tables-001/`; combined assets:
`../local-data/gameplay-base-010/`. All original fields/cells are independently
compared. All 203 resource tests pass, including 24 inventory tests. The Mac game
and converter link; the shared game change/importer compile for iPadOS arm64.
Formal startup passes scrolls and potion matrices, then stops at required
`engine/data_tables/portrait_frame_data.txt`; optional house animations are still
missing. No device app was updated. Full startup, rendered inventory/scroll UI,
alchemy effects and a playable loop remain unaccepted.

The low-confidence overall estimate remains roughly 35%–40%. This data phase does
not change a major acceptance milestone or establish an engineering completion
ratio. The earlier progress correction below still applies.

## Earlier: race data, selection and progress correction (2026-09-07)

Overall progress is a low-confidence estimate of roughly 35%–40%, unchanged by
this data phase. The earlier successive single-point estimates were subjective
and overstated precision; the previous weighted table is superseded by the
milestone evidence in `../OpenYAMM-iPadOS-开发进度.md`. Test/table counts do not
automatically increase progress. Formal startup and the first playable loop
remain unpassed, so this range cannot predict remaining time or usage.

The native `race_import` exports race starting stats, racial skill rules and
character selection to three shared files. It preserves 11 race IDs, 77 stat
records, 18 rules, 68 race/class pairs and four original continent lists. Explicit
stat-cost denominators preserve the original source parser's default of one.
Peasant remains selectable where authored; Between time metadata does not add a
fourth start button. See `RACE_IMPORT_PROFILE.md` for schemas and boundaries.

Production loaders check all 21,879 class/race/skill combinations under OpenYAMM's
existing three-field interpretation. An isolated Lua capture probe confirmed a
difference for nine original 0/1/0 rules; raw tokens are retained and
`legacy_lua_parity=false`. These tests do not establish original Windows gameplay
parity or a working character creation screen.

Latest tables: `../local-data/race-tables-002/`; combined root:
`../local-data/gameplay-base-009/`. Independent comparison verifies all stat
fields, rule tokens and selection lists against the original files. All 179
resource tests pass, including 20 new race tests; the converter links on Mac and
compiles for iPadOS arm64. Formal startup passes race starting stats and now stops
at required `engine/data_tables/english/scroll.txt`; optional house animations
remain missing. The later merged YAML phase has not been reached in full startup.
No iPad was connected or updated; runtime and character creation flags stay false.

## Earlier: Chinese character inspection and semantic lookup (2026-09-07)

The native `inspection_import` exports three shared tables for 26 attributes,
39 skills and 51 active classes. Chinese display names and descriptions are kept
separate from canonical gameplay keys. `CharacterInspectTable` supports optional
display-name columns and skips skill/class headers; legacy English tables retain
their previous display rules. Existing inspection overlays consume these titles.
The rest of the character UI still requires localization and rendering checks.

The supplied tables are positional, so the converter verifies full base and ZHCN
label order before overlaying text. It reconstructs the unquoted Peasant paragraph
without shifting class IDs. Class 47 retains key DarkArchMage and display name
死灵大法师; inactive slots 49/52 remain excluded. All prior class mechanics are
unchanged. See `INSPECTION_IMPORT_PROFILE.md` for schema and profile limitations.

Latest tables: `../local-data/inspection-tables-001/`; combined root:
`../local-data/gameplay-base-008/`. Independent source comparison verifies every
output cell. All 159 resource tests pass, including 21 new inspection tests; the
Mac game links and the shared loader/import library compile for iPadOS arm64.
Formal startup passes inspection loading and now stops at required
`engine/data_tables/class_starting_stats.txt`. Optional house animations remain
missing. Original translated prose has not been reconciled with current formulas.
No device was connected or updated, and the full gameplay/UI flags remain false.
The historical 39% estimate is superseded by the progress correction above.

## Earlier: class resources and skill identity (2026-09-07)

The native `class_import` converts four shared tables covering 51 active classes
and 39 skills. Source HP/SP values, explicit merged IDs and promotion metadata
remain intact. Source mana attribute tokens are mapped to the shared loader's
semantic names. Unused slots 49 and 52 are reported and excluded without shifting
High Priest 50 or Master Wizard 51. See `CLASS_IMPORT_PROFILE.md`.

Starting skills follow the original scripts' floor(classId/2) grouping. The
converter transposes all 1,989 cells and validates all 102 default assignments,
all class-pair promotion comparisons and higher-promotion skill caps. Production
base HP/SP formulas are checked for every class at levels 1,10,25. A source
ArchMage default Dark magic / zero-cap conflict is preserved and reported; race
overrides and actual character creation remain pending.

Class tables: `../local-data/class-tables-002/`; that milestone's combined root:
`../local-data/gameplay-base-007/`. Direct-source comparison verifies every active
numeric/cap/starting cell and original ID. Formal startup passes class loaders and
now fails at required `engine/data_tables/english/stats.txt`; optional house
animation metadata remains missing. All 138 resource tests pass, including 16 new
class tests, and the library compiles for iPadOS arm64 offline. No device was
connected or updated. At that milestone, estimated completion was about 38%, using the same
milestone weights in `../OpenYAMM-iPadOS-开发进度.md`. Chinese character inspection
and the complete gameplay loop remain unavailable; readiness flags remain false.

## Earlier: localized awards and journals (2026-09-07)

The native `journal_import` exports five shared tables: 104 character awards,
31 party award display aliases, 692 nonempty autonotes and 46 active MM7/MM8
history entries. Original IDs, priorities, categories and history time tokens
are retained. Chinese body/title text is merged by ID; inactive source slots
remain inactive. Strict paragraph reconstruction preserves the old unquoted
Chinese multiline format. See `JOURNAL_IMPORT_PROFILE.md` for the source contract.

Each party alias points to its original merged QBit or autonote. All 18 QBit-to-note
source associations are checked without rewriting quest data or assigning guessed
old award numbers. Production state checks verify showing/clearing all 135 award
entries and confirm that party aliases do not mutate character awards. These are
diagnostic state checks; actual quests and chapter unlocking remain unverified.
Five source Award-category notes retain the loader's existing Misc classification
and are explicitly reported as an inherited limitation.

Journal tables: `../local-data/journal-tables-002/`; that milestone's combined root:
`../local-data/gameplay-base-006/`. Formal startup passes the award and journal
loaders, then fails at required `engine/data_tables/class_multipliers.txt`.
Optional house animation metadata remains missing. Source comparison verifies
every output cell and all alias joins. All 122 resource tests pass, including
15 new journal tests; the native library compiles for iPadOS arm64 offline.
Evidence is under `../local-data/validation/`, including `gameplay-start-006/`.
No iPad was connected or updated. At that milestone, estimated completion was about 37%,
using the fixed milestone weights in `../OpenYAMM-iPadOS-开发进度.md`.
The full gameplay loop and Start Game remain unavailable.

## Earlier: chest geometry and localized houses (2026-09-07)

The new native `service_import` exports eight chest definitions and 1,561 active
house records. Chest storage remains 14×10, while the inspected MM8 visible grid
is 9×9. Grid offsets are converted from source screen coordinates to coordinates
relative to the background. The source profile and all eight rows were verified
against the owned executable's static arrays and chest descriptor cache without
running Windows code. See `SERVICE_IMPORT_PROFILE.md` for evidence and commands.

House names, proprietor names/titles and entrance text use the supplied Chinese
overlay by ID. All other fields remain unchanged, including service type names,
prices, hours, map and portrait IDs. Forty-six unnamed source rows remain inactive
and are reported; translated names cannot activate them or renumber later venues.

The converter also supports matching chest backgrounds with explicit archive
ownership, bounds checks and rollback. All eight unprefixed MM8 backgrounds are
missing in the supplied source; existing MM6/MM7 art is not substituted. No chest
images were produced by the real-source run, and actual opening/looting is not yet
available. House animations, residents, service rules and video remain pending.

Service tables: `../local-data/service-tables-002/`; that milestone's combined root:
`../local-data/gameplay-base-005/`. Formal startup passes chest/house base loading,
reports optional missing `house_animations.txt`, then fails at required
`engine/data_tables/english/awards.txt`. Logs are under
`../local-data/validation/gameplay-start-005/`. All 107 resource tests pass,
including 15 new service tests; iPadOS arm64 compilation succeeds offline.
No iPad was connected or updated. At that milestone, estimated completion was about 36%,
using the same milestone weights in `../OpenYAMM-iPadOS-开发进度.md`.

## Earlier: enchantment identity and localization (2026-09-07)

`combat_import` now emits six shared tables, including 25 standard and 73 special
enchants. The converter validates the MMerge positional profile, extracts only the
entry block, and keeps the original mechanical weights, price modifiers and rarity.
English source keys at standard column 16 and special column 20 keep effect identity
independent of localized labels. Legacy tables retain their existing interpretation.
Chinese special descriptions use the original visible bonus text; the language
pack's English design-note column is not presented as a Chinese translation.

Slots 25 (`Great might`) and 73 (`of Sparking`) remain explicit unsupported effects
with zero random generation weights. They are neither removed nor reassigned.
Treasure-level footer ranges are not imported. These remaining behavior gaps are
recorded in the manifest; see `COMBAT_IMPORT_PROFILE.md` for the complete schema.

The six-table combat output is `../local-data/combat-tables-003/`, and that milestone's
combined root is `../local-data/gameplay-base-004/`. Formal production startup reloaded
both enchantment tables and stops at `engine/data_tables/chest_data.txt`; evidence
is in `../local-data/validation/gameplay-start-004/`. The supplied `Chest.txt` has
eight types and five columns, lacking the runtime's grid offsets/dimensions.
Those fields need an authoritative source before a chest table can be promoted.

All 98 enchantment rows were compared with the supplied base/localized data;
the previous four combat outputs remain byte-identical. Nineteen combat/enchantment
tests and all 92 resource tests pass. The Mac game links with the shared loader fix,
and the converter plus game dependencies compile for iPadOS arm64. No physical
iPad was connected or updated. There is no playable equipment/chest/combat loop.

At that milestone, overall completion was approximately 35% against the full playable iPad objective,
using the weighted milestone baseline in `../OpenYAMM-iPadOS-开发进度.md`. This is a
rough progress estimate, not a success probability or time/cost forecast.

## Earlier native combat tables (2026-09-07)

`combat_import` now exports 14 monster missile bindings, 111 active spell records,
2,200 item records plus the original zero slot, and 2,200 random treasure weight
rows. The effective Spells2 mana/recovery/damage overrides and all item mechanics
are retained. Chinese spell/item display fields are imported without changing
semantic spell lookup: an appended `Base Name` field preserves the source name
used by monster descriptors and runtime rules. Existing English tables continue
to work. Original files and previous generated outputs remain unchanged.

The fixed MM8 spell object/sound metadata and patch-specific missile aliases were
checked by static inspection of the supplied files; neither EXE nor DLL was run.
The converter uses the verified metadata contract and does not require Windows
programs on iPad. See `COMBAT_IMPORT_PROFILE.md` for evidence and limits. It detects
stale core registries, preserves Dagger as a nonprojectile descriptor, reports 21
unused spell slots and 414 obsolete icon labels in the random-item table.

```sh
cmake --build build-apple --target combat_import openyamm
build-apple/combat_import /path/to/MMMerge /path/to/core-tables /path/to/new-combat-tables
python -m pytest tests/test_combat_resources.py -q
cmake --build build-ios --target openyamm_combat_import
```

This earlier output is `../local-data/combat-tables-002/`; its separate combined root
is `../local-data/gameplay-base-003/`. At that milestone, production startup loaded
maps, monsters, projectiles, objects, spells and items, stopping at
`engine/data_tables/standard_item_enchants.txt`. Its failure log is retained under
`../local-data/validation/gameplay-start-003/`. Source/output mechanical comparisons
are in `../local-data/validation/combat-source-comparison.json`.

All 239 active missile references and 222 resolvable monster spell references pass
data linkage checks. The shared engine still explicitly lacks day-o-gods, finger
of death, mass curse and paralyze for monsters; the manifest lists affected IDs.
Spell-specific behavior, art, audio, item icons/inventory footprints and interactive
combat are not established by this data conversion. `runtime_ready` and
`inventory_footprints_ready` remain false. Combined importer tests: 83 passed;
iPadOS arm64 library compilation passes. No device was required or updated.

## Earlier core tables and offline map validation (2026-09-07)

The Mac-only development round adds `core_import`, sharing the native legacy text
decoder with story conversion. It exports seven global tables and validates them
with production loaders: 207 maps, 652 monster stats/descriptors, 540 object
records, 45 additional death-drop rules, 162 unique names and the complete
218-by-218 hostile matrix. All source IDs, object-description order and mechanical
columns are retained. Map ownership is explicit, including unprefixed MM6 dungeons.
The original installation is read without changes.

The old Chinese monster overlay contains conflicting names at ID 651 and no name
at ID 652. Both keep base names; 650 unambiguous names are applied. Another 206 map
names are applied only where both map ID and filename agree. The manifest records
missing map geometry `zddb05.blv` (ID 193), blank object 414 sprite reference and
525 legacy tint channels narrowed to bytes. These are visible conversion findings,
not proof that every map and visual effect is supported. See the TXT inventory
for source contracts and remaining script/bolster/art limitations.

```sh
cmake --build build-apple --target core_import openyamm_core_check
build-apple/core_import /path/to/MMMerge /path/to/new-core-tables
# Make a separate combined root from existing outdoor/story outputs and overlay
# the new core engine directory. Never overlay the original installation.
build-apple/openyamm_core_check /path/to/combined-assets /path/to/new-check-directory
python -m pytest tests/test_core_resources.py -q
cmake --build build-ios --target openyamm_core_import
```

Actual outputs are `../local-data/core-tables-001/` and the combined diagnostic
root `../local-data/gameplay-base-002/`. `openyamm_core_check` reloads all seven
tables from disk and loads New Sorpigal with canonical ID `world.mm6.map.oute3`
(source ID 151). All 43 original companion actors resolve their full monster
appearance, stats and names; all object-description indices and additional-drop
rules round-trip through the typed loaders. Its report is under
`../local-data/validation/core-state-001/`. There is no renderer or interactive
game loop in this check.

The unmodified headless gameplay startup now passes maps and monster tables and
stops at `engine/data_tables/monster_projectiles.txt`. The failure is retained in
`../local-data/validation/gameplay-start-002/startup.log`; no fake mandatory tables
were introduced. Further dependencies still include items, spells, classes, party,
houses, auxiliary tables and exported events. Outputs remain `runtime_ready=false`.

Combined resource tests: 73 passed. The core import library also compiles for
iPadOS arm64 with Xcode's iOS SDK. No connected device was used, and no new app
was installed in this round. This advances data integration, not playable combat,
quests, cross-world travel or physical keyboard/mouse acceptance.

## Native story tables and earlier startup check (2026-09-07)

The native story converter now reads the supplied base and Chinese T archives,
restores GB2312 glyph runs to UTF-8, and overlays only display fields by existing
IDs. It emits seven shared NPC/journal tables: 1,224 NPCs, 332 greetings, 2,714
dialogue texts, 1,794 topics, 921 news records, 102 group associations and 1,719
QBit records (604 with nonempty quest text). All pass their production table
loaders. Source metadata and mechanical columns take precedence over translation
package differences. The full mapping is in the TXT integration inventory.

A shared runtime fix preserves source topic semantics independently of translated
labels. Localized roster recruitment and legacy English recruitment both pass
the native check. It also writes and rereads an isolated diagnostic save using
the production serializer, checking the QBit, journal text, Chinese save name and
an imported NPC identity. This is state serialization coverage, not a completed
gameplay or quest loop. The existing outdoor viewer does not use these story
tables yet, and the main Start button remains disabled for raw imports.

```sh
cmake --build build-apple --target story_import openyamm_story_check
build-apple/story_import /path/to/MMMerge /path/to/new-story-tables
build-apple/openyamm_story_check /path/to/new-story-tables /path/to/new-check-directory
python -m pytest tests/test_story_resources.py -q
```

The Release iPad app provides the developer launch argument `--story-check`. It
converts from the active owned source import on the device, writes to a fresh
`Documents/StoryValidation-UUID/assets` directory, runs the same checks under
`checks/`, reports its path to the console, and returns to the launcher. It never
overwrites an existing import or player save. The signed device build passes;
the physical M5 iPad reports 1,224 NPCs, 604 quest texts and successful localized
recruitment and save-state roundtrip checks. Combined resource tests: 59 passed.

The actual upstream headless gameplay loader was also run against a separate root
combining the outdoor and story outputs. That earlier root fails on
`engine/data_tables/map_stats.txt`, before loading a gameplay map. The retained
historical result is under `../local-data/validation/gameplay-start-001/`; the core
table round above advances this barrier to monster projectiles. No complete
playable loop has passed.

## Build

Install full Xcode and select it, or set `DEVELOPER_DIR` to its `Contents/Developer` directory.
CMake, Ninja, make and an internet connection for pinned source dependencies are required.

```sh
scripts/build_apple.sh macos
scripts/build_apple.sh simulator
scripts/build_apple.sh ios
```

`CMAKE=/path/to/cmake` and `OPENYAMM_BUILD_JOBS=6` may be set explicitly.
The host build directory is `build-apple`; simulator and device use separate directories.
Cross-platform builds restore the host's full CMake configuration after preparing
shaderc, so `menu_import` and `apple_import` remain available in `build-apple`.
The iOS development target is arm64 with deployment target 17.0. Code signing and
provisioning need a developer identity before installation on a device.

After the device dependencies are prepared, generate a project for automatic signing:

```sh
OPENYAMM_DEVELOPMENT_TEAM=YOUR_TEAM_ID scripts/configure_ipados_xcode.sh
```

Open `build-ios-xcode/OpenYAMM.xcodeproj`, select the `openyamm_ipados` scheme
and the connected iPad. The script keeps the development team in the CMake cache;
do not rely on manually editing generated project files. No team ID is committed.
The app bundle identifier defaults to `org.openyamm.ipados` and can be changed via
the `OPENYAMM_BUNDLE_IDENTIFIER` CMake cache variable.

With the iPad unlocked and connected, build, install, then run the rendering check:

```sh
xcodebuild -project build-ios-xcode/OpenYAMM.xcodeproj -scheme openyamm_ipados \
  -configuration Release -destination 'id=YOUR_DEVICE_UDID' \
  -allowProvisioningUpdates -allowProvisioningDeviceRegistration build
xcrun devicectl device install app --device YOUR_DEVICE_UDID \
  build-ios-xcode/ios/Release-iphoneos/openyamm_ipados.app
xcrun devicectl device process launch --device YOUR_DEVICE_UDID \
  --terminate-existing --console org.openyamm.ipados --metal-smoke
```

The console launch stays attached while the launcher is open. Retrieve the render
readback from the app data container's `Documents/metal-smoke.ppm` using
`devicectl device copy from` and visually inspect it. A successful install alone
does not establish that the rendering check passed.

For the macOS rendering check:

```sh
build-apple/openyamm_metal_smoke build-apple/metal-smoke.ppm
```

It requires a graphical macOS session. A successful exit checks initialization,
shader loading and framebuffer readback; visually inspect the triangle as well.

## Importing owned MMMerge resources

The initial Python 3.11+ importer audits an installed game directory and stages
legacy texts and optional maps. It never runs Windows executables or mod hooks.

```sh
python tools/mmerge_import/import_source.py /path/to/MMMerge /path/to/new-output --extract-maps
python -m pytest tests/test_mmerge_import.py -q
```

The output directory must be new and separate from the source. Archives are kept
in distinct namespaces, with bounds validation and source SHA256 hashes. GB2312
Chinese text is decoded and complete legacy glyph control sequences are restored
to Unicode. Remaining control codes are recorded for review.

The supplied MMMerge installation passed the first archive audit: 41 LOD archives,
470 archive texts, 414 map-related payloads, 57 loose texts, no reported errors;
498,598 Chinese glyphs restored. The 15 importer tests passed.

The later completeness check found missing MM8 base `Data/bitmaps.lod`,
`Data/sprites.lod` and `Data/icons.lod`. The supplied `mm8.bitmaps.lod` is a patch
containing one entry, not the original base texture archive. The first audit's
zero parse errors did not establish a complete game installation. Both the audit
and launcher now report these missing archives. The user has no additional MM8
original files; work continues on the available MM6 assets. Full MM8 support is
blocked on the missing base resources as well as the remaining integration work.

**This output is not an OpenYAMM runtime asset pack.** `manifest.json` intentionally
sets `runtime_ready` to false. Texture/sprite/audio conversion, map export and event
adaptation remain necessary. Canonical IDs and archive override order must be
validated before replacing any runtime tables. The language configuration targets
Merge 2020-08-16, older than the supplied 2023/2024 installation.

## Native title conversion and scene candidates

Build and exercise the C++ converter used by the iPad app:

```sh
cmake --build build-apple --target menu_import apple_import
build-apple/menu_import /path/to/MMMerge /path/to/new-menu-preview
python -m pytest tests/test_menu_resources.py tests/test_apple_resource_import.py tests/test_mmerge_import.py -q
python tools/mmerge_import/export_scenes.py /path/to/source-audit /path/to/new-scene-candidates
```

The title conversion reads `Data/mm8.D.lod`, `Data/mmmerge.D.lod`, then the supplied
`Data/10 LocZHCN.EnglishD.lod` overlay when present. This explicit priority applies
only to the 13 menu images; it is not a complete implementation of MMMerge mod
ordering. Wrapped PCX art is preserved; indexed art is decoded to BMP. The generated
layout uses the source title's original dimensions. `conversion.json` records each
asset's archive and dimensions and keeps `runtime_ready=false`.
Shared icon ownership and output naming are tracked in `MMERGE_ICON_OWNERSHIP.md`.

After importing raw resources in the app, choose the eight-second menu preview.
It converts in app-owned storage and uses the existing engine menu renderer without
starting a gameplay session. Menu actions return to the launcher; this is an art and
rendering check. In this phase Chinese text is part of the original bitmap artwork,
not proof of Unicode dialogue or journal rendering.

For development testing, resources can be copied to `Documents/MMMergeSource` in
the app's data container. Launch with `--import-source MMMergeSource --menu-preview`
to exercise the same native import implementation and menu conversion. The path
must be relative to Documents. `--menu-preview` alone reuses the active raw import.
The GPU readback is written to `Documents/menu-preview.ppm`.

Scene candidates stay separated by their source archive, including patch/breach
archives. No override or global ID remapping is inferred by the batch exporter.
It uses the upstream MMMerge-aware indoor format selection for MM6/7 companion
data. Scene parse success still requires in-engine geometry, collision, events and
texture checks before use in a gameplay package. This batch step currently runs
on the Mac. Menu conversion and the single-map outdoor conversion below also run
natively on iPad.

## New Sorpigal outdoor diagnostic

```sh
cmake --build build-apple --target outdoor_import sprite_import openyamm_outdoor_preview openyamm_input_check openyamm_text_check
build-apple/outdoor_import /path/to/MMMerge /path/to/new-outdoor-preview --actors
build-apple/openyamm_outdoor_preview /path/to/new-outdoor-preview /path/to/captures
build-apple/openyamm_input_check
build-apple/openyamm_text_check
python -m pytest tests/test_menu_resources.py tests/test_apple_resource_import.py tests/test_mmerge_import.py tests/test_outdoor_resources.py tests/test_sprite_resources.py -q
```

The C++ converter shares the validated LOD reader with menu conversion. It
validates the compressed map wrapper, retains original geometry, and decodes
indexed art with either one image plane or the four stored mip levels. It uses
the loose `Data/Tables/Tile3.txt` override, preserving row IDs and flags in engine
scope. See `MMERGE_TXT_TABLE_INTEGRATION_INVENTORY.md` for the table decision.
Image priority for this diagnostic is explicitly MM6, MM7, MM8, Merge, then the
`00 patch` bitmap archive; later selected entries override earlier ones.

`--actors` adds native actor presentation conversion; without it, the converter
still produces the earlier static surface diagnostic. The iPad always requests
actors. Loose `MonList.txt` and `SFT.txt` overrides are used instead of stale
archive binaries. The original companion bytes and actor positions are preserved;
display lookup uses the shared `monster_info_id`, with source MonList row + 1.
Seven descriptors and their original internal names remain explicitly preview
tables under engine scope; stats, translated names and actor AI are not promoted.

Sparse sprite rows are decoded to indexed 8-bit BMPs. Palette index zero remains
the transparency key; separate ACT palettes allow the existing runtime to retain
palette variants, directional frames and animation sequences. Sprite source order
is MM6, MM7, then Merge; palette source order is the bitmap order above.
The transaction rolls back the entire new scene output if actor conversion fails.
`actor_conversion.json` records image ownership and excluded functionality.

In the iPad launcher, choose “浏览新索匹格镇（60 秒）”. Left-side dragging moves,
right-side dragging looks around, and tapping the upper-left corner or Escape
returns. WASD and mouse/arrow input paths are connected; physical keyboard,
mouse and trackpad behavior still require user testing. The preview returns
automatically after 60 seconds. The preview uses Chinese UTF-8 labels and controls.

Development launch `--outdoor-check` reuses the active raw import, performs native
conversion, tests a real building face, captures frames before and after two
seconds of forward input, then returns to the launcher. Readbacks are
`Documents/outdoor-start.ppm` and `Documents/outdoor-end.ppm`. Use
`--outdoor-preview` for manual browsing. Generated assets are under Application
Support/OutdoorPreview; imported originals and existing saves are not modified.

The preview reuses the production map loader, outdoor renderer and movement
systems, with empty gameplay registries and no event execution. Actor presentation
is visible, but actor AI, decoration sprites, dialogue, combat, quests, saves and
audio are not connected. A single wall
probe and one ground route do not validate collision across every map. Output
continues to declare `runtime_ready=false`.
The displayed actors do not yet participate in AI, interactions or body collisions.
The current device image has a visible ground texture seam; terrain filtering
still needs inspection. Rendering success is not a claim of visual parity.

`engine/AppleText.mm` has no GPU dependency. `MenuScreenBase::drawUtf8Text` exposes
the Metal-backed text path to shared game UI, with an LRU texture cache limited to
64 entries and 16 MiB per screen. Text measurement, wrapping and truncation use
the same CoreText font and preserve composed character boundaries. Existing
256-glyph bitmap-font screens still need migration; the new preview HUD is not
the original game's full HUD. System glyph appearance can vary between OS versions.

## iPad work still required

The app now includes a Chinese UIKit launcher, a system folder picker, background
copying with security-scoped access, and app-owned versioned imports under
Application Support/GameImports. The active manifest is replaced atomically only
after an import succeeds. Only root `mm8.exe` is retained as non-executable data for the fingerprinted FX
profile; other Windows executables and DLLs are excluded. Original
files are copied without modification or execution.

Raw MMMerge folders are recognized by required LOD files and signatures. The launcher
keeps Start disabled for raw imports because conversion is still under development.
Folders containing engine.zip and worlds/mm6.zip, worlds/mm7.zip and worlds/mm8.zip are recognized as candidate
runtime packages; their contents have not been validated, and importing them does
not establish gameplay readiness. The native copy path has been tested on macOS;
the native import implementation also ran on the iPad through development launch
arguments. The system document-picker interaction still needs an end-to-end test.

The launcher also exposes a rendering check; `--metal-smoke` invokes it immediately
when launched through development tools. Its PPM readback is saved in Documents.
UIKit/SDL transitions currently emit appearance-transition warnings on both device and simulator;
lifecycle behavior needs further work before normal gameplay testing.

Remaining acceptance work includes complete scene content, foreground/background
lifecycle, asset conversion and picker testing, Unicode font
rendering and wrapping throughout gameplay, physical touch acceptance, physical
keyboard/mouse/trackpad behavior, and save/load and cross-world gameplay testing.
The intended first device is the user's iPad Pro M5 (16 GB), iPadOS 27 beta 3.
