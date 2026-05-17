//-----------------------------------------------------------------------------
//
//  Siv3D_MessageBus サンプル: オンライン対戦リバーシ
//
//  MessageBus を用いて、2 人のプレイヤーが同じ盤面を共有しながら対戦できる
//  シンプルなリバーシゲームのサンプルです。
//
//  ---------------------------------------------------------------------------
//  使用している主な機能
//  ---------------------------------------------------------------------------
//    * MessageBus::MessageBus
//        Redis サーバー経由でプレイヤー間の通信を仲介するメインクラス
//    * MessageBus::SharedVariable<JSON>
//        全プレイヤー間で同期される共有変数 (盤面の状態を保持)
//    * bus.emit / bus.subscribe / bus.events
//        join / put などのゲームイベントを送受信
//    * bus.id / bus.onlineIdList
//        自分の ID とオンライン中のプレイヤー一覧の取得
//
//  ---------------------------------------------------------------------------
//  画面遷移
//  ---------------------------------------------------------------------------
//    1. 接続画面     : Redis のホストアドレスとルーム ID を入力して接続
//    2. 接続待機画面 : 接続が確立されるまでスピナーを表示
//    3. ゲーム画面   : リバーシの対戦を行う
//                      最初に接続したプレイヤーが黒、後から Join した
//                      プレイヤーが白となり、黒側が進行を管理します
//
//  ---------------------------------------------------------------------------
//  遊び方
//  ---------------------------------------------------------------------------
//    Redis サーバーを起動した状態で、2 つ以上のクライアントから同じ
//    ホストアドレス・ルーム ID を入力して接続してください。
//
//-----------------------------------------------------------------------------


#include <Siv3D.hpp>
#include <MessageBus/MessageBus.hpp>

namespace
{
	// Layout
	static constexpr RectF BoardRect{ 30, 20, 500, 500 };
	static constexpr SizeF CellSize = BoardRect.size / 8;
	static constexpr double PieceLabelCenterY = 558;
	static constexpr Vec2 IdLabelPos{ 550, 20 };
	static constexpr Vec2 IdPos{ 550, 50 };
	static constexpr Vec2 OnlineIdLabelPos{ 550, 80 };
	static constexpr Point OnlineIdStartPos{ 550, 110 };
	static constexpr Vec2 BoardTextLabelPos{ 550, 340 };
	static constexpr Rect BoardTextArea{ 550, 370, 230, 200 };
	static constexpr Vec2 EventLogLabelPos{ 790, 20 };
	static constexpr Rect EventLogArea{ 790, 50, 230, 520 };

	// Color
	static constexpr Color BackgroundColor{ 249, 249, 249 };
	static constexpr Color BoardColor{ 53, 154, 79 };
	static constexpr Color BoardFrameColor{ 8, 46, 9 };
	static constexpr Color PrimaryTextColor{ 30, 30, 30 };
	static constexpr Color SecondaryTextColor{ 68, 68, 68 };
	static constexpr Color HighlightedTextColor{ 35, 90, 34 };
	static constexpr Color BlackPieceInnerColor{ 54, 57, 59 };
	static constexpr Color BlackPieceOuterColor{ 40, 40, 40 };
	static constexpr Color BlackPieceFrameColor{ 70, 70, 70 };
	static constexpr Color WhitePieceInnerColor{ 230, 230, 230 };
	static constexpr Color WhitePieceOuterColor{ 252, 251, 253 };
	static constexpr Color WhitePieceFrameColor{ 100, 100, 100 };
	static constexpr Color PieceShadowColor{ 0, 0, 0, 76 };
	static constexpr Color CellHighlightColor{ 255, 255, 255, 76 };
}

// リバーシの盤面と進行状態を表すクラス
struct ReversiBoard
{
	// 黒プレイヤーのIDを指定して新規の盤面を作成
	ReversiBoard(StringView blackId)
		: updatedAt(DateTime::Now())
		, board(8, 8, 0)
		, state(0)
		, blackId(blackId)
		, whiteId()
	{
		board[3][3] = 2;
		board[4][4] = 2;
		board[3][4] = 1;
		board[4][3] = 1;
	}

