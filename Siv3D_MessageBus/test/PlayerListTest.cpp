#include <gtest/gtest.h>

#include <MessageBus/detail/PlayerList.hpp>

#include "RedisDockerTestFixture.hpp"
#include "Utility.hpp"

#include <Siv3D.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <string_view>
#include <string>
#include <fmt/format.h>

class PlayerListTest : public RedisDocker
{
protected:
	static void SetUpTestSuite()
	{
		RedisDocker::SetUpTestSuite();
		StartContainer();
	}

	static void TearDownTestSuite()
	{
		RedisDocker::TearDownTestSuite();
	}

public:
	// PlayerList の beforeTick / tick / afterTick を回しながら条件待ち
	template <class Pred>
	static bool WaitUntil(MessageBus::detail::PlayerList& plist, MessageBus::detail::RedisConnection& conn, Pred&& predicate, Duration timeout = 5s)
	{
		Stopwatch sw{ StartImmediately::Yes };
		while (sw < timeout && !predicate())
		{
			plist.beforeTick(conn);
			conn.tick();
			plist.afterTick();
			System::Sleep(TICK_INTERVAL);
		}
		return predicate();
	}

	static bool RedisExists(const std::string& key)
	{
		auto [exitCode, output] = ExecRedisCli({ "EXISTS", key });
		if (exitCode != 0)
		{
			return false;
		}
		return (std::stol(output) == 1);
	}

	static int64_t RedisPTTL(const std::string& key)
	{
		auto [exitCode, output] = ExecRedisCli({ "PTTL", key });
		EXPECT_EQ(exitCode, 0);
		if (output.empty())
		{
			return -9999;
		}
		return std::stoll(output);
	}
};

// UID

TEST_F(PlayerListTest, UidIsGeneratedInConstructor)
{
	MessageBus::detail::PlayerList list{ {} };
	const std::string_view uid = list.uidUtf8();
	EXPECT_FALSE(uid.empty());

	for (const unsigned char ch : uid)
	{
		EXPECT_TRUE(std::isalnum(ch)) << "uid=" << uid;
	}
}

TEST_F(PlayerListTest, UidIsUniqueAcrossInstances)
{
	MessageBus::detail::PlayerList a{ {} };
	MessageBus::detail::PlayerList b{ {} };

	EXPECT_NE(a.uidUtf8(), b.uidUtf8());
}

// onConnect

TEST_F(PlayerListTest, OnConnectSetsSessionKeyWithTtlAndValue)
{
	MessageBus::detail::RedisConnection conn{ { .ip=U"127.0.0.1", .port=6379 } };
	MessageBus::detail::PlayerList plist{ {
		.sessionTtlMs = 2000,
	} };
	std::string key = fmt::format("s3d-mbus:player:{}", plist.uidUtf8());

	ASSERT_TRUE(WaitForConnection(conn, 10s));

	plist.onConnect(conn);
	EXPECT_EQ(plist.sessionStatus(), MessageBus::detail::PlayerList::SessionStatus::InactiveOrExpired);

	ASSERT_TRUE(WaitUntil(plist, conn, [&]() { 
		return plist.sessionStatus() == MessageBus::detail::PlayerList::SessionStatus::Active; 
	}, 1s));
	ASSERT_TRUE(RedisExists(key));

	// TTLを取得
	const long long pttl = RedisPTTL(key);
	EXPECT_GT(pttl, 0);
	EXPECT_LE(pttl, 2000);
}

TEST_F(PlayerListTest, OnConnectPublishesPlayerJoin)
{
	MessageBus::detail::RedisConnection conn{ { .ip=U"127.0.0.1", .port=6379 } };
	MessageBus::detail::PlayerList plist{ {
		.sessionTtlMs = 10,
		.sessionRefreshInterval = 30ms,
	} };
	ASSERT_TRUE(WaitForConnection(conn, 10s));
	plist.onConnect(conn);

	const int received = WithSubscription("s3d-mbus:player-join", [&]()
	{
		// refreshを複数回させる
		Sleep(plist, conn, 100ms);
	});

	EXPECT_GE(received, 1);
}

