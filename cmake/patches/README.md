# bgfx iOS Metal depth format correction

`bgfx-metal-ios-depth-format.patch` targets the pinned bgfx revision
`a73c12db8f502022d292cc6fb8872482997b0664`.

Its Metal initialization first selects Depth32Float_Stencil8 for iOS, then
unconditionally queries `isDepth24Stencil8PixelFormatSupported`. Apple's SDK marks
that selector unavailable on iOS. On an iPad Pro M5 running iPadOS 27, this causes
an unrecognized-selector exception before rendering. The simulator did not expose
the failure because its Metal device accepted the selector.

The correction limits that query to macOS and preserves the existing iOS format
selection. It does not change macOS format selection or dependency versions.
The CMake helper applies it to the fetched source during iOS configuration, accepts
an already-applied patch, and fails if an updated source needs manual review.

References: [Apple API documentation](https://developer.apple.com/documentation/metal/mtldevice/isdepth24stencil8pixelformatsupported),
[Metal feature tables](https://developer.apple.com/metal/capabilities/).

Validation: the full device Release build installs, renders through SDL/bgfx/Metal
and reads back a 2420 x 1668 color triangle on the same iPad M5. The simulator
regression run reads back 2752 x 2064 after applying the same patch. Reconfiguration
also succeeds with the patch already applied. This check does not
cover all game texture formats, render passes or lifecycle transitions.
