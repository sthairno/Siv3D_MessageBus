# Siv3D_MessageBusの導入方法

Siv3D_MessageBus は、Siv3Dアプリケーション間でメッセージをやり取りするためのライブラリです。このドキュメントでは、Windows 環境に導入するための手順を解説します。

本ライブラリは、メッセージングの中継サーバーとして **Redis** を利用します。Windows で Redis を手軽に動かすため、**Docker** という仕組みを使います。

## 1. 前提条件

- **対応 SDK**: OpenSiv3D SDK v0.6.16（Windows版）
- **対応 OS**: Windows 10 以降
- **必要ソフト**:
  - Visual Studio 2022 以降（C++ 開発環境）
    - ※ 「C++によるデスクトップ開発」ワークロードが必要です。
  - Docker Desktop

## 2. インストール手順

### Step 1. OpenSiv3D SDK を用意する

1. [OpenSiv3D SDK v0.6.16 (Windows)](https://siv3d.github.io/ja-jp/download/windows/) をダウンロードしてインストールします
2. インストーラの指示に従い、任意の場所に展開してください

### Step 2. Siv3D_MessageBus のライブラリをダウンロードする

> [!NOTE]
> 
> ブラウザのセキュリティ設定によってダウンロードがブロックされることがあります。下のダウンロードボタンをクリックしても反応しない場合は、右クリックして「名前をつけてリンク先を保存」を試してください

1. 以下のリンクから `Siv3D_MessageBus_Windows.zip` をダウンロードします  
    | [Siv3D_MessageBusをダウンロード](http://sthairno.github.io/Siv3D_MessageBus/Siv3D_MessageBus_Windows.zip) |
    | --- |
2. ダウンロードした ZIP ファイルを任意の作業フォルダに展開します
    
    https://github.com/user-attachments/assets/332176ee-b63c-4221-b9b2-30aa3a1da5bc

### Step 3. SDK フォルダへファイルをコピーする

1. OpenSiv3D SDK のインストール先フォルダを開きます。手早く開きたい場合は `Win` + `R` キーを押し、次のコマンドを実行してください

   ```
   explorer.exe %SIV3D_0_6_16%
   ```

2. 展開した Siv3D_MessageBus の中身を、OpenSiv3D SDK フォルダへそのままコピーします

### Step 4. Docker Desktop をインストールする

Redisサーバーを簡単に起動するため、Docker Desktop をセットアップします。

1. **WSL2 を有効化してPCを再起動する**  
   PowerShell を **管理者として** 実行し、以下のコマンドを入力します。これにより、Docker の動作に必要となる WSL (Windows Subsystem for Linux) が有効になります。
   完了後は、指示に従ってPCを再起動してください。
   ```
   wsl --install
   ```

2. **Docker Desktop をインストールする**  
   [Docker Desktop 公式ページ](https://www.docker.com/ja-jp/products/docker-desktop/) から Windows 版インストーラ (`Docker Desktop Installer.exe`) をダウンロードして実行します。
   インストーラの指示に従い、「Use WSL 2 instead of Hyper-V」が有効になっていることを確認してインストールを進めてください。完了後、再度PCの再起動が求められる場合があります。

3. **Docker Desktop を起動する**  
   インストール後、Docker Desktop を起動します。初回起動時にはチュートリアルの表示やアカウント登録の案内がありますが、これらはスキップしても構いません。
   タスクトレイのクジラのアイコンが「Running」という緑色の状態になれば準備完了です。

### Step 5. Redis サーバーを起動する

Docker Desktop が `Running` 状態になっていることを確認したら、Redis コンテナを起動します。

1. **Redis のコンテナを起動する**  
   PowerShell を開き、以下のコマンドをコピー＆ペーストして実行します。
   このコマンドは、Redisの公式イメージをダウンロードし、Siv3Dから接続できるように設定してバックグラウンドで起動します。
   ```
   docker run -d --name siv3d-redis -p 6379:6379 --restart unless-stopped --rm redis:latest
   ```

2. **コンテナの動作を確認する**  
   以下のコマンドを実行して、`siv3d-redis` という名前のコンテナが `Up` 状態になっていれば成功です。
   ```bash
   docker ps
   ```

これで、Siv3Dアプリ から `localhost:6379` に接続すれば Redis を利用できます。

### Step 6. Siv3Dアプリケーションで接続する

Siv3Dアプリケーションで Redis サーバーに接続するには、以下のようにコードを記述します。

```cpp
#include <Siv3D.hpp>
#include <MessageBus/MessageBus.hpp>

void Main()
{
    MessageBus::MessageBus bus(U"localhost", 6379);
    
    while (System::Update())
    {
        bus.tick();
    }
}
```
