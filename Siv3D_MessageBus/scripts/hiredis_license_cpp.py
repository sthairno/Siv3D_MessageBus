from __future__ import annotations

import sys
from pathlib import Path
from typing import Tuple

def read_text_utf8(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="strict")

def split_copyright_and_text(raw: str) -> Tuple[str, str]:
    ALL_RIGHTS_RESERVED = "All rights reserved."
    parts = raw.split(ALL_RIGHTS_RESERVED, 2)
    if len(parts) == 2:
        return (parts[0] + ALL_RIGHTS_RESERVED).strip(), parts[1].strip()
    else:
        return "", raw.strip()

def render_cpp(title: str, copyright_text: str, license_text: str) -> str:
    return (
        "// This file is auto-generated. Do not edit manually.\n"
        "#include \"MessageBus/GeneratedLicenses.hpp\"\n"
        "\n"
        "namespace MessageBus::Generated\n"
        "{\n"
        f"\tstatic const s3d::LicenseInfo license = {{\n"
        f"\t\tU\"{title}\",\n"
        f"\t\tUR\"HIREDISCOPY({copyright_text})HIREDISCOPY\",\n"
        f"\t\tUR\"HIREDISTEXT({license_text})HIREDISTEXT\"\n"
        "\t};\n"
        "\n"
        "\tconst s3d::LicenseInfo& HiredisLicense()\n"
        "\t{\n"
        "\t\treturn license;\n"
        "\t}\n"
        "}\n"
    )

def main() -> int:
    project_root = Path(__file__).resolve().parents[1]
    source = project_root / "vcpkg_installed" / "x64-windows-static" / "x64-windows-static" / "share" / "hiredis" / "copyright"
    out = project_root / "src" / "generated" / "HiredisLicense.cpp"
    title = "hiredis"

    try:
        raw = read_text_utf8(source)
    except OSError as exc:
        print(f"[hiredis_license_cpp] failed to read source: {exc}", file=sys.stderr)
        return 1

    copyright_text, license_text = split_copyright_and_text(raw)

    cpp = render_cpp(title, copyright_text, license_text)

    try:
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(cpp, encoding="utf-8", errors="strict")
    except OSError as exc:
        print(f"[hiredis_license_cpp] failed to write output: {exc}", file=sys.stderr)
        return 1

    print(f"[hiredis_license_cpp] generated: {out}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