// beforeTick

TEST_F(PlayerListTest, BeforeTickUpdatesSessionKey)
{
	MessageBus::detail::RedisConnection conn{ { .ip=U"127.0.0.1", .port=6379 } };
	MessageBus::detail::PlayerList plist{ {
		.sessionRefreshInterval = 500ms,
	} };
	std::string key = fmt::format("s3d-mbus:player:{}", plist.uidUtf8());
	ASSERT_TRUE(WaitForConnection(conn, 10s));

	// セッションを作成 (10~100ms)
	plist.onConnect(conn);
	ASSERT_TRUE(WaitUntil(plist, conn, [&]() { 
		return plist.sessionStatus() == MessageBus::detail::PlayerList::SessionStatus::Active; 
	}, 100ms));

	// リフレッシュ直前でTTLを取得 (300ms)
	System::Sleep(300ms);
	int64_t pttlBeforeRefresh = RedisPTTL(key);
	
	// リフレッシュされるまで待機、同時に、SessionStatusがExpiredにならないことを確認
	EXPECT_TRUE(WaitUntil(plist, conn, [&]() { 
		return 
			RedisPTTL(key) > pttlBeforeRefresh && 
			plist.sessionStatus() == MessageBus::detail::PlayerList::SessionStatus::Active; 
	}, 500ms));
}

TEST_F(PlayerListTest, BeforeTickDoesNotPublishPlayerJoinOnRefresh)
{
	MessageBus::detail::RedisConnection conn{ { .ip=U"127.0.0.1", .port=6379 } };
	MessageBus::detail::PlayerList plist{ {
		.sessionTtlMs = 60 * 1000,
		.sessionRefreshInterval = 10ms,
	} };

	ASSERT_TRUE(WaitForConnection(conn, 10s));
	plist.onConnect(conn);

	ASSERT_TRUE(WaitUntil(plist, conn, [&]() { 
		return plist.sessionStatus() == MessageBus::detail::PlayerList::SessionStatus::Active; 
	}, 100ms));
	Sleep(plist, conn, 100ms);

	const int received = WithSubscription("s3d-mbus:player-join", [&]()
	{
		// refreshを複数回させる
		Sleep(plist, conn, 100ms);
	});

	EXPECT_EQ(received, 0);
}

TEST_F(PlayerListTest, BeforeTickRecreatesSessionWhenExpired)
{
	MessageBus::detail::RedisConnection conn{ { .ip=U"127.0.0.1", .port=6379 } };
	MessageBus::detail::PlayerList plist{ MessageBus::detail::PlayerList::Options{
		.sessionTtlMs = 200,
		.sessionRefreshInterval = 100ms,
	} };
	std::string key = fmt::format("s3d-mbus:player:{}", plist.uidUtf8());
	ASSERT_TRUE(WaitForConnection(conn, 10s));

	// セッションを作成
	plist.onConnect(conn);
	ASSERT_TRUE(WaitUntil(plist, conn, [&]() { 
		return plist.sessionStatus() == MessageBus::detail::PlayerList::SessionStatus::Active; 
	}, 100ms));

	// 失効されるまで十分な時間待機して、削除されたことを確認
	System::Sleep(210ms);
	ASSERT_FALSE(RedisExists(key));

	// 次回Tickで失効扱いになっていることを確認
	// → 失効した直後にセッションが再生成される
	plist.beforeTick(conn);
	conn.tick();
	ASSERT_TRUE(plist.sessionStatus() == MessageBus::detail::PlayerList::SessionStatus::InactiveOrExpired);

	// セッションが再作成されるまで待機
	ASSERT_TRUE(WaitUntil(plist, conn, [&]() { 
		return plist.sessionStatus() == MessageBus::detail::PlayerList::SessionStatus::Active; 
	}, 100ms));
	ASSERT_TRUE(RedisExists(key));
}

