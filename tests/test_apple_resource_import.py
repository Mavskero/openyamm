"""Exercise the same native importer compiled into the iPad app (macOS only)."""
import fcntl
import json
import os
import re
from pathlib import Path
import signal
import subprocess
import time
import uuid
import sys
import zipfile

import pytest

TOOL = Path(os.environ.get("APPLE_IMPORT_TOOL", Path(__file__).parents[1] / "build-apple/apple_import"))
pytestmark = pytest.mark.skipif(sys.platform != "darwin" or not TOOL.is_file(), reason="Build apple_import on macOS")


def source_files(root):
    names = [f"mm{v}.{kind}.lod" for v in (6, 7, 8) for kind in ("games", "icons", "bitmaps")]
    names += ["mmmerge.T.lod", "mmmerge.D.lod"]
    (root / "Data").mkdir(parents=True)
    for name in names:
        (root / "Data" / name).write_bytes(b"LOD\0" + bytes(284))
    (root / "Data/中文文本.txt").write_bytes("魔法门".encode("gb2312"))
    (root / "Data/unused.dll").write_bytes(b"not needed")
    return root


def run(source, support):
    return subprocess.run([str(TOOL), str(source), str(support)], capture_output=True, text=True)


def active(support):
    return json.loads((support / "GameImports/active.json").read_text())


