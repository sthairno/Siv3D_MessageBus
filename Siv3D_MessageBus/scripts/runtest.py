from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

VALID_CONFIGURATIONS = {"Debug", "Release"}

def parse_args() -> tuple[str, list[str], bool]:
    parser = argparse.ArgumentParser(
        description="Run Google Test. Optionally specify configuration (Debug/Release) as the first argument.",
        add_help=True,
    )
    parser.add_argument("configuration", nargs="?", help="Build configuration to use. Defaults to Debug.")
    parser.add_argument("gtest_args", nargs=argparse.REMAINDER, help="Additional arguments passed to Google Test.")

    args = parser.parse_args()
    configuration = args.configuration
    extra_args = args.gtest_args or []

    used_default = False
    if configuration is None:
        print("No configuration specified. Using default Debug.")
        used_default = True
        configuration = "Debug"
    elif configuration not in VALID_CONFIGURATIONS:
        print(f'Configuration "{configuration}" is invalid. Using default Debug.')
        used_default = True
        extra_args = [configuration] + extra_args
        configuration = "Debug"

    return configuration, extra_args, used_default


def main() -> int:
    configuration, extra_args, notified_default = parse_args()

    script_root = Path(__file__).resolve().parent
    project_root = script_root.parent

    if configuration == "Debug":
        test_exe = project_root / "build" / "Test" / "debug" / "bin" / "Test.exe"
    else:
        test_exe = project_root / "build" / "Test" / "release" / "bin" / "Test.exe"

    if not test_exe.exists():
        print(f"Error: Test executable not found: {test_exe}", file=sys.stderr)
        print(
            f"Please build first by running scripts/msbuild.py "
            f"{'Test Debug' if configuration == 'Debug' else 'Test Release'}.",
            file=sys.stderr,
        )
        return 1

    work_dir = project_root / "test" / "App"

    print(f"Running tests: configuration={configuration}")
    print(f"Test.exe: {test_exe}")
    print(f"Working directory: {work_dir}")
    if extra_args:
        print(f"Additional arguments: {' '.join(extra_args)}")
    elif notified_default:
        print("Additional arguments: (none)")

    env = os.environ.copy()
    env.setdefault("GTEST_COLOR", "1")

    try:
        completed = subprocess.run(
            [str(test_exe), *extra_args],
            cwd=work_dir,
            env=env,
            check=False,
        )
    except OSError as exc:
        print(f"Failed to execute tests: {exc}", file=sys.stderr)
        return 1

    return completed.returncode


if __name__ == "__main__":
    raise SystemExit(main())