TEST_F(PlayerListTest, BeforeTickPublishesPlayerJoinWhenSessionRecreated)
{
	MessageBus::detail::RedisConnection conn{ { .ip=U"127.0.0.1", .port=6379 } };
	MessageBus::detail::PlayerList plist{ MessageBus::detail::PlayerList::Options{
		.sessionTtlMs = 200,
		.sessionRefreshInterval = 100ms,
	} };
	ASSERT_TRUE(WaitForConnection(conn, 10s));

	// セッションを作成
	plist.onConnect(conn);
	ASSERT_TRUE(WaitUntil(plist, conn, [&]() { 
		return plist.sessionStatus() == MessageBus::detail::PlayerList::SessionStatus::Active; 
	}, 100ms));

	// セッション再作成
	System::Sleep(210ms);
	plist.beforeTick(conn);
	conn.tick();
	ASSERT_TRUE(plist.sessionStatus() == MessageBus::detail::PlayerList::SessionStatus::InactiveOrExpired);

	const int received = WithSubscription("s3d-mbus:player-join", [&]()
	{
		// 次回tickでセッションが再生成されたことを検知
		WaitUntil(plist, conn, [&]() {
			return plist.sessionStatus() == MessageBus::detail::PlayerList::SessionStatus::Active;
		}, 1s);

		// 次のtickでjoinが再度publishされる
		Sleep(plist, conn, 100ms);
	});

	EXPECT_EQ(received, 1);
}

// beforeDisconnect

TEST_F(PlayerListTest, BeforeDisconnectDeletesSessionKey)
{
	MessageBus::detail::PlayerList plist{ { } };
	MessageBus::detail::RedisConnection conn{ {
		.ip = U"127.0.0.1",
		.port = 6379,
		.onDisconnect = [&]()
		{
			plist.onDisconnect();
		},
	} };
	std::string key = fmt::format("s3d-mbus:player:{}", plist.uidUtf8());

	// 接続を確立
	ASSERT_TRUE(WaitForConnection(conn, 10s));
	plist.onConnect(conn);

	// セッションが作成されるまで待機
	ASSERT_TRUE(WaitUntil(plist, conn, [&]() { 
		return plist.sessionStatus() == MessageBus::detail::PlayerList::SessionStatus::Active; 
	}, 5s));
	ASSERT_TRUE(RedisExists(key));

	// beforeDisconnectを呼び出し
	plist.beforeDisconnect(conn);
	conn.disconnect();

	// セッションが失効されるのを確認
	ASSERT_TRUE(WaitUntil(plist, conn, [&]() { 
		return plist.sessionStatus() == MessageBus::detail::PlayerList::SessionStatus::InactiveOrExpired; 
	}, 5s));
	EXPECT_FALSE(RedisExists(key));
}

TEST_F(PlayerListTest, BeforeDisconnectPublishesPlayerLeft)
{
	MessageBus::detail::PlayerList plist{ { } };
	MessageBus::detail::RedisConnection conn{ {
		.ip = U"127.0.0.1",
		.port = 6379,
		.onDisconnect = [&]()
		{
			plist.onDisconnect();
		},
	} };

	ASSERT_TRUE(WaitForConnection(conn, 10s));
	plist.onConnect(conn);

	ASSERT_TRUE(WaitUntil(plist, conn, [&]() {
		return plist.sessionStatus() == MessageBus::detail::PlayerList::SessionStatus::Active;
	}, 5s));

	const int received = WithSubscription("s3d-mbus:player-left", [&]()
	{
		plist.beforeDisconnect(conn);
		conn.disconnect();

		WaitUntil(plist, conn, [&]() { return conn.state() == MessageBus::detail::RedisConnectionState::Disconnected; }, 3s);
	});

	EXPECT_EQ(received, 1);
}

// onDisconnect

