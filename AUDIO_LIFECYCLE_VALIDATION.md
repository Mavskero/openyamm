# Audio cache and lifecycle — checkpoint 055 (2026-09-11)

Mac-only development and generic unsigned iOS compilation. No iPad was queried, connected, installed
or operated. The existing checkpoint-054 resource package is used unchanged; no source assets,
catalog entries, IDs or save formats change in this checkpoint.

## Reproduced retention and shared engine policy

The original engine retained every WAV loaded on demand until explicit release or shutdown. A native
check issued and stopped 500 distinct original sound requests from the accepted package. Before the
fix, all 500 decoded clips remained, totaling 275,337,136 PCM bytes (about 263 MiB). This exercises
real decoding and request cleanup, not 500 complete listening sessions or a long gameplay session.

`AudioSystem` now gives on-demand WAV cache data a default 64 MiB budget. When over budget, unused
clips are removed in least-recently-used order. Replaying an evicted sound reloads its original WAV.
Clips still held by active or paused instances are protected, including multiple instances of the
same clip. Complete sample data already copied to SDL's output queue does not depend on cache entries.

Explicit preloads and externally registered clips are pinned. Existing map preload begin/end calls
still control map-specific releases; shared preloads persist, and GameAudioSystem still controls
registered music eviction. This is OpenYAMM cache management, not an implementation of the original
Sounds.txt preload/swap/lock flags. Those fields and their compatibility limits remain unchanged.

The budget is for decoded on-demand PCM, not whole-app RAM. Active clips can exceed the budget while
playing; pinned preloads, registered music, decoder work buffers, graphics and SDL buffers are outside
that idle-eviction guarantee. `setTransientCacheLimit` permits a different byte quota; the zero/small
quotas used in diagnostics are not the production default.

After the fix, the same 500 original requests retain 93 clips totaling 66,553,096 bytes (63.47 MiB).
The highest settled sample-byte observation is 67,107,960, below 64 MiB. These counters measure sample
storage in the cache, not measured process RSS, allocator capacity or a peak during active decoding.

## Validation

- Four native/Python fixture cases verify eviction order and reload, active/paused/shared instance
  protection, explicit preload/registration protection and release, cached-preload promotion, and
  cleanup when a one-shot naturally finishes in the mixer. PCM fixtures are generated test data only.
- Existing music transition/loop/resampler-tail and partial ADPCM block regressions still pass.
  The focused total is 11 passing Python checks.
- A native GameAudioSystem check uses original sounds 31201, 42186 and 42602 with music 37. Under a
  diagnostic zero-idle-cache quota, it replaces map preloads, preserves shared dialogue and paused
  music, reloads/stops a transient sound, resumes music, releases music on stop, clears the map
  preload, and finally releases all PCM on shutdown. It calls the real preload lifecycle methods;
  it is not a full geography transition or physical input test.
- `application_scene_capture --check-party-audio` loads the original normal MM6 save into the formal
  Mac GameApplication and queues one explicitly marked `Party::requestSpeech(0, Yes)` diagnostic.
  Normal gameplay frames drain that queue and the audio trace records sound 5083/`pc5039b.wav` as a
  queued Speech instance while music 37 remains active. The application renders 181 Metal frames and
  writes a fresh diagnostic save. This validates the party-queue/runtime/player connection, not a
  real button press or every action that should produce speech.
- The application reports 20 cached clips, 80,499,296 total PCM bytes, of which 202,088 are transient.
  Its music and preloaded sounds explain why total PCM can exceed the transient budget. No eviction
  was required in that short scene run. Generic unsigned iOS Release builds successfully.

Evidence lives in `../local-data/validation/audio-lifecycle-055`: `retention-before.json`,
`retention-after.json`, `regression-tests.log`, `map-cache.json`/`.log`,
`mm6-party-audio-001.log`, and `ios-release.log`. Initial and final retention use the same 500 original
resources; the source ZIP package is the accepted `sound-payloads-054/runtime-001`.

## Remaining work

Actual gameplay triggers across dialogue, movement, combat, spells and all voice variants still need
coverage, along with spatial mixing, music/video/voice coexistence, interruptions and long sessions.
The 723 registered missing sound samples from checkpoint 054 remain missing; no unrelated audio is
substituted. Full campaigns/travel, other missing resources, import recovery/storage and physical
device input/audio/lifecycle acceptance remain open. Overall stays a low-confidence 40%–45% estimate.
