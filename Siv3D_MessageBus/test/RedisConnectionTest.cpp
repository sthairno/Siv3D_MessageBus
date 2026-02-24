#include "RedisDockerTestFixture.hpp"
#include "Utility.hpp"

// ============================================================================
// パスワードなし環境でのテスト
// ============================================================================

// 基本接続テスト
class RedisConnectionBasic : public RedisDocker
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
};

TEST_F(RedisConnectionBasic, ConstructorAndGetters)
{
	MessageBus::detail::RedisConnection conn{ { U"127.0.0.1", 6379, none } };

	EXPECT_EQ(conn.ip(), U"127.0.0.1");
	EXPECT_EQ(conn.port(), 6379);
	EXPECT_FALSE(conn.password().has_value());
	EXPECT_EQ(conn.state(), MessageBus::detail::RedisConnectionState::Connecting);
}

TEST_F(RedisConnectionBasic, ConnectionSuccess)
{
	int connectedCount = 0;
	int readyCount = 0;

	MessageBus::detail::RedisConnection conn{ {
		.ip = U"127.0.0.1",
		.port = 6379,
		.password = none,
		.heartbeatInterval = 1s,
		.onConnect = [&](redisAsyncContext*) { ++connectedCount; },
		.onReady = [&](redisAsyncContext*) { ++readyCount; },
	} };

	EXPECT_EQ(conn.state(), MessageBus::detail::RedisConnectionState::Connecting);
	EXPECT_EQ(WaitForNextState(conn, 10s), MessageBus::detail::RedisConnectionState::HelloSent);
	EXPECT_EQ(WaitForNextState(conn, 10s), MessageBus::detail::RedisConnectionState::ClientTrackingSent);
	EXPECT_EQ(WaitForNextState(conn, 10s), MessageBus::detail::RedisConnectionState::Connected);
	EXPECT_GE(connectedCount, 1);
	EXPECT_GE(readyCount, 1);
}

TEST_F(RedisConnectionBasic, ReconnectOnDisconnect)
{
	int connectedCount = 0;
	int readyCount = 0;

	MessageBus::detail::RedisConnection conn{ {
		.ip = U"127.0.0.1",
		.port = 6379,
		.password = none,
		.heartbeatInterval = 1s,
		.onConnect = [&](redisAsyncContext*) { ++connectedCount; },
		.onReady = [&](redisAsyncContext*) { ++readyCount; },
	} };

	EXPECT_TRUE(WaitForConnection(conn, 10s));
	EXPECT_EQ(connectedCount, 1);
	EXPECT_EQ(readyCount, 1);

	StopContainer();

	EXPECT_EQ(WaitForNextState(conn, 10s), MessageBus::detail::RedisConnectionState::Disconnected);
	EXPECT_TRUE(conn.isFailed());
	EXPECT_TRUE(conn.isReconnecting());

	StartContainer();

	EXPECT_TRUE(conn.isReconnecting());
	EXPECT_EQ(WaitForNextState(conn, 10s), MessageBus::detail::RedisConnectionState::Connecting);
	EXPECT_EQ(WaitForNextState(conn, 10s), MessageBus::detail::RedisConnectionState::HelloSent);
	EXPECT_EQ(WaitForNextState(conn, 10s), MessageBus::detail::RedisConnectionState::ClientTrackingSent);
	EXPECT_EQ(WaitForNextState(conn, 10s), MessageBus::detail::RedisConnectionState::Connected);
	EXPECT_FALSE(conn.isReconnecting());
	EXPECT_EQ(connectedCount, 2);
	EXPECT_EQ(readyCount, 2);
}

TEST_F(RedisConnectionBasic, ManualDisconnect)
{
	int connectedCount = 0;
	int disconnectedCount = 0;

	MessageBus::detail::RedisConnection conn{ {
		.ip = U"127.0.0.1",
		.port = 6379,
		.password = none,
		.heartbeatInterval = 1s,
		.onConnect = [&](redisAsyncContext*) { ++connectedCount; },
		.onReady = [](redisAsyncContext*) {},
		.onDisconnect = [&]() { ++disconnectedCount; },
	} };

	EXPECT_TRUE(WaitForConnection(conn, 10s));
	EXPECT_EQ(conn.state(), MessageBus::detail::RedisConnectionState::Connected);
	EXPECT_GE(connectedCount, 1);

	conn.disconnect();

	EXPECT_EQ(WaitForNextState(conn, 10s), MessageBus::detail::RedisConnectionState::Disconnected);
	EXPECT_FALSE(conn.isReconnecting());
	EXPECT_GE(disconnectedCount, 1);
}

