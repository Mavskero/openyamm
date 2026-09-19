# Minimap arrows, navigation and package namespaces — checkpoint 052

2026-09-11. Mac development and real Metal application captures only. Generic unsigned iOS Release
compilation is included; no iPad query, connection, installation, launch or demonstration occurred.

## Resulting behavior

The MM6/MM7 minimaps now show their original eight-direction party arrows. SceneResources imports
6mapdir1–8 and 7mapdir1–8 without changing names or pixels. Each world's ui/minimap_arrows.txt declares
the correspondence between direction slots 1–8 and its original image filenames. These are presentation
bindings; shared gameplay registries remain engine-owned. Missing images and ambiguous owners stay
explicit in the scene conversion report.

GameplayUiRuntime reads only the active world's declared bindings. Declared filenames resolve under
that world's icons namespace, preventing a missing image from silently selecting another world's art.
Bindings are validated for schema, eight ordered slots, filename safety and size, and are cleared when
the asset world changes. A malformed declaration disables its arrows. Packages without any declaration
retain the earlier shared MAPDIR1–8 convention; a missing declared image does not use that convention.

The shared black-transparency rule now recognizes qualified filenames and the original 6mapdir/7mapdir
families. Their black background becomes transparent; all original RGB values and visible outline/body
pixels are retained. Camera octant calculation, map coordinates, zoom rules and movement physics are unchanged.

## Package mounting defect fixed

The new binding checks exposed a pre-existing packaged-content problem: mountPackagedAssetRoot called
PHYSFS_mount twice for the same ZIP filename, first at the root and then at worlds/<id>. PhysicsFS treats
an already-mounted filename as success without adding the second mount. The qualified world paths were
therefore missing. This also reproduced on the actual converted runtime, not only on synthetic fixtures.

AssetFileSystem now uses PhysicsFS's public mountIo API with a unique archive identity for an additional
namespace. Its independent read-only file stream supports bounded reads, seeks, length/tell, duplication
and owned cleanup. It streams the existing ZIP rather than loading or copying the whole archive into
memory. The root mount retains its previous search precedence, and the namespace accesses the correct
world even while another world is active.

## Acceptance evidence

- 11 headless binding tests pass. They cover valid world declarations, undeclared legacy packages,
  explicit missing art despite competing engine images, eight malformed metadata cases, out-of-range
  directions and mm6→mm7→mm6 cache invalidation. Compressed namespace streams also pass independent
  seek/read, EOF, active root precedence and inactive-world byte checks.
- Five isolated importer checks pass: original arrow ownership/names, existing-output protection,
  duplicate-source rejection, corrupt second-world arrow rolling back the whole fresh output, and a
  missing direction remaining explicitly absent. They use copied source payloads; user originals are untouched.
- 16 original arrow images / 784 RGB pixels match independent source decoding. The native HUD loader
  also checks original colors and black-key alpha across all directions during mm6→mm7→mm6 rebinding.
- Full native conversion with the corrected namespace implementation loads the existing 140 MM6/MM7
  scenes. Four packages contain 19,594 entries, all passing CRC. All 051 resource payloads are unchanged;
  only 16 original arrow BMPs and two world binding files are added. Final runtime-003 payloads also
  match the preceding runtime-002 that was used during intermediate development.
- Final Mac Metal runs each render 261 frames and save to a fresh test path. MM6 and MM7 each capture
  eight camera directions, default/half/double zoom, and a subsequent actual movement sample: 13 captures
  per world including the ordinary scene captures. Available-face characters are explicit visual fixtures.
- All 16 rendered arrow directions match the corresponding original arrow shape, including negative
  wrapped camera angles. Bright-body template similarity exceeds 0.9 and separates the next-best
  direction by more than 0.15 in every capture.
- Zoom button centers are exercised through the shared press/release controller. Repeated presses
  respect 0.5× and 2× limits, then return to 1×. Across default/half/double zoom and the moved state,
  512 sampled map pixels match bilinear sampling of the original map at the reported UVs; worst RGB
  channel difference is below 0.64 on a 0–255 scale.
- The normal outdoor movement/collision code moves the MM6 fixture 762 source units and the MM7 fixture
  66.5 units east. Minimap UV deltas agree with the actual party positions, and the captured map crop
  reflects the movement. This is a short movement check, not a route or campaign playthrough.
- Generic unsigned iOS Release compilation passes. Separate normal MM6/MM7 cold-load results are
  recorded in cold-mm6.log and cold-mm7.log; these use the unmodified earlier normal-start saves.

The first binding test run failed and led to the actual namespace fix. The first navigation capture also
exposed two diagnostic mistakes: comparing wrapped camera angles as plain numbers and requesting two
screenshots in one frame. Those checks were corrected without changing gameplay behavior. Captures made
before the black-transparency fix are retained as intermediate evidence, not accepted visual results.
Final accepted captures are mm6-navigation-002 and mm7-navigation-003.

## Remaining scope

No new physical keyboard, mouse, trackpad or touch acceptance is claimed. Direct HUD-controller clicks
and movement inputs do not validate SDL event delivery or iPad system gestures. Default/half/double zoom
and short movement on two outdoor maps do not validate edge clamping, Wizard Eye markers, exploration
fog, full indoor navigation, all maps or long sessions. Footstep files rungrass.wav/runstone.wav remain
unavailable in these runs; missing original/default MM8 art, broader audio, MM8/Breach content, complete
quests/travel, MMExtension behavior, import recovery/storage and sustained gameplay remain open.

Overall stays a low-confidence **40%–45% estimate**, not a measured completion rate.

Evidence: ../local-data/validation/minimap-navigation-052/result.json, binding-tests-accepted.log,
original-arrow-pixels.log, import-checks.json, package-validation.json, rendered-pixel-checks.json,
runtime-003.log, mm6-navigation-final.log, mm7-navigation-final.log and ios-release-accepted.log.
