#!/usr/bin/env python3
"""Prepare the 195 iPad handoff locally; never build, upload, sign, or run an App.

Run only after committing the final production source. --runtime must contain all
six final runtime files; all six are copied as flat release attachments. Install
the four world ZIPs under worlds/ before checking SHA256SUMS. Existing generated
outputs are never reused; a prewritten README is preserved.
"""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import plistlib
import re
import shutil
import stat
import subprocess
import sys
import tarfile
import traceback
import zipfile


ROOT = Path(__file__).resolve().parents[3]
TAG = "ipados-ui-function-195-20261029"
RUNTIME_NAMES = ("engine.zip", "conversion.json", "worlds/mm6.zip", "worlds/mm7.zip",
                 "worlds/mm8.zip", "worlds/mmmerge.zip")
RAW_TREES = {"assets_dev", "assets_editor_dev", "data"}
SOURCE_ASSET_PREFIXES = ("assets_dev/engine/scripts/", "assets_dev/engine/rendering/",
                         "assets_dev/engine/ui/")
SIGNATURE_NAMES = {"_CodeSignature", "embedded.mobileprovision", "CodeResources"}
MACHO_MAGIC = {bytes.fromhex(value) for value in (
    "feedface", "cefaedfe", "feedfacf", "cffaedfe", "cafebabe", "bebafeca", "cafebabf", "bfbafeca")}


def now():
    return datetime.now(timezone.utc).isoformat()


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def sha(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def write_json(path, value):
    with path.open("x", encoding="utf-8") as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2)
        stream.write("\n")


class Commands:
    def __init__(self, release):
        self.log = release / "evidence/prepare-commands.jsonl"
        self.log.parent.mkdir()

    def run(self, argv, cwd=None, check=True):
        result = subprocess.run([str(item) for item in argv], cwd=cwd, capture_output=True,
                                env={**os.environ, "LC_ALL": "C", "LANG": "C"})
        record = {"time": now(), "argv": [str(item) for item in argv], "cwd": str(cwd) if cwd else None,
                  "exit_code": result.returncode, "stdout_bytes": len(result.stdout),
                  "stdout_sha256": hashlib.sha256(result.stdout).hexdigest(),
                  "stdout": result.stdout[:4096].decode("utf-8", errors="replace"),
                  "stderr": result.stderr.decode("utf-8", errors="replace")}
        with self.log.open("a", encoding="utf-8") as stream:
            stream.write(json.dumps(record, ensure_ascii=False) + "\n")
        require(not check or result.returncode == 0, f"Command failed ({result.returncode}): {argv}")
        return result


def bundle_manifest(bundle):
    entries = []
    for path in sorted(bundle.rglob("*")):
        name = path.relative_to(bundle).as_posix()
        mode = stat.S_IMODE(path.lstat().st_mode)
        if path.is_symlink():
            require(path.resolve().is_relative_to(bundle), f"Nonportable App symlink: {name}")
            entries.append({"path": name, "mode": mode, "symlink": os.readlink(path)})
        elif path.is_file():
            entries.append({"path": name, "mode": mode, "bytes": path.stat().st_size, "sha256": sha(path)})
    return entries


