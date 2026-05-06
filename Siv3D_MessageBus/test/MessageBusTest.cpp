#include "RedisDockerTestFixture.hpp"
#include <MessageBus/MessageBus.hpp>
#include "Utility.hpp"

#include <algorithm>
#include <gtest/gtest.h>

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

TEST_F(MessageBusBasic, ConnectNotAllowedIfAlreadyConnectingByConnectMethod)
{
	MessageBus::MessageBus bus{};

	EXPECT_FALSE(bus.isConnected());
	bus.connect(U"127.0.0.1", 6379, none);
	EXPECT_THROW(
		bus.connect(U"127.0.0.1", 6379, none),
		MessageBus::ConnectNotAllowedError
	);
}

TEST_F(MessageBusBasic, ConnectNotAllowedIfAlreadyConnectingByAddressConstructor)
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
// MessageBus イベント送信/ライフサイクルテスト
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

TEST_F(MessageBusEvents, EventsDefaultEmpty)
{
	MessageBus::MessageBus bus;

	const auto& events = bus.events();
	EXPECT_TRUE(events.isEmpty());
}

TEST_F(MessageBusEvents, SubscribeReceivesPublishedEvent)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus, 10s);
	bus.update();

	ASSERT_TRUE(bus.subscribe(U"user-event"));
	Sleep(bus, 0.5s);

	Publish("user-event", R"({"k":1})");
	ASSERT_TRUE(WaitForEventMatching(bus, [](const auto& event)
	{
		return event.channel == U"user-event" && event.value[U"k"].template get<int32>() == 1;
	}, 5s));
}

TEST_F(MessageBusEvents, UnsubscribeStopsPublishedEvent)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus, 10s);
	bus.update();

	ASSERT_TRUE(bus.subscribe(U"user-event-unsubscribe"));
	Sleep(bus, 0.5s);

	Publish("user-event-unsubscribe", R"({"k":1})");
	ASSERT_TRUE(WaitForEventMatching(bus, [](const auto& event)
	{
		return event.channel == U"user-event-unsubscribe" && event.value[U"k"].template get<int32>() == 1;
	}, 5s));

	ASSERT_TRUE(bus.unsubscribe(U"user-event-unsubscribe"));
	Sleep(bus, 0.5s);

	Publish("user-event-unsubscribe", R"({"k":2})");
	EXPECT_FALSE(WaitForEventMatching(bus, [](const auto& event)
	{
		return event.channel == U"user-event-unsubscribe";
	}, 1s));
}

TEST_F(MessageBusEvents, EmitSendsJSONPayload)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus, 10s);

	auto payload = UR"({ "k": 123 })"_json;
	const int received = WithSubscription("p1", [&]()
	{
		ASSERT_TRUE(bus.emit(U"p1", payload));
		Sleep(bus, 0.5s);
	});

	EXPECT_GE(received, 1);
}

TEST_F(MessageBusEvents, EmitSendsEmptyAsInvalid)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus, 10s);

	const int received = WithSubscription("p2", [&]()
	{
		ASSERT_TRUE(bus.emit(U"p2"));
		Sleep(bus, 0.5s);
	});

	EXPECT_GE(received, 1);
}

TEST_F(MessageBusEvents, EmitEmptyChannelNameThrows)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	EXPECT_THROW(
		bus.emit(U""),
		MessageBus::InvalidNameError
	);
}

TEST_F(MessageBusEvents, EmitSystemReservedChannelNameThrows)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	EXPECT_THROW(
		bus.emit(U"s3d-mbus:test"),
		MessageBus::InvalidNameError
	);
}

TEST_F(MessageBusEvents, EmitBeforeConnectionReturnsFalse)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	EXPECT_FALSE(bus.emit(U"early", UR"({ "a": 1 })"_json));
}

TEST_F(MessageBusEvents, SubscribeEmptyChannelNameThrows)
{
	MessageBus::MessageBus bus;
	EXPECT_THROW(bus.subscribe(U""), MessageBus::InvalidNameError);
}

TEST_F(MessageBusEvents, SubscribeSystemReservedChannelNameThrows)
{
	MessageBus::MessageBus bus;
	EXPECT_THROW(bus.subscribe(U"s3d-mbus:test"), MessageBus::InvalidNameError);
}

TEST_F(MessageBusEvents, UnsubscribeEmptyChannelNameThrows)
{
	MessageBus::MessageBus bus;
	EXPECT_THROW(bus.unsubscribe(U""), MessageBus::InvalidNameError);
}

