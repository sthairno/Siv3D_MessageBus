# Siv3D_MessageBusの導入方法

Siv3D_MessageBus は、Siv3Dアプリケーション間でメッセージをやり取りするためのライブラリです。このドキュメントでは、Windows 環境に導入するための手順を解説します。

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
    
### Step 3. SDK フォルダへファイルをコピーする

1. 展開したフォルダ内にある `Open OpenSiv3D SDK` というショートカットをダブルクリックします。OpenSiv3D SDK のインストール先フォルダが自動的に開きます

2. 展開した Siv3D_MessageBus の `include` フォルダと `lib` フォルダを、開いた OpenSiv3D SDK フォルダへそのままコピーします

### Step 4. Redisサーバーを起動する

Siv3Dのアプリを起動する前に、Redisサーバーを起動する必要があります

Windowsでの起動方法は`3. Redisサーバーの起動方法（Windows）`を参照してください

### Step 5. Siv3Dアプリケーションで接続する

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

## 3. Redisサーバーの起動方法（Windows）

本ライブラリは、メッセージングの中継サーバーとして **Redis** を利用します。Windows で Redis を手軽に動かすため、Docker Desktopの環境構築とRedisの起動方法を解説します。

### Step1. Docker Desktop をインストールする

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

> [!WARNING]
> **Docker Desktop の起動に失敗したり、WSL関連のエラーが表示された場合**
>
> Docker Desktop の起動時や、`wsl --install` でインストールされたLinuxディストリビューションの起動時に `WslRegisterDistribution failed with error: 0x80370102` のようなエラーが表示された場合、PCの仮想化支援機能が無効になっている可能性があります。
>
> この機能を有効にするには、PCのBIOS/UEFI設定を変更する必要があります。設定方法はPCのメーカーによって異なりますが、以下の公式ドキュメントが参考になります。
>
> [Windows で仮想化を有効にする - Microsoft サポート](https://support.microsoft.com/ja-jp/windows/windows-%E3%81%A7%E4%BB%AE%E6%83%B3%E5%8C%96%E3%82%92%E6%9C%89%E5%8A%B9%E3%81%AB%E3%81%99%E3%82%8B-c5578302-6e43-4b4b-a449-8ced115f58e1)
>
> BIOS/UEFI設定画面で「Virtualization Technology (VT-x)」や「AMD-V」といった項目を探して `Enabled` (有効) に設定します。設定変更後は、PCを再起動して再度Docker Desktopの起動を試してください。

### Step 2. Redis サーバーを起動する

Docker Desktop が `Running` 状態になっていることを確認したら、Redis コンテナを起動します。

1. **Redis のコンテナを起動する**  
   PowerShell を開き、以下のコマンドをコピー＆ペーストして実行します。
   このコマンドは、Redisの公式イメージをダウンロードし、Siv3Dから接続できるように設定してバックグラウンドで起動します。
   ```
   docker run -d --name siv3d-redis -p 6379:6379 --restart unless-stopped redis:latest
   ```

2. **コンテナの動作を確認する**  
   以下のコマンドを実行して、`siv3d-redis` という名前のコンテナが `Up` 状態になっていれば成功です。
   ```bash
   docker ps
   ```

これで、Siv3Dアプリ から `localhost:6379` に接続すれば Redis を利用できます。
