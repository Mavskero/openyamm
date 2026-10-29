# Keyboard turn modes 202

Classic controls now honor Turn Rate indoors and outdoors: 16 advances 22.5 degrees per tap, 32 advances 11.25 degrees, and Smooth turns continuously. Held discrete input repeats using elapsed time at the existing 1.75 radians/second speed so refresh rate does not control turning speed. Exact tap angles come from the original implementation; the held cadence is an explicit project adaptation. Movement in the same frame uses the new facing direction.

Turn Rate no longer incorrectly changes outdoor mouse sensitivity. Mouse input retains its independent sensitivity setting and existing base speed.

Native and unsigned generic iOS builds passed. One focused numerical case passed 22 assertions covering taps, held-time consistency and disabled input. No App, campaign replay or device operation was performed. Runtime198/runtime-003, Save104 and the published remote195 package are unchanged. Actual target-device feel and full natural campaign completion remain unproved.

Next confirmed work: prevent duplicate keyboard/mouse bindings from cancelling actions, and stop the legacy M map shortcut from consuming explicitly rebound input.