TEST_F(RedisConnectionBasic, ManualDisconnectWhileHandshaking)
{
	int connectedCount = 0;
	int disconnectedCount = 0;

	MessageBus::detail::RedisConnection conn{ {
		.ip = U"127.0.0.1",
		.port = 6379,
		.password = none,
		.heartbeatInterval = 1s,
		.onConnect = [&](redisAsyncContext*) { ++connectedCount; },
		.onReady = [](redisAsyncContext*) {},
		.onDisconnect = [&]() { ++disconnectedCount; },
	} };

	// ハンドシェイク到達(HELLO 送信済み)まで待機
	EXPECT_EQ(WaitForNextState(conn, 10s), MessageBus::detail::RedisConnectionState::HelloSent);
	EXPECT_GE(connectedCount, 1);

	// ハンドシェイク途中で手動切断
	conn.disconnect();

	// Disconnected へ遷移し、再接続は抑止、コールバックが呼ばれる
	EXPECT_EQ(WaitForNextState(conn, 10s), MessageBus::detail::RedisConnectionState::Disconnected);
	EXPECT_FALSE(conn.isReconnecting());
	EXPECT_GE(disconnectedCount, 1);
}

TEST_F(RedisConnectionBasic, DisconnectWhileHandshaking)
{
	MessageBus::detail::RedisConnection conn{ { U"127.0.0.1", 6379, none } };

	EXPECT_EQ(WaitForNextState(conn, 10s), MessageBus::detail::RedisConnectionState::HelloSent);

	StopContainer();

	EXPECT_TRUE(WaitForState(conn, MessageBus::detail::RedisConnectionState::Disconnected, 30s));
	EXPECT_TRUE(conn.isFailed());
	EXPECT_TRUE(conn.isReconnecting());
	EXPECT_EQ(WaitForNextState(conn, 30s), MessageBus::detail::RedisConnectionState::Connecting);

	StartContainer();
}

TEST_F(RedisConnectionBasic, InvalidHost)
{
	MessageBus::detail::RedisConnection conn{ { U"192.0.2.1", 6379, none } };

	EXPECT_EQ(conn.state(), MessageBus::detail::RedisConnectionState::Connecting);
	EXPECT_EQ(WaitForNextState(conn, 11s), MessageBus::detail::RedisConnectionState::Disconnected);
	EXPECT_TRUE(conn.isFailed());
	EXPECT_TRUE(conn.isReconnecting());
}

TEST_F(RedisConnectionBasic, InvalidPort)
{
	MessageBus::detail::RedisConnection conn{ { U"127.0.0.1", 6380, none } };

	EXPECT_EQ(conn.state(), MessageBus::detail::RedisConnectionState::Connecting);
	EXPECT_EQ(WaitForNextState(conn, 11s), MessageBus::detail::RedisConnectionState::Disconnected);
	EXPECT_TRUE(conn.isFailed());
	EXPECT_TRUE(conn.isReconnecting());
}

TEST_F(RedisConnectionBasic, NoPasswordNeeded)
{
	MessageBus::detail::RedisConnection conn{ { U"127.0.0.1", 6379, U"unnecessary_password" } };

	EXPECT_EQ(conn.state(), MessageBus::detail::RedisConnectionState::Connecting);
	EXPECT_EQ(WaitForNextState(conn, 10s), MessageBus::detail::RedisConnectionState::AuthSent);
	EXPECT_EQ(WaitForNextState(conn, 10s), MessageBus::detail::RedisConnectionState::Disconnected);
	EXPECT_TRUE(conn.isFailed());
	EXPECT_FALSE(conn.isReconnecting());
}

// ============================================================================
// パスワードあり環境でのテスト
// ============================================================================

class RedisConnectionAuth : public RedisDocker
{
protected:
	static void SetUpTestSuite()
	{
		RedisDocker::SetUpTestSuite();
		StartContainer(REDIS_IMAGE, "password");
	}

