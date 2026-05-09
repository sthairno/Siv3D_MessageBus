from __future__ import annotations

import shutil
import sys
from pathlib import Path

try:
    import win32com.client
except ImportError:
    win32com = None

class PackagingError(RuntimeError):
    pass

def resolve_path(path: Path, description: str) -> Path:
    if not path.exists():
        raise PackagingError(f"Required resource not found: {description} -> {path}")
    return path.resolve()

def create_shortcut(lnk_path: Path, target_path: str, arguments: str = "") -> None:
    """Create a Windows shortcut (.lnk) file.

    Args:
        lnk_path: Path where the .lnk file will be created
        target_path: Path to the target executable
        arguments: Command-line arguments for the target
    """
    if win32com is None:
        raise PackagingError("win32com.client is not available. Please install pywin32.")

    ws = win32com.client.Dispatch("wscript.shell")
    scut = ws.CreateShortcut(str(lnk_path))
    scut.TargetPath = target_path
    if arguments:
        scut.Arguments = arguments
    scut.Save()


def _find_mac_messagebus_lib(project_root: Path, base_name: str) -> Path:
    """Find MessageBus static library on Mac (Xcode/CMake output may vary)."""
    candidates = [
        project_root / "build" / "Release" / base_name,
        project_root / "build" / "Release" / "Release" / base_name,
        project_root / "build" / "Debug" / base_name,
        project_root / "build" / "Debug" / "Debug" / base_name,
    ]
    for path in candidates:
        if path.exists():
            return path
    # Fallback: glob under build/
    build_dir = project_root / "build"
    if build_dir.exists():
        for path in build_dir.rglob(base_name):
            if path.is_file():
                return path
    raise PackagingError(f"Required resource not found: {base_name} (searched build/ and build/**/{base_name})")


def _package_mac(project_root: Path, dest_root: Path, include_source: Path) -> None:
    """Build Mac package layout (include + lib/macOS) for Siv3D SDK merge."""
    readme_source = project_root / "MACOS_HOW_TO_INSTALL.md"
    vcpkg_root = project_root / "build" / "Release" / "vcpkg_installed" / "x64-osx"
    message_bus_release_lib = _find_mac_messagebus_lib(project_root, "libsiv3d-messagebus.a")
    hiredis_license = vcpkg_root / "share" / "hiredis" / "copyright"

    include_dest = dest_root / "include" / "ThirdParty" / "MessageBus"
    message_bus_lib_dest = dest_root / "lib" / "macOS" / "MessageBus"
    readme_dest = dest_root / readme_source.name

    print("Validating required resources...")
    resolve_path(include_source, "MessageBus public headers")
    resolve_path(message_bus_release_lib, "libsiv3d-messagebus.a (Release)")
    resolve_path(hiredis_license, "hiredis license file")
    resolve_path(readme_source, readme_source.name)

    print("Creating directories...")
    for directory in (include_dest, message_bus_lib_dest):
        directory.mkdir(parents=True, exist_ok=True)

    print("Copying headers...")
    if include_dest.exists():
        shutil.rmtree(include_dest)
        include_dest.mkdir(parents=True, exist_ok=True)
    for source_path in include_source.rglob("*"):
        if source_path.is_dir():
            continue
        relative_path = source_path.relative_to(include_source)
        destination_path = include_dest / relative_path
        destination_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source_path, destination_path)

    # Xcode expects static libs without "lib" prefix in the package
    print("Copying MessageBus libraries...")
    shutil.copy2(message_bus_release_lib, message_bus_lib_dest / "siv3d-messagebus.a")

    print("Placing hiredis license...")
    shutil.copy2(hiredis_license, message_bus_lib_dest / "LICENSE_hiredis.txt")

    print(f"Copying {readme_source.name}...")
    shutil.copy2(readme_source, readme_dest)


