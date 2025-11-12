from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

VALID_CONFIGURATIONS = {"Debug", "Release"}

def parse_args() -> tuple[str, list[str], bool]:
    parser = argparse.ArgumentParser(
        description="Google Test を実行します。第1引数に構成 (Debug/Release) を指定できます。",
        add_help=True,
    )
    parser.add_argument("configuration", nargs="?", help="使用するビルド構成。省略時は Debug。")
    parser.add_argument("gtest_args", nargs=argparse.REMAINDER, help="Google Test に渡す追加引数。")

    args = parser.parse_args()
    configuration = args.configuration
    extra_args = args.gtest_args or []

    used_default = False
    if configuration is None:
        print("構成が指定されていません。既定値 Debug を使用します。")
        used_default = True
        configuration = "Debug"
    elif configuration not in VALID_CONFIGURATIONS:
        print(f'構成 "{configuration}" は無効です。既定値 Debug を使用します。')
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
        print(f"エラー: テスト実行ファイルが見つかりません: {test_exe}", file=sys.stderr)
        print(f"先に scripts/msbuild.py { 'Test Debug' if configuration == 'Debug' else 'Test Release' } を実行してください。", file=sys.stderr)
        return 1

    work_dir = project_root / "test" / "App"

    print(f"テストを実行します: 構成={configuration}")
    print(f"Test.exe: {test_exe}")
    print(f"作業ディレクトリ: {work_dir}")
    if extra_args:
        print(f"追加引数: {' '.join(extra_args)}")
    elif notified_default:
        print("追加引数: (なし)")

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
        print(f"テスト実行に失敗しました: {exc}", file=sys.stderr)
        return 1

    return completed.returncode


if __name__ == "__main__":
    raise SystemExit(main())