TEST_F(PlayerListTest, OnDisconnectResetsSessionStatus)
{
	MessageBus::detail::RedisConnection conn{ { .ip=U"127.0.0.1", .port=6379 } };
	MessageBus::detail::PlayerList plist{ { } };
	std::string key = fmt::format("s3d-mbus:player:{}", plist.uidUtf8());

	ASSERT_TRUE(WaitForConnection(conn, 10s));

	plist.onConnect(conn);
	ASSERT_TRUE(WaitUntil(plist, conn, [&]() { 
		return plist.sessionStatus() == MessageBus::detail::PlayerList::SessionStatus::Active; 
	}, 5s));

	plist.onDisconnect();
	EXPECT_EQ(plist.sessionStatus(), MessageBus::detail::PlayerList::SessionStatus::InactiveOrExpired);
}

// handlePubSubMessage

TEST_F(PlayerListTest, HandlePubSubMessageAddsUidOnPlayerJoin)
{
	MessageBus::detail::RedisConnection conn{ { .ip = U"127.0.0.1", .port = 6379 } };
	MessageBus::detail::PlayerList plist{ {} };

	ASSERT_TRUE(WaitForConnection(conn, 10s));
	plist.onConnect(conn);
	ASSERT_TRUE(WaitUntil(plist, conn, [&]() {
		return plist.sessionStatus() == MessageBus::detail::PlayerList::SessionStatus::Active;
	}, 5s));

	const bool handled = plist.handlePubSubMessage("s3d-mbus:player-join", "user-123");
	EXPECT_TRUE(handled);

	const auto& uids = plist.connectedPlayerUidsUtf8();
	EXPECT_NE(uids.find("user-123"), uids.end());
}

TEST_F(PlayerListTest, HandlePubSubMessageRemovesUidOnPlayerLeft)
{
	MessageBus::detail::RedisConnection conn{ { .ip = U"127.0.0.1", .port = 6379 } };
	MessageBus::detail::PlayerList plist{ {} };

	ASSERT_TRUE(WaitForConnection(conn, 10s));
	plist.onConnect(conn);
	ASSERT_TRUE(WaitUntil(plist, conn, [&]() {
		return plist.sessionStatus() == MessageBus::detail::PlayerList::SessionStatus::Active;
	}, 5s));

	plist.handlePubSubMessage("s3d-mbus:player-join", "user-123");
	ASSERT_NE(plist.connectedPlayerUidsUtf8().find("user-123"), plist.connectedPlayerUidsUtf8().end());

	const bool handled = plist.handlePubSubMessage("s3d-mbus:player-left", "user-123");
	EXPECT_TRUE(handled);
	EXPECT_EQ(plist.connectedPlayerUidsUtf8().find("user-123"), plist.connectedPlayerUidsUtf8().end());
}

TEST_F(PlayerListTest, HandlePubSubMessageReturnsFalseForUnknownChannel)
{
	MessageBus::detail::RedisConnection conn{ { .ip = U"127.0.0.1", .port = 6379 } };
	MessageBus::detail::PlayerList plist{ {} };

	ASSERT_TRUE(WaitForConnection(conn, 10s));
	plist.onConnect(conn);
	ASSERT_TRUE(WaitUntil(plist, conn, [&]() {
		return plist.sessionStatus() == MessageBus::detail::PlayerList::SessionStatus::Active;
	}, 5s));

	const size_t sizeBefore = plist.connectedPlayerUidsUtf8().size();
	const bool handled = plist.handlePubSubMessage("other-channel", "payload");
	EXPECT_FALSE(handled);
	EXPECT_EQ(plist.connectedPlayerUidsUtf8().size(), sizeBefore);
}

// connectedPlayerUidsUtf8

TEST_F(PlayerListTest, ConnectedPlayerUidsUtf8IsEmptyBeforeConnect)
{
	MessageBus::detail::PlayerList plist{ {} };
	EXPECT_TRUE(plist.connectedPlayerUidsUtf8().empty());
}