TEST_F(MessageBusEvents, UnsubscribeSystemReservedChannelNameThrows)
{
	MessageBus::MessageBus bus;
	EXPECT_THROW(bus.unsubscribe(U"s3d-mbus:test"), MessageBus::InvalidNameError);
}

// ============================================================================
// MessageBus プレイヤー一覧 テスト
// ============================================================================

class MessageBusPlayerList : public RedisDocker
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

TEST_F(MessageBusPlayerList, SelfIdIsAvailableBeforeConnection)
{
	MessageBus::MessageBus bus;
	const auto& id = bus.id();
	EXPECT_FALSE(id.isEmpty());
}

TEST_F(MessageBusPlayerList, OnlineIdListDefaultEmpty)
{
	MessageBus::MessageBus bus;

	const auto& ids = bus.onlineIdList();
	EXPECT_TRUE(ids.isEmpty());
}

TEST_F(MessageBusPlayerList, OnlineIdListIsUpdatedWhenConnected)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus, 10s);

	const auto& ids = bus.onlineIdList();
	ASSERT_EQ(ids.size(), 1);
	EXPECT_EQ(ids[0], bus.id());
}

TEST_F(MessageBusPlayerList, OnlineIdListIsUpdatedWhenAnotherClientJoins)
{
	MessageBus::MessageBus bus1{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus1, 10s);
	ASSERT_EQ(bus1.onlineIdList().size(), 1);
	bus1.update();
	
	MessageBus::MessageBus bus2{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus2, 10s);
	
	bool joined = WaitUntil(bus1, bus2, [&]() {
		return bus1.onlineIdList().size() == 2;
	}, 10s);
	EXPECT_TRUE(joined);
	ASSERT_EQ(bus1.onlineIdList().size(), 2);
	EXPECT_TRUE(std::find(bus1.onlineIdList().begin(), bus1.onlineIdList().end(), bus1.id()) != bus1.onlineIdList().end());
	EXPECT_TRUE(std::find(bus1.onlineIdList().begin(), bus1.onlineIdList().end(), bus2.id()) != bus1.onlineIdList().end());
}

TEST_F(MessageBusPlayerList, OnlineIdListIsUpdatedWhenAnotherClientLeaves)
{
	MessageBus::MessageBus bus1{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus1, 10s);
	ASSERT_EQ(bus1.onlineIdList().size(), 1);
	bus1.update();
	
	MessageBus::MessageBus bus2{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus2, 10s);
	
	bool joined = WaitUntil(bus1, bus2, [&]() {
		return bus1.onlineIdList().size() == 2;
	}, 10s);
	ASSERT_TRUE(joined);

	bus2.shutdown();

	bool left = WaitUntil(bus1, [&]() {
		return bus1.onlineIdList().size() == 1;
	}, 10s);
	ASSERT_TRUE(left);
	ASSERT_EQ(bus1.onlineIdList().size(), 1);
	EXPECT_TRUE(std::find(bus1.onlineIdList().begin(), bus1.onlineIdList().end(), bus1.id()) != bus1.onlineIdList().end());
	EXPECT_FALSE(std::find(bus1.onlineIdList().begin(), bus1.onlineIdList().end(), bus2.id()) != bus1.onlineIdList().end());
}

TEST_F(MessageBusPlayerList, OnlineIdListIsUpdatedWhenThereIsExistingPlayerSession)
{
	MessageBus::MessageBus bus1{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus1, 10s);
	
	MessageBus::MessageBus bus2{ U"127.0.0.1", 6379, none };
	ASSERT_EQ(bus2.onlineIdList().size(), 0);
	WaitForConnection(bus2, 10s);
	
	ASSERT_EQ(bus2.onlineIdList().size(), 2);
	EXPECT_TRUE(std::find(bus2.onlineIdList().begin(), bus2.onlineIdList().end(), bus1.id()) != bus2.onlineIdList().end());
	EXPECT_TRUE(std::find(bus2.onlineIdList().begin(), bus2.onlineIdList().end(), bus2.id()) != bus2.onlineIdList().end());
}

TEST_F(MessageBusPlayerList, JoinEventIsAddedWhenConnected)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus, 10s);

	const auto& events = bus.events();
	ASSERT_EQ(events.size(), 1);
	EXPECT_EQ(events[0].channel, U"s3d-mbus:join");
	EXPECT_EQ(events[0].value, JSON(bus.id()));
}

