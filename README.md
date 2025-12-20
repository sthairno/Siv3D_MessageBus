# Siv3D_MessageBus

## 0. 導入方法

Windowsは [HOW_TO_INSTALL.md](https://github.com/sthairno/Siv3D_MessageBus/blob/main/Siv3D_MessageBus/HOW_TO_INSTALL.md) を参照してください。

## 1. 基本
サーバーへの接続や更新処理など、ライブラリの基本的な機能です。

### 1.1 初期化と接続
- `MessageBus(ip, port, password)` は Redis サーバーへの接続を作成します
- `ip`: サーバーの IP アドレスまたはホスト名（文字列）
- `port`: サーバーのポート番号（整数）
- `password`: 認証パスワード（オプション）

```cpp
# include <Siv3D.hpp>
# include <MessageBus/MessageBus.hpp>

void Main()
{
	// ローカルホストの 6379 ポートに接続
	MessageBus::MessageBus bus{ U"localhost", 6379 };

	// パスワード付きの場合
	// MessageBus::MessageBus bus{ U"localhost", 6379, U"password" };

	while (System::Update())
	{
		bus.update();
	}
}
```

### 1.2 定期更新
- `update()` はイベントの送受信処理や、共有変数の同期を行います
- メインループ（`System::Update()`）の中で毎フレーム呼び出す必要があります

```cpp
# include <Siv3D.hpp>
# include <MessageBus/MessageBus.hpp>

void Main()
{
	MessageBus::MessageBus bus{ U"localhost", 6379 };

	while (System::Update())
	{
		// 通信処理を実行
		bus.update();
	}
}
```

### 1.3 接続状態とエラー確認
- `isConnected()` はサーバーに接続できているかを `bool` 型で返します
	- `MessageBus` インスタンスを `bool` として評価することでも確認できます
- `error()` は直近のエラーメッセージを `String` 型で返します

```cpp
# include <Siv3D.hpp>
# include <MessageBus/MessageBus.hpp>

void Main()
{
	MessageBus::MessageBus bus{ U"localhost", 6379 };

	while (System::Update())
	{
		bus.update();

		if (bus.isConnected())
		{
			Print << U"Connected!";
		}
		else
		{
			Print << U"Disconnected: " << bus.error();
		}
	}
}
```

## 2. イベント
アプリケーション間でメッセージを送受信する機能です。

### 2.1 イベントの送信
- `emit(channel, payload)` は指定したチャンネルにメッセージを送信します
- `channel`: チャンネル名（文字列）
- `payload`: 送信するデータ（`JSON`）。省略可能

```cpp
# include <Siv3D.hpp>
# include <MessageBus/MessageBus.hpp>

void Main()
{
	MessageBus::MessageBus bus{ U"localhost", 6379 };

	while (System::Update())
	{
		bus.update();

		if (SimpleGUI::Button(U"Send", Vec2{ 20, 20 }))
		{
			// "game/start" チャンネルにメッセージを送信
			bus.emit(U"game/start", JSON{ {U"stage", 1} });
		}
	}
}
```

### 2.2 イベントの受信
- `subscribe(channel)` で指定したチャンネルのイベントを受け取るように設定します
- `unsubscribe(channel)` で指定したチャンネルの購読を解除します
- `events()` で受信したイベントのリスト（`Array<Event>`）を取得します
	- `Event` 構造体は `channel` (String) と `value` (JSON) を持ちます

```cpp
# include <Siv3D.hpp>
# include <MessageBus/MessageBus.hpp>

void Main()
{
	MessageBus::MessageBus bus{ U"localhost", 6379 };

	// "game/start" チャンネルを購読
	bus.subscribe(U"game/start");

	while (System::Update())
	{
		bus.update();

		// 受信したイベントを処理
		for (const auto& event : bus.events())
		{
			if (event.channel == U"game/start")
			{
				Print << U"Game Start: " << event.value;
			}
		}
	}
}
```

## 3. 共有変数
アプリケーション間で値を共有・同期する機能です。

### 3.1 変数の宣言
- `variable<Type>(name, defaultValue)` で共有変数を作成します
- `Type`: 変数の型（`int32`, `double`, `bool`, `String`, `JSON` に対応）
- `name`: 変数名（文字列）。この名前でサーバー上の値と紐付きます
- `defaultValue`: 初期値。サーバーに値がない場合に使われます

```cpp
# include <Siv3D.hpp>
# include <MessageBus/MessageBus.hpp>

void Main()
{
	MessageBus::MessageBus bus{ U"localhost", 6379 };

	// "score" という名前の整数型の共有変数を作成
	auto score = bus.variable<int32>(U"score", 0);

	// "playerName" という名前の文字列型の共有変数を作成
	auto playerName = bus.variable<String>(U"playerName", U"Guest");

	while (System::Update())
	{
		bus.update();
	}
}
```

### 3.2 値の読み書き
- `get()` で現在の値を取得します
	- サーバー側で値が更新されると、自動的に同期されます
- `set(value)` で値を更新します
	- 更新した値は他のアプリケーションにも反映されます

```cpp
# include <Siv3D.hpp>
# include <MessageBus/MessageBus.hpp>

void Main()
{
	MessageBus::MessageBus bus{ U"localhost", 6379 };

	auto score = bus.variable<int32>(U"score", 0);

	while (System::Update())
	{
		bus.update();

		// 現在の値を表示
		Print << U"Score: " << score.get();

		if (SimpleGUI::Button(U"Add Score", Vec2{ 20, 100 }))
		{
			// 値を更新
			score.set(score.get() + 100);
		}
	}
}
```
