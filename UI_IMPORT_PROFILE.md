# Gameplay UI import and packaging profile

Checkpoint 050 adds explicit source texture regions for the wide-HUD bar, end cap, status banner
and gold/food icons. The loader reads original image regions at runtime; no derived bitmap is bundled
or fabricated. Single/five-person Mac captures and nonoverlap checks pass after a shared wide-HUD
scale adjustment. Missing default/MM8 portraits and outdoor minimap imagery remain visible gaps.
See HUD_SOURCE_VALIDATION.md. Earlier counts below describe their original checkpoints.

`ui_import SOURCE GAMEPLAY_LAYOUT_DIRECTORY UI_SET_ID NEW_DESTINATION` combines
OpenYAMM's authored gameplay layouts with the user's original interface images
and bitmap fonts. The native converter builds for Mac and iPadOS. It does not
read Python audit output or run the original Windows program.

The current MM6 checkpoint selects set 3 from `Data/Tables/Additional UI.txt`:
the original author's `6UI.icons.lod` and `6UI.EnglishD.lod` skin. This is an
explicit selection for this runtime, not a claim that the old installation's
saved interface selection was recovered. Set 1 is the default and set 2 names
the supplied MM7 skin; their source selection is tested, but their rendered
layouts have not been accepted.

## Authored layouts and resource lookup

The converter loads each authored YAML with the production UiLayoutManager,
preserves its bytes, and gathers image/font references from the parsed elements. Declared
texture_regions contribute original source images; their virtual names are not missing image files.
Regions must fit the declared source shape, and present imported sources must match it. Region chains
and duplicate names fail. The conversion report lists the source, rectangle and availability.
All 26 gameplay layouts are written under `engine/ui/gameplay`. The existing
converted Chinese main-menu layout is retained separately. The iPad application
now bundles these 26 authored files at `runtime/gameplay_layouts`, with CMake
tracking them as bundle resources. Original game art is still imported by the
user; it is not added to the application bundle.

Icon and language-image namespaces are resolved independently. An unresolved
collision between the two namespaces fails conversion. Within each namespace,
this explicit import profile applies later layers last:

- Icons: optional `icons.lod`, `mmmerge.icons.lod`, `00 add.icons.lod`,
  `00 patch.icons.lod`, `icons.UI.lod`, `z10 LocZHCN.icons.lod`, then the selected
  Additional UI icon archive. `icons.UI.lod` supplies available interface art
  referenced by OpenYAMM's layouts; the original DLL's UILayout code is not run.
- Language images: `mm8.D.lod`, `mmmerge.D.lod`, optional Chinese EnglishD,
  then the selected Additional UI language-image archive.
- Fonts: `mmmerge.T.lod`, then optional Chinese EnglishT. Font payloads remain
  byte-identical and are placed at the existing `engine/icons/*.fnt` lookup path.

Conditional skin archives must exist when selected. Missing individual image or
font references are recorded without placeholders. A present corrupt image,
invalid layout, unsafe path, or escaped symlink fails and rolls back the fresh
output. Output cannot overwrite or sit inside either input root. Names stay
lowercase and files use mode 0644.

When the authored set includes `town_portal.yml`, the converter also validates
the supplied `Data/Tables/TownPortalSwitch.txt` TPGlobal profile and imports its
dynamic Dimension Door art: `TPGlobal`, `TPJadam1` through `TPJadam8`,
`TPAntag1` through `TPAntag8`, and `TPEnroth1` through `TPEnroth5`. These 22
names are selected at runtime and therefore do not appear as direct YAML asset
references. They use the same icon archive precedence above. Missing members
remain explicit in `missing_images` and the narrower
`missing_dimension_door_images`; no placeholder is generated. If the authored
layouts omit `town_portal.yml`, this profile and its source table are not
required.

Dynamic portrait, paperdoll, inventory, spellbook and generated mobile/control
imagery still need their own source mappings. Original Additional UI
indicator/ring metadata was preserved by the support-table stage; this
converter does not apply new coordinate or layering overrides. It does not
claim general DLL UI patch/order equivalence.

## Verification checkpoint, 2026-09-09

