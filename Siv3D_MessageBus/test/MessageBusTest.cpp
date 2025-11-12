#include "RedisDockerTestFixture.hpp"
#include <MessageBus/MessageBus.hpp>
#include "Utility.hpp"

// ============================================================================
// MessageBus基本接続テスト
// ============================================================================

class MessageBusBasic : public RedisDocker
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

TEST_F(MessageBusBasic, ConnectionSuccess)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };

	EXPECT_FALSE(bus.isConnected());
	WaitForConnection(bus, 10s);
};

// ============================================================================
// MessageBus認証テスト
// ============================================================================

class MessageBusAuth : public RedisDocker
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

TEST_F(MessageBusAuth, ConnectionWithPassword)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, U"password" };

	EXPECT_FALSE(bus.isConnected());
	WaitForConnection(bus, 10s);
}

// ============================================================================
// MessageBus イベント購読/受信テスト
// ============================================================================

class MessageBusEvents : public RedisDocker
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

TEST_F(MessageBusEvents, SubscribeBeforeConnection)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	ASSERT_TRUE(bus.subscribe(U"t1"));
	WaitForConnection(bus, 10s);
	Sleep(bus, 0.5s); // Wait for subscribe to be processed

	Publish("t1", R"({"k":1})");
	ASSERT_TRUE(WaitForEvent(bus, 5s));

	const auto& events = bus.events();
	EXPECT_EQ(events.size(), 1);
	EXPECT_EQ(events[0].channel, U"t1");
	EXPECT_EQ(events[0].value[U"k"].get<int32>(), 1);
}

TEST_F(MessageBusEvents, SubscribeAfterConnection)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus, 10s);

	EXPECT_TRUE(bus.subscribe(U"t1"));
	Sleep(bus, 1s);

	Publish("t1", R"({"k":1})");
	ASSERT_TRUE(WaitForEvent(bus, 5s));

	const auto& events = bus.events();
	EXPECT_EQ(events.size(), 1);
	EXPECT_EQ(events[0].channel, U"t1");
	EXPECT_EQ(events[0].value[U"k"].get<int32>(), 1);
}

TEST_F(MessageBusEvents, ReceiveMultipleEvents)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	ASSERT_TRUE(bus.subscribe(U"t1"));
	WaitForConnection(bus, 10s);
	Sleep(bus, 0.5s);

	Publish("t1", R"({"k":1})");
	Publish("t1", R"({"k":2})");
	System::Sleep(1s);
	ASSERT_TRUE(WaitForEvent(bus, 5s));

	const auto& events = bus.events();
	ASSERT_EQ(events.size(), 2);
	EXPECT_EQ(events[0].channel, U"t1");
	EXPECT_EQ(events[0].value[U"k"].get<int32>(), 1);
	EXPECT_EQ(events[1].channel, U"t1");
	EXPECT_EQ(events[1].value[U"k"].get<int32>(), 2);
}

TEST_F(MessageBusEvents, DoesNotReceiveUnsubscribedEvents)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	EXPECT_TRUE(bus.subscribe(U"t1"));
	WaitForConnection(bus, 10s);

	Publish("t2", R"({"k":1})");
	EXPECT_FALSE(WaitForEvent(bus, 1s));

	const auto& events = bus.events();
	EXPECT_EQ(events.size(), 0);
}

TEST_F(MessageBusEvents, UnsubscribeThenNoLongerReceive)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	ASSERT_TRUE(bus.subscribe(U"u"));
	WaitForConnection(bus, 10s);
	Sleep(bus, 0.5s);

	Publish("u", "1");
	ASSERT_TRUE(WaitForEvent(bus, 5s));

	bus.unsubscribe(U"u");
	System::Sleep(1s);
	Publish("u", "2");

	ASSERT_FALSE(WaitForEvent(bus, 5s));
}

TEST_F(MessageBusEvents, AutoResubscribeAfterReconnect)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	ASSERT_TRUE(bus.subscribe(U"r1"));
	WaitForConnection(bus, 10s);

	StopContainer();
	WaitForDisconnect(bus, 10s);
	StartContainer();
	WaitForConnection(bus, 15s);
	Sleep(bus, 0.5s); // Wait for reconnect to be processed

	Publish("r1", "");
	EXPECT_TRUE(WaitForEvent(bus, 5s));
	const auto& events = bus.events();
	ASSERT_EQ(events.size(), 1);
	EXPECT_EQ(events[0].channel, U"r1");
	EXPECT_EQ(events[0].value, JSON::Invalid());
}

// ============================================================================
// MessageBus emit 送信テスト
// ============================================================================

TEST_F(MessageBusEvents, EmitSendsJSONPayload)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	ASSERT_TRUE(bus.subscribe(U"p1"));
	WaitForConnection(bus, 10s);
	Sleep(bus, 0.5s); // Wait for subscribe to be processed

	auto payload = UR"({ "k": 123 })"_json;
	ASSERT_TRUE(bus.emit(U"p1", payload));

	ASSERT_TRUE(WaitForEvent(bus, 5s));
	const auto& events = bus.events();
	ASSERT_EQ(events.size(), 1);
	EXPECT_EQ(events[0].channel, U"p1");
	EXPECT_EQ(events[0].value[U"k"].get<int32>(), 123);
}

TEST_F(MessageBusEvents, EmitSendsEmptyAsInvalid)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	ASSERT_TRUE(bus.subscribe(U"p2"));
	WaitForConnection(bus, 10s);
	Sleep(bus, 0.5s);

	ASSERT_TRUE(bus.emit(U"p2"));
	ASSERT_TRUE(WaitForEvent(bus, 5s));

	const auto& events = bus.events();
	ASSERT_EQ(events.size(), 1);
	EXPECT_EQ(events[0].channel, U"p2");
	EXPECT_EQ(events[0].value, JSON::Invalid());
}

TEST_F(MessageBusEvents, EmitInvalidChannelReturnsFalse)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	EXPECT_FALSE(bus.emit(U""));
}

TEST_F(MessageBusEvents, EmitBeforeConnectionReturnsFalse)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	EXPECT_FALSE(bus.emit(U"early", UR"({ "a": 1 })"_json));
}
