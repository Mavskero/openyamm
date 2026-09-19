# Original merged music — checkpoint 053 (2026-09-11)

This checkpoint uses the Mac and a generic unsigned iOS build only. No iPad was queried, connected,
installed, launched or demonstrated. SDL dummy output checks the software audio path, not speakers,
headphones, Bluetooth routing, iPad interruptions or listening quality.

## Import and ownership

The supplied `Music` directory has 49 original MP3s, totaling 299,778,654 bytes. The media converter
previously omitted it entirely. It now preserves each payload byte for byte under
`engine/music/<track>.mp3`. These are the merged installation's shared numeric IDs, already referenced
by the global map registry and menu/house audio. They are not renumbered by original game, and the map
registry is unchanged. The source directory copy already includes Music; the existing native runtime
media stage now packages it without a separate desktop transcode step.

Only direct MP3 entries are selected, with canonical positive decimal stems of at most six digits.
Extensions are case-insensitive; output uses lowercase and mode 0644. Invalid names, duplicates,
non-files and source escapes fail the complete fresh media stage. Limits are 512 tracks, 128 MiB per
track and 1 GiB total. Initial ID3/MPEG Layer III signatures are screened; this is explicitly not a
full MP3 validation. Missing Music is recorded; the converter does not invent a soundtrack.

The media report adds music count, bytes, source/path/track entries, missing-directory status and
`music_payloads_transcoded=false`. It still keeps `runtime_ready=false` and `playback_validated=false`:
successful extraction alone does not constitute playback acceptance. The actual source passes full
decoding in the separate shared-player checks below.

## Shared player fixes and checks

- Pausing while a different track was requested left the old instance active on resume. The shared
  `GameAudioSystem` now runs the pending request through the normal transition after resuming.
  This preserves the pause while a cutscene is active and fades to the requested map/house music later.
- FFmpeg decoding reached EOF without flushing the resampler's filter tail. The two checked original
  tracks lost 17 stereo frames each. Flushing that tail now produces exactly 5,847,040 frames for track
  35 and 9,202,560 for track 37 at 48 kHz, matching independent desktop FFmpeg decoding.
- The native `music_playback_check` uses the actual asset filesystem, asynchronous game decoder,
  music cache, engine mixer and SDL postmix callback. All 49 imported tracks decode to finite,
  nonzero PCM, totaling 385,739,445 stereo frames. Settled tracks retain one decoded clip, and immediate
  stop releases it. No original audio is synthesized or replaced.
- Original tracks 35/37 pass output, pause/drain silence, stable paused position, resume, mute/unmute,
  paused track replacement, stale asynchronous decode replacement and fade-to-stop/cache eviction.
  Original output RMS observed by SDL postmix is 0.00436642 in the recorded run.
- A separately marked 0.824-second excerpt copied from original track 35 loops at least twice. It is
  diagnostic data only and never enters the production resource package.
- Three reproducible Python playback regressions use generated test tones: loop/complete tail at
  44.1 and 48 kHz plus the full transition sequence. Thirty media-import checks pass, including new
  music preservation, missing-directory reporting, malformed input rollback and source protection.

## Actual application and package evidence

`../local-data/validation/music-source-053/runtime-001` is a fresh complete native conversion. All
140 map-load checks pass. The four ZIPs have 19,643 CRC-checked entries; all 19,594 prior checkpoint-052
payloads are unchanged, and exactly 49 original MP3s are added. Their hashes match the source inventory.

The existing Mac `application_scene_capture` opens the original checkpoint-048 normal save through
the formal GameApplication for MM6 and MM7. Each renders 181 Metal frames, captures two frames and
saves to a fresh diagnostic output. Audio traces verify MM6 `oute3.odm` requests/starts track 37 and
MM7 `7out01.odm` requests/starts track 35, without missing/decode-failed music warnings. Audio output
uses SDL dummy; keyboard, pointer, touch, speakers and physical navigation are not being validated.

Evidence in that directory: `package-validation.json`, `source-music-inventory.json`,
`all-tracks-final.jsonl`, `independent-decode.json`, `lifecycle-final.jsonl`, `loop-final.jsonl`,
`import-tests.log`, `playback-tests.log`, the two application logs and `ios-release.log`.
`lifecycle-001` preserves the reproduced pre-fix transition failure; `all-tracks-001` predates the
resampler-tail fix. Those intermediate results are not final acceptance.

## Remaining audio work

One positive map music reference has no supplied MP3: MM8 `d35.blv` uses track 1. MM8 gameplay is not
yet accepted. No replacement is generated, and missing music remains explicit at runtime.

The absence of standalone SND/WAV files does not mean all effect/speech payloads are absent: an index
audit of all 41 supplied LODs finds 8,732 WAV member records across five D.lod archives, including
language resources. These records include overlapping names; they are not 8,732 accepted unique
sounds. Their payload decoding, catalog matching, language precedence and engine ownership need the
next integration step. Footstep requests, combat sounds and voice playback remain unaccepted.

Source/native import recovery, cancellation and storage pressure, full campaigns and travel,
long-session audio/memory stability and iPad audio routing/interruption tests also remain open.
Overall completion stays a low-confidence estimate of 40%–45%; this checkpoint does not establish
complete MM6/7/8 gameplay or physical-device acceptance.
