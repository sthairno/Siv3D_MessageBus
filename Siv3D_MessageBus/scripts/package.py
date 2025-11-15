from __future__ import annotations

import shutil
import sys
from pathlib import Path

class PackagingError(RuntimeError):
    pass

def resolve_path(path: Path, description: str) -> Path:
    if not path.exists():
        raise PackagingError(f"Required resource not found: {description} -> {path}")
    return path.resolve()

def main() -> int:
    script_root = Path(__file__).resolve().parent
    project_root = script_root.parent

    dest_root = project_root / "dest"
    include_source = project_root / "include" / "ThirdParty" / "MessageBus"
    readme_source = project_root / "HOW_TO_INSTALL.md"

    message_bus_release_lib = project_root / "build" / "Siv3D_MessageBus" / "release" / "bin" / "siv3d-messagebus.lib"
    message_bus_debug_lib = project_root / "build" / "Siv3D_MessageBus" / "debug" / "bin" / "siv3d-messagebus_d.lib"

    vcpkg_root = project_root / "vcpkg_installed" / "x64-windows-static" / "x64-windows-static"
    hiredis_release_lib = vcpkg_root / "lib" / "hiredis.lib"
    hiredis_debug_lib = vcpkg_root / "debug" / "lib" / "hiredisd.lib"
    hiredis_license = vcpkg_root / "share" / "hiredis" / "copyright"

    include_dest = dest_root / "include" / "ThirdParty" / "MessageBus"
    message_bus_lib_dest = dest_root / "lib" / "Windows" / "MessageBus"
    hiredis_lib_dest = dest_root / "lib" / "Windows" / "hiredis"
    readme_dest = dest_root / "HOW_TO_INSTALL.md"

    try:
        print(f"Cleaning destination directory: {dest_root}")
        shutil.rmtree(dest_root, ignore_errors=True)

        print("Validating required resources...")
        resolve_path(include_source, "MessageBus public headers")
        resolve_path(message_bus_release_lib, "siv3d-messagebus.lib (Release)")
        resolve_path(message_bus_debug_lib, "siv3d-messagebus_d.lib (Debug)")
        resolve_path(hiredis_release_lib, "hiredis.lib (Release)")
        resolve_path(hiredis_debug_lib, "hiredisd.lib (Debug)")
        resolve_path(hiredis_license, "hiredis license file")
        resolve_path(readme_source, "HOW_TO_INSTALL.md")

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

        print("Copying HOW_TO_INSTALL.md...")
        shutil.copy2(readme_source, readme_dest)

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
