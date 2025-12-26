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

TEST_F(MessageBusBasic, ConnectFromDefaultConstructorThenConnectNotAllowed)
{
	MessageBus::MessageBus bus{};

	EXPECT_FALSE(bus.isConnected());
	bus.connect(U"127.0.0.1", 6379, none);
	WaitForConnection(bus, 10s);

	EXPECT_THROW(
		bus.connect(U"127.0.0.1", 6379, none),
		MessageBus::ConnectNotAllowedError
	);
}

TEST_F(MessageBusBasic, ConnectNotAllowedIfAddressConstructorUsed)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };

	EXPECT_THROW(
		bus.connect(U"127.0.0.1", 6379, none),
		MessageBus::ConnectNotAllowedError
	);
}

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

// ============================================================================
// MessageBus shutdown テスト
// ============================================================================

TEST_F(MessageBusEvents, ShutdownWhenConnected)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus, 10s);
	ASSERT_TRUE(bus.isConnected());

	bus.shutdown();
	EXPECT_FALSE(bus.isConnected());
}

TEST_F(MessageBusEvents, ShutdownWhenDisconnected)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	EXPECT_FALSE(bus.isConnected());

	// 既に切断されている状態でshutdown()を呼び出しても問題ないことを確認
	bus.shutdown();
	EXPECT_FALSE(bus.isConnected());
}

// ============================================================================
// MessageBus 空コンストラクタテスト
// ============================================================================

class MessageBusEmptyConstructor : public ::testing::Test
{
};

TEST_F(MessageBusEmptyConstructor, DefaultConstructorCreatesEmptyBus)
{
	MessageBus::MessageBus bus;
	EXPECT_FALSE(bus.isConnected());
	EXPECT_TRUE(bus.error().isEmpty());
}

TEST_F(MessageBusEmptyConstructor, IsConnectedReturnsFalse)
{
	MessageBus::MessageBus bus;
	EXPECT_FALSE(bus.isConnected());
	EXPECT_FALSE(static_cast<bool>(bus));
}

TEST_F(MessageBusEmptyConstructor, ErrorReturnsEmptyString)
{
	MessageBus::MessageBus bus;
	const auto& error = bus.error();
	EXPECT_TRUE(error.isEmpty());
}

TEST_F(MessageBusEmptyConstructor, DisconnectIsSafe)
{
	MessageBus::MessageBus bus;
	// 接続していない状態でdisconnect()を呼び出しても問題ないことを確認
	bus.disconnect();
	EXPECT_FALSE(bus.isConnected());
}

TEST_F(MessageBusEmptyConstructor, ShutdownIsSafe)
{
	MessageBus::MessageBus bus;
	// 接続していない状態でshutdown()を呼び出しても問題ないことを確認
	bus.shutdown();
	EXPECT_FALSE(bus.isConnected());
}

TEST_F(MessageBusEmptyConstructor, UpdateIsSafe)
{
	MessageBus::MessageBus bus;
	// 接続していない状態でupdate()を呼び出しても問題ないことを確認
	bus.update();
	EXPECT_FALSE(bus.isConnected());
	const auto& events = bus.events();
	EXPECT_TRUE(events.isEmpty());
}

TEST_F(MessageBusEmptyConstructor, SubscribeWorksWithoutConnection)
{
	MessageBus::MessageBus bus;
	// connが空でもsubscribe()は動作する（チャンネル状態は保持される）
	EXPECT_TRUE(bus.subscribe(U"test_channel"));
	EXPECT_TRUE(bus.subscribe(U"another_channel"));
}

TEST_F(MessageBusEmptyConstructor, UnsubscribeWorksWithoutConnection)
{
	MessageBus::MessageBus bus;
	// 購読していないチャンネルのunsubscribe()はfalseを返す
	EXPECT_FALSE(bus.unsubscribe(U"not_subscribed"));

	// 購読してからunsubscribe()は成功する
	EXPECT_TRUE(bus.subscribe(U"test_channel"));
	EXPECT_TRUE(bus.unsubscribe(U"test_channel"));
}

TEST_F(MessageBusEmptyConstructor, EmitReturnsFalseWithoutConnection)
{
	MessageBus::MessageBus bus;
	// connが空の場合、emit()はfalseを返す
	EXPECT_FALSE(bus.emit(U"test_channel"));
	EXPECT_FALSE(bus.emit(U"test_channel", UR"({ "k": 1 })"_json));
}

TEST_F(MessageBusEmptyConstructor, VariableWorksWithoutConnection)
{
	MessageBus::MessageBus bus;
	// connが空でもvariable()は動作する（変数は作成可能）
	auto var1 = bus.variable<int32>(U"test_var", 42);
	EXPECT_EQ(var1.get(), 42);

	auto var2 = bus.variable<String>(U"test_string", U"hello");
	EXPECT_EQ(var2.get(), U"hello");
}

TEST_F(MessageBusEmptyConstructor, EventsBufferIsEmpty)
{
	MessageBus::MessageBus bus;
	const auto& events = bus.events();
	EXPECT_TRUE(events.isEmpty());

	// update()を呼び出してもイベントは空のまま
	bus.update();
	const auto& eventsAfterUpdate = bus.events();
	EXPECT_TRUE(eventsAfterUpdate.isEmpty());
}
