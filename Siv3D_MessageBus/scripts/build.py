from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

CMAKE_TARGETS = {
    "Siv3D_MessageBus": {"target": "siv3d-messagebus", "enable_tests": False},
    "Test": {"target": "siv3d-messagebus-tests", "enable_tests": True},
}

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
    parser = argparse.ArgumentParser(description="Build the project using MSBuild on Windows or CMake on Linux.")
    parser.add_argument("project", help="Project name to build (.vcxproj extension optional).")
    parser.add_argument("configuration", choices=["Debug", "Release"], help="Build configuration.")
    parser.add_argument("--msbuild-path", help="Absolute path to MSBuild.exe (if omitted, detected via vswhere).")
    return parser.parse_args()

def build_with_msbuild(
    project_root: Path,
    project_name: str,
    configuration: str,
    explicit_msbuild: str | None,
) -> int:
    project_path = project_root / project_name
    if not project_path.exists():
        print(f"Error: Project file not found: {project_path}", file=sys.stderr)
        return 1

    try:
        msbuild_path = find_msbuild(explicit_msbuild)
    except BuildError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1

    print(f"MSBuild: {msbuild_path}")
    print(f"Project: {project_path}")
    print(f"Configuration: {configuration}")

    command = [
        str(msbuild_path),
        str(project_path),
        "/property:GenerateFullPaths=true",
        "/t:build",
        f"/p:configuration={configuration}",
        f"/p:platform=x64",
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

def ensure_vcpkg_root_env() -> bool:
    vcpkg_root = os.environ.get("VCPKG_ROOT")
    install_root = os.environ.get("VCPKG_INSTALLATION_ROOT")

    if vcpkg_root:
        return True
    
    if install_root:
        os.environ["VCPKG_ROOT"] = install_root
        return True

    return False

def cmake_configure_preset_name(platform: str, configuration: str, project_stem: str) -> str:
    target = CMAKE_TARGETS.get(project_stem)
    config = configuration.lower()
    suffix = "-test" if target["enable_tests"] else ""
    return f"{platform}-{config}{suffix}"

def cmake_build_preset_name(platform: str, configuration: str, project_stem: str) -> str:
    target = CMAKE_TARGETS.get(project_stem)
    config = configuration.lower()
    suffix = "-test" if target["enable_tests"] else ""
    return f"build-{platform}-{config}{suffix}"

def build_with_cmake(
    project_root: Path,
    project_stem: str,
    configuration: str,
    platform: str
) -> int:
    target = CMAKE_TARGETS.get(project_stem)
    if not target:
        known_projects = ", ".join(sorted(CMAKE_TARGETS))
        print(
            f"Error: Unsupported project '{project_stem}'. Supported projects: {known_projects}",
            file=sys.stderr,
        )
        return 1

    if not ensure_vcpkg_root_env():
        print("Error: Environment variable VCPKG_ROOT or VCPKG_INSTALLATION_ROOT is not set.", file=sys.stderr)
        return 1

    toolchain_file = Path(os.environ["VCPKG_ROOT"]) / "scripts" / "buildsystems" / "vcpkg.cmake"
    if not toolchain_file.exists():
        print(f"Error: Vcpkg toolchain file not found: {toolchain_file}", file=sys.stderr)
        return 1

    configure_preset = cmake_configure_preset_name(platform, configuration, project_stem)
    build_preset = cmake_build_preset_name(platform, configuration, project_stem)

    cmake_configure_args = ["cmake", "--preset", configure_preset]
    print(f"CMake configure preset: {configure_preset}")
    try:
        configure = subprocess.run(cmake_configure_args, cwd=project_root, check=False, capture_output=True, text=True)
    except OSError as exc:
        print(f"Failed to launch CMake: {exc}", file=sys.stderr)
        return 1

    if configure.returncode != 0:
        print(f"CMake configuration failed with code {configure.returncode}.", file=sys.stderr)
        if configure.stderr:
            print(configure.stderr, file=sys.stderr)
        if configure.stdout:
            print(configure.stdout, file=sys.stderr)
        return configure.returncode

    build_cmd = ["cmake", "--build", "--preset", build_preset]
    print(f"CMake build preset: {build_preset}")
    try:
        build = subprocess.run(build_cmd, cwd=project_root, check=False)
    except OSError as exc:
        print(f"Failed to launch CMake build: {exc}", file=sys.stderr)
        return 1

    if build.returncode != 0:
        print(f"CMake build failed with code {build.returncode}.", file=sys.stderr)
        return build.returncode

    print("Build completed successfully.")
    return 0


def main() -> int:
    args = parse_args()

    script_root = Path(__file__).resolve().parent
    project_root = script_root.parent

    project_name = args.project
    if not project_name.endswith(".vcxproj"):
        project_name = f"{project_name}.vcxproj"
    project_stem = Path(project_name).stem

    if sys.platform.startswith("win"):
        return build_with_msbuild(project_root, project_name, args.configuration, args.msbuild_path)
    if sys.platform.startswith("linux"):
        if args.msbuild_path:
            print("Info: --msbuild-path is ignored on Linux.", file=sys.stderr)
        return build_with_cmake(project_root, project_stem, args.configuration, "linux")
    if sys.platform == "darwin":
        if args.msbuild_path:
            print("Info: --msbuild-path is ignored on macOS.", file=sys.stderr)
        return build_with_cmake(project_root, project_stem, args.configuration, "macos")

    print(f"Error: Unsupported platform '{sys.platform}'.", file=sys.stderr)
    return 1

if __name__ == "__main__":
    raise SystemExit(main())
