# Complete Yell hold behavior 214

Yell now executes immediately, waits500ms before repeating, then repeats every67ms while held. Modal/blocked input, focus interruptions and missed controller frames cancel the old hold until release. Repeated dispatch of one input frame does not execute twice; a delayed frame does not replay accumulated actions. Other action repeat rules remain independent.

Focus is carried as an event generation so losing and regaining focus between two frames still cancels. The no-window host check remains usable until a real focus event changes state. All fields are transient; Save104 and resources stay compatible.

One existing input check was extended with a held-frame fixture and real monotonic waits. Native and unsigned generic iOS compilation pass; first actual short-press world behavior is recorded at213. Native held-key and new target-iPad acceptance remain unproved.
