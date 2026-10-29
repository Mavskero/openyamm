# Configured keys override global shortcuts 206

Assigning F9/F10 in Keyboard no longer triggers quicksave/load. Queueing and consumption both honor the active Keyboard page and explicit key ownership; held latches keep updating to prevent a deferred trigger when leaving the page. Time-advance behavior and normal unbound quick shortcuts remain available.

Indoor F8/F12 geometry diagnostic shortcuts now require world input and yield to explicit bindings, so rebinding keys in a menu cannot hide the world. Native and unsigned generic iOS passed. Source/build evidence only for these shortcuts; no actual save/load or geometry-toggle exercise, campaign or device operation.