TEST_F(MessageBusPlayerList, JoinEventIsAddedWhenAnotherClientJoins)
{
	MessageBus::MessageBus bus1{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus1, 10s);
	ASSERT_EQ(bus1.events().size(), 1);
	bus1.update();

	MessageBus::MessageBus bus2{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus2, 10s);
	
	bool joined = WaitUntil(bus1, bus2, [&]() {
		if (bus1.events().size() == 0) return false;
		const auto& event = bus1.events()[0];
		return event.channel == U"s3d-mbus:join" && event.value == JSON(bus2.id());
	}, 10s);

	EXPECT_TRUE(joined);
}

TEST_F(MessageBusPlayerList, JoinEventIsAddedWhenThereIsExistingPlayerSession)
{
	MessageBus::MessageBus bus1{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus1, 10s);
	
	MessageBus::MessageBus bus2{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus2, 10s);
	
	ASSERT_EQ(bus2.events().size(), 2);
	bool joinEventForBus1 = true;
	bool joinEventForBus2 = true;
	for (const auto& event : bus2.events())
	{
		joinEventForBus1 |= event.channel == U"s3d-mbus:join" && event.value == JSON(bus1.id());
		joinEventForBus2 |= event.channel == U"s3d-mbus:join" && event.value == JSON(bus2.id());
	}
	EXPECT_TRUE(joinEventForBus1);
	EXPECT_TRUE(joinEventForBus2);
}

TEST_F(MessageBusPlayerList, LeftEventIsAddedWhenAnotherClientLeaves)
{
	MessageBus::MessageBus bus1{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus1, 10s);
	ASSERT_EQ(bus1.events().size(), 1);
	bus1.update();
	
	MessageBus::MessageBus bus2{ U"127.0.0.1", 6379, none };

	WaitForConnection(bus2, 10s);
	bool joined = WaitUntil(bus1, bus2, [&]() {
		if (bus1.events().size() == 0) return false;
		const auto& event = bus1.events()[0];
		return event.channel == U"s3d-mbus:join" && event.value == JSON(bus2.id());
	}, 10s);
	ASSERT_TRUE(joined);

	bus2.shutdown();

	bool left = WaitUntil(bus1, [&]() {
		if (bus1.events().size() == 0) return false;
		const auto& event = bus1.events()[0];
		return event.channel == U"s3d-mbus:left" && event.value == JSON(bus2.id());
	}, 10s);
	EXPECT_TRUE(left);
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

TEST_F(MessageBusEvents, ShutdownWhenDisconnecting)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus, 10s);
	ASSERT_TRUE(bus.isConnected());

	// Disconnecting状態を作るため送信中の処理を追加しておく
	bus.variable<int32>(U"dummy", 0);
	
	bus.update();

	bus.disconnect();
	ASSERT_TRUE(bus.isDisconnecting());
	bus.shutdown();

	EXPECT_FALSE(bus.isConnected());
}

TEST_F(MessageBusEvents, ShutdownWhenConnecting)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };

	bus.shutdown();

	EXPECT_FALSE(bus.isConnected());
	EXPECT_FALSE(bus.isDisconnecting());
}

TEST(MessageBusShutdownRegression, ShutdownWhenErrorOccurs)
{
	// NOTE:
	// RedisConnection::tryConnect() が初期化エラー（m_context->err）で return する経路で
	// state が Disconnected に戻らない場合、MessageBus::shutdown() 相当の待機ループが永久に抜けられない。

	// 無効なホストを指定して、redisAsyncConnectWithOptions() が直ちに m_context->err を返す状況を作る
	MessageBus::detail::RedisConnection conn{
		MessageBus::detail::RedisConnectionOptions{
			.ip = U"256.256.256.256",
			.port = 6379,
			.password = none,
			.onConnect = nullptr,
			.onReady = nullptr,
			.onDisconnect = nullptr,
			.onInvalidate = nullptr,
		}
	};

	ASSERT_TRUE(conn.isFailed()) << "Expected initialization failure to reproduce the freeze condition";

	// shutdown() は Disconnected になるまで待機するため、ここで Disconnected に遷移できないと永久ループになり得る
	conn.disconnect();
	EXPECT_TRUE(WaitForState(conn, MessageBus::detail::RedisConnectionState::Disconnected, 200ms));
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

TEST_F(MessageBusEmptyConstructor, EmitReturnsFalseWithoutConnection)
{
	MessageBus::MessageBus bus;
	// connが空の場合、emit()はfalseを返す
	EXPECT_FALSE(bus.emit(U"test_channel"));
	EXPECT_FALSE(bus.emit(U"test_channel", UR"({ "k": 1 })"_json));
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
