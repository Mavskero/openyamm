# Hardware-water import and Mac rendering — checkpoint 049

2026-09-11. No iPad queries, connections, installations or demonstrations.

Checkpoint 048 exposed magenta water along Emerald Island's shoreline. The native import retained
HW water textures.txt as metadata, but omitted the material definitions and animation images consumed
by the existing SurfaceMaterialTable/terrain-atlas renderer. Without the material, shoreline transition
textures were drawn without their water animation and transparency composition.

SurfaceResources now converts the six original software-texture/hardware-prefix bindings into
engine/rendering/surface_materials.yml. It reads only the literal HDWTRCount/HDWTRDelay assignments
in Scripts/General/MiscTweaks.lua, which explicitly override mm8.ini. This source specifies 14 frames
and a delay of 15. No Lua or Windows code is executed. The supported profile rejects unknown liquid
names, duplicate bindings, unsafe prefixes, absent/zero/excessive settings, duplicate assignments and
expressions instead of inventing values. This is not a general Lua evaluator or arbitrary-mod support.

The shared renderer already supports material animations and terrain transition composition, so no
new Metal shader or separate iPad water path is needed. Existing animation-clock units remain unchanged;
this test does not establish original-engine playback-speed parity. Exact source bindings are supported;
other script-driven/continent-specific water overrides and every animated face remain unverified.

SceneResources collects referenced hardware frames alongside TFT frames, uses normal archive precedence
(including 00 patch.bitmaps.lod), and stores images in their owning world's textures directory. Material
definitions remain engine-owned. Original raw tables stay intact. Missing images are still reported and
are not generated or substituted. A material definition does not resolve an absent base texture such as
the already-reported wtrtyl reference in one MM6 scene.

## Validation

- Ten new malformed-input cases plus material selection, lava semantics, repeated oil-prefix targets,
  first/last frame names, exact frame boundaries and wraparound pass through SurfaceMaterialTable.
  The existing terrain and TFT focused checks also pass.
- A fresh 31-stage direct native conversion loads all 140 MM6/MM7 scenes successfully. It adds one
  shared material file, 28 MM6 textures and 14 MM7 textures. All 17,083 ZIP entries pass CRC.
- New Sorpigal and Emerald Island each import their 14 named source-water frames. Archive ownership
  is recorded in the scene report. No artwork was authored to hide a source gap.
- Actual Mac Metal runs load the existing MM6 and MM7 normal-start saves, render approximately
  181 frames, capture frames 30/150 and save to separate new output directories.
- Emerald Island's captured water is blue and animated. In the same fixed 80-by-17-pixel water area,
  the prior magenta-pixel count drops from 179 to zero in both new captures; 1,354 of 1,360 pixels
  differ between the two animation captures. The whole-frame magenta count is not expected to be zero
  because flowers and other scene objects contain that color. This is a local visual check, not full
  map or campaign visual acceptance.
- MM6's captured town remains visible. Its initial camera does not expose a large water surface, so
  this is a scene-render regression, not physical acceptance of every MM6 water variant.
- MM7 separate-process reload preserves the checked normal-start state. Generic unsigned iOS Release
  compilation passes. The app's retained source copy contains the identical newly consumed Lua source;
  this checkpoint does not rerun full app-API publication or use a device.

The wide-HUD basebar and related authored assets remain absent in the current imported package.
No default-layout switch or substitute artwork masks this separate defect. Full quest/travel chains,
MM8/Breach scenes, missing original resources/audio, importer recovery/storage and sustained sessions
remain open. Overall estimate remains 40%–45%, low confidence and not a measured completion score.

Evidence: ../local-data/validation/hardware-water-049/result.json, runtime.log, surface-check.log,
package-validation.json, water-capture-comparison.json, mm7-metal-001/scene-first.png,
mm7-metal-001/scene-later.png, mm6-metal-001/scene-first.png, mm7-cold.log and ios-release.log.
