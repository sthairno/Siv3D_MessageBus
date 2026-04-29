#include "RedisDockerTestFixture.hpp"
#include "Utility.hpp"

#include <MessageBus/detail/RedisConnection.hpp>
#include <MessageBus/detail/SubscriptionWorker.hpp>

// ============================================================================
// SubscriptionWorker 単体テスト
// ============================================================================

TEST(SubscriptionWorkerUnit, SubscribeWorksWithoutConnection)
{
	MessageBus::detail::SubscriptionWorker worker;

	EXPECT_TRUE(worker.subscribe(U"test_channel"));
	EXPECT_TRUE(worker.subscribe(U"another_channel"));
}

TEST(SubscriptionWorkerUnit, UnsubscribeWorksWithoutConnection)
{
	MessageBus::detail::SubscriptionWorker worker;

	EXPECT_FALSE(worker.unsubscribe(U"not_subscribed"));
	EXPECT_TRUE(worker.subscribe(U"test_channel"));
	EXPECT_TRUE(worker.unsubscribe(U"test_channel"));
	EXPECT_FALSE(worker.unsubscribe(U"test_channel"));
}

TEST(SubscriptionWorkerUnit, HandlePubSubMessageAddsEventForSubscribedChannel)
{
	MessageBus::detail::SubscriptionWorker worker;

	ASSERT_TRUE(worker.subscribe(U"t1"));
	EXPECT_TRUE(worker.handlePubSubMessage("t1", R"({"k":1})"));

	const auto& events = worker.events();
	ASSERT_EQ(events.size(), 1);
	EXPECT_EQ(events[0].channel, U"t1");
	EXPECT_EQ(events[0].value[U"k"].get<int32>(), 1);
}

TEST(SubscriptionWorkerUnit, HandlePubSubMessageIgnoresUnsubscribedChannel)
{
	MessageBus::detail::SubscriptionWorker worker;

	EXPECT_FALSE(worker.handlePubSubMessage("t1", R"({"k":1})"));
	EXPECT_TRUE(worker.events().isEmpty());
}

TEST(SubscriptionWorkerUnit, HandlePubSubMessageEmptyPayloadAsInvalid)
{
	MessageBus::detail::SubscriptionWorker worker;

	ASSERT_TRUE(worker.subscribe(U"t1"));
	EXPECT_TRUE(worker.handlePubSubMessage("t1", ""));

	const auto& events = worker.events();
	ASSERT_EQ(events.size(), 1);
	EXPECT_EQ(events[0].value, JSON::Invalid());
}

// ============================================================================
// SubscriptionWorker Redis購読/受信テスト
// ============================================================================

class SubscriptionWorkerEvents : public RedisDocker
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

TEST_F(SubscriptionWorkerEvents, SubscribeBeforeConnection)
{
	MessageBus::detail::SubscriptionWorker worker;
	ASSERT_TRUE(worker.subscribe(U"t1"));

	MessageBus::detail::RedisConnection conn{ {
		.ip = U"127.0.0.1",
		.port = 6379,
		.password = none,
		.heartbeatInterval = 1s,
		.onReady = [&](redisAsyncContext*) {
			worker.onConnect(conn);
		},
		.onDisconnect = [&]() {
			worker.onDisconnect();
		},
	} };
	ASSERT_TRUE(WaitForConnection(conn, 10s));
	Sleep(worker, conn, 0.5s);

	Publish("t1", R"({"k":1})");
	ASSERT_TRUE(WaitForEvent(worker, conn, 5s));

	const auto& events = worker.events();
	ASSERT_EQ(events.size(), 1);
	EXPECT_EQ(events[0].channel, U"t1");
	EXPECT_EQ(events[0].value[U"k"].get<int32>(), 1);
}

TEST_F(SubscriptionWorkerEvents, SubscribeAfterConnection)
{
	MessageBus::detail::SubscriptionWorker worker;
	MessageBus::detail::RedisConnection conn{ {
		.ip = U"127.0.0.1",
		.port = 6379,
		.password = none,
		.heartbeatInterval = 1s,
		.onReady = [&](redisAsyncContext*) {
			worker.onConnect(conn);
		},
		.onDisconnect = [&]() {
			worker.onDisconnect();
		},
	} };
	ASSERT_TRUE(WaitForConnection(conn, 10s));

	EXPECT_TRUE(worker.subscribe(U"t1"));
	Sleep(worker, conn, 1s);

	Publish("t1", R"({"k":1})");
	ASSERT_TRUE(WaitForEvent(worker, conn, 5s));

	const auto& events = worker.events();
	ASSERT_EQ(events.size(), 1);
	EXPECT_EQ(events[0].channel, U"t1");
	EXPECT_EQ(events[0].value[U"k"].get<int32>(), 1);
}