TEST_F(PlayerListTest, ConnectedPlayerUidsUtf8ContainsSelfAfterFirstKeysResponse)
{
	MessageBus::detail::RedisConnection conn{ { .ip = U"127.0.0.1", .port = 6379 } };
	MessageBus::detail::PlayerList plist{ {
		.sessionTtlMs = 5000,
		.playerListPollInterval = 100ms,
	} };

	ASSERT_TRUE(WaitForConnection(conn, 10s));
	plist.onConnect(conn);

	ASSERT_TRUE(WaitUntil(plist, conn, [&]() {
		const auto& uids = plist.connectedPlayerUidsUtf8();
		if (uids.empty()) return false;
		return std::find(uids.begin(), uids.end(), plist.uidUtf8()) != uids.end();
	}, 5s));

	const auto& uids = plist.connectedPlayerUidsUtf8();
	EXPECT_GE(uids.size(), 1u);
	EXPECT_NE(std::find(uids.begin(), uids.end(), std::string(plist.uidUtf8())), uids.end());
}

TEST_F(PlayerListTest, ConnectedPlayerUidsUtf8FirstKeysSeesPreExistingSessions)
{
	const std::string preExistingUid = "pre-existing-session-uid";
	const std::string preExistingKey = "s3d-mbus:player:" + preExistingUid;

	// 接続前に Redis CLI でセッションキーを 1 件追加
	{
		auto [exitCode, _] = ExecRedisCli({ "SET", preExistingKey, "1" });
		ASSERT_EQ(exitCode, 0);
	}
	ASSERT_TRUE(RedisExists(preExistingKey));

	MessageBus::detail::RedisConnection conn{ { .ip = U"127.0.0.1", .port = 6379 } };
	MessageBus::detail::PlayerList plist{ {
		.sessionTtlMs = 5000,
		.playerListPollInterval = 100ms,
	} };

	ASSERT_TRUE(WaitForConnection(conn, 10s));
	plist.onConnect(conn);

	// 初回 KEYS のレスポンスで、接続前からあったセッションと自 UID の両方が含まれること
	ASSERT_TRUE(WaitUntil(plist, conn, [&]() {
		const auto& uids = plist.connectedPlayerUidsUtf8();
		return uids.size() >= 2u &&
			std::find(uids.begin(), uids.end(), plist.uidUtf8()) != uids.end() &&
			std::find(uids.begin(), uids.end(), preExistingUid) != uids.end();
	}, 5s));

	const auto& uids = plist.connectedPlayerUidsUtf8();
	EXPECT_GE(uids.size(), 2u);
	EXPECT_NE(std::find(uids.begin(), uids.end(), plist.uidUtf8()), uids.end());
	EXPECT_NE(std::find(uids.begin(), uids.end(), preExistingUid), uids.end());
}

TEST_F(PlayerListTest, ConnectedPlayerUidsUtf8RefreshSeesSessionAddedViaRedisCli)
{
	const std::string addedUid = "redis-cli-added-session-uid";
	const std::string addedKey = "s3d-mbus:player:" + addedUid;

	MessageBus::detail::RedisConnection conn{ { .ip = U"127.0.0.1", .port = 6379 } };
	MessageBus::detail::PlayerList plist{ {
		.sessionTtlMs = 5000,
		.playerListPollInterval = 100ms,
	} };

	ASSERT_TRUE(WaitForConnection(conn, 10s));
	plist.onConnect(conn);

	// 初回 KEYS のレスポンスを待つ（自 UID のみ）
	ASSERT_TRUE(WaitUntil(plist, conn, [&]() {
		const auto& uids = plist.connectedPlayerUidsUtf8();
		if (uids.empty()) return false;
		return std::find(uids.begin(), uids.end(), plist.uidUtf8()) != uids.end();
	}, 5s));

	// リフレッシュ前に Redis CLI でセッションを 1 件追加
	{
		auto [exitCode, _] = ExecRedisCli({ "SET", addedKey, "1" });
		ASSERT_EQ(exitCode, 0);
	}
	ASSERT_TRUE(RedisExists(addedKey));

	// 次の KEYS ポーリング（リフレッシュ）で追加した UID がリストに含まれること
	ASSERT_TRUE(WaitUntil(plist, conn, [&]() {
		const auto& uids = plist.connectedPlayerUidsUtf8();
		const std::string selfUid(plist.uidUtf8());
		return uids.size() >= 2u &&
			std::find(uids.begin(), uids.end(), plist.uidUtf8()) != uids.end() &&
			std::find(uids.begin(), uids.end(), addedUid) != uids.end();
	}, 5s));

	const auto& uids = plist.connectedPlayerUidsUtf8();
	EXPECT_GE(uids.size(), 2u);
	EXPECT_NE(std::find(uids.begin(), uids.end(), plist.uidUtf8()), uids.end());
	EXPECT_NE(std::find(uids.begin(), uids.end(), addedUid), uids.end());
}