	// JSON から盤面を復元するコンストラクタ（他プレイヤーから受信した状態の反映に使用）
	ReversiBoard(DateTime timestamp, const JSON& json)
		: updatedAt(timestamp)
		, board(8, 8, 0)
		, state(json[U"state"].get<int32>())
		, blackId(json[U"blackId"].get<String>())
		, whiteId(json[U"whiteId"].get<String>())
	{
		JSON boardJson = json[U"board"];
		for (auto idx : Iota<size_t>(64))
		{
			board[{idx % 8, idx / 8}] = boardJson[idx].get<int32>();
		}
	}

	// 8 方向の隣接マスへのオフセット（石の反転判定に使用）
	static constexpr std::array<Point, 8> AllDeltas = {
		Point{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}
	};

	// 盤面の状態を JSON に変換
	JSON asJson() const
	{
		JSON json;
		json[U"state"] = state;
		json[U"blackId"] = blackId;
		json[U"whiteId"] = whiteId;
		Array<int32> boardArray(64);
		for (auto idx : Iota<size_t>(64))
		{
			boardArray[idx] = board[{idx % 8, idx / 8}];
		}
		json[U"board"] = boardArray;
		return json;
	}

	// 白プレイヤーの ID を設定してゲームを開始（stateを 1=黒の手番 へ遷移）
	void setWhiteId(StringView id)
	{
		assert(state == 0 && id);

		whiteId = id;
		state = 1;
		updatedAt = DateTime::Now();
	}

	// 指定位置に石を置く。dryRun=true なら配置可否のみを判定する
	bool putPiece(Point pos, int32 type, bool dryRun = false)
	{
		assert(state == 1 || state == 2);

		if (not isInside(pos)) return false;
		if (board[pos] != 0) return false;

		// 8方向それぞれについて、相手の石を挟めるかを事前にチェックする
		bool flipSuccessAny = false;
		std::array<bool, 8> flipSuccess = { false, false, false, false, false, false, false, false };
		for (size_t i = 0; i < 8; i++)
		{
			bool success = flipPiece(pos, AllDeltas[i], type, true);
			if (success && dryRun) return true;
			flipSuccess[i] = success;
			flipSuccessAny |= success;
		}
		if (not flipSuccessAny || dryRun) return false;

		// 実際に挟めた方向の石を反転する
		for (size_t i = 0; i < 8; i++)
		{
			if (flipSuccess[i])
			{
				flipPiece(pos, AllDeltas[i], type);
			}
		}

		board[pos] = type;
		// 次の手番を決定する。両者ともに置けない場合はゲーム終了 (3)
		switch (state)
		{
		case 1:
			state = canPutPieceAny(2) ? 2 : canPutPieceAny(1) ? 1 : 3;
			break;
		case 2:
			state = canPutPieceAny(1) ? 1 : canPutPieceAny(2) ? 2 : 3;
			break;
		}
		updatedAt = DateTime::Now();

		return true;
	}

	// 指定した色の石の数を数える
	int32 countPieces(int32 type) const
	{
		int32 count = 0;
		for (auto pos : Iota2D(8, 8))
		{
			if (board[pos] == type)
			{
				count++;
			}
		}

		return count;
	}

	DateTime updatedAt;

	// 0 = Empty
	// 1 = Black
	// 2 = White
	Grid<int32> board;

	// 0 = Match making
	// 1 = Black's turn
	// 2 = White's turn
	// 3 = Game Over
	int32 state;

	String blackId;

	String whiteId;

private:

	// 座標が 8x8 の盤面の内側にあるか判定する
	static bool isInside(Point pos)
	{
		return Rect{ 0, 0, 8, 8 }.contains(pos);
	}

	// 指定した色が盤面のどこかに合法手を持っているか判定する
	bool canPutPieceAny(int32 type)	
	{
		for (auto pos : Iota2D(8, 8))
		{
			if (putPiece(pos, type, true))
			{
				return true;
			}
		}

		return false;
	}

	// pos から delta 方向に挟んだ相手の石を反転させる。dryRun = true なら判定のみ
	bool flipPiece(Point pos, Point delta, int32 type, bool dryRun = false)
	{
		int32 count = 0;
		pos += delta;

		while (isInside(pos))
		{
			int32& current = board[pos];

			if (current == 0)
			{
				return false;
			}
			else if (current == type)
			{
				return count != 0;
			}
			else if (not dryRun)
			{
				current = type;
			}

			count++;
			pos += delta;
		}

		return false;
	}
};