def _package_windows(project_root: Path, dest_root: Path, include_source: Path) -> None:
    """Build Windows package layout (include + lib/Windows) and shortcut."""
    readme_source = project_root / "WINDOWS_HOW_TO_INSTALL.md"
    message_bus_release_lib = project_root / "build" / "Siv3D_MessageBus" / "release" / "bin" / "siv3d-messagebus.lib"
    message_bus_debug_lib = project_root / "build" / "Siv3D_MessageBus" / "debug" / "bin" / "siv3d-messagebus_d.lib"
    vcpkg_root = project_root / "vcpkg_installed" / "x64-windows-static" / "x64-windows-static"
    hiredis_release_lib = vcpkg_root / "lib" / "hiredis.lib"
    hiredis_debug_lib = vcpkg_root / "debug" / "lib" / "hiredisd.lib"
    hiredis_license = vcpkg_root / "share" / "hiredis" / "copyright"

    include_dest = dest_root / "include" / "ThirdParty" / "MessageBus"
    message_bus_lib_dest = dest_root / "lib" / "Windows" / "MessageBus"
    hiredis_lib_dest = dest_root / "lib" / "Windows" / "hiredis"
    readme_dest = dest_root / readme_source.name

    print("Validating required resources...")
    resolve_path(include_source, "MessageBus public headers")
    resolve_path(message_bus_release_lib, "siv3d-messagebus.lib (Release)")
    resolve_path(message_bus_debug_lib, "siv3d-messagebus_d.lib (Debug)")
    resolve_path(hiredis_release_lib, "hiredis.lib (Release)")
    resolve_path(hiredis_debug_lib, "hiredisd.lib (Debug)")
    resolve_path(hiredis_license, "hiredis license file")
    resolve_path(readme_source, readme_source.name)

    print("Creating directories...")
    for directory in (include_dest, message_bus_lib_dest, hiredis_lib_dest):
        directory.mkdir(parents=True, exist_ok=True)

    print("Copying headers...")
    if include_dest.exists():
        shutil.rmtree(include_dest)
        include_dest.mkdir(parents=True, exist_ok=True)
    for source_path in include_source.rglob("*"):
        if source_path.is_dir():
            continue
        relative_path = source_path.relative_to(include_source)
        destination_path = include_dest / relative_path
        destination_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source_path, destination_path)

    print("Copying MessageBus libraries...")
    shutil.copy2(message_bus_release_lib, message_bus_lib_dest / message_bus_release_lib.name)
    shutil.copy2(message_bus_debug_lib, message_bus_lib_dest / message_bus_debug_lib.name)

    print("Copying hiredis libraries...")
    shutil.copy2(hiredis_release_lib, hiredis_lib_dest / hiredis_release_lib.name)
    shutil.copy2(hiredis_debug_lib, hiredis_lib_dest / hiredis_debug_lib.name)

    print("Placing hiredis license...")
    shutil.copy2(hiredis_license, hiredis_lib_dest / "LICENSE_hiredis.txt")

    print(f"Copying {readme_source.name}...")
    shutil.copy2(readme_source, readme_dest)

    print("Creating shortcut to OpenSiv3D SDK folder...")
    shortcut_path = dest_root / "Open OpenSiv3D SDK.lnk"
    create_shortcut(shortcut_path, "explorer.exe", "%SIV3D_0_6_16%")


def main() -> int:
    script_root = Path(__file__).resolve().parent
    project_root = script_root.parent

    dest_root = project_root / "dest"
    include_source = project_root / "include" / "ThirdParty" / "MessageBus"
    if sys.platform == "darwin":
        pack_fn = _package_mac
    elif sys.platform.startswith("win"):
        pack_fn = _package_windows
    else:
        print("Error: Unsupported platform. Use this script on Windows or macOS (darwin).", file=sys.stderr)
        return 1

    try:
        print(f"Cleaning destination directory: {dest_root}")
        shutil.rmtree(dest_root, ignore_errors=True)

        pack_fn(project_root, dest_root, include_source)

        print(f"Package layout prepared at: {dest_root}")
        return 0
    except PackagingError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1
    except OSError as exc:
        print(f"An error occurred while performing file operations: {exc}", file=sys.stderr)
        return 1

if __name__ == "__main__":
    raise SystemExit(main())
