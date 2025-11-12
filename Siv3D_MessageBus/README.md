# Siv3D MessageBus

Siv3D 向けのメッセージバスライブラリです。`Siv3D_MessageBus` プロジェクトとしてビルドされ、`Test` プロジェクトで挙動を確認できます。

## ビルド

```
scripts\msbuild.bat Siv3D_MessageBus Release
scripts\msbuild.bat Siv3D_MessageBus Debug
```

単体テストを実行する場合:

```
scripts\msbuild.bat Test Debug
scripts\runtest.bat Debug
```

## 配布パッケージの作成

```
pwsh scripts/package.ps1
```

実行すると `dest/` 配下に、`OpenSiv3D_0.6.16` へコピー可能なフォルダ構造で成果物が配置されます。`dest` を ZIP 化し、ライブラリ利用者に配布してください。

## ライセンス

- Siv3D MessageBus: MIT ライセンス（予定）
- hiredis: BSD-3-Clause（`dest/lib/Windows/hiredis/LICENSE_hiredis.txt` に同梱）

