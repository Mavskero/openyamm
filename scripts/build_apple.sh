#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
target_platform="${1:-macos}"
cmake_program="${CMAKE:-cmake}"
build_jobs="${OPENYAMM_BUILD_JOBS:-6}"
host_build="${repo_root}/build-apple"

case "$target_platform" in
    macos) ;;
    ios|simulator)
        sdk_name=iphoneos
        [[ "$target_platform" == simulator ]] && sdk_name=iphonesimulator
        if ! xcrun --sdk "$sdk_name" --show-sdk-path >/dev/null 2>&1; then
            echo "Missing $sdk_name SDK. Install/select full Xcode before building this target." >&2
            exit 2
        fi
        ;;
    *) echo "Usage: $0 [macos|ios|simulator]" >&2; exit 2 ;;
esac

"$cmake_program" -S "$repo_root" -B "$host_build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DOPENYAMM_FETCH_DEPENDENCIES_ONLY=ON \
    -DOPENYAMM_BUILD_TESTS=OFF

bgfx_source="$host_build/_deps/bgfx-src"
BX_DIR="$host_build/_deps/bx-src" BIMG_DIR="$host_build/_deps/bimg-src" \
    make -C "$bgfx_source" shaderc -j"$build_jobs"
shader_compiler="$bgfx_source/tools/bin/darwin/shaderc"

# Restore the host targets after bootstrapping, including when cross-compiling.
# Otherwise an iOS build leaves menu_import and apple_import unavailable to Ninja.
"$cmake_program" -S "$repo_root" -B "$host_build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DOPENYAMM_FETCH_DEPENDENCIES_ONLY=OFF \
    -DOPENYAMM_HOST_SHADERC="$shader_compiler" -DOPENYAMM_BUILD_TESTS=OFF

build_directory="$host_build"
if [[ "$target_platform" != macos ]]; then
    build_directory="$repo_root/build-$target_platform"
    "$cmake_program" -S "$repo_root" -B "$build_directory" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DOPENYAMM_FETCH_DEPENDENCIES_ONLY=OFF \
        -DOPENYAMM_HOST_SHADERC="$shader_compiler" -DOPENYAMM_BUILD_TESTS=OFF \
        -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 \
        -DCMAKE_OSX_SYSROOT="$sdk_name" -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0 \
        -DOPENYAMM_USE_SYSTEM_SDL3=OFF -DOPENYAMM_BUILD_DESKTOP_EXECUTABLE=OFF
fi
"$cmake_program" --build "$build_directory" --parallel "$build_jobs"
