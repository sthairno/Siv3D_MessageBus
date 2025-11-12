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
        raise BuildError("環境変数 ProgramFiles(x86) が設定されていません。Visual Studio がインストールされているか確認してください。")
    return Path(program_files_x86) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"

def find_msbuild(explicit_path: str | None) -> Path:
    if explicit_path:
        msbuild_path = Path(explicit_path)
        if not msbuild_path.exists():
            raise BuildError(f"指定された MSBuild パスが見つかりません: {msbuild_path}")
        return msbuild_path.resolve()

    vswhere = default_vswhere_path()
    if not vswhere.exists():
        raise BuildError(f"vswhere.exe が見つかりません: {vswhere}")

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
        raise BuildError(f"vswhere を用いた MSBuild の検索に失敗しました: {exc.stderr.strip()}") from exc

    for line in completed.stdout.splitlines():
        msbuild_candidate = Path(line.strip())
        if msbuild_candidate.exists():
            return msbuild_candidate.resolve()

    raise BuildError("MSBuild.exe を検出できませんでした。Visual Studio のインストールを確認してください。")

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="MSBuild を用いて Visual Studio プロジェクトをビルドします。")
    parser.add_argument("project", help="ビルド対象のプロジェクト名（.vcxproj を省略可）")
    parser.add_argument("configuration", choices=["Debug", "Release"], help="ビルド構成")
    parser.add_argument("--platform", default="x64", help="ターゲットプラットフォーム（既定: x64）")
    parser.add_argument("--msbuild-path", help="MSBuild.exe の絶対パス（指定しない場合は vswhere で検索）")
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
        print(f"エラー: プロジェクトファイルが見つかりません: {project_path}", file=sys.stderr)
        return 1

    try:
        msbuild_path = find_msbuild(args.msbuild_path)
    except BuildError as exc:
        print(f"エラー: {exc}", file=sys.stderr)
        return 1

    print(f"MSBuild: {msbuild_path}")
    print(f"プロジェクト: {project_path}")
    print(f"構成: {args.configuration}")
    print(f"プラットフォーム: {args.platform}")

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
        print(f"MSBuild の実行に失敗しました: {exc}", file=sys.stderr)
        return 1

    if completed.returncode != 0:
        print(f"MSBuild がエラーコード {completed.returncode} で終了しました。", file=sys.stderr)
        return completed.returncode

    print("ビルドが正常に完了しました。")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