TEST_F(SubscriptionWorkerEvents, ReceiveMultipleEvents)
{
	MessageBus::detail::SubscriptionWorker worker;
	ASSERT_TRUE(worker.subscribe(U"t1"));

	MessageBus::detail::RedisConnection conn{ {
		.ip = U"127.0.0.1",
		.port = 6379,
		.password = none,
		.heartbeatInterval = 1s,
		.onReady = [&](redisAsyncContext*) {
			worker.onConnect(conn);
		},
		.onDisconnect = [&]() {
			worker.onDisconnect();
		},
	} };
	ASSERT_TRUE(WaitForConnection(conn, 10s));
	Sleep(worker, conn, 0.5s);

	Publish("t1", R"({"k":1})");
	Publish("t1", R"({"k":2})");
	System::Sleep(1s);
	ASSERT_TRUE(WaitForEvent(worker, conn, 5s));

	const auto& events = worker.events();
	ASSERT_EQ(events.size(), 2);
	EXPECT_EQ(events[0].channel, U"t1");
	EXPECT_EQ(events[0].value[U"k"].get<int32>(), 1);
	EXPECT_EQ(events[1].channel, U"t1");
	EXPECT_EQ(events[1].value[U"k"].get<int32>(), 2);
}

TEST_F(SubscriptionWorkerEvents, DoesNotReceiveUnsubscribedEvents)
{
	MessageBus::detail::SubscriptionWorker worker;
	EXPECT_TRUE(worker.subscribe(U"t1"));

	MessageBus::detail::RedisConnection conn{ {
		.ip = U"127.0.0.1",
		.port = 6379,
		.password = none,
		.heartbeatInterval = 1s,
		.onReady = [&](redisAsyncContext*) {
			worker.onConnect(conn);
		},
		.onDisconnect = [&]() {
			worker.onDisconnect();
		},
	} };
	ASSERT_TRUE(WaitForConnection(conn, 10s));

	Publish("t2", R"({"k":1})");
	EXPECT_FALSE(WaitForEvent(worker, conn, 1s));
	EXPECT_TRUE(worker.events().isEmpty());
}

TEST_F(SubscriptionWorkerEvents, UnsubscribeThenNoLongerReceive)
{
	MessageBus::detail::SubscriptionWorker worker;
	ASSERT_TRUE(worker.subscribe(U"u"));

	MessageBus::detail::RedisConnection conn{ {
		.ip = U"127.0.0.1",
		.port = 6379,
		.password = none,
		.heartbeatInterval = 1s,
		.onReady = [&](redisAsyncContext*) {
			worker.onConnect(conn);
		},
		.onDisconnect = [&]() {
			worker.onDisconnect();
		},
	} };
	ASSERT_TRUE(WaitForConnection(conn, 10s));
	Sleep(worker, conn, 0.5s);

	Publish("u", "1");
	ASSERT_TRUE(WaitForEvent(worker, conn, 5s));
	worker.clearEventsBuffer();

	ASSERT_TRUE(worker.unsubscribe(U"u"));
	Sleep(worker, conn, 1s);
	Publish("u", "2");

	ASSERT_FALSE(WaitForEvent(worker, conn, 5s));
}

TEST_F(SubscriptionWorkerEvents, AutoResubscribeAfterReconnect)
{
	MessageBus::detail::SubscriptionWorker worker;
	ASSERT_TRUE(worker.subscribe(U"r1"));

	MessageBus::detail::RedisConnection conn{ {
		.ip = U"127.0.0.1",
		.port = 6379,
		.password = none,
		.heartbeatInterval = 1s,
		.onReady = [&](redisAsyncContext*) {
			worker.onConnect(conn);
		},
		.onDisconnect = [&]() {
			worker.onDisconnect();
		},
	} };
	ASSERT_TRUE(WaitForConnection(conn, 10s));

	StopContainer();
	EXPECT_TRUE(WaitForState(conn, MessageBus::detail::RedisConnectionState::Disconnected, 30s));
	StartContainer();
	EXPECT_EQ(WaitForNextState(conn, 30s), MessageBus::detail::RedisConnectionState::Connecting);
	EXPECT_EQ(WaitForNextState(conn, 30s), MessageBus::detail::RedisConnectionState::HelloSent);
	EXPECT_EQ(WaitForNextState(conn, 30s), MessageBus::detail::RedisConnectionState::ClientTrackingSent);
	EXPECT_EQ(WaitForNextState(conn, 30s), MessageBus::detail::RedisConnectionState::Connected);
	Sleep(worker, conn, 0.5s);

	Publish("r1", "");
	EXPECT_TRUE(WaitForEvent(worker, conn, 5s));
	const auto& events = worker.events();
	ASSERT_EQ(events.size(), 1);
	EXPECT_EQ(events[0].channel, U"r1");
	EXPECT_EQ(events[0].value, JSON::Invalid());
}