// ランダムな5文字のルームIDを生成する
String GenerateRandomBoardId()
{
	static constexpr StringView CharacterList = U"ABCDEFGHJKLMNPQRSTUVWXYZ0123456789";

	String roomId;
	for (auto _ : Iota(5))
	{
		roomId.push_back(CharacterList[RandomClosedOpen<size_t>(0, CharacterList.size())]);
	}

	return roomId;
}

// 1つの石を描画 (type: 1=黒, 2=白)
void DrawPiece(Vec2 pos, int32 type)
{
	Circle piece{ pos, 24 };
	switch (type)
	{
	case 1:
		piece.drawShadow({ 0, 2 }, 4, 2, PieceShadowColor);
		piece.draw(BlackPieceInnerColor, BlackPieceOuterColor);
		piece.drawFrame(1, BlackPieceFrameColor);
		break;
	case 2:
		piece.drawShadow({ 0, 2 }, 4, 2, PieceShadowColor);
		piece.draw(WhitePieceInnerColor, WhitePieceOuterColor);
		piece.drawFrame(1, WhitePieceFrameColor);
		break;
	}
}

// 盤面を描画
void DrawBoard(const ReversiBoard& board)
{
	RoundRect rrect{ BoardRect, 10 };

	rrect.draw(BoardColor).drawFrame(4, BoardFrameColor);

	// 格子線を描く
	for (auto idx : Range(1, 7))
	{
		Vec2 point = BoardRect.pos + CellSize * idx;
		Line horizontalLine{ BoardRect.leftX(), point.y, BoardRect.rightX(), point.y };
		Line verticalLine{ point.x, BoardRect.topY(), point.x, BoardRect.bottomY() };

		horizontalLine.draw(2, BoardFrameColor);
		verticalLine.draw(2, BoardFrameColor);
	}

	// 4 か所の基準点を描く
	Circle{ BoardRect.pos + CellSize * Vec2{ 2, 2 }, 4 }.draw(BoardFrameColor);
	Circle{ BoardRect.pos + CellSize * Vec2{ 6, 2 }, 4 }.draw(BoardFrameColor);
	Circle{ BoardRect.pos + CellSize * Vec2{ 2, 6 }, 4 }.draw(BoardFrameColor);
	Circle{ BoardRect.pos + CellSize * Vec2{ 6, 6 }, 4 }.draw(BoardFrameColor);

	// 各マスの石を描く
	for (auto pos : Iota2D(8, 8))
	{
		DrawPiece(BoardRect.pos + CellSize * (pos + Vec2{ 0.5, 0.5 }), board.board[pos]);
	}
}

