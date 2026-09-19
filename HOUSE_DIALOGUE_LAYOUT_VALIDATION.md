# House dialogue layout validation

Checkpoint120 validates a finite Mac Metal matrix for the temple service menu and real healing feedback, a long
Chinese NPC response, and an ordinary shop. Each dialogue page was captured at a true 1024x768 drawable and the
current 2420x1668 reference drawable; the long NPC response also passed at 1280x720. Window-point and drawable
sizes, normal pointer selections, shop item purchase, and close navigation are recorded with each capture.

Dialogue rendering now paints an opaque-black, non-overlapping perimeter around the complete 4:3 UI viewport in
the below-HUD pass. It does not stretch the source art and handles side, top, and bottom gutters. House service
feedback uses only one visible channel: ordinary healing remains in the dialogue body, while existing
training/donation status-only behavior is preserved.

All 15 final BMPs were inspected. Chinese service labels and the two-line NPC response remain readable without
overlap or clipping, shop stock stays operable, and the HUD/close controls remain intact. Seven gutter bands passed
exact pixel checks. The character skills comparison preserved its accepted larger type, and the Dimension Door
comparison is pixel-identical to checkpoint117.

Evidence is in `local-data/validation/house-dialogue-layout-120/result.json`. Checkpoint120 does not claim broad UI
coverage, shop localization, natural Dimension Door contact, iPad/simulator behavior, or the final generic iOS
Release gate; the latter remains checkpoint122.
