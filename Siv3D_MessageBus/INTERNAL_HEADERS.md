# 内部実装ヘッダーファイル一覧

## 概要
このドキュメントは、MessageBusライブラリの内部実装用ヘッダーファイルを洗い出した結果です。
これらのファイルはユーザーが直接インクルードする必要がなく、`MessageBus::detail`名前空間に移動する予定です。

## 内部実装ヘッダーファイル（5ファイル）

### 1. SharedVariableImpl.hpp
- **理由**: `SharedVariable`の内部実装クラス。ユーザーは`SharedVariable`を通じてのみアクセスする。
- **使用箇所**: 
  - `SharedVariable.hpp`（公開APIから間接的に使用）
  - `MessageBus.cpp`（内部実装で使用）

### 2. RedisConnection.hpp
- **理由**: Redis接続の内部実装クラス。ユーザーは`MessageBus`を通じてのみアクセスする。
- **使用箇所**: 
  - `MessageBus.cpp`（内部実装で使用）
  - テストコード（`test/RedisDockerTestFixture.hpp`, `test/Utility.hpp`, `test/Main.cpp`）

### 3. RedisConnectionState.hpp
- **理由**: `RedisConnection`の内部状態を表す列挙型。ユーザーは直接参照する必要がない。
- **使用箇所**: 
  - `RedisConnection.hpp`（内部実装で使用）
  - `MessageBus.cpp`（内部実装で使用）
  - テストコード（`test/RedisDockerTestFixture.hpp`, `test/Utility.hpp`）

### 4. WindowsLibrary.hpp
- **理由**: Windowsプラットフォーム固有のライブラリリンク設定。内部実装の詳細。
- **使用箇所**: 
  - `MessageBus.hpp`（プラットフォーム固有の設定として使用）

### 5. GeneratedLicenses.hpp
- **理由**: ライセンス情報の生成ヘッダー。内部実装の詳細。
- **使用箇所**: 
  - `src/generated/HiredisLicense.cpp`（生成コードで使用）
  - `src/RedisConnection.cpp`（内部実装で使用）

## 公開APIヘッダーファイル（3ファイル）

### 1. MessageBus.hpp
- **理由**: メインの公開API。ユーザーが直接インクルードする。
- **使用箇所**: 
  - `Example/Main.cpp`（ユーザーコード）
  - テストコード

### 2. SharedVariable.hpp
- **理由**: 公開API。ユーザーが`MessageBus::variable()`で取得する型。
- **使用箇所**: 
  - `MessageBus.hpp`（公開APIから間接的に使用）

### 3. TypeMismatchError.hpp
- **理由**: 公開APIの例外型。ユーザーがキャッチする可能性がある。
- **使用箇所**: 
  - `SharedVariable.hpp`（公開APIから間接的に使用）

## 確認方法

### ユーザーコードでの使用状況
- `Example/Main.cpp`: `MessageBus.hpp`のみ使用
- 他のユーザーコード: 確認されていないが、`MessageBus.hpp`のみが想定される

### テストコードでの使用状況
- `RedisConnection.hpp`, `RedisConnectionState.hpp`はテストコードで使用されているが、これはテストの都合であり、ユーザーが使う必要はない。

## 次のステップ
- [x] 1. 内部実装のヘッダーを洗い出し
- [x] 2. 内部実装のクラス等を`detail`に移動してhpp/cppも更新
- [ ] 3. テストを実行
- [ ] 4. コミット
- [ ] 5. `git mv`で内部実装を`include/ThirdParty/MessageBus/detail`に移動
- [ ] 6. コミット
- [ ] 7. includeのパスを更新してテストを実行
- [ ] 8. コミット

