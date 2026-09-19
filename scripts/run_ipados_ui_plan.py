#!/usr/bin/env python3
"""Run a JSON UI action plan against the installed, already running iPad game."""

import argparse
import json
from pathlib import Path
import plistlib
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device", required=True)
    parser.add_argument("--plan", required=True, type=Path)
    parser.add_argument("--result", required=True, type=Path, help="New .xcresult path")
    parser.add_argument("--derived-data", type=Path, default=Path("build-ios-uitests"))
    args = parser.parse_args()
    plan = json.loads(args.plan.read_text())
    if not isinstance(plan, list) or not plan:
        parser.error("Plan must be a nonempty JSON array")
    result = args.result.resolve()
    if result.exists():
        parser.error("Result path already exists; select a new path to preserve evidence")
    products = args.derived_data.resolve() / "Build/Products"
    candidates = list(products.glob("OpenYammUITests_iphoneos*.xctestrun"))
    if len(candidates) != 1:
        parser.error("Build the standalone ios/UITests project first; expected one device xctestrun")
    configuration = plistlib.loads(candidates[0].read_bytes())
    target = configuration["OpenYammUITests"]
    target.setdefault("EnvironmentVariables", {})["OPENYAMM_UI_STEPS"] = json.dumps(plan, ensure_ascii=False)
    target["SystemAttachmentLifetime"] = "keepAlways"
    target["UserAttachmentLifetime"] = "keepAlways"
    target["TestTimeoutsEnabled"] = True
    target["DefaultTestExecutionTimeAllowance"] = 180
    target["MaximumTestExecutionTimeAllowance"] = 180

    def resolve_test_root(value):
        if isinstance(value, str):
            return value.replace("__TESTROOT__", str(products))
        if isinstance(value, list):
            return [resolve_test_root(item) for item in value]
        if isinstance(value, dict):
            return {key: resolve_test_root(item) for key, item in value.items()}
        return value

    result.parent.mkdir(parents=True, exist_ok=True)
    runfile = result.with_suffix(".xctestrun")
    with runfile.open("xb") as file:
        plistlib.dump(resolve_test_root(configuration), file)
    command = [
        "xcodebuild", "test-without-building", "-xctestrun", str(runfile),
        "-destination", f"id={args.device}", "-resultBundlePath", str(result),
        "-parallel-testing-enabled", "NO", "-maximum-concurrent-test-device-destinations", "1",
        "-only-testing:OpenYammUITests/OpenYammUITests/testDevicePlan",
    ]
    return subprocess.run(command, check=False).returncode


if __name__ == "__main__":
    raise SystemExit(main())
