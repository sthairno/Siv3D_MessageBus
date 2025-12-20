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
    parser.add_argument("--platform", default="x64", help="Target platform (default: x64).")
    parser.add_argument("--msbuild-path", help="Absolute path to MSBuild.exe (if omitted, detected via vswhere).")
    return parser.parse_args()


def build_with_msbuild(
    project_root: Path,
    project_name: str,
    configuration: str,
    platform_name: str,
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
    print(f"Platform: {platform_name}")

    command = [
        str(msbuild_path),
        str(project_path),
        "/property:GenerateFullPaths=true",
        "/t:build",
        f"/p:configuration={configuration}",
        f"/p:platform={platform_name}",
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


def build_with_cmake(project_root: Path, project_stem: str, configuration: str, generator: str | None = None) -> int:
    target = CMAKE_TARGETS.get(project_stem)
    if not target:
        known_projects = ", ".join(sorted(CMAKE_TARGETS))
        print(
            f"Error: Unsupported project '{project_stem}'. Supported projects: {known_projects}",
            file=sys.stderr,
        )
        return 1

    vcpkg_root = os.environ.get("VCPKG_ROOT") or os.environ.get("VCPKG_INSTALLATION_ROOT")
    if not vcpkg_root:
        print("Error: Environment variable VCPKG_ROOT or VCPKG_INSTALLATION_ROOT is not set.", file=sys.stderr)
        return 1

    toolchain_file = Path(vcpkg_root) / "scripts" / "buildsystems" / "vcpkg.cmake"
    if not toolchain_file.exists():
        print(f"Error: Vcpkg toolchain file not found: {toolchain_file}", file=sys.stderr)
        return 1

    build_dir = project_root / "build" / configuration
    cmake_cache_args = [
        "cmake",
        "-S",
        str(project_root),
        "-B",
        str(build_dir),
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file}",
        f"-DSIV3D_MESSAGEBUS_BUILD_TESTS={'ON' if target['enable_tests'] else 'OFF'}",
    ]
    if generator:
        cmake_cache_args.extend(["-G", generator])
        # Xcodeジェネレータの場合、Intel x64アーキテクチャを強制
        if generator == "Xcode":
            cmake_cache_args.append("-DCMAKE_OSX_ARCHITECTURES=x86_64")
            # vcpkgのトリプレットをx64-osxに設定（Apple Silicon非対応）
            cmake_cache_args.append("-DVCPKG_TARGET_TRIPLET=x64-osx")
    else:
        # Single-configuration generators need CMAKE_BUILD_TYPE
        cmake_cache_args.append(f"-DCMAKE_BUILD_TYPE={configuration}")

    print(f"CMake configure command: {' '.join(cmake_cache_args)}")
    try:
        configure = subprocess.run(cmake_cache_args, check=False, capture_output=True, text=True)
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

    build_cmd = [
        "cmake",
        "--build",
        str(build_dir),
        "--target",
        target["target"],
    ]
    # Multi-configuration generators (like Xcode) need --config
    if generator == "Xcode":
        build_cmd.append(f"--config={configuration}")

    print(f"CMake build command: {' '.join(build_cmd)}")
    try:
        build = subprocess.run(build_cmd, check=False)
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
        return build_with_msbuild(project_root, project_name, args.configuration, args.platform, args.msbuild_path)
    if sys.platform.startswith("linux"):
        if args.platform not in {"x64", ""}:
            print(f"Info: --platform={args.platform} is ignored on Linux.", file=sys.stderr)
        if args.msbuild_path:
            print("Info: --msbuild-path is ignored on Linux.", file=sys.stderr)
        return build_with_cmake(project_root, project_stem, args.configuration)
    if sys.platform == "darwin":
        if args.platform not in {"x64", ""}:
            print(f"Info: --platform={args.platform} is ignored on macOS.", file=sys.stderr)
        if args.msbuild_path:
            print("Info: --msbuild-path is ignored on macOS.", file=sys.stderr)
        return build_with_cmake(project_root, project_stem, args.configuration, "Xcode")

    print(f"Error: Unsupported platform '{sys.platform}'.", file=sys.stderr)
    return 1

if __name__ == "__main__":
    raise SystemExit(main())
