# Settings Return to game 208

The Controls, Keyboard and Video settings pages use the original Return to game button artwork. Their click handlers previously returned only to the menu or parent settings page. Each now completes the existing close chain and closes the menu, restoring gameplay. Back and Escape keep their existing navigation.

Existing close methods retain dirty audio/gamma commits, preview cleanup and input-latch handling. The change is four added calls across the three handlers, with no new state or resources.

Validation: combined native and unsigned generic iOS compilation; source/build acceptance only. No new unit cases, App replay, campaign or device operation. Remote release 195 stays fixed.