void Main()
{
	Scene::SetBackground(BackgroundColor);
	Scene::Resize(1040, 600);
	Window::Resize(1040, 600);
	
	// [共通] MessageBus 本体（プレイヤー間の通信と共有変数の管理を行う）
	MessageBus::MessageBus bus;

	// [共通] アイコンが表示できるフォント
	const Font font{ 30 };
	const Font emojiFont{ 30, Typeface::Icon_MaterialDesign };
	font.addFallback(emojiFont);

	// [接続画面] 接続先ホストとルーム ID のテキスト入力状態
	TextEditState hostInputState{ U"127.0.0.1" };
	TextEditState roomIdInputState{ GenerateRandomBoardId() };

	// [ゲーム画面] 全プレイヤーで共有する盤面（JSON 形式）と、それを描画用に展開したキャッシュ
	std::unique_ptr<MessageBus::SharedVariable<JSON>> sharedBoard;
	std::unique_ptr<ReversiBoard> cachedBoard;

	// [ゲーム画面] 受信イベントのログ（直近 10 件まで保持）
	Array<String> eventLog;

	while (System::Update())
	{
		// MessageBus の通信を 1 フレーム分処理し、新着イベントを取得
		bus.update();

		// 受信したイベントをログに追加（古いものから順に破棄）
		for (const auto& event : bus.events())
		{
			eventLog.push_back(U"{}:{}"_fmt(event.channel, event.value.formatMinimum()));
			if (eventLog.size() > 10)
			{
				eventLog.pop_front();
			}
		}

		// ================================================
		// [接続画面] 接続先とルームIDを入力し、ConnectボタンがクリックされたらMessageBus::connectを呼び出す 
		// ================================================
		if (not sharedBoard)
		{
			// Redisのアドレス
			font(U"Host Address")
				.draw(20, Vec2{ 10, 10 }, PrimaryTextColor);
			SimpleGUI::TextBox(hostInputState, Vec2{ 10, 40 }, 180, unspecified);

			// ルームID
			font(U"Room ID")
				.draw(20, Vec2{ 10, 80 }, PrimaryTextColor);
			SimpleGUI::TextBox(roomIdInputState, Vec2{ 10, 110 }, 180, unspecified);

			bool enableConnectButton = hostInputState.text && roomIdInputState.text;
			if (SimpleGUI::Button(U"Connect", Vec2{ 10, 160 }, 100, enableConnectButton))
			{
				// 自分を黒プレイヤーとした初期盤面を初期値として共有変数を初期化
				// (既に同じIDの盤面があればそれで上書きされる仕組み)
				// - board:{roomId}
				ReversiBoard initialBoard{ bus.id() };
				sharedBoard = std::make_unique<MessageBus::SharedVariable<JSON>>(
					bus.variable<JSON>(U"board:" + roomIdInputState.text, initialBoard.asJson())
				);
				// ゲーム進行に必要なイベントを購読
				// - board:{roomId}:join
				// - board:{roomId}:put
				bus.subscribe(U"board:{}:join"_fmt(roomIdInputState.text));
				bus.subscribe(U"board:{}:put"_fmt(roomIdInputState.text));

				// Redisサーバーに接続
				// 自動的に接続待機画面へ遷移します
				bus.connect(hostInputState.text, 6379);
			}

			continue;
		}
		
		// ================================================
		// [接続待機画面] 接続が確立されるまでスピナーを表示する
		// ================================================
		if (not bus.isConnected())
		{
			// スピナー
			Circle spinner{ Scene::CenterF(), 30 };
			spinner.drawArc(
				Periodic::Sawtooth0_1(1s) * Math::TwoPi,
				Math::TwoPi * 3 / 4,
				0,
				10,
				SecondaryTextColor
			);

			// エラー表示
			if (bus.error())
			{
				font(bus.error())
					.draw(
						20,
						Arg::topCenter = Scene::CenterF().movedBy(0, 50),
						SecondaryTextColor
					);
			}

			continue;
		}

		// ================================================
		// [ゲーム画面] 最新の盤面とステータスを表示する
		// ================================================

		// マウスカーソルがあるマスの座標を求める
		Optional<Point> hoveredCellPos;
		if (BoardRect.contains(Cursor::PosF()) && Window::GetState().focused)
		{
			hoveredCellPos = ((Cursor::PosF() - BoardRect.pos) / (BoardRect.size / 8)).asPoint();
		}

		// 更新を監視して、共有変数(MessageBus::SharedVariable)の値をcachedBoardに同期
		if (not cachedBoard || sharedBoard->updatedAt() > cachedBoard->updatedAt)
		{
			cachedBoard = std::make_unique<ReversiBoard>(
				sharedBoard->updatedAt(), sharedBoard->get()
			);
		}

		// 自分が黒プレイヤーか白プレイヤーかを判定
		// 
		// 黒プレイヤー中心にゲームの進行を管理させることで、競合を防ぎます
		// 最初に盤面を作成したクライアントが自動的に黒プレイヤーになります
		
		bool isBlackPlayer = cachedBoard->blackId == bus.id(); // ゲームの進行を管理
		bool isWhitePlayer = cachedBoard->whiteId == bus.id();

		// カーソル位置にハイライトを描画するフラグ
		bool renderCellHighlight = false;
		
		// [state: 0] 白プレイヤーの参加待ち
		if (cachedBoard->state == 0)
		{
			// 黒プレイヤー: Joinイベントを受信したら白プレイヤーのIDを設定する
			if (isBlackPlayer)
			{
				for (auto event : bus.events())
				{
					if (event.channel == U"board:{}:join"_fmt(roomIdInputState.text))
					{
						cachedBoard->setWhiteId(event.value[U"id"].get<String>());
						sharedBoard->set(cachedBoard->asJson()); // 共有変数に反映
						break;
					}
				}
			}
		}
		// [state: 1] 黒の手番
		else if (cachedBoard->state == 1)
		{
			// 黒プレイヤー: マスをクリックで黒石を置く
			if (isBlackPlayer)
			{
				renderCellHighlight = true;
				if (MouseL.down() && hoveredCellPos)
				{
					cachedBoard->putPiece(hoveredCellPos.value(), 1);
					sharedBoard->set(cachedBoard->asJson()); // 共有変数に反映
				}
			}
		}
		// [state: 2] 白の手番
		else if (cachedBoard->state == 2)
		{
			// 黒プレイヤー: putイベントを受信したら白石を置く
			if (isBlackPlayer)
			{
				for (auto event : bus.events())
				{
					if (event.channel == U"board:{}:put"_fmt(roomIdInputState.text))
					{
						Point pos{ event.value[U"x"].get<int32>(), event.value[U"y"].get<int32>() };
						int32 type = event.value[U"type"].get<int32>();
						
						if (cachedBoard->putPiece(pos, type))
						{
							sharedBoard->set(cachedBoard->asJson()); // 共有変数に反映
						}
						break;
					}
				}
			}
			
			// 白プレイヤー: マスをクリックしてputイベントを送信する
			if (isWhitePlayer)
			{
				renderCellHighlight = true;
				if (MouseL.down() && hoveredCellPos)
				{
					bus.emit(U"board:{}:put"_fmt(roomIdInputState.text), JSON{
						{ U"x", hoveredCellPos->x },
						{ U"y", hoveredCellPos->y },
						{ U"type", 2 }
					});
				}
			}
		}

		// --- ゲーム画面の描画 ---

		// 盤面
		DrawBoard(*cachedBoard);

		// マスのハイライト
		if (renderCellHighlight && hoveredCellPos)
		{
			RectF{ BoardRect.pos + CellSize * hoveredCellPos.value(), CellSize }
				.draw(CellHighlightColor);
		}

		// 黒プレイヤーのステータス表示
		DrawPiece({ 62, PieceLabelCenterY }, 1);
		String blackPieceCountText = Format(cachedBoard->countPieces(1));
		if (cachedBoard->blackId == bus.id()) blackPieceCountText += U" (You)";
		if (cachedBoard->state == 1) blackPieceCountText += U" \U000F06C1";
		font(blackPieceCountText).draw(30, Arg::leftCenter = Vec2{ 96, PieceLabelCenterY }, PrimaryTextColor);

		// 白プレイヤーのステータス表示
		// 参加待ち状態であればステータスの代わりにJoinボタンを表示
		DrawPiece({ 312, PieceLabelCenterY }, 2);
		if (cachedBoard->state == 0)
		{
			if (SimpleGUI::ButtonAt(U"Join", { 396, PieceLabelCenterY }, 100))
			{
				// クリックされたらjoinイベントを送信
				bus.emit(U"board:{}:join"_fmt(roomIdInputState.text), JSON{
					{ U"id", bus.id() }
				});
			}
		}
		else
		{
			String whitePieceCountText = Format(cachedBoard->countPieces(2));
			if (cachedBoard->whiteId == bus.id()) whitePieceCountText += U" (You)";
			if (cachedBoard->state == 2) whitePieceCountText += U" \U000F06C1";
			font(whitePieceCountText).draw(30, Arg::leftCenter = Vec2{ 346, PieceLabelCenterY }, PrimaryTextColor);
		}

		// MessageBusのステータス表示
		// - 自分のID
		// - オンライン状態のIDの一覧
		// - 共有変数の内容
		// - イベント履歴

		font(U"\U000F1C1B Your ID")
			.draw(20, IdLabelPos, PrimaryTextColor);
		font(bus.id())
			.draw(15, IdPos, SecondaryTextColor);

		font(U"\U000F1B0A Online ID(s)")
			.draw(20, OnlineIdLabelPos, PrimaryTextColor);
		for (auto i : Iota<int32>(bus.onlineIdList().size()))
		{
			StringView id = bus.onlineIdList()[i];
			Vec2 pos = OnlineIdStartPos + Vec2{ 0, font.height(15) * i };
			bool isSelfId = id == bus.id();

			font(id)
				.draw(15, pos, isSelfId ? HighlightedTextColor : SecondaryTextColor);
		}

		font(U"\U000F10FB " + sharedBoard->name())
			.draw(20, BoardTextLabelPos, PrimaryTextColor);
		font(sharedBoard->get().formatMinimum())
			.draw(16, BoardTextArea, SecondaryTextColor);

		font(U"\U000F140B Event")
			.draw(20, EventLogLabelPos, PrimaryTextColor);
		String eventLogText = U"";
		for (auto [_, log] : ReverseIndexedRef(eventLog))
		{
			eventLogText += log + U"\n";
		}
		font(eventLogText).draw(16, EventLogArea, SecondaryTextColor);
	}

	// 終了時に MessageBus を切断・破棄する
	bus.shutdown();
}
