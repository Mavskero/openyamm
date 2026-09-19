# Original UI regions and character portraits — checkpoint 050

2026-09-11. Development and Metal capture on Mac only; generic unsigned iOS Release build.
No device queries, connections, installations or demonstrations.

## Changes

The imported package lacked the authored wide-HUD names Basebar_cut, Basebar_ender, status_bar,
scorebg_gold and scorebg_food. GameplayUiRenderer returned before drawing the party and other HUD
art when the basebar was absent. The original source already contains the needed regions in
6UI.icons.lod's Basebar (640×114) and scoreBG (170×23).

The gameplay layout now declares five texture_regions. UiLayoutManager validates names, source shapes,
rectangle bounds, duplicate declarations and nonrecursive source ownership. UiResources imports the
underlying original image, preserves the layout bytes, validates available source dimensions and records
each region in its report. Missing sources stay explicit. It does not write replacement/cropped bitmap
files. The shared HUD loader reads the declared pixels and original transparency into its runtime texture
cache, including integral asset scaling. Undeclared assets retain ordinary lookup. Region-backed portrait
spacing and vertical placement are also used by hit testing. No platform-specific renderer is added.

The first five-person capture exposed overlapping side panels at the old HUD scale. Wide-HUD roots now
share an explicit 960-pixel width reference through a YAML anchor. This keeps the party, command strip,
buff panels and minimap frame separate at the captured 2420×1660 drawable size. Character-sheet fonts
and the standard/overlay layout definitions are unchanged. Other window sizes and touch-target comfort
still need separate acceptance; this is not a claim of every window configuration being tested.

FrameResources also reads original Character portraits.txt prefix bindings and joins them to the PFT's
two-digit texture indices. Present images use the existing top-level icon archive ownership checks and
are imported into engine/icons because the party carries its identity across worlds. Repeated prefixes
are deduplicated. Unknown schema/IDs/unsafe names fail instead of being interpreted as new artwork.
The frame-only CLI still accepts its earlier table-only fixtures; full runtime import independently
requires the character table through the doll stage.

This source has 73 distinct prefixes and 4,088 required pose references. **2,465 original images exist;
1,623 are absent, including normal poses for 29 prefixes.** Missing normal names are pc01–pc28 and pc30
with the -01 suffix. The default character uses pc01-01, so its face remains unavailable. Neither that
character nor old saves are silently changed to another face. Complete portrait readiness stays false.

## Checks and limits

- 44 UI/frame import tests pass, covering original skin precedence, layout preservation, source regions,
  missing poses, malformed regions and normal failure/output protection. One new synthetic test initially
  used the wrong LOD version for its image header; the fixture was corrected, not the production decoder.
- The headless runtime region checker compares 59,200 pixels, including alpha, against the original
  decoded images; source-shape and integral-scale checks pass.
- Full native runtime conversion retains 140 MM6/MM7 map loads. The package has 19,548 ZIP entries.
  Final package CRC, entry equivalence with the rendered UI fixture and bundled layout checks are recorded
  in final-package-validation.json. The UI fixture is not app-API publication.
- Actual Mac Metal captures run the normal application's load, update and render callbacks for about
  181 frames. One-person and five-person **explicit visual fixtures** use available original portraits
  and varied health. They are loaded as new test saves; the input normal-start save is untouched.
- At capture frames 30 and 150, each portrait resolves within the screen, its center maps to the same
  party index, its pose texture loads, and its rectangle does not intersect the command strip, buff
  panels or minimap frame. This checks geometry, not physical keyboard/mouse/touch actions.
- The first two five-person diagnostic attempts changed only the session party after load; the world
  restored the original one-person state, so assertions failed before capture. The diagnostic now loads
  a separate fixture save through the actual load path. No production party synchronization was changed.
- five-metal-003 is before-fix evidence of the side-panel overlap; five-metal-004 and one-metal-001 show
  the corrected layout. The fixed frame geometry is not a five-character campaign or combat playthrough.

Known remaining HUD gaps include the absent default/MM8 portraits, outdoor minimap imagery (only its
frame currently appears), other missing dynamically referenced icons, and full input/resize acceptance.
The first captured basic Knight has zero maximum mana; these fixtures do not validate nonzero mana fills
or spellcasting. Other worlds, full quests/travel, source gaps/audio, import recovery/storage and sustained
performance remain open. Overall estimate stays 40%–45%, low confidence, not a measured completion rate.

Evidence: ../local-data/validation/hud-source-050/result.json, runtime-002.log, import-tests-002.log,
region-final.log, final-package-validation.json, five-metal-004/scene-first.png,
one-metal-001/scene-first.png, mm7-cold.log and ios-release-final.log.
