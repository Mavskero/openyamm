#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cmake_program="${CMAKE:-cmake}"
shader_compiler="$repo_root/build-apple/_deps/bgfx-src/tools/bin/darwin/shaderc"
if [[ ! -x "$shader_compiler" ]]; then
    echo "Run scripts/build_apple.sh ios first to prepare the shader compiler and dependencies." >&2
    exit 2
fi
xcrun --sdk iphoneos --show-sdk-path >/dev/null
source_args=()
for dependency in sdl3 lua fmt spdlog freetype physfs yaml-cpp imgui bx bimg bgfx ffmpeg; do
    dependency_source="$repo_root/build-ios/_deps/$dependency-src"
    if [[ -d "$dependency_source" ]]; then
        dependency_upper="$(printf '%s' "$dependency" | tr '[:lower:]' '[:upper:]')"
        source_args+=("-DFETCHCONTENT_SOURCE_DIR_$dependency_upper=$dependency_source")
    fi
done
"$cmake_program" -S "$repo_root" -B "$repo_root/build-ios-xcode" -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0 -DCMAKE_CONFIGURATION_TYPES=Release \
    -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO \
    -DOPENYAMM_BUILD_TESTS=OFF -DOPENYAMM_BUILD_DESKTOP_EXECUTABLE=OFF \
    -DOPENYAMM_USE_SYSTEM_SDL3=OFF -DOPENYAMM_HOST_SHADERC="$shader_compiler" \
    -DOPENYAMM_DEVELOPMENT_TEAM="${OPENYAMM_DEVELOPMENT_TEAM:-}" \
    "${source_args[@]}"
echo "Xcode project: $repo_root/build-ios-xcode/OpenYAMM.xcodeproj"
