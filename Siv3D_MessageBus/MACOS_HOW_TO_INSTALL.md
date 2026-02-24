# Siv3D_MessageBusの導入方法（macOS）

Siv3D_MessageBus は、Siv3Dアプリケーション間でメッセージをやり取りするためのライブラリです。このドキュメントでは、macOS 環境に導入するための手順を解説します。

## 1. 前提条件

- **対応 SDK**: OpenSiv3D SDK v0.6.16（macOS版）
- **対応 OS**: macOS Tahoe 以降
- **対応アーキテクチャ**: Intel (x86_64) のみ
  - Apple Silicon (M1/M2/M3) の Mac では Rosetta 2 を介して動作します
- **必要ソフト**:
  - Xcode（C++ 開発環境）
  - [Homebrew](https://brew.sh/ja/)（パッケージマネージャ）

## 2. インストール手順

### Step 1. OpenSiv3D SDK を用意する

1. [OpenSiv3D SDK v0.6.16 (macOS)](http://sthairno.github.io/Siv3D_MessageBus/Siv3D_MessageBus_macOS.zip) をダウンロードします
2. ダウンロードした ZIP ファイルを展開し、中身を任意のフォルダに配置してください（例: `~/OpenSiv3D_SDK_v0.6.16`）
3. [macOS で Siv3D プログラミングを始める](https://siv3d.github.io/ja-jp/download/macos/)に従い、Siv3Dプロジェクトの初期設定をしてください

### Step 2. Siv3D_MessageBus のライブラリをダウンロードする

> [!NOTE]
> 
> ブラウザのセキュリティ設定によってダウンロードがブロックされることがあります。下のダウンロードボタンをクリックしても反応しない場合は、右クリックして「リンク先を別名で保存」を試してください

1. 以下のリンクから `Siv3D_MessageBus_macOS.zip` をダウンロードします  
    | [Siv3D_MessageBusをダウンロード](http://sthairno.github.io/Siv3D_MessageBus/Siv3D_MessageBus_macOS.zip) |
    | --- |
2. ダウンロードした ZIP ファイルを任意の作業フォルダに展開します

### Step 3. SDK フォルダへファイルをコピーする

展開した Siv3D_MessageBus の **`include`** フォルダと **`lib`** フォルダを、OpenSiv3D SDK のフォルダへそのままコピーします。

https://github.com/user-attachments/assets/550f7925-c533-4bbd-981f-57846f7a277d

### Step 4. Xcode プロジェクトを設定する

Siv3D_MessageBus のライブラリを Xcode プロジェクトにリンクする設定を行います。

#### 4-1. Siv3D プロジェクトを開く

Siv3D の Xcode プロジェクトテンプレートから作成したプロジェクト、またはお使いの Siv3D プロジェクトを Xcode で開きます。

#### 4-2. Link Binary With Libraries にライブラリを追加する

1. Xcode の左側にある **Project Navigator**（フォルダアイコン）でプロジェクト名（青いアイコン）をクリックします
2. 中央のペインで **TARGETS** の下にあるターゲット名を選択します
3. 上部のタブから **Build Phases** を選択します
4. **Link Binary With Libraries** セクションを展開し、左下の **+** ボタンをクリックします

    ![Step 4](https://github.com/user-attachments/assets/e002c47c-a301-45ab-bcd5-f2835f085bfb)

5. 表示されたダイアログの左下にある **Add Other...** → **Add Files...** をクリックします

    ![Step 5](https://github.com/user-attachments/assets/b4286df0-c030-4036-bdf3-68fd740a49b3)

6. Step 3 でコピーした SDK フォルダ内から、`lib/macOS/MessageBus/siv3d-messagebus.a`を選択して追加します

### Step 5. Redis サーバーをインストール・起動する

Siv3D_MessageBus はメッセージングの中継として **Redis** サーバーを利用します。macOS では Homebrew を使って簡単にインストールできます。

ターミナルで以下のコマンドを順に実行します:

```bash
brew install redis
brew services start redis
```

`brew services start` で起動すると、Mac を再起動しても自動的に Redis が起動するようになります。

### Step 6. Siv3Dアプリケーションで接続する

Siv3Dアプリケーションで Redis サーバーに接続するには、以下のようにコードを記述します。

```cpp
#include <Siv3D.hpp>
#include <MessageBus/MessageBus.hpp>

void Main()
{
    MessageBus::MessageBus bus(U"<Redisサーバーが起動しているホスト名>", 6379);
    
    while (System::Update())
    {
        bus.update();
    }

    bus.shutdown();
}
```

ローカルで Redis を起動している場合は、ホスト名に `U"localhost"` を指定します。
