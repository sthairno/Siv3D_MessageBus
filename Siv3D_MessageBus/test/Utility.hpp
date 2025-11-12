#pragma once
#include <MessageBus/RedisConnection.hpp>
#include <MessageBus/MessageBus.hpp>

static constexpr auto TICK_INTERVAL = 20ms;

// 接続が確立するまで待機(タイムアウト付き)
static bool WaitForConnection(MessageBus::RedisConnection& conn, Duration timeout = 30s)
{
	Stopwatch sw{ StartImmediately::Yes };
	while (sw.elapsed() < timeout)
	{
		conn.tick();
		if (conn.state() == MessageBus::RedisConnectionState::Connected) return true;
		if (conn.state() == MessageBus::RedisConnectionState::Failed) return false;
		System::Sleep(TICK_INTERVAL);
	}
	return false;
}

static MessageBus::RedisConnectionState WaitForNextState(MessageBus::RedisConnection& conn, Duration timeout = 5s)
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
static void Sleep(MessageBus::RedisConnection& conn, Duration time)
{
	Stopwatch sw{ StartImmediately::Yes };
	while (sw < time)
	{
		conn.tick();
		System::Sleep(TICK_INTERVAL);
	}
}

// 条件が満たされるまで待機（RedisConnection 用）
template <class Pred>
static bool WaitUntil(MessageBus::RedisConnection& conn, Pred&& predicate, Duration timeout = 5s)
{
	Stopwatch sw{ StartImmediately::Yes };
	while (sw < timeout && !predicate())
	{
		conn.tick();
		System::Sleep(TICK_INTERVAL);
	}
	return predicate();
}

// 状態が特定の値になるまで待機
static bool WaitForState(MessageBus::RedisConnection& conn, MessageBus::RedisConnectionState targetState, Duration timeout = 5s)
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
		bus.tick();
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
		bus.tick();
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
		bus.tick();
		const auto& evs = bus.events();
		if (!evs.isEmpty()) return true;
		System::Sleep(TICK_INTERVAL);
	}
	return false;
}

static void Sleep(MessageBus::MessageBus& bus, Duration time)
{
	Stopwatch sw{ StartImmediately::Yes };
	while (sw < time)
	{
		bus.tick();
		System::Sleep(TICK_INTERVAL);
	}
}
