# Final house exit facing 205

FlipOnExit now runs at the common final close of an active house dialog, updating indoor and outdoor facing by180degrees while retaining pitch. Dialogue state is cleared first, so a repeated close cannot turn again. Subpage returns and nonhouse conversations do not trigger the hook. The old outdoor-only consumer was removed.

Native and unsigned generic iOS passed with203–206. This change is source/build reviewed only; no house replay, new unit case, device operation or Save schema change. Official fixed-source semantics are recorded in source-semantics.json.
