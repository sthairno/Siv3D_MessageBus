from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

class BuildError(RuntimeError):
    pass

def default_vswhere_path() -> Path:
    program_files_x86 = os.environ.get("ProgramFiles(x86)")
    if not program_files_x86:
        raise BuildError("Environment variable ProgramFiles(x86) is not set. Please verify that Visual Studio is installed.")
    return Path(program_files_x86) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"

def find_msbuild(explicit_path: str | None) -> Path:
    if explicit_path:
        msbuild_path = Path(explicit_path)
        if not msbuild_path.exists():
            raise BuildError(f"Specified MSBuild path was not found: {msbuild_path}")
        return msbuild_path.resolve()

    vswhere = default_vswhere_path()
    if not vswhere.exists():
        raise BuildError(f"vswhere.exe was not found: {vswhere}")

    try:
        completed = subprocess.run(
            [
                str(vswhere),
                "-latest",
                "-requires",
                "Microsoft.Component.MSBuild",
                "-find",
                r"MSBuild\**\Bin\MSBuild.exe",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
    except subprocess.CalledProcessError as exc:
        raise BuildError(f"Failed to locate MSBuild via vswhere: {exc.stderr.strip()}") from exc

    for line in completed.stdout.splitlines():
        msbuild_candidate = Path(line.strip())
        if msbuild_candidate.exists():
            return msbuild_candidate.resolve()

    raise BuildError("Could not detect MSBuild.exe. Please confirm that Visual Studio is installed.")

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build a Visual Studio project using MSBuild.")
    parser.add_argument("project", help="Project name to build (.vcxproj extension optional).")
    parser.add_argument("configuration", choices=["Debug", "Release"], help="Build configuration.")
    parser.add_argument("--platform", default="x64", help="Target platform (default: x64).")
    parser.add_argument("--msbuild-path", help="Absolute path to MSBuild.exe (if omitted, detected via vswhere).")
    return parser.parse_args()

def main() -> int:
    args = parse_args()

    script_root = Path(__file__).resolve().parent
    project_root = script_root.parent

    project_name = args.project
    if not project_name.endswith(".vcxproj"):
        project_name = f"{project_name}.vcxproj"

    project_path = project_root / project_name
    if not project_path.exists():
        print(f"Error: Project file not found: {project_path}", file=sys.stderr)
        return 1

    try:
        msbuild_path = find_msbuild(args.msbuild_path)
    except BuildError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1

    print(f"MSBuild: {msbuild_path}")
    print(f"Project: {project_path}")
    print(f"Configuration: {args.configuration}")
    print(f"Platform: {args.platform}")

    command = [
        str(msbuild_path),
        str(project_path),
        "/property:GenerateFullPaths=true",
        "/t:build",
        f"/p:configuration={args.configuration}",
        f"/p:platform={args.platform}",
        "/verbosity:quiet",
        "/nologo",
    ]

    try:
        completed = subprocess.run(command, cwd=project_root, check=False)
    except OSError as exc:
        print(f"Failed to launch MSBuild: {exc}", file=sys.stderr)
        return 1

    if completed.returncode != 0:
        print(f"MSBuild exited with error code {completed.returncode}.", file=sys.stderr)
        return completed.returncode

    print("Build completed successfully.")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
