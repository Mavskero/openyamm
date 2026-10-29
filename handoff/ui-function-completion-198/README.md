# Video brightness and preview 198

Video options previously showed only the original Brightness/Preview titles. It now provides a logarithmic 50–200% slider with 100% at the center, arrow steps, immediate display adjustment, and commit on release or closing. Invalid INI gamma values recover to neutral; Save104 is unchanged.

The app renders into a neutral target and applies display gamma once. A separate world-only preview is captured on opening, resize, or affected checkbox change, before HUD drawing. Normal gameplay does not copy the preview each frame. Loading presentation resumes safely inside an outer frame; zero-size drawable frames are skipped. The ten original `convol10`…`convol90`/`convol00` images missing from the previous runtime are now required imports, repairing existing volume indicators too.

Native and unsigned generic iOS Release builds passed. Final affected Mac flow passed in 13.340 seconds / 53 frames: held slider to200%, release/INI commit, arrow step, return to100%, Escape during pending edit,30normalframes and save/decode. Three final images plus two earlier partial images were reviewed. A same-preview pixel comparison matched one gamma2 pass (mean error0.248/255), not a duplicate pass.

The first assembly caught missing original knob images before App execution. Two early capture attempts asserted the arrow result on mouse-down rather than the queued mouse-up; both failures are preserved, the unnecessary experimental input change was removed, and the capture now observes actual release. No mouse input-policy change remains. No new unit cases, campaign replay, cold replay, or device/simulator operation.

Complete development runtime: `local-data/validation/ui-function-completion-198/runtime-003`, derived from195 with one changed layout and ten added original images; all worlds and other members retained. The published remote195 release remains pinned and unchanged. This is scoped Mac UI evidence, not natural campaign or target-iPad acceptance. Outdoor/backends/resize/background variants are source/build reviewed only.

Next concrete work: fix `coloredLights` omissions for dynamic indoor/outdoor lighting and cache invalidation. Existing `tinting` semantics are unresolved; no effect was invented.
