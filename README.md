# Siv3D_MessageBus

## 導入方法

> [!NOTE]
> 現在Windows版Siv3Dのみ対応しています (Mac,Linux,Webは今後対応予定)

1. [Windows版 OpenSiv3D SDK v0.6.16](https://siv3d.github.io/ja-jp/download/windows/)をインストールする

2. Siv3D_MessageBusのzipファイルをダウンロード

    | [ダウンロード](http://sthairno.github.io/Siv3D_MessageBus/Siv3D_MessageBus_Windows.zip) |
    | --- |

3. OpenSiv3D SDKのフォルダを開き、ダウンロードした.zipファイルの中身をコピーする

    > **SDKのフォルダを簡単に開く方法**    
    > `Win`+`R`キーを押し、以下のコマンドを実行してください：
    > ```
    > explorer.exe %SIV3D_0_6_16%
    > ```
    
    https://github.com/user-attachments/assets/332176ee-b63c-4221-b9b2-30aa3a1da5bc

4. Visual Studioで通常通りOpenSiv3Dのプロジェクトを作成する

---

# 要件

**件名**：Siv3D向けミニマルRedisラッパ（イベント＋共有変数）
**目的**：アプリ間通信を初心者でも扱いやすいAPIにして提供する

## 1. スコープ

* **提供機能**

1. **イベント配信**
  - `subscribe(key)`: イベント購読、アスタリスクによる一括購読には非対応
  - `unsubscribe(key)`: イベント購読解除
  - `emit(key[, params])`: イベント発火、paramsはJSON固定
  - `events()`: イベント取得

使用イメージ

```cpp
#include <Siv3D.hpp>

void Main()
{
    MessageBus bus(...);

    bus.subscribe(U"game/start");
    bus.subscribe(U"game/end");

    while (System::Update())
    {
        bus.tick();
        
        if (SimpleGUI::Button(U"ゲーム開始"))
        {
            bus.emit(U"game/start", UR"({ "some": "data" })"_json);
            bus.emit(U"game/start"); // 空にもできる
        }

        for (const auto& event : bus.events())
        {
            if (event.channel == U"game/start")
            {
                if (const auto& param = event.value)
                {
                    Print << param[U"some"];
                }
            }
        }
    }
}
```

2. **共有変数** (SharedVariable)
  - `variable<type>(name, default)`: 変数の宣言
    - `get()`: 最後に設定or取得された値、値の型が異なる場合はこのタイミングでエラーになる
    - `set(...)`: 値の設定、複数回呼び出し対策にフレームの最後にRedisと同期

使用イメージ

```cpp
#include <Siv3D.hpp>

void Main()
{
    MessageBus bus(...);

    auto score = bus.variable<int32_t>(U"score", 0);

    Font font{ 20 };

    while (System::Update())
    {
        bus.tick();

        if (SimpleGUI::Button(U"10を設定"))
        {
            score.set(10);
        }

        font(U"{}点"_fmt(score.get()))
            .drawAt(Scene::CenterF());
    }
}
```

---

## 2. 前提・制約

* **構成**：1台（司令塔PC）でRedisを起動（Docker/Homebrew等）。他PCが接続。
* **依存**：`hiredis v1.3.0`（Redisプロトコルは**直接操作させない**）。
* **データ形式**：**JSON**（Siv3Dの`JSON`/`String`で完結）。
* **Siv3D連携**：メインループは**ノンブロッキング**。メインスレッド内にネットワークIOを行わず、ワーカースレッドで行う。
* **ビルド**：C++20 以上、Siv3D v0.6+ 想定（Windows/macOS）

---

## 3. 典型ユースケース

* ゲームの「開始/停止/更新」等の**イベント通知**。
* スコアやフィールド状態など、**共有ステータス**の読取・更新。

---

## 4. API 要件（インターフェース）

### 4.1 初期化

```cpp
class MessageBus {
public:
    MessageBus(StringView ip, uint16 port, Optional<StringView> password = none)
    void close(); // 終了（内部スレッド停止）
    void tick(); // イベント処理
    bool isConnected() const; // 接続状態フラグ
    operator bool() const; // isConnectedと同じ
};
```

### 4.2 イベント

```cpp
// 購読/解除（RAIIは不採用。明示関数のみ）
bool subscribe(StringView channel);      // true=登録成功
bool unsubscribe(StringView channel);

bool emit(StringView channel, Optional<JSON> payload = none); // 非同期送信（即return）

struct Event {
    String channel;
    Optional<JSON> value;
};

Array<Event> events();
```

### 4.3 共有変数

```cpp
template<class Type>
class SharedVariable {
public:
    const String& name() const;
    const Type& get() const;
    void set(const Type& value);
    DateTime updatedAt() const;
};

template<class Type>
SharedVariable<Type> variable(StringView name, const Type& defaultValue);
```

**仕様**

* `variable(name, default)` 初期値とキーを設定する。もしすでにキーが存在する場合は初期値で上書きせず、元の値をバッファに保存する。
* `get()` 値を取得する。
* `set()` 値を設定する。もし同じフレーム内に２回以上呼び出された場合は最後に設定された値を送信する

---

## 5. 内部動作・設計要件（外部仕様に影響しない範囲）

* **非同期IO**：

  * **送信ワーカースレッド**：`emit`/`set`のキューを処理し、Redisへ送信。
  * **受信ワーカースレッド**：イベント購読接続を維持し、受信をローカルバッファへ格納。

* **キャッシュ**：

  * `get()`は**キャッシュの値を返却するだけ**。必要な更新処理は**バックグラウンド**で行い、メインループでは通信しない。

* **再接続**：

  * 断検知→自動再接続→イベント購読の再確立。共有変数の再同期は実装側方針（例：必要キーのみMGET等）で実装。

* **エラーハンドリング**：

  * 変数に格納されている値の取得できないなど一時的なエラーは基本的に無視する。
  * 型が期待と違う場合など再起不能なエラーは異常ステータスとして記録し、バッファの値は更新しない。代わりに、get()を呼び出したタイミングでエラーを表示する
