# Original outdoor minimaps — checkpoint 051

2026-09-11. Mac-only development and actual Metal application captures; generic unsigned iOS Release build.
No iPad queries, connection, installation, launch or demonstration.

## Change and ownership

The shared outdoor runtime already requests an image named after the map stem, and the shared HUD
already loads that image through the active world's icon directory. The scene importer had omitted these
dynamic references. SceneResources now collects the original map-stem images from the source icon LODs
and writes them under worlds/<registered-world>/icons. No renderer, world coordinate formula, map geometry,
source palette or gameplay rule is changed.

Sources are the explicit base/MM6/MM7/MM8/merged/patch icon archive set. An image with multiple owners is
rejected pending an explicit override profile; archive iteration order does not choose a winner. Missing
images are reported as minimap.available=false. Indoor entries have minimap=null and retain their vector
minimap behavior. Available images report their archive and dimensions, also joining image_owners.
Malformed image data removes the new staging directory, including maps already converted in that attempt.

This source contains all 28 requested outdoor images: 15 MM6 and 13 MM7. They are 512×512 original indexed
images. MM6 outc1 comes from mmmerge.icons.lod; other MM6 images come from mm6.icons.lod and MM7 images
come from mm7.icons.lod. No hand-drawn, generated or substitute map image is introduced.

## Evidence

- Full native import and gameplay map loading pass for the existing 140 MM6/MM7 scenes.
- All 7,340,032 imported map RGB pixels equal independently decoded original indices and palettes.
- Four package CRC checks pass for 19,576 entries. Every previous 050 resource payload is unchanged;
  the only additions are the 28 world-owned minimap BMPs.
- Five focused local checks pass: correct MM6/MM7 ownership with case-insensitive names; existing output
  preservation; rejection of duplicate owners; corrupt second image rolling back the already-converted
  first map; missing images remaining explicit without substitute output. These use isolated copies of
  real source map/table payloads with controlled icon archive variants; user source files are never edited.
- Actual Mac GameApplication/Metal captures run 181 frames each for MM6 oute3 and MM7 7out01. At frames
  30 and 150, the expected map-stem texture loads through the shared HUD, at 512×512; screenshots show
  the map image within the existing lower-right frame. One- and five-person explicit portrait fixtures
  retain the 050 geometry checks. These are visual checks, not a multi-character campaign or input test.
- Generic iOS Release compilation passes with CODE_SIGNING_ALLOWED=NO. The build is not installed.

## Remaining limits

The map image is restored, but the navigation subsystem is not fully accepted. The runtime requests
MAPDIR1–8 for the party arrow. Source inventory finds only 6mapdir1–8 and 7mapdir1–8, so explicit source
binding is still needed; the captures have no party direction arrow. Zoom controls, arrow orientation,
movement tracking, Wizard Eye markers, fog/exploration behavior and full indoor minimaps remain pending.
Only two outdoor maps have visual capture acceptance; 28-image import and 140-map load counts do not
mean all those maps have been played through.

Default MM8 portrait/source gaps, MM8/Breach content, complete quests/travel, MMExtension behavior,
audio, import recovery/storage, sustained play and later iPad regression remain open. Overall estimate
stays 40%–45%, low confidence and not a measured engineering completion rate.

Evidence: ../local-data/validation/minimap-source-051/result.json, package-validation.json,
import-checks.json, runtime.log, mm6-metal.log, mm7-metal.log, ios-release.log,
mm6-metal-001/scene-first.png, mm7-metal-001/scene-first.png and arrow-source-all-archives.json.