TEST_F(PlayerListTest, ConnectedPlayerUidsUtf8IsEmptyAfterOnDisconnect)
{
	MessageBus::detail::RedisConnection conn{ { .ip = U"127.0.0.1", .port = 6379 } };
	MessageBus::detail::PlayerList plist{ {
		.sessionTtlMs = 5000,
		.playerListPollInterval = 100ms,
	} };

	ASSERT_TRUE(WaitForConnection(conn, 10s));
	plist.onConnect(conn);

	ASSERT_TRUE(WaitUntil(plist, conn, [&]() {
		return plist.connectedPlayerUidsUtf8().size() >= 1u;
	}, 5s));

	plist.onDisconnect();
	EXPECT_TRUE(plist.connectedPlayerUidsUtf8().empty());
}

// isReady

TEST_F(PlayerListTest, IsReadyIsFalseBeforeFirstKeysResponse)
{
	MessageBus::detail::PlayerList plist{ {} };
	EXPECT_FALSE(plist.isReady());
}

TEST_F(PlayerListTest, IsReadyIsTrueAfterFirstKeysResponse)
{
	MessageBus::detail::RedisConnection conn{ { .ip = U"127.0.0.1", .port = 6379 } };
	MessageBus::detail::PlayerList plist{ {
		.sessionTtlMs = 5000,
		.playerListPollInterval = 100ms,
	} };

	ASSERT_TRUE(WaitForConnection(conn, 10s));
	plist.onConnect(conn);

	ASSERT_TRUE(WaitUntil(plist, conn, [&]() { return plist.isReady(); }, 5s));
	EXPECT_TRUE(plist.isReady());
}

TEST_F(PlayerListTest, IsReadyIsFalseAfterOnDisconnect)
{
	MessageBus::detail::RedisConnection conn{ { .ip = U"127.0.0.1", .port = 6379 } };
	MessageBus::detail::PlayerList plist{ {
		.sessionTtlMs = 5000,
		.playerListPollInterval = 100ms,
	} };

	ASSERT_TRUE(WaitForConnection(conn, 10s));
	plist.onConnect(conn);
	ASSERT_TRUE(WaitUntil(plist, conn, [&]() { return plist.isReady(); }, 5s));

	plist.onDisconnect();
	EXPECT_FALSE(plist.isReady());
}

// afterTick

TEST_F(PlayerListTest, AfterTickReportsEmptyAddedAndRemovedWhenUnchanged)
{
	MessageBus::detail::RedisConnection conn{ { .ip = U"127.0.0.1", .port = 6379 } };
	MessageBus::detail::PlayerList plist{ {} };

	ASSERT_TRUE(WaitForConnection(conn, 10s));
	plist.onConnect(conn);
	ASSERT_TRUE(WaitUntil(plist, conn, [&]() {
		return plist.connectedPlayerUidsUtf8().size() >= 1u;
	}, 5s));

	plist.beforeTick(conn);
	conn.tick();
	plist.afterTick();

	EXPECT_EQ(plist.addedPlayerUidsUtf8(), std::vector<std::string>{});
	EXPECT_EQ(plist.removedPlayerUidsUtf8(), std::vector<std::string>{});
}

