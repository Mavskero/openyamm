# Monster Tinting 200

The video Tinting switch now applies each monster descriptor’s existing RGB to actor lighting. Outdoor actors carry a separate vertex factor so ambient and dynamic light are both tinted without splitting texture batches; indoor actors apply it after cached lighting. Texture transparency, selection outlines, fog and non-actor billboards retain their prior behavior. The video preview refreshes immediately.

Normal runtime198 has652 descriptors,394 with nonzero RGB; all source alpha bytes are zero. The shared helper explicitly maps imported AARRGGBB channels and preserves sprite opacity. Palette variants and descriptor tint are separate original inputs. No resource rebuild or Save104 change is required. See source-data.json for pinned primary references and the runtime member hash. Applying the same control indoors is a project consistency choice; original indoor parity is not claimed.

Native and unsigned generic iOS builds passed. One13.684second Metal flow (20frames/3reviewedimages) used original Emerald Island Dragonfly C, source tint00FF78B4, and a declared party/camera pose. A real queued SDL checkbox click updated preview and gameplay; the actor visibly changed color without disappearing. Time was paused throughout; no natural travel/combat/save/campaign/device claim. The first13.307second setup exited before rendering because its capture-only predicate found no friendly colored actor; its failure is preserved. Zero new unit cases.

Runtime198/runtime-003 and published remote195 stay fixed. Next concrete work is the indoor WalkSound setting and short touch drags on volume controls. Full natural campaign and target-iPad acceptance remain unproved.
