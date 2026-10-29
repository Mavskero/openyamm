# Controls audio and touch fixes 201

Disabling Walk Sound now stops indoor footsteps immediately. Paused, disabled or invalid scenes clear the old walking loop and position baseline, so resuming does not interpret a stale position change as a new step.

Sound, Music and Voice sliders now accept a touch drag completed between input samples and retain the final release position. A slider owns its gesture until release; unrelated drags cannot click a button or inherit a press from the previous overlay. Audio volume previews during dragging, with one settings commit on release or closing Controls. Arrow buttons still commit immediately.

Native and unsigned generic iOS builds passed. This small consumer/input change received source and build checks only: no new tests, App runs, campaign replays or device operations. Actual sound and target touch acceptance remain unproved. Runtime198/runtime-003, Save104 and the published remote195 package are unchanged.

The next confirmed defect is TurnRate: 16/32 modes are keyboard angle steps, but the current code changes outdoor mouse sensitivity instead. That correction is separate upcoming work; it is not included here.
