#pragma once
#include <MessageBus/detail/PlayerList.hpp>
#include <MessageBus/detail/RedisConnection.hpp>
#include <MessageBus/detail/SubscriptionWorker.hpp>
#include <MessageBus/detail/VariableWorker.hpp>
#include <MessageBus/MessageBus.hpp>

static constexpr auto TICK_INTERVAL = 20ms;

// 接続が確立するまで待機(タイムアウト付き)
static bool WaitForConnection(MessageBus::detail::RedisConnection& conn, Duration timeout = 30s)
{
	Stopwatch sw{ StartImmediately::Yes };
	while (sw.elapsed() < timeout)
	{
		conn.tick();
		if (conn.state() == MessageBus::detail::RedisConnectionState::Connected) return true;
		// 失敗かつ再接続予定なしの場合は早期終了
		if (conn.isFailed() && !conn.isReconnecting()) return false;
		System::Sleep(TICK_INTERVAL);
	}
	return false;
}

static MessageBus::detail::RedisConnectionState WaitForNextState(MessageBus::detail::RedisConnection& conn, Duration timeout = 5s)
{
	auto initialState = conn.state();
	Stopwatch sw{ StartImmediately::Yes };
	while (sw.elapsed() < timeout)
	{
		conn.tick();
		if (conn.state() != initialState) return conn.state();
		System::Sleep(TICK_INTERVAL);
	}
	return initialState;
}

// 一定時間 tick を回す（RedisConnection 用）
static void Sleep(MessageBus::detail::RedisConnection& conn, Duration time)
{
	Stopwatch sw{ StartImmediately::Yes };
	while (sw < time)
	{
		conn.tick();
		System::Sleep(TICK_INTERVAL);
	}
}

// 一定時間 tick を回す（PlayerList + RedisConnection 用）
static void Sleep(MessageBus::detail::PlayerList& plist, MessageBus::detail::RedisConnection& conn, Duration time)
{
	Stopwatch sw{ StartImmediately::Yes };
	while (sw < time)
	{
		plist.beforeTick(conn);
		conn.tick();
		plist.afterTick();
		System::Sleep(TICK_INTERVAL);
	}
}

// 一定時間 tick を回す（VariableWorker + RedisConnection 用）
static void Sleep(MessageBus::detail::VariableWorker& worker, MessageBus::detail::RedisConnection& conn, Duration time)
{
	Stopwatch sw{ StartImmediately::Yes };
	while (sw < time)
	{
		worker.beforeTick(conn);
		conn.tick();
		worker.afterTick();
		System::Sleep(TICK_INTERVAL);
	}
}

// 一定時間 tick を回す（SubscriptionWorker + RedisConnection 用）
static void Sleep(MessageBus::detail::SubscriptionWorker& worker, MessageBus::detail::RedisConnection& conn, Duration time)
{
	Stopwatch sw{ StartImmediately::Yes };
	while (sw < time)
	{
		worker.beforeTick(conn);
		conn.tick();
		worker.afterTick();
		System::Sleep(TICK_INTERVAL);
	}
}

// 条件が満たされるまで待機（RedisConnection 用）
template <class Pred>
static bool WaitUntil(MessageBus::detail::RedisConnection& conn, Pred&& predicate, Duration timeout = 5s)
{
	Stopwatch sw{ StartImmediately::Yes };
	while (sw < timeout && !predicate())
	{
		conn.tick();
		System::Sleep(TICK_INTERVAL);
	}
	return predicate();
}

// 条件が満たされるまで待機（VariableWorker + RedisConnection 用）
template <class Pred>
static bool WaitUntil(MessageBus::detail::VariableWorker& worker, MessageBus::detail::RedisConnection& conn, Pred&& predicate, Duration timeout = 5s)
{
	Stopwatch sw{ StartImmediately::Yes };
	while (sw < timeout && !predicate())
	{
		worker.beforeTick(conn);
		conn.tick();
		worker.afterTick();
		System::Sleep(TICK_INTERVAL);
	}
	return predicate();
}

