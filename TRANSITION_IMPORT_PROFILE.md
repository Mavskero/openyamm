# MMMerge transition text import profile

`transition_import SOURCE NEW_DESTINATION` reads `Data/mmmerge.T.lod/trans.txt`
and the optional `10 LocZHCN.EnglishT.lod` translation using native LOD and text
code. The output is `engine/data_tables/english/trans.txt`, with original IDs.
The destination must be fresh and outside the source. Source archives are never
modified; published files have mode 0644. Invalid input publishes no partial output.

## Schema and localization

The supported header is exactly `2D#`, `Transition Description`, `Note`, with
three columns per record and unique IDs 1–65535. The base table controls identity
and active rows. Empty descriptions and literal `Placeholder Text` are inactive;
translation cannot activate them. Usable translations must retain the base note.
Missing or placeholder translations preserve the newer base description and
are reported. Extra translation IDs are ignored and counted.

The supplied source has 271 records: 164 active and 107 inactive. Of the active
records, 161 receive Chinese text; legacy glyph runs restore 5,513 characters.
The older Chinese package has placeholders for IDs 673, 674 and 675, so the newer
English tunnel descriptions and their notes are retained. No text is invented.

## Shared dialog semantics

`TransitionTable` recognizes the original Note header and stores column three as
`sourceNote`, separate from display title. Legacy title tables retain their old
behavior. Dialog titles continue to use destination/current map names or explicit
overrides. Notes may identify an existing transition video without appearing as
English technical metadata in the title. Exact known names match first; numeric
area suffixes such as `Dragon Cave (area 12)` and `Ilsingore's Cave 58` can be
removed only when the remaining name matches an existing video association.
This is an explicit port compatibility policy, not proof that the video exists.

## Validation and limits

All 164 output records round-trip through the production table and dialog-content
assembly; inactive IDs remain absent. Checks also exercise legacy title behavior,
Chinese destination names, numeric video notes and rejection of nonnumeric suffixes.
An independent Python decoder compared every active ID, description and note
against the original archives. Twenty synthetic native integration tests cover
Chinese controls, quoted multiline text, older translations, invalid schemas,
archive damage, source boundaries and publication permissions.

Checkpoint `transition-tables-001` is included in `gameplay-base-014`. Formal
startup now reaches the next missing required table, `roster.txt`; optional
`house_animations.txt` is still absent. Mac and iPadOS builds pass. Actual rendered
dialogs, video playback, map transfer and complete gameplay remain unvalidated.
The manifest deliberately retains `runtime_ready=false`.
