# Town Portal first contact — checkpoint 115

Status: focused checkpoint115 acceptance passed. All work ran on NVMeSSD without an iPad or simulator.

## Implemented behavior

The shared Town Portal service now checks for Verdant contact after an otherwise valid cast has reached
the utility-selection stage. At an existing native Dimension Door trigger, an unmet party first opens
NPC803 through the ordinary dialogue controller. This executes the imported NPC-enter hook before
presenting the dialogue, making topic1778 available. Opening the dialogue does not itself set MetVerdant;
the first introduction topic does.

The contact deadline follows the supplied `Quest_CrossContinents.lua`: strict `now > deadline`, followed
by a new deadline four game minutes later on every otherwise valid, unmet Town Portal attempt, even
away from a trigger. The state belongs to the gameplay session, survives world rebinding, and resets
on session clear or save restoration. Reset on load is an explicit lifecycle choice for the original
transient script-local variable; it is not a new serialized save field.

The TPGlobal destination loader no longer requires GotMainQuest. Original Dimension Door travel has no
such gate. Dismissing Verdant and casting again during the quiet window can therefore show destinations
before the introduction is accepted. The daily destination data and existing native location detector
are unchanged.

Contact requires NPC803 and global events65500/1778, the capability convention established by the
validated selective importer profile in 114. Base-only packages skip the new NPC interception; their
TPGlobal table can still supply Dimension Door destinations. Missing destination data remains an error.

## Validation boundary

The focused executable uses imported runtime085 resources with the accepted 114 development overlay,
real GameDataLoader, GameplaySpellService, dialogue controller and event runtime. Its party, position,
time and hostile actor inputs are controlled fixtures. These are integration boundary tests, not
natural acquisition of Town Portal or a continuous cross-world playthrough.

Evidence is under `../local-data/validation/town-portal-contact-115/`:

- `town-portal-tests-003.log`: **7 cases / 478 assertions passed**. Covers strict moving deadline,
  real spell-to-dialogue routing with imported Chinese topic/message text, no first-contact mana or
  recovery cost, a three-destination retry before accepting the quest, suppression after the first
  intro click, ordinary/invalid casts, missing global-program readiness, world rebinding, and session
  clear/restore. The null-program negative is a controlled boundary; base085 behavior is also reviewed
  statically against its actual archive contents.
- `ios-build-result-001.json`: unsigned generic iOS Release passed, with changed shared translation
  units compiled for arm64 and matching source hashes before and after the build.
- `production-review-001.json`: Astra independently matched the current production sources to the
  successful iOS build and reviewed the shared service, dialogue and session call paths.
- `audit/interface-test-plan.md`: original-source review, importer capability assumptions and explicit
  differences between the current native trigger detector and the original activation system.

The first integration run failed before contact because the actual class table recalculated the
fixture caster's mana to zero. A level10 caster with suitable attributes and a post-seed mana
precondition fixed that fixture. The second run exposed a separate assertion comparing NPCText1778
with NPCTopic1778; the final test reads the topic and speech registries separately. Both required one
successful Sol fixture repair; no production fix or bypass flag was used to satisfy these failures.
Original failed logs and test-source snapshots remain available.

The native configuration first failed on mixed physical/compatibility paths in its dependency cache.
Restoring aliases that resolve to the same NVMe directories allowed configuration and compilation.
No build tree was cleaned. Astra xhigh owns implementation/review; crossworld_tests and crossworld_audit
use Sol high, and ios_build_check uses Sol medium.

## Remaining work

The existing detector covers four tiles and two radius checks. It does not implement the original
eight-minute switch lifetime, every direct map event, exact original radius semantics, or the scroll's
greeting329/contact wrapper. Verdant placement, later quest topics, continent rewards and final quest
also remain outside this checkpoint.

Runtime085, the accepted 114 evidence, and normal continuation save113 are unchanged. No new visual,
physical input or iPad acceptance is claimed. Overall estimate remains 50–55%, low confidence; this is
an engineering estimate rather than a percentage of game quests passing tests.