def zip_app(app, target):
    with zipfile.ZipFile(target, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for path in [app, *sorted(app.rglob("*"))]:
            name = path.relative_to(app.parent).as_posix()
            if path.is_symlink():
                info = zipfile.ZipInfo(name)
                info.create_system = 3
                info.external_attr = path.lstat().st_mode << 16
                archive.writestr(info, os.readlink(path).encode("utf-8"))
            else:
                archive.write(path, name)
    with zipfile.ZipFile(target) as archive:
        require(archive.testzip() is None, "Copied App ZIP failed CRC verification")
        require(not any(part in SIGNATURE_NAMES for name in archive.namelist()
                        for part in PurePosixPath(name).parts), "Signature metadata entered the App ZIP")


def source_snapshot(repo, release, head, commands):
    raw = commands.run(["git", "ls-tree", "-rlz", head], cwd=repo).stdout
    tree = {}
    for row in raw.split(b"\0"):
        if row:
            metadata, name = row.split(b"\t", 1)
            mode, kind, oid, size = metadata.decode().split()
            tree[name.decode("utf-8")] = {"git_mode": mode, "git_type": kind, "git_blob_oid": oid,
                                         "bytes": int(size) if size.isdigit() else None}
    owned_path = "assets_dev/engine/ui/gameplay/owned_images.txt"
    owned_text = commands.run(["git", "show", f"{head}:{owned_path}"], cwd=repo).stdout.decode("utf-8")
    owned_images = []
    for line in owned_text.splitlines():
        name = line.strip()
        if name and not name.startswith("#"):
            require(PurePosixPath(name).name == name and name not in (".", ".."), "Invalid owned image name")
            owned_images.append("assets_dev/engine/icons/" + name)
    require(owned_images and len(owned_images) == len(set(owned_images)), "Invalid owned image source list")
    require(all(name in tree for name in owned_images), "A required owned image is absent from committed source")
    selected = {name for name in tree if name.split("/")[0] not in RAW_TREES
                or name.startswith(SOURCE_ASSET_PREFIXES) or name in owned_images}
    required = {"CMakeLists.txt", "engine/CMakeLists.txt", "game/CMakeLists.txt", "ios/CMakeLists.txt",
                "ios/IOSMain.mm", "ios/Info.plist.in", "scripts/build_apple.sh", "scripts/configure_ipados_xcode.sh",
                "cmake/Dependencies.cmake", "cmake/BgfxRuntime.cmake", "cmake/Ffmpeg.cmake",
                "cmake/PatchSdlCocoa.cmake", "cmake/PatchSdlUIKit.cmake", "cmake/PatchBgfxMetal.cmake",
                "cmake/patches/sdl-cocoa-button-position.patch", "cmake/patches/sdl-cocoa-button-release.patch",
                "cmake/patches/sdl-uikit-scene-bounds.patch", "cmake/patches/bgfx-metal-ios-depth-format.patch",
                "third_party/doctest/doctest/doctest.h", "settings_release.ini",
                "assets_dev/engine/scripts/common/event_support.lua", owned_path, *owned_images}
    require(required <= selected, f"Committed build inputs missing: {sorted(required - selected)}")
    require(all(tree[name]["git_type"] == "blob" for name in selected), "Unarchived dependency/submodule in source")
    top_names = {name.split("/")[0] for name in selected if name.split("/")[0] not in RAW_TREES}
    selectors = sorted(top_names) + [prefix.rstrip("/") for prefix in SOURCE_ASSET_PREFIXES] + owned_images
    archive_path = release / "source.tar.gz"
    commands.run(["git", "-c", "tar.umask=0022", "archive", "--format=tar.gz", f"--output={archive_path}", "--prefix=openyamm/",
                  head, "--", *selectors], cwd=repo)
    object_format = commands.run(["git", "rev-parse", "--show-object-format"], cwd=repo).stdout.decode().strip()
    entries = []
    with tarfile.open(archive_path, "r:gz") as archive:
        for member in archive:
            if member.isdir():
                continue
            name = member.name.removeprefix("openyamm/")
            require(name in selected and ".." not in PurePosixPath(name).parts, f"Unexpected archive entry: {name}")
            entry = {"path": name, "mode": member.mode, "git_blob_oid": tree[name]["git_blob_oid"]}
            if member.issym():
                content = member.linkname.encode("utf-8")
                blob = hashlib.new(object_format, f"blob {len(content)}\0".encode() + content)
                entry["symlink"] = member.linkname
            else:
                require(member.isfile(), f"Unsupported archive entry: {name}")
                digest = hashlib.sha256()
                blob = hashlib.new(object_format, f"blob {member.size}\0".encode())
                with archive.extractfile(member) as stream:
                    for block in iter(lambda: stream.read(1024 * 1024), b""):
                        digest.update(block)
                        blob.update(block)
                entry.update(bytes=member.size, sha256=digest.hexdigest())
            require(blob.hexdigest() == tree[name]["git_blob_oid"], f"Archive content differs from HEAD: {name}")
            entries.append(entry)
    require({entry["path"] for entry in entries} == selected, "Git archive omitted selected source files")
    dependencies = commands.run(["git", "show", f"{head}:cmake/Dependencies.cmake"], cwd=repo).stdout
    manifest = {"source_head": head, "git_object_format": object_format, "archive": archive_path.name,
                "archive_bytes": archive_path.stat().st_size, "archive_sha256": sha(archive_path),
                "archive_root": "openyamm/", "files": sorted(entries, key=lambda entry: entry["path"]),
                "scope": "All committed code/configuration/docs plus RC3-style build-required source assets",
                "excluded_resource_trees": sorted(RAW_TREES), "included_asset_prefixes": SOURCE_ASSET_PREFIXES,
                "owned_ui_image_count": len(owned_images),
                "excluded": "Other raw game assets, Git history, working files, builds and fetched dependency caches",
                "dependency_versions": dict(re.findall(r'set\(OPENYAMM_([A-Z0-9_]+)_VERSION\s+"([^"]+)"',
                                                       dependencies.decode())),
                "dependency_declaration_sha256": hashlib.sha256(dependencies).hexdigest(),
                "fresh_build": "NOT_RUN; requires the receiving Mac toolchain, network FetchContent and host shaderc",
                "archive_verification": "Each archived file or symlink matches its committed Git blob; no fresh build"}
    write_json(release / "SOURCE-MANIFEST.json", manifest)
    return manifest


def prepare(args, release):
    commands = Commands(release)
    repo = args.repo.resolve(strict=True)
    runtime = args.runtime.resolve(strict=True)
    source_app = args.ios_app.resolve(strict=True)
    require(sys.platform == "darwin", "Prepare this iOS App copy on macOS")
    require(source_app.is_dir() and source_app.suffix == ".app", "--ios-app must be a built .app directory")
    for source in (repo, runtime, source_app):
        require(not release.is_relative_to(source), "Output must be outside each source directory")
    head = commands.run(["git", "rev-parse", "HEAD"], cwd=repo).stdout.decode().strip()
    require(not args.expected_head or head == args.expected_head, "HEAD differs from --expected-head")
    commands.run(["git", "diff", "--quiet", "HEAD", "--"], cwd=repo)
    untracked = commands.run(["git", "ls-files", "--others", "--exclude-standard", "-z", "--",
                              "game", "engine", "cmake", "ios", "scripts", "packaging", "tools", "tests",
                              "third_party", "CMakeLists.txt", "settings_release.ini"], cwd=repo).stdout
    require(not untracked, "Untracked source/build inputs must be committed before archiving")
    runtime_entries = []
    source_stats = {}
    for name in RUNTIME_NAMES:
        source = runtime / name
        require(source.is_file(), f"Missing complete runtime file: {name}")
        source_stats[name] = source.stat()
        entry = {"path": name, "bytes": source.stat().st_size, "sha256": sha(source),
                 "included_in_release": True, "release_asset_name": PurePosixPath(name).name}
        target = release / entry["release_asset_name"]
        copied = commands.run(["/bin/cp", "-c", source, target], check=False)
        if copied.returncode:
            require(not target.exists(), f"Failed clone left a partial file: {target}")
            shutil.copy2(source, target)
        require(target.is_file() and not target.is_symlink() and sha(target) == entry["sha256"],
                f"Runtime copy differs: {name}")
        runtime_entries.append(entry)
    with (release / "SHA256SUMS").open("x") as stream:
        for entry in runtime_entries:
            stream.write(f"{entry['sha256']}  {entry['path']}\n")

    before_app = bundle_manifest(source_app)
    with (source_app / "Info.plist").open("rb") as stream:
        app_info = plistlib.load(stream)
    executable_name = app_info["CFBundleExecutable"]
    require(PurePosixPath(executable_name).name == executable_name, "Invalid CFBundleExecutable")
    copied_app = release / "iPadOS-unsigned" / source_app.name
    copied_app.parent.mkdir()
    shutil.copytree(source_app, copied_app, symlinks=True, ignore=shutil.ignore_patterns(*SIGNATURE_NAMES))
    stripped = []
    for path in sorted(copied_app.rglob("*")):
        if not path.is_file() or path.is_symlink():
            continue
        with path.open("rb") as stream:
            if stream.read(4) not in MACHO_MAGIC:
                continue
        display = commands.run(["/usr/bin/codesign", "--display", path], check=False)
        if display.returncode == 0:
            commands.run(["/usr/bin/codesign", "--remove-signature", path])
            stripped.append(path.relative_to(copied_app).as_posix())
        else:
            require(b"not signed at all" in display.stderr, f"Unrecognized copied signature state: {path}")
    require((copied_app / executable_name).is_file(), "Copied App executable is missing")
    verify = commands.run(["/usr/bin/codesign", "--verify", "--strict", copied_app], check=False)
    require(verify.returncode != 0 and b"not signed at all" in verify.stderr, "Copied App is not demonstrably unsigned")
    require(not any(path.name in SIGNATURE_NAMES for path in copied_app.rglob("*")), "Signature metadata remains")
    after_app = bundle_manifest(copied_app)
    write_json(release / "evidence/ios-app-manifest.json", {"source": before_app, "unsigned_copy": after_app,
                                                         "removed_embedded_signatures": stripped})
    app_zip = release / "OpenYAMM-195-iPadOS-unsigned.zip"
    zip_app(copied_app, app_zip)
    source_manifest = source_snapshot(repo, release, head, commands)
    require(bundle_manifest(source_app) == before_app, "Original iOS App changed during preparation")
    for name, previous in source_stats.items():
        current = (runtime / name).stat()
        require((previous.st_dev, previous.st_ino, previous.st_size, previous.st_mtime_ns)
                == (current.st_dev, current.st_ino, current.st_size, current.st_mtime_ns),
                f"Original runtime changed during preparation: {name}")
    require(commands.run(["git", "rev-parse", "HEAD"], cwd=repo).stdout.decode().strip() == head,
            "Source HEAD changed during preparation")
    commands.run(["git", "diff", "--quiet", "HEAD", "--"], cwd=repo)
    version = {"release_tag": TAG, "prepared_at": now(), "checkpoint": 195, "save_version": 104,
               "source_head": head, "source_archive": source_manifest["archive"],
               "source_archive_sha256": source_manifest["archive_sha256"],
               "source_manifest_sha256": sha(release / "SOURCE-MANIFEST.json"),
               "source_scope": source_manifest["scope"], "runtime_source": str(runtime),
               "runtime_payloads": runtime_entries, "runtime_files_all_included": True,
               "runtime_installation": "All six files are included; place mm6/mm7/mm8/mmmerge.zip under worlds/",
               "ios": {"build_input_app": str(source_app), "bundle_identifier": app_info.get("CFBundleIdentifier"),
                       "minimum_os_version": app_info.get("MinimumOSVersion"), "archive": app_zip.name,
                       "archive_bytes": app_zip.stat().st_size, "archive_sha256": sha(app_zip),
                       "input_executable_sha256": sha(source_app / executable_name),
                       "unsigned_executable_sha256": sha(copied_app / executable_name),
                       "embedded_signatures_removed": stripped, "unsigned_copy_verified": True,
                       "requires_receiving_mac_signing": True},
               "original_inputs_unchanged": True, "world_archives_included": True, "world_archives_repacked": False,
               "device_test": "NOT_RUN_BY_PACKAGER", "fresh_source_build": "NOT_RUN_BY_PACKAGER",
               "app_run": "NOT_RUN_BY_PACKAGER", "publication_status_at_preparation": "NOT_YET_UPLOADED",
               "publication_receipt": "See release delivery.json after publication", "mac_package": False}
    write_json(release / "VERSION.json", version)
    publish_names = [PurePosixPath(name).name for name in RUNTIME_NAMES] + [
        "SHA256SUMS", "OpenYAMM-195-iPadOS-unsigned.zip", "source.tar.gz", "SOURCE-MANIFEST.json", "VERSION.json"]
    if (release / "README-IPAD-195.md").is_file():
        publish_names.append("README-IPAD-195.md")
    with (release / "ARTIFACT-SHA256SUMS").open("x") as stream:
        for name in publish_names:
            stream.write(f"{sha(release / name)}  {name}\n")
    write_json(release / "evidence/preparation-result.json", {"status": "PREPARED_LOCAL_ONLY", "finished_at": now(),
                                                             "source_head": head, "release": str(release),
                                                             "publish_files": list(publish_names) + ["ARTIFACT-SHA256SUMS"]})
    print(json.dumps({"status": "PREPARED_LOCAL_ONLY", "release": str(release), "source_head": head,
                      "release_tag": TAG}, ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--ios-app", type=Path, required=True)
    parser.add_argument("--expected-head", help="Exact final committed HEAD; recommended")
    parser.add_argument("--repo", type=Path, default=ROOT / "openyamm")
    parser.add_argument("--output", type=Path, default=ROOT / "releases/ipados-remote-test-195-20261029")
    args = parser.parse_args()
    release = args.output.absolute()
    require(not release.is_symlink(), f"Output must not be a symlink: {release}")
    if release.exists():
        require(release.is_dir(), f"Output already exists and is not a directory: {release}")
        require(all(path.name == "README-IPAD-195.md" and path.is_file() and not path.is_symlink()
                    for path in release.iterdir()), f"Output contains an existing artifact; preserve it: {release}")
    for source in (args.repo, args.runtime, args.ios_app):
        require(not release.resolve().is_relative_to(source.resolve()), "Output must be outside each source directory")
    release.parent.mkdir(parents=True, exist_ok=True)
    release.mkdir(exist_ok=True)
    release = release.resolve()
    try:
        prepare(args, release)
    except BaseException as error:
        write_json(release / "PREPARATION-FAILED.json", {"status": "FAILED_INPUTS_AND_PARTIAL_OUTPUT_PRESERVED",
                                                       "time": now(), "error": str(error),
                                                       "traceback": traceback.format_exc()})
        raise


if __name__ == "__main__":
    main()