- `../local-data/ui-resources-001`, assembled into `gameplay-base-027`.
- 26 layouts, 826 parsed elements, 575 distinct image references and seven
  font references. All layouts also match the built iPad application bundle.
- 256 original images imported. Independent Pillow decoding matches all
  8,766,844 RGB pixels; all seven font payloads, totaling 313,767 bytes, match
  their source archives. All exported images pass native engine decoding.
- 319 image references remain absent, including missing base art and generated
  or mobile assets that are not raw LOD members. This is a reference inventory,
  not 319 errors encountered on the active standard HUD screen.
- 15 new tests and all 495 resource tests pass. Mac and iPadOS builds pass.
- The formal Mac application visibly displays the MM6 top/bottom frame,
  character strip, health/mana bars, and food/gold digits. The check explicitly
  uses the existing standard UI layout; the widescreen layout references
  additional cut/derived art that has not been imported.
- Full HUD and gameplay remain unaccepted: portraits are absent; UTF-8 Chinese
  text still needs the shared Unicode path; sending Escape through the UI tool
  did not visibly open a menu. This observation does not by itself distinguish
  tool delivery from an input/runtime fault. The window close button exits 0.
- Starting position remains at the map center: `decoration_data.txt` is absent,
  so `resolveOutdoorPartyStartPoint` returns before inspecting entry markers.
  The current explicit map override supplies no replacement coordinates. The
  decoration registry and actual entrance data must be integrated next.
- No iPad was connected, installed or exercised. Import orchestration and
  production launch from the iPad launcher remain unfinished.

Evidence: `../local-data/validation/ui-source-comparison.json`,
`gameplay-ui-source-research.json`, `resource-tests-ui.log`, `ui-mac-build.log`,
`ui-ios-build.log` and `gameplay-app-start-027/`, including `actual-hud.png`.
Overall remains a low-confidence 35%–40% estimate.

## Runtime text checkpoint, 2026-09-10

Physical testing found that the selected `6UI.icons.lod` contains `butn4u/d`
as 1x1 placeholder images. They are present, correctly decoded assets, not
unresolved references. The authored standard/widescreen options elements now
declare a `Menu` label, rendered by the shared HUD at the existing hit target.
No original image is replaced and the imported art/font ownership is unchanged.
The native converter preserves the updated YAML in `ui-resources-menu-002`;
the staged runtime package changes only `ui/gameplay/gameplay.yml`.
Mac/iPad builds and the existing input check pass; the app and a separate runtime
import are installed. The label is visible and has been used for physical
Chinese save/restart/load/resave, as recorded below.

The label is now confirmed visible on iPad, but its first finger-tap test failed:
the shared HUD input gate treated touch as a mouse pointer blocked by mouse-look.
GameplayInputFrame now retains touch pointer origin through press/release, and
the shared HUD/portrait gates admit touch independently of camera capture. Modal
blocks remain in force. The input check exercises the actual screen/HUD/menu
controllers with a touch tap, mouse-look mouse click and modal-blocked touch.
Mac/iPad builds and that regression pass. Initial physical taps after installation
still failed, while touching the skull panel opened its explanation. SDL touch
event recording then observed a down/up pair at normalized (0.823554, 0.0395683)
and the user confirmed a short tap opened Menu. The debugger was detached.
Subsequent Chinese save, whole-app restart/load and resave all succeeded without
a debugger. Actual device files preserve `真机验证` and `读档复验`, with matching
selected position, party, quest and inventory ID/quantity fields. This accepts
that save/load sequence, including the user's Chinese input, but does not explain
all earlier failed taps or establish broad Menu first-tap reliability. See
`../local-data/validation/ipad-gameplay-031/player-save/comparison.json` and
`window-recovery/menu-touch-event.json` under the same validation root.

The shared Mac/iPad menu and gameplay HUD paths now shape valid multibyte UTF-8
with CoreText and use matching Unicode measurements, wrapping and truncation.
Original imported bitmap fonts and images are unchanged. Actual save/load screens
display `新索匹格镇`; a Chinese save name has been pasted, edited with Backspace,
and saved by Return. This does not accept every dialogue/quest screen, IME
composition or mobile input. See the current `APPLE_PORT.md` checkpoint and
`../local-data/validation/save-text-trace-002/`.
