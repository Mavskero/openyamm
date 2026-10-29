# Implement the configured Yell action 211

The Keyboard page offered Yell, but no gameplay consumer handled it. A fresh press now requires normal gameplay and an active character who can act, removes party Invisibility, and requests the existing Yell speech/portrait reaction. Real-time play also asks the active world to apply the nearby-actor response. Turn-based play keeps speech and invisibility removal without actor flight or turn/recovery cost.

Input gating checks both before and after UI handling so closing a modal does not pass the same gesture through. Existing source-action gating and settings bindings are respected. Holding the key does not repeat; the original OpenEnroth 500 ms/67 ms key-repeat behavior is intentionally outside this first implementation.

The world response reuses existing actor motion, action timers and navigation. It does not apply Fear, summon guards or change permanent hostility/reputation. Original rules are taken from the pinned OpenEnroth revision `87b3e29b51b931cc5f0e8998f238a686cafa86a4`, particularly Party::yell and Actor::AI_Flee. Current actor mobility and status constraints remain authoritative. The original special Treant StandOrBored animation is not claimed: merged source identity is insufficient for a reliable Treant-only classification, and existing immobility constraints are preserved. The current actor model has no independent stone status to extend here.

The world gate uses strict 3D distance below512 map units, excludes long-hostility and stationary/unavailable actors, and keeps existing immobility constraints. Duration follows the source128-ticks-per-second formula with a2-second ceiling; direction is sampled across the half-circle away from the party. Existing bound event RNG is used only after eligibility; an unbound world does nothing. This reuses current XY fleeing and does not add source-specific flight pitch.

Validation scope and exact source/build results are recorded in scoped-result.json. No save schema or resource change; release195 stays fixed. Full natural campaign and target-device acceptance of this action remain unproved.