def resource_status(support):
    selection = support / "GameImports/active.json"
    before = selection.read_bytes()
    result = subprocess.run([str(TOOL), "--status", str(support)], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    assert selection.read_bytes() == before
    return json.loads(result.stdout)


def portable_runtime(root, audit=None, include_manifest=True):
    (root / "worlds").mkdir(parents=True)
    for name in ("engine.zip", "worlds/mm6.zip", "worlds/mm7.zip", "worlds/mm8.zip"):
        with zipfile.ZipFile(root / name, "w") as pack:
            pack.writestr("fixture.txt", "Metadata fixture only, not validated game content")
    if include_manifest:
        manifest = {"schema": 1, "runtime_ready": False}
        if audit is not None:
            manifest["resource_audit"] = audit
        (root / "conversion.json").write_text(json.dumps(manifest))
    return root


def resource_audit(missing=True):
    return {
        "schema": 1, "base_archives_checked": True, "chest_backgrounds_checked": True,
        "missing_mm8_base_archives": ["Data/bitmaps.lod", "Data/sprites.lod", "Data/icons.lod"] if missing else [],
        "missing_chest_backgrounds": [f"icons/chest0{i}.bmp" for i in range(1, 9)] if missing else [],
    }


def test_source_resource_status_is_readonly_and_does_not_claim_chest_validation(tmp_path):
    source = source_files(tmp_path / "source")
    # A directory with a base archive's name must not count as a file.
    (source / "Data/icons.lod").mkdir()
    support = tmp_path / "support"
    assert run(source, support).returncode == 0
    status = resource_status(support)
    assert status["base_archives_checked"] is True
    assert status["chest_backgrounds_checked"] is False
    assert status["missing_mm8_base_archives"] == resource_audit()["missing_mm8_base_archives"]
    assert status["has_known_gaps"] is True and "完整 MM8 安装目录" in status["message"]


def test_portable_runtime_keeps_known_gaps_without_original_source_directory(tmp_path):
    source = portable_runtime(tmp_path / "runtime", resource_audit())
    support = tmp_path / "support"
    assert run(source, support).returncode == 0
    record = active(support)
    assert "source_id" not in record
    assert record["resource_audit"] == resource_audit()
    status = resource_status(support)
    assert status["has_known_gaps"] is True
    assert status["missing_chest_backgrounds"] == resource_audit()["missing_chest_backgrounds"]
    assert "缺少 8 张宝箱背景" in status["message"]
    assert record["runtime_validated"] is False


@pytest.mark.parametrize("include_manifest", [True, False])
def test_old_portable_runtime_is_unknown_not_complete(tmp_path, include_manifest):
    source = portable_runtime(tmp_path / "runtime", include_manifest=include_manifest)
    support = tmp_path / "support"
    assert run(source, support).returncode == 0
    status = resource_status(support)
    assert status["base_archives_checked"] is False
    assert status["chest_backgrounds_checked"] is False
    assert "尚未核查" in status["message"]
    assert active(support)["runtime_validated"] is False


@pytest.mark.parametrize("change", [
    {"schema": 2}, {"base_archives_checked": "true"}, {"chest_backgrounds_checked": False},
    {"missing_mm8_base_archives": "Data/icons.lod"},
    {"missing_chest_backgrounds": ["../../unexpected.bmp"]},
    {"missing_chest_backgrounds": ["icons/chest01.bmp", "icons/chest01.bmp"]},
])
def test_invalid_or_future_audit_is_not_shown_as_validated(tmp_path, change):
    audit = resource_audit() | change
    source = portable_runtime(tmp_path / "runtime", audit)
    support = tmp_path / "support"
    assert run(source, support).returncode == 0
    assert "resource_audit" not in active(support)
    assert "尚未核查" in resource_status(support)["message"]


@pytest.mark.parametrize("payload", [b"x" * (1024 * 1024 + 1), b"{broken json"], ids=["oversized", "malformed"])
def test_oversized_or_invalid_manifest_does_not_block_status_or_claim_complete(tmp_path, payload):
    source = portable_runtime(tmp_path / "runtime")
    (source / "conversion.json").write_bytes(payload)
    support = tmp_path / "support"
    assert run(source, support).returncode == 0
    assert "尚未核查" in resource_status(support)["message"]


def test_legacy_native_conversion_keeps_its_preexisting_missing_base_warning(tmp_path):
    source = portable_runtime(tmp_path / "runtime")
    support = tmp_path / "support"
    assert run(source, support).returncode == 0
    path = support / "GameImports/active.json"
    record = active(support)
    record["missing_mm8_base_archives"] = ["Data/icons.lod"]
    path.write_text(json.dumps(record))
    status = resource_status(support)
    assert status["missing_mm8_base_archives"] == ["Data/icons.lod"]
    assert status["has_known_gaps"] is True
    assert status["chest_backgrounds_checked"] is False


def test_reimporting_updated_profile_replaces_warning_without_deleting_previous_data(tmp_path):
    incomplete = portable_runtime(tmp_path / "incomplete", resource_audit())
    corrected = portable_runtime(tmp_path / "corrected", resource_audit(False))
    support = tmp_path / "support"
    assert run(incomplete, support).returncode == 0
    old = active(support)
    old_zip = support / "GameImports" / old["id"] / "engine.zip"
    old_bytes = old_zip.read_bytes()
    assert run(corrected, support).returncode == 0
    assert active(support)["id"] != old["id"]
    assert old_zip.read_bytes() == old_bytes
    status = resource_status(support)
    assert status["has_known_gaps"] is False
    assert status["base_archives_checked"] is True and status["chest_backgrounds_checked"] is True
    assert "其它素材与完整玩法仍在验证" in status["message"]
    assert active(support)["runtime_validated"] is False


def test_raw_import_preserves_original_bytes_and_records_unconverted_state(tmp_path):
    source = source_files(tmp_path / "source")
    support = tmp_path / "support"
    result = run(source, support)
    assert result.returncode == 0, result.stderr
    record = active(support)
    target = support / "GameImports" / record["id"]
    assert record["kind"] == "source"
    assert record["runtime_validated"] is False
    assert record["missing_mm8_base_archives"] == ["Data/bitmaps.lod", "Data/sprites.lod", "Data/icons.lod"]
    assert (target / "Data/中文文本.txt").read_bytes() == (source / "Data/中文文本.txt").read_bytes()
    assert (source / "Data/unused.dll").exists()
    assert not (target / "Data/unused.dll").exists()


def test_failed_import_keeps_previous_active_copy(tmp_path):
    source = source_files(tmp_path / "source")
    support = tmp_path / "support"
    assert run(source, support).returncode == 0
    original = active(support)
    (source / "Data/mm8.games.lod").write_bytes(b"BAD!")
    assert run(source, support).returncode != 0
    assert active(support) == original
    assert (support / "GameImports" / original["id"] / "Data/mm8.games.lod").read_bytes().startswith(b"LOD\0")


def test_only_root_mm8_executable_is_preserved_as_nonexecutable_data(tmp_path):
    source = source_files(tmp_path / "source")
    support = tmp_path / "support"
    payload = b"MZ original file data; never execute"
    (source / "MM8.EXE").write_bytes(payload)
    (source / "MM8.EXE").chmod(0o755)
    (source / "MM8Setup.Exe").write_bytes(b"excluded installer")
    (source / "Data/mm8.exe").write_bytes(b"excluded nested executable")
    result = run(source, support)
    assert result.returncode == 0, result.stderr
    record = active(support)
    target = support / "GameImports" / record["id"]
    assert (target / "mm8.exe").read_bytes() == payload
    assert (target / "mm8.exe").stat().st_mode & 0o777 == 0o644
    assert (source / "MM8.EXE").stat().st_mode & 0o777 == 0o755
    assert not (target / "MM8Setup.Exe").exists()
    assert not (target / "Data/mm8.exe").exists()
    assert record["runtime_validated"] is False


def test_symlink_rolls_back_staging_without_touching_target(tmp_path):
    source = source_files(tmp_path / "source")
    support = tmp_path / "support"
    outside = tmp_path / "outside.txt"
    outside.write_text("unchanged")
    (source / "Data/external.txt").symlink_to(outside)
    assert run(source, support).returncode != 0
    assert outside.read_text() == "unchanged"
    assert {p.name for p in (support / "GameImports").iterdir()} == {".operation.lock"}


def test_rejects_nested_destination_before_creating_it(tmp_path):
    source = source_files(tmp_path / "source")
    support = source / "new-support"
    assert run(source, support).returncode != 0
    assert not support.exists()


def test_missing_archive_does_not_publish_selection(tmp_path):
    source = source_files(tmp_path / "source")
    (source / "Data/mm6.icons.lod").unlink()
    support = tmp_path / "support"
    assert run(source, support).returncode != 0
    assert not (support / "GameImports/active.json").exists()


def test_runtime_packages_are_identified_without_claiming_content_validation(tmp_path):
    source = tmp_path / "packages"
    source.mkdir()
    (source / "worlds").mkdir()
    for name in ("engine", "mm6", "mm7", "mm8"):
        path = source / (f"{name}.zip" if name == "engine" else f"worlds/{name}.zip")
        with zipfile.ZipFile(path, "w") as pack:
            pack.writestr("example.txt", "fixture, not gameplay assets")
    manifest = '{"stage":"native-runtime-assembly","default_world":"mm6","runtime_ready":false}\n'
    (source / "conversion.json").write_text(manifest)
    support = tmp_path / "support"
    assert run(source, support).returncode == 0
    record = active(support)
    assert record["kind"] == "runtime"
    assert record["runtime_validated"] is False
    target = support / "GameImports" / record["id"]
    assert (target / "worlds/mm8.zip").read_bytes() == (source / "worlds/mm8.zip").read_bytes()
    assert (target / "conversion.json").read_text() == manifest


def test_flat_world_archives_are_rejected_because_engine_does_not_mount_them(tmp_path):
    source = tmp_path / "flat"
    source.mkdir()
    for name in ("engine", "mm6", "mm7", "mm8"):
        with zipfile.ZipFile(source / f"{name}.zip", "w") as pack:
            pack.writestr("example.txt", "fixture")
    assert run(source, tmp_path / "support").returncode != 0


def test_conversion_failure_keeps_source_selection_and_cleans_partial_output(tmp_path):
    source = source_files(tmp_path / "source")
    support = tmp_path / "support"
    assert run(source, support).returncode == 0
    before = active(support)
    layouts = Path(__file__).parents[1] / "assets_dev/engine/ui/gameplay"
    # The LOD header fixtures can be copied, but contain no resources for actual conversion.
    result = subprocess.run([str(TOOL), "--convert", str(support), str(layouts)], capture_output=True, text=True)
    assert result.returncode != 0
    assert "Runtime conversion failed at menu" in result.stderr
    assert active(support) == before
    assert {p.name for p in (support / "GameImports").iterdir()} == {"active.json", ".operation.lock", before["id"]}
    assert (source / "Data/mm8.games.lod").read_bytes() == b"LOD\0" + bytes(284)


def test_convert_without_an_active_source_does_not_create_storage(tmp_path):
    support = tmp_path / "support"
    layouts = Path(__file__).parents[1] / "assets_dev/engine/ui/gameplay"
    result = subprocess.run([str(TOOL), "--convert", str(support), str(layouts)], capture_output=True, text=True)
    assert result.returncode != 0
    assert not support.exists()


def recover(support):
    return subprocess.run([str(TOOL), "--recover", str(support)], capture_output=True, text=True)


def pending(imports, operation="convert", identifier=None):
    identifier = identifier or str(uuid.uuid4()).upper()
    journal = imports / f".pending-{identifier}.json"
    journal.write_text(json.dumps({"schema": 1, "id": identifier, "operation": operation}))
    return identifier, journal


def test_recovery_reclaims_interrupted_copy_conversion_and_legacy_staging(tmp_path):
    source = source_files(tmp_path / "source")
    support = tmp_path / "support"
    assert run(source, support).returncode == 0
    imports = support / "GameImports"
    original = (imports / "active.json").read_bytes()
    completed = imports / str(uuid.uuid4())
    completed.mkdir()
    (completed / "keep.txt").write_text("completed unselected legacy import")
    orphan_paths = []
    for operation, moved in [("copy", False), ("convert", False), ("copy", True)]:
        identifier, _ = pending(imports, operation)
        orphan = imports / (identifier if moved else f".staging-{identifier}")
        (orphan / ".work").mkdir(parents=True)
        (orphan / ".work/partial.dat").write_bytes(bytes(100))
        orphan_paths.append(orphan)
    legacy = imports / f".staging-{uuid.uuid4()}"
    legacy.mkdir()
    orphan_paths.append(legacy)
    result = recover(support)
    assert result.returncode == 0, result.stderr
    assert json.loads(result.stdout)["removed_incomplete_directories"] == 4
    assert all(not path.exists() for path in orphan_paths)
    assert not list(imports.glob(".pending-*"))
    assert (imports / "active.json").read_bytes() == original
    assert (completed / "keep.txt").read_text() == "completed unselected legacy import"
    assert (imports / active(support)["id"] / "Data/mm8.games.lod").exists()
    assert json.loads(recover(support).stdout)["removed_incomplete_directories"] == 0


def test_recovery_preserves_published_version_and_referenced_source(tmp_path):
    source = source_files(tmp_path / "source")
    support = tmp_path / "support"
    assert run(source, support).returncode == 0
    imports = support / "GameImports"
    original = active(support)
    source_id = original["id"]
    assert run(source, support).returncode == 0
    published = active(support)
    published["source_id"] = source_id.lower()
    (imports / "active.json").write_text(json.dumps(published))
    before = (imports / "active.json").read_bytes()
    pending(imports, "copy", source_id)
    pending(imports, "convert", published["id"])
    result = recover(support)
    assert result.returncode == 0, result.stderr
    assert json.loads(result.stdout)["finished_operations"] == 2
    assert (imports / "active.json").read_bytes() == before
    assert (imports / source_id / "Data/mm8.games.lod").exists()
    assert (imports / published["id"] / "Data/mm8.games.lod").exists()
    assert not list(imports.glob(".pending-*"))


@pytest.mark.parametrize("record", [[], {"id": "../outside", "kind": "source"}, "broken JSON"])
def test_unreadable_active_selection_keeps_unpublished_final_directory(tmp_path, record):
    support = tmp_path / "support"
    imports = support / "GameImports"
    imports.mkdir(parents=True)
    raw = record if isinstance(record, str) else json.dumps(record)
    (imports / "active.json").write_text(raw)
    identifier, journal = pending(imports)
    destination = imports / identifier
    destination.mkdir()
    (destination / "keep.txt").write_text("commit state cannot be proven")
    result = recover(support)
    assert result.returncode == 0, result.stderr
    assert json.loads(result.stdout)["preserved_unrecognized_operations"] == 1
    assert journal.exists() and destination.exists()
    assert (imports / "active.json").read_text() == raw


def test_malformed_future_and_link_operations_do_not_authorize_deletion(tmp_path):
    support = tmp_path / "support"
    imports = support / "GameImports"
    imports.mkdir(parents=True)
    outside = tmp_path / "outside"
    outside.mkdir()
    (outside / "precious.txt").write_text("unchanged")
    (imports / ".pending-.json").write_text("{}")
    future_id, future = pending(imports)
    future.write_text(json.dumps({"schema": 2, "id": future_id, "operation": "convert"}))
    future_stage = imports / f".staging-{future_id}"
    future_stage.mkdir()
    invalid_id, invalid = pending(imports)
    invalid.write_text(json.dumps({"schema": 1, "id": "../outside", "operation": "copy"}))
    invalid_final = imports / invalid_id
    invalid_final.mkdir()
    linked_id, linked = pending(imports)
    (imports / f".staging-{linked_id}").symlink_to(outside, target_is_directory=True)
    result = recover(support)
    assert result.returncode == 0, result.stderr
    assert json.loads(result.stdout)["removed_incomplete_directories"] == 0
    assert future_stage.exists() and invalid_final.exists() and linked.exists()
    assert (outside / "precious.txt").read_text() == "unchanged"


def test_recovery_rejects_symlinked_storage_root(tmp_path):
    support = tmp_path / "support"
    support.mkdir()
    outside = tmp_path / "outside"
    outside.mkdir()
    stage = outside / f".staging-{uuid.uuid4()}"
    stage.mkdir()
    (support / "GameImports").symlink_to(outside, target_is_directory=True)
    assert recover(support).returncode != 0
    assert stage.exists()
    assert not (outside / ".operation.lock").exists()


def test_live_operation_lock_prevents_recovery_and_second_import(tmp_path):
    source = source_files(tmp_path / "source")
    support = tmp_path / "support"
    assert run(source, support).returncode == 0
    imports = support / "GameImports"
    original = active(support)
    identifier, journal = pending(imports)
    stage = imports / f".staging-{identifier}"
    stage.mkdir()
    with (imports / ".operation.lock").open("r+") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        assert recover(support).returncode != 0
        assert run(source, support).returncode != 0
        assert stage.exists() and journal.exists() and active(support) == original
    assert recover(support).returncode == 0
    assert not stage.exists() and active(support) == original


def test_real_process_kill_releases_lock_and_recovers_partial_copy(tmp_path):
    source = source_files(tmp_path / "source")
    support = tmp_path / "support"
    assert run(source, support).returncode == 0
    imports = support / "GameImports"
    original = (imports / "active.json").read_bytes()
    for index in range(4000):
        (source / "Data" / f"payload-{index:04}.txt").write_bytes(b"owned test fixture" * 64)
    with (tmp_path / "interrupted-process.log").open("w") as log:
        process = subprocess.Popen([str(TOOL), str(source), str(support)], stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 10
            stage = None
            while time.monotonic() < deadline and process.poll() is None:
                candidates = list(imports.glob(".staging-*"))
                if candidates and len(list(candidates[0].glob("Data/*"))) >= 10:
                    stage = candidates[0]
                    break
                time.sleep(0.002)
            assert stage is not None and process.poll() is None, "No live partial import observed"
            process.send_signal(signal.SIGSTOP)
            _, status = os.waitpid(process.pid, os.WUNTRACED)
            assert os.WIFSTOPPED(status)
            assert recover(support).returncode != 0
            assert stage.exists() and (imports / "active.json").read_bytes() == original
            copied_files = len(list(stage.rglob("*.txt")))
            process.kill()
            assert process.wait(timeout=10) == -signal.SIGKILL
            result = recover(support)
            assert result.returncode == 0, result.stderr
            assert json.loads(result.stdout)["removed_incomplete_directories"] == 1
            assert not stage.exists() and not list(imports.glob(".pending-*"))
            assert (imports / "active.json").read_bytes() == original
            assert (source / "Data/payload-3999.txt").read_bytes() == b"owned test fixture" * 64
            (tmp_path / "process-evidence.json").write_text(json.dumps({
                "pid": process.pid, "live_partial_files": copied_files, "confirmed_stopped": True,
                "returncode": process.returncode, "lock_blocked_recovery_while_live": True,
                "post_kill_recovery": json.loads(result.stdout), "active_bytes_unchanged": True}))
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=10)


def test_publication_failure_rolls_back_new_version_and_keeps_existing_data(tmp_path):
    source = source_files(tmp_path / "source")
    support = tmp_path / "support"
    imports = support / "GameImports"
    imports.mkdir(parents=True)
    # A directory at the selection filename makes the final atomic write fail after the copy.
    selection = imports / "active.json"
    selection.mkdir()
    (selection / "keep.txt").write_text("existing contents")
    completed = imports / str(uuid.uuid4())
    completed.mkdir()
    (completed / "keep.txt").write_text("previous completed import")
    result = run(source, support)
    assert result.returncode != 0
    assert (selection / "keep.txt").read_text() == "existing contents"
    assert (completed / "keep.txt").read_text() == "previous completed import"
    assert {p.name for p in imports.iterdir()} == {".operation.lock", "active.json", completed.name}


def cancel_copy(source, support, *options):
    return subprocess.run([str(TOOL), *options, str(source), str(support)], capture_output=True, text=True, timeout=20)


def test_pre_cancelled_copy_does_not_create_storage_or_require_readable_source(tmp_path):
    support = tmp_path / "support"
    result = cancel_copy(tmp_path / "missing-source", support, "--cancel-before-start")
    assert result.returncode == 3, result.stderr
    assert "已取消" in result.stderr
    assert not support.exists()


def test_pre_cancelled_copy_does_not_run_recovery_or_change_existing_selection(tmp_path):
    source = source_files(tmp_path / "source")
    support = tmp_path / "support"
    assert run(source, support).returncode == 0
    imports = support / "GameImports"
    before = (imports / "active.json").read_bytes()
    stage = imports / f".staging-{uuid.uuid4()}"
    stage.mkdir()
    (stage / "keep.txt").write_text("pre-cancel must not start cleanup")
    result = cancel_copy(source, support, "--cancel-before-start")
    assert result.returncode == 3, result.stderr
    assert (stage / "keep.txt").read_text() == "pre-cancel must not start cleanup"
    assert (imports / "active.json").read_bytes() == before


@pytest.mark.parametrize("kind", ["source", "runtime"])
def test_cancel_during_large_file_preserves_prior_resources_and_allows_retry(tmp_path, kind):
    original = source_files(tmp_path / "original")
    support = tmp_path / "support"
    assert run(original, support).returncode == 0
    imports = support / "GameImports"
    before = (imports / "active.json").read_bytes()
    previous_id = active(support)["id"]
    source = tmp_path / "new-source"
    payload = b"cancel copy inside a single file\0" * 262144
    if kind == "source":
        source_files(source)
        (source / "Data/large-payload.bin").write_bytes(payload)
    else:
        (source / "worlds").mkdir(parents=True)
        for name in ["engine.zip", "worlds/mm6.zip", "worlds/mm7.zip", "worlds/mm8.zip"]:
            with zipfile.ZipFile(source / name, "w") as pack:
                pack.writestr("fixture.bin", payload if name == "engine.zip" else b"world")
    threshold = 100003
    result = cancel_copy(source, support, "--cancel-copy-after", str(threshold))
    assert result.returncode == 3, result.stderr
    match = re.search(r"cancel_accepted=1 bytes=(\d+) files=(\d+)", result.stderr)
    assert match, result.stderr
    assert threshold <= int(match[1]) < threshold + 65536
    assert (imports / "active.json").read_bytes() == before
    assert {p.name for p in imports.iterdir()} == {"active.json", ".operation.lock", previous_id}
    assert (imports / previous_id / "Data/mm8.games.lod").read_bytes() == (original / "Data/mm8.games.lod").read_bytes()
    retry = run(source, support)
    assert retry.returncode == 0, retry.stderr
    record = active(support)
    assert record["id"] != previous_id
    target = imports / record["id"]
    copied = list(p for p in target.rglob("*") if p.is_file())
    assert record["bytes"] == sum(p.stat().st_size for p in copied)
    for p in copied:
        assert p.read_bytes() == (source / p.relative_to(target)).read_bytes()
    assert not list(imports.glob(".staging-*")) and not list(imports.glob(".pending-*"))


def test_cancel_before_first_conversion_stage_keeps_source_and_cleans_transaction(tmp_path):
    source = source_files(tmp_path / "source")
    support = tmp_path / "support"
    assert run(source, support).returncode == 0
    imports = support / "GameImports"
    before = (imports / "active.json").read_bytes()
    layouts = Path(__file__).parents[1] / "assets_dev/engine/ui/gameplay"
    result = subprocess.run([str(TOOL), "--cancel-convert-at", "menu", "--convert", str(support), str(layouts)],
                            capture_output=True, text=True, timeout=20)
    assert result.returncode == 3, result.stderr
    assert "cancel_accepted=1 stage=menu" in result.stderr
    assert "Runtime conversion failed" not in result.stderr
    assert (imports / "active.json").read_bytes() == before
    assert {p.name for p in imports.iterdir()} == {"active.json", ".operation.lock", active(support)["id"]}


def test_cancel_after_successful_publication_is_rejected_and_keeps_completed_copy(tmp_path):
    source = source_files(tmp_path / "source")
    support = tmp_path / "support"
    result = cancel_copy(source, support, "--cancel-after-publication")
    assert result.returncode == 0, result.stderr
    assert "post_publication_cancel_accepted=0" in result.stderr
    imports = support / "GameImports"
    record = active(support)
    assert (imports / record["id"] / "Data/mm8.games.lod").exists()
    assert not list(imports.glob(".staging-*")) and not list(imports.glob(".pending-*"))
