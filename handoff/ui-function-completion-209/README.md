# Explicit bindings take precedence over legacy gameplay aliases 209

Binding an action to keypad Enter no longer also toggles turn-based mode through the legacy keypad shortcut. The configured Combat action still works, including when explicitly assigned keypad Enter.

Ordinary party selection uses 1–5 and follower dialogue uses 6–9. These fixed shortcuts now yield when the corresponding key is explicitly bound to an action. Main-row and keypad variants are checked independently, preserving the unassigned alternative. This prevents a movement binding from selecting another character or opening a follower dialogue that blocks movement.

The legacy P shortcut for the Adventurers Inn roster also yields to an explicit binding. With Forward=P and a populated roster, it previously opened the roster and blocked movement. Unassigned P retains its existing behavior.

Validation: combined native and unsigned generic iOS compilation; source/build acceptance only. No new unit cases, App replay, campaign or device operation. No save schema or resource change. Remote release 195 stays fixed.