	static void TearDownTestSuite()
	{
		RedisDocker::TearDownTestSuite();
	}
};

TEST_F(RedisConnectionAuth, ConstructorAndGetters)
{
	MessageBus::detail::RedisConnection conn{ { U"127.0.0.1", 6379, U"password" } };

	EXPECT_EQ(conn.ip(), U"127.0.0.1");
	EXPECT_EQ(conn.port(), 6379);
	ASSERT_TRUE(conn.password().has_value());
	EXPECT_EQ(conn.password().value(), U"password");
}

TEST_F(RedisConnectionAuth, ConnectionWithPassword)
{
	int connectedCount = 0;
	int readyCount = 0;

	MessageBus::detail::RedisConnection conn{ {
		.ip = U"127.0.0.1",
		.port = 6379,
		.password = U"password",
		.heartbeatInterval = 1s,
		.onConnect = [&](redisAsyncContext*) { ++connectedCount; },
		.onReady = [&](redisAsyncContext*) { ++readyCount; },
	} };

	EXPECT_EQ(conn.state(), MessageBus::detail::RedisConnectionState::Connecting);
	EXPECT_EQ(WaitForNextState(conn, 10s), MessageBus::detail::RedisConnectionState::AuthSent);
	EXPECT_EQ(WaitForNextState(conn, 10s), MessageBus::detail::RedisConnectionState::HelloSent);
	EXPECT_EQ(WaitForNextState(conn, 10s), MessageBus::detail::RedisConnectionState::ClientTrackingSent);
	EXPECT_EQ(WaitForNextState(conn, 10s), MessageBus::detail::RedisConnectionState::Connected);
	EXPECT_GE(connectedCount, 1);
	EXPECT_GE(readyCount, 1);
}

TEST_F(RedisConnectionAuth, ConnectionWithWrongPassword)
{
	MessageBus::detail::RedisConnection conn{ { U"127.0.0.1", 6379, U"wrong_password" } };

	EXPECT_EQ(conn.state(), MessageBus::detail::RedisConnectionState::Connecting);
	EXPECT_EQ(WaitForNextState(conn, 10s), MessageBus::detail::RedisConnectionState::AuthSent);
	EXPECT_EQ(WaitForNextState(conn, 10s), MessageBus::detail::RedisConnectionState::Disconnected);
	EXPECT_TRUE(conn.isFailed());
	EXPECT_FALSE(conn.isReconnecting());
}

TEST_F(RedisConnectionAuth, ConnectionWithoutPassword)
{
	MessageBus::detail::RedisConnection conn{ { U"127.0.0.1", 6379, none } };

	EXPECT_EQ(conn.state(), MessageBus::detail::RedisConnectionState::Connecting);
	EXPECT_EQ(WaitForNextState(conn, 10s), MessageBus::detail::RedisConnectionState::HelloSent);
	EXPECT_EQ(WaitForNextState(conn, 10s), MessageBus::detail::RedisConnectionState::Disconnected);
	EXPECT_TRUE(conn.isFailed());
	EXPECT_FALSE(conn.isReconnecting());
}

// ============================================================================
// 古いRedisサーバー(RESP3非対応)でのテスト
// ============================================================================

class RedisConnectionOldServer : public RedisDocker
{
protected:
	static void SetUpTestSuite()
	{
		RedisDocker::SetUpTestSuite();
		StartContainer(REDIS_OLD_IMAGE);
	}

	static void TearDownTestSuite()
	{
		RedisDocker::TearDownTestSuite();
	}
};

TEST_F(RedisConnectionOldServer, ConnectionToOldServer)
{
	MessageBus::detail::RedisConnection conn{ { U"127.0.0.1", 6379, none } };

	EXPECT_EQ(conn.state(), MessageBus::detail::RedisConnectionState::Connecting);
	EXPECT_EQ(WaitForNextState(conn, 10s), MessageBus::detail::RedisConnectionState::HelloSent);
	EXPECT_EQ(WaitForNextState(conn, 10s), MessageBus::detail::RedisConnectionState::Disconnected);
	EXPECT_TRUE(conn.isFailed());
	EXPECT_FALSE(conn.isReconnecting());
}
