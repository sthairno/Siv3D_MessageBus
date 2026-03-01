#include <gtest/gtest.h>

#include <MessageBus/detail/PlayerList.hpp>

#include "RedisDockerTestFixture.hpp"
#include "Utility.hpp"

#include <Siv3D.hpp>

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
	// PlayerList の beforeTick と RedisConnection の tick を両方回しながら条件待ち
	template <class Pred>
	static bool WaitUntil(MessageBus::detail::PlayerList& plist, MessageBus::detail::RedisConnection& conn, Pred&& predicate, Duration timeout = 5s)
	{
		Stopwatch sw{ StartImmediately::Yes };
		while (sw < timeout && !predicate())
		{
			plist.beforeTick(conn);
			conn.tick();
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
