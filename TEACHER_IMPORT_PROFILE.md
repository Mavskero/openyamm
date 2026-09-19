# MMMerge teacher import profile

`teacher_import SOURCE BASE_ASSETS NEW_DESTINATION` converts the original loose
`Teacher topics.txt` and `Teacher autonotes.txt` into shared engine tables. It
requires the existing merged NPC and journal registries. All original cells are
preserved, including blank overrides and inactive notes; output is UTF-8, mode
0644, and published only to a fresh directory outside both inputs.

The supplied source has 178 active topics. Skill IDs remain zero-based and source
mastery values 1/2/3 map to Expert/Master/Grandmaster. Three blaster topics
971/972/973 explicitly require 2,000/5,000/10,000 gold and skill 4/7/10. Other
prices and levels remain blank, selecting the existing gameplay defaults. An
explicit zero override is rejected because Lua treats it differently from the
engine's zero-as-default representation; no supplied record needs that case.

The exported topic table adds `#TeacherTopicMode\tComplete` before the original
header. `RemoveNPCTablesLimits.lua` loads an existing table as the complete active
set. The shared resolver now honors this marker instead of synthesizing omitted
legacy topics 300–416. Unmarked extension tables and the no-table legacy path
retain their prior fallback behavior.

The autonote source has 253 nonempty NPC/note bindings and one empty pair. Old
blaster topics 321/322/323 are absent from the current training definitions but
remain in the note source, including two nonempty bindings. They are preserved
and reported as inactive training associations, without moving them to new
teachers. This does not establish whether every original note event is inactive:
`NPCTeacherAutonotes.lua` also considers explicit note mappings when entering an
NPC. That event path and generated map notes remain unvalidated.

Production loaders resolve every topic/text/NPC/journal reference, mastery and
override. A focused shared gameplay check covers successful training, exact gold
deduction, repeated training, insufficient gold and insufficient skill. This is
not validation of all original default prices or special training requirements.
NPC and journal text already use their Chinese registries; the full training UI
has not been localized or accepted on screen.

`teacher-tables-001` is independently field-equal to both original source tables
(apart from the explicit mode directive). It is incorporated into
`gameplay-base-017`; formal startup passes both teacher tables and next requires
`npc_professions.txt`. Optional house animations remain missing. The focused
teacher suite passes 21 tests, the complete resource suite passes 326 tests,
and Mac/iPadOS arm64 builds pass. No device was
connected or updated. Full gameplay, rendered training and teacher-note event
acceptance remain pending.