TEST_F(PlayerListTest, AfterTickReportsAddedUids)
{
	MessageBus::detail::RedisConnection conn{ { .ip = U"127.0.0.1", .port = 6379 } };
	MessageBus::detail::PlayerList plist{ {} };

	ASSERT_TRUE(WaitForConnection(conn, 10s));
	plist.onConnect(conn);
	ASSERT_TRUE(WaitUntil(plist, conn, [&]() {
		return plist.connectedPlayerUidsUtf8().size() >= 1u;
	}, 5s));

	plist.beforeTick(conn);
	conn.tick();
	plist.afterTick();

	plist.handlePubSubMessage("s3d-mbus:player-join", "new-player-uid");

	plist.beforeTick(conn);
	conn.tick();
	plist.afterTick();

	std::vector<std::string> expectedAdded = { "new-player-uid" };
	std::vector<std::string> actualAdded = plist.addedPlayerUidsUtf8();
	std::sort(expectedAdded.begin(), expectedAdded.end());
	std::sort(actualAdded.begin(), actualAdded.end());
	EXPECT_EQ(actualAdded, expectedAdded);
	EXPECT_EQ(plist.removedPlayerUidsUtf8(), std::vector<std::string>{});
}

TEST_F(PlayerListTest, AfterTickReportsRemovedUids)
{
	MessageBus::detail::RedisConnection conn{ { .ip = U"127.0.0.1", .port = 6379 } };
	MessageBus::detail::PlayerList plist{ {} };

	ASSERT_TRUE(WaitForConnection(conn, 10s));
	plist.onConnect(conn);
	ASSERT_TRUE(WaitUntil(plist, conn, [&]() {
		return plist.connectedPlayerUidsUtf8().size() >= 1u;
	}, 5s));

	plist.handlePubSubMessage("s3d-mbus:player-join", "to-remove-uid");

	plist.beforeTick(conn);
	conn.tick();
	plist.afterTick();

	ASSERT_NE(plist.connectedPlayerUidsUtf8().find("to-remove-uid"), plist.connectedPlayerUidsUtf8().end());

	plist.handlePubSubMessage("s3d-mbus:player-left", "to-remove-uid");

	plist.beforeTick(conn);
	conn.tick();
	plist.afterTick();

	EXPECT_EQ(plist.addedPlayerUidsUtf8(), std::vector<std::string>{});
	std::vector<std::string> expectedRemoved = { "to-remove-uid" };
	std::vector<std::string> actualRemoved = plist.removedPlayerUidsUtf8();
	std::sort(expectedRemoved.begin(), expectedRemoved.end());
	std::sort(actualRemoved.begin(), actualRemoved.end());
	EXPECT_EQ(actualRemoved, expectedRemoved);
	EXPECT_EQ(plist.connectedPlayerUidsUtf8().find("to-remove-uid"), plist.connectedPlayerUidsUtf8().end());
}

TEST_F(PlayerListTest, AfterTickClearsAddedAndRemovedOnDisconnect)
{
	MessageBus::detail::RedisConnection conn{ { .ip = U"127.0.0.1", .port = 6379 } };
	MessageBus::detail::PlayerList plist{ {} };

	ASSERT_TRUE(WaitForConnection(conn, 10s));
	plist.onConnect(conn);
	ASSERT_TRUE(WaitUntil(plist, conn, [&]() {
		return plist.connectedPlayerUidsUtf8().size() >= 1u;
	}, 5s));

	plist.handlePubSubMessage("s3d-mbus:player-join", "temp-uid");
	plist.afterTick();

	plist.onDisconnect();
	EXPECT_EQ(plist.addedPlayerUidsUtf8(), std::vector<std::string>{});
	EXPECT_EQ(plist.removedPlayerUidsUtf8(), std::vector<std::string>{});
}
