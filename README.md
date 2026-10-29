# OpenYAMM

**iPadOS branch development (2026-10-29):** this branch contains the consolidated
[device-independent changes after RC3](handoff/device-independent-completion-190/README.md), retaining Save103.
Start with [IPADOS_HANDOFF.md](IPADOS_HANDOFF.md) for current status,
fresh-Mac builds, separately transferred runtime assets and physical-iPad debugging.
The ready-to-paste Codex task is [CODEX_IPAD_DEVICE_PROMPT.md](CODEX_IPAD_DEVICE_PROMPT.md).
This development batch creates no new device checkpoint or release. Its complete runtime is local;
published RC3 assets remain unchanged. New iPad acceptance and a complete natural campaign remain unverified; the
general upstream description below is not evidence of either.

OpenYAMM is Open Yet Another Might and Magic: a modern C++ reimplementation of
Might and Magic VI, VII, and VIII, inspired by MMMerge and built on an engine
that started from Might and Magic VIII.

The goal is to keep the original game data and gameplay feel while providing a
clean, maintainable, cross-platform engine with modern rendering, audio, UI,
save/load, tooling, and editor support.

## Status

OpenYAMM is playable across Might and Magic VI, VII, and VIII, including
continent travel between the games. Windows, Linux, and Android builds are
supported.

Main stories, promotions, quests, mechanics, spells, travel, dialogue, shops,
houses, chests, combat, projectiles, inventory, party state, save/load, audio,
video, and UI systems are implemented. Development is still active, with ongoing
work on polish, compatibility, tooling, and editor workflows.

## Features

- C++20 codebase
- SDL3 platform, input, and audio layer
- bgfx renderer
- PhysicsFS asset filesystem
- FFmpeg-backed video playback
- Lua-powered event scripts
- tab-separated gameplay tables
- YAML scene and UI layout data
- unit and regression test coverage for gameplay systems

## Assets

Development assets are loaded from:

```text
assets_dev/
```

The default development layout is:

```text
assets_dev/
  Anims/
  Data/
  Music/
```

Runtime packages can be distributed as ZIP archives under:

```text
assets/
```

The engine keeps practical original asset formats such as TXT gameplay tables, BMP-style
art assets, WAV sound effects, MP3/FLAC music, and OGV video. Legacy archive and video
container formats are replaced for runtime use.

## Building

Requirements:

- CMake 3.24 or newer
- C++20 compiler
- Lua 5.3 or 5.4 development package
- standard native build tools for your platform

Configure and build:

```sh
cmake -S . -B build
cmake --build build --target openyamm -j25
```

Run:

```sh
./build/game/openyamm
```

Build tests:

```sh
cmake --build build --target openyamm_unit_tests -j25
./build/tests/openyamm_unit_tests
```

Build the editor:

```sh
cmake -S . -B build -DOPENYAMM_BUILD_EDITOR=ON
cmake --build build --target openyamm-editor -j25
```

Run the editor:

```sh
./build/editor/openyamm-editor
```

## Nightly Builds

The [Nightly releases](https://github.com/pjasicek/openyamm/actions/workflows/nightly.yml) GitHub Actions workflow
builds unsigned Windows x64, x86_64 Flatpak, and signed Android arm64 packages every day at 03:27 UTC. After all
packages pass structural and checksum checks, the workflow updates the rolling
[nightly prerelease](https://github.com/pjasicek/openyamm/releases/tag/nightly). The same workflow can be run manually,
with publishing optionally disabled so the packages remain short-lived workflow artifacts.

Every push to `main` also gets its own package-build run for Windows x64, x86_64 Flatpak, and Android arm64. Open the
commit's `Package builds` run on the Actions page to download its SHA-named artifacts. Commit artifacts are retained for
one day; scheduled builds continue to update the rolling nightly prerelease.

Extract the Windows zip and run `openyamm.exe`. Install or update the Flatpak bundle with:

```sh
flatpak install --user --reinstall OpenYAMM-nightly-x86_64.flatpak
```

On an arm64 Android 6.0 or newer device, download `OpenYAMM-nightly-android-arm64.apk` and open it to install or update
the nightly. Android may ask you to allow installs from the browser or file manager used to open the APK.

Nightly packages are development snapshots and may be unstable. Back up existing saves before using them. Publishing
packaged game assets remains subject to the asset distribution rights noted in the Windows distribution documentation.

## Tagged Releases

Pushing a canonical `X.Y` tag, such as `0.7`, runs the same validated package builds and creates a normal GitHub
release for that tag. The release contains versioned Windows, Flatpak, and signed Android packages plus SHA256 files.
The Android version name matches the tag; its monotonically increasing version code is calculated as
`major * 10000 + minor * 100` (`0.7` becomes `700`).

Create a release only after the workflow changes are present on the commit being tagged:

```sh
git tag -a 0.7 -m "OpenYAMM 0.7"
git push origin 0.7
```

The workflow rejects non-canonical versions, does not mark tagged releases as prereleases, and does not overwrite an
existing release with the same tag.

## Useful CMake Options

```text
OPENYAMM_BUILD_EDITOR=ON      Build the editor target
OPENYAMM_BUILD_TOOLS=ON       Build asset and data tooling
OPENYAMM_BUILD_TESTS=ON       Build unit tests
OPENYAMM_DEV_ASSETS_DIR=...   Override the development asset directory
OPENYAMM_USE_SYSTEM_SDL3=ON   Use an installed SDL3 package
```

## Repository Layout

```text
engine/              shared runtime systems
game/                game application and gameplay systems
editor/              editor application
tools/               asset and data tools
tests/               unit and regression tests
assets_dev/          development asset root
assets_editor_dev/   editor development asset root
res/                 README screenshots
```

## License

No license has been declared yet.

## Screenshots

![Gameplay screenshot 1](res/ss_1.webp)
![Gameplay screenshot 2](res/ss_2.webp)
![Gameplay screenshot 3](res/ss_3.webp)
![Gameplay screenshot 4](res/ss_4.webp)
![Gameplay screenshot 5](res/ss_5.webp)
![Gameplay screenshot 6](res/ss_6.webp)
![Gameplay screenshot 7](res/ss_7.webp)
![Gameplay screenshot 8](res/ss_8.webp)

## Editor Screenshots

![Editor screenshot 1](res/editor_1.webp)
![Editor screenshot 2](res/editor_2.webp)

## Credits

OpenYAMM builds on years of work and research from the Might and Magic community,
with thanks due to:

- the OpenEnroth development team
- Rodril and all MMMerge contributors