// 条件が満たされるまで待機（SubscriptionWorker + RedisConnection 用）
template <class Pred>
static bool WaitUntil(MessageBus::detail::SubscriptionWorker& worker, MessageBus::detail::RedisConnection& conn, Pred&& predicate, Duration timeout = 5s)
{
	Stopwatch sw{ StartImmediately::Yes };
	while (sw < timeout && !predicate())
	{
		worker.beforeTick(conn);
		conn.tick();
		worker.afterTick();
		System::Sleep(TICK_INTERVAL);
	}
	return predicate();
}

// 条件が満たされるまで待機（MessageBus 用）
template <class Pred>
static bool WaitUntil(MessageBus::MessageBus& bus, Pred&& predicate, Duration timeout = 5s)
{
	Stopwatch sw{ StartImmediately::Yes };
	while (sw < timeout && !predicate())
	{
		bus.update();
	}
	return predicate();
}

// 条件が満たされるまで待機（2つの MessageBus を同時に進める）
template <class Pred>
static bool WaitUntil(MessageBus::MessageBus& bus1, MessageBus::MessageBus& bus2, Pred&& predicate, Duration timeout = 5s)
{
	Stopwatch sw{ StartImmediately::Yes };
	while (sw < timeout && !predicate())
	{
		bus1.update();
		bus2.update();
	}
	return predicate();
}

// 状態が特定の値になるまで待機
static bool WaitForState(MessageBus::detail::RedisConnection& conn, MessageBus::detail::RedisConnectionState targetState, Duration timeout = 5s)
{
	Stopwatch sw{ StartImmediately::Yes };
	while (sw.elapsed() < timeout)
	{
		conn.tick();
		if (conn.state() == targetState) return true;
		System::Sleep(TICK_INTERVAL);
	}
	return false;
}

// 接続待機
static void WaitForConnection(MessageBus::MessageBus& bus, Duration timeout = 30s)
{
	Stopwatch sw{ StartImmediately::Yes };
	while (sw.elapsed() < timeout)
	{
		bus.update();
		if (bus.isConnected()) return;
		System::Sleep(TICK_INTERVAL);
	}
	FAIL() << "MessageBus connection timed out";
}

// 切断待機
static void WaitForDisconnect(MessageBus::MessageBus& bus, Duration timeout = 30s)
{
	Stopwatch sw{ StartImmediately::Yes };
	while (sw.elapsed() < timeout)
	{
		bus.update();
		if (!bus.isConnected()) return;
		System::Sleep(TICK_INTERVAL);
	}
	FAIL() << "MessageBus disconnection timed out";
}


// events 到着待機（条件を満たしたら true）
static bool WaitForEvent(MessageBus::MessageBus& bus, Duration timeout = 5s)
{
	Stopwatch sw{ StartImmediately::Yes };
	while (sw < timeout)
	{
		bus.update();
		const auto& evs = bus.events();
		if (!evs.isEmpty()) return true;
		System::Sleep(TICK_INTERVAL);
	}
	return false;
}

template <class Pred>
static bool WaitForEventMatching(MessageBus::MessageBus& bus, Pred&& predicate, Duration timeout = 5s)
{
	Stopwatch sw{ StartImmediately::Yes };
	while (sw < timeout)
	{
		bus.update();
		for (const auto& event : bus.events())
		{
			if (predicate(event))
			{
				return true;
			}
		}
		System::Sleep(TICK_INTERVAL);
	}
	return false;
}

// SubscriptionWorker の events 到着待機（条件を満たしたら true）
static bool WaitForEvent(MessageBus::detail::SubscriptionWorker& worker, MessageBus::detail::RedisConnection& conn, Duration timeout = 5s)
{
	return WaitUntil(worker, conn, [&] { return !worker.events().isEmpty(); }, timeout);
}

static void Sleep(MessageBus::MessageBus& bus, Duration time)
{
	Stopwatch sw{ StartImmediately::Yes };
	while (sw < time)
	{
		bus.update();
		System::Sleep(TICK_INTERVAL);
	}
}
