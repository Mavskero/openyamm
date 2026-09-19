# Dimension Door scroll validation — checkpoint 118 accepted

Item190 now keeps the original one-minute reversion cadence without changing natural Dimension Door locations,
which continue to register eight-minute timers. Each activation stores its deadline and interval together. Mixed
registrations retain the original `RemoveTimer(function)` behavior: when a later timer fires first, the earliest
matching registration is removed and the firing timer repeats with its own interval. Info-screen deferral also uses
that registration's interval. Day selection remains frozen at activation until another activation occurs.

The real `GameplayItemService` held-item/member entry was exercised with a healthy, recovered controlled member who
did not know Town Portal and had zero SP. Item190 was the only held item. It was consumed once and opened the ordinary
pointer-driven destination UI with the original GM10, free, no-recovery direct-cast context. Closing through the
normal button queued no map, spent no SP, added no recovery and did not refund the scroll; the activation reverted at
one minute. Selecting the day-23 Antagarich target queued the expected map once, and a second click after the overlay
closed produced no duplicate request. Existing learned Town Portal 20-SP/recovery behavior remains covered.

Evidence: `../local-data/validation/dimension-door-scroll-118/result.json`. The final native regression passed
45 cases and 1,585 assertions across Dimension Door activation, destination/item interaction and Verdant contact.
The original runtime117 and event overlay116 were mounted read-only; no original Lua, save format, iPad or simulator
was changed. The held scroll was a controlled fixture, not proof of natural acquisition. Final unsigned generic iOS
Release compilation is intentionally deferred to the batch gate in checkpoint 122.

