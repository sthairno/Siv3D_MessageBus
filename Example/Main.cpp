#include <Siv3D.hpp>
#include <MessageBus/MessageBus.hpp>

// 指定座標とその上下左右のセルを切り替えする関数
static void ToggleCell(JSON& board, int32 x, int32 y, int32 gridSize)
{
	static const std::initializer_list<Point> offsets = { {0, 0}, {0, -1}, {0, 1}, {-1, 0}, {1, 0} };

	for (const auto& offset : offsets)
	{
		int32 nx = x + offset.x;
		int32 ny = y + offset.y;

		if (nx >= 0 && nx < gridSize && ny >= 0 && ny < gridSize)
		{
			int32 index = ny * gridSize + nx;
			board[index] = !board[index].get<bool>();
		}
	}
}

void Main()
{
	// 背景の色を設定する
	Scene::SetBackground(Color{ 5, 14, 20 });

	// グリッドのサイズとセルの大きさを定義
	constexpr int32 GridSize = 5;
	constexpr int32 CellSize = 60;
	constexpr int32 GridMargin = 20;
	constexpr int32 GridWidth = GridSize * CellSize;
	constexpr int32 GridHeight = GridSize * CellSize;

	// Redisサーバーに接続する
	// 第1引数: サーバーのホスト名（localhostは同じマシン上を意味する）
	// 第2引数: サーバーのポート番号（6379はRedisのデフォルトポート）
	MessageBus::MessageBus bus{ U"localhost", 6379 };

	// 共有変数を作成する
	// variable<T>(キー名, 初期値) で、複数のアプリケーション間でデータを共有できます
	// ここでは5x5=25個のライトのON/OFF状態をJSON配列で管理します
	// キー名 "lightsout/board" は他のアプリケーションからも同じキーでアクセスできます
	auto board = bus.variable<JSON>(U"lightsout/board", JSON(Array<bool>(GridSize * GridSize)));

	// イベントチャンネルを購読する
	// subscribe(チャンネル名) で、指定したチャンネルに送信されたイベントを受信できるようになります
	// ここでは "lightsout/toggle" チャンネルでセルの切替イベントを受信します
	bus.subscribe(U"lightsout/toggle");

	// グリッドを画面中央に配置するための左上座標を計算
	const Vec2 GridPos{ (Scene::Width() - GridWidth) / 2.0, 100 };

	// テキストを描画するためのフォントを作成
	Font font{ 30 };

	// アニメーション表示用の変数
	Point animationTarget{ 0, 0 };
	Timer animationTimer{ 0.2s, StartImmediately::No };

	while (System::Update())
	{
		// MessageBusの更新処理を行う
		// 毎フレーム呼び出すことで、共有変数の変更やイベントの受信が反映されます
		bus.update();

		// MessageBusの接続状態を表示する
		// 接続中は緑色、切断中は赤色でエラーメッセージを表示
		if (bus.isConnected())
		{
			font(U"Connected").draw(20, 20, 20, Palette::Green);
		}
		else
		{
			font(U"Disconnected\n" + bus.error()).draw(20, 20, 20, Palette::Red);
		}

		// 共有変数から現在のボード状態を取得する
		// get() で共有変数の最新の値を取得できます（他のアプリケーションが変更した値も含む）
		auto currentBoard = board.get();

		// セルのクリックを検出
		if (MouseL.down())
		{
			for (int32 y = 0; y < GridSize; ++y)
			{
				for (int32 x = 0; x < GridSize; ++x)
				{
					int32 index = y * GridSize + x;
					RectF cell{ GridPos.x + x * CellSize, GridPos.y + y * CellSize, CellSize };

					if (cell.mouseOver())
					{
						// 現在のボード状態をコピー
						JSON newBoard = currentBoard;

						// クリックされたセルとその上下左右を切り替え
						ToggleCell(newBoard, x, y, GridSize);

						// 共有変数を更新する
						// set(値) で共有変数に新しい値を設定します
						// この変更は他のアプリケーションにも自動的に反映されます
						board.set(newBoard);
						currentBoard = newBoard;

						// イベントを発行する
						// emit(チャンネル名, データ) で、指定したチャンネルにイベントを送信します
						// ここではクリックしたセルの座標を他のアプリケーションに通知します
						bus.emit(U"lightsout/toggle", JSON{ {U"x", x}, {U"y", y} });
					}
				}
			}
		}

		// クリア判定：すべてのライトがOFFになっているかチェック
		bool allOff = true;
		for (int32 i = 0; i < GridSize * GridSize; ++i)
		{
			if (currentBoard[i].get<bool>())
			{
				allOff = false;
				break;
			}
		}

		// 受信したイベントを処理する
		// events() で、このフレームで受信したすべてのイベントを取得できます
		// 他のアプリケーションが emit() で送信したイベントがここに届きます
		for (const auto& event : bus.events())
		{
			// イベントのデータから座標を取得
			auto x = event.value[U"x"].getOpt<int32>();
			auto y = event.value[U"y"].getOpt<int32>();
			
			// "lightsout/toggle" チャンネルのイベントで、座標が正しく取得できた場合
			if (event.channel == U"lightsout/toggle" && x != none && y != none)
			{
				// アニメーションを開始
				animationTarget = Point{ *x, *y };
				animationTimer.restart();
			}
		}

		// グリッドの各セルを描画する
		for (int32 y = 0; y < GridSize; ++y)
		{
			for (int32 x = 0; x < GridSize; ++x)
			{
				int32 index = y * GridSize + x;
				bool isOn = currentBoard[index].get<bool>();
				
				RectF cell{ GridPos.x + x * CellSize, GridPos.y + y * CellSize, CellSize };
				
				// セルの状態に応じて色を変えて描画する
				// ONのときは黄色、OFFのときは暗い色
				if (isOn)
				{
					cell.stretched(-5).draw(Color{ 248, 221, 107 });
					cell.stretched(-1).drawFrame(4, 0, Color{ 235, 175, 41 });
				}
				else
				{
					cell.stretched(-5).draw(Color{ 14, 28, 31 });
					cell.stretched(-1).drawFrame(4, 0, Color{ 32, 42, 38 });
				}
			}
		}

		// クリア時のメッセージを表示
		if (allOff)
		{
			font(U"CLEAR!!").drawAt(GridPos.x + GridWidth / 2, GridPos.y + GridHeight + 30, Palette::Yellow);
		}

		// 他のアプリケーションからの切り替えイベントを視覚的に表示するアニメーション
		// プラス記号が拡大・フェードアウトするアニメーション
		if (animationTimer.isRunning())
		{
			RectF cell{ GridPos.x + animationTarget.x * CellSize, GridPos.y + animationTarget.y * CellSize, CellSize };

			double scale = 1.0 + EaseOutQuint(animationTimer.progress0_1()) * 0.1;
			ColorF color = ColorF{ Palette::Lightgray, EaseOutQuint(animationTimer.progress1_0()) * 0.4 };

			Shape2D::Plus(CellSize * 1.5 * scale, CellSize * scale, cell.center())
				.draw(color);
		}

		// Resetボタンを表示し、クリックされたときの処理
		if (SimpleGUI::ButtonAt(U"Reset", Vec2{ GridPos.x + GridWidth / 2, GridPos.y + GridHeight + 70 }))
		{
			// パズルをリセットして新しい問題を生成する
			// すべてOFFの状態から11回（奇数回）ランダムに切り替えすることで
			// 必ず解けるパズルを生成する
			JSON newBoard = JSON(Array<bool>(GridSize * GridSize, false));

			for (int32 i = 0; i < 11; ++i)
			{
				int32 rx = Random(0, GridSize - 1);
				int32 ry = Random(0, GridSize - 1);
				ToggleCell(newBoard, rx, ry, GridSize);
			}

			// 新しいボード状態を共有変数に設定
			// この変更は他のアプリケーションにも反映されます
			board.set(newBoard);
			currentBoard = newBoard;
		}
	}
}
