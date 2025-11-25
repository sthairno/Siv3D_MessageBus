#include "RedisDockerTestFixture.hpp"
#include "Utility.hpp"

extern "C" {
#include <hiredis/async.h>
}

namespace
{
	bool parseInvalidatePush(redisReply* reply)
	{
		if (!reply || reply->type != REDIS_REPLY_PUSH || reply->elements < 2)
		{
			return false;
		}

		auto* kind = reply->element[0];
		if (!kind || kind->type != REDIS_REPLY_STRING)
		{
			return false;
		}

		std::string_view kindView{ kind->str, static_cast<size_t>(kind->len) };
		if (kindView.compare("invalidate") != 0)
		{
			return false;
		}

		auto* arr = reply->element[1];
		if (!arr || arr->type != REDIS_REPLY_ARRAY || arr->elements == 0)
		{
			return false;
		}

		// 少なくとも 1 件の文字列キーが含まれていれば true
		for (size_t i = 0; i < arr->elements; ++i)
		{
			auto* k = arr->element[i];
			if (k && k->type == REDIS_REPLY_STRING)
			{
				return true;
			}
		}
		return false;
	}
}

// =========================================================================
// RESP3 PUSH: CLIENT TRACKING の invalidate を受信できること
// =========================================================================

class RedisConnectionPush : public RedisDocker
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

TEST_F(RedisConnectionPush, ReceiveInvalidatePush)
{
	int invalidateCount = 0;

	MessageBus::RedisConnection conn({
		.ip = U"127.0.0.1",
		.port = 6379,
		.password = s3d::none,
		.heartbeatInterval = 1s,
		.onConnect = nullptr,
		.onDisconnect = nullptr,
		.onPush = [&](redisAsyncContext*, redisReply* r) {
			if (parseInvalidatePush(r))
			{
				++invalidateCount;
			}
		}
	});

	ASSERT_TRUE(WaitForConnection(conn, 10s));

	// SET で invalidate を発生させる（docker exec 経由の redis-cli）
	auto [code, out] = ExecRedisCli({ "SET", "invalidate:key", "1" });
	ASSERT_EQ(code, 0);

	// invalidate を 1 件以上受け取るまで（またはタイムアウトまで）回す
	WaitUntil(conn, [&] { return invalidateCount >= 1; }, 5s);

	EXPECT_GE(invalidateCount, 1);
}

TEST_F(RedisConnectionPush, ReceiveInvalidatePushAfterReconnect)
{
	int invalidateCount = 0;

	MessageBus::RedisConnection conn({
		.ip = U"127.0.0.1",
		.port = 6379,
		.password = s3d::none,
		.heartbeatInterval = 1s,
		.onConnect = nullptr,
		.onDisconnect = nullptr,
		.onPush = [&](redisAsyncContext*, redisReply* r) {
			if (parseInvalidatePush(r))
			{
				++invalidateCount;
			}
		}
	});

	ASSERT_TRUE(WaitForConnection(conn, 10s));

	// いったんRedisサーバーを停止し、再接続させる
	StopContainer();
	EXPECT_TRUE(WaitForState(conn, MessageBus::RedisConnectionState::Failed, 30s));
	EXPECT_TRUE(conn.isReconnecting());

	StartContainer();
	EXPECT_TRUE(conn.isReconnecting());
	EXPECT_EQ(WaitForNextState(conn, 30s), MessageBus::RedisConnectionState::Connecting);
	EXPECT_EQ(WaitForNextState(conn, 30s), MessageBus::RedisConnectionState::HelloSent);
	EXPECT_EQ(WaitForNextState(conn, 30s), MessageBus::RedisConnectionState::ClientTrackingSent);
	EXPECT_EQ(WaitForNextState(conn, 30s), MessageBus::RedisConnectionState::Connected);
	EXPECT_FALSE(conn.isReconnecting());

	const int before = invalidateCount;

	auto [code, out] = ExecRedisCli({ "SET", "invalidate:key:reconnect", "1" });
	ASSERT_EQ(code, 0);

	EXPECT_TRUE(WaitUntil(conn, [&] { return invalidateCount >= before + 1; }, 5s));
	EXPECT_GE(invalidateCount, before + 1);
}


