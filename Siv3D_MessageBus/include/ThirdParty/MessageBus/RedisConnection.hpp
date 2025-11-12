#pragma once

extern "C" {
#include <hiredis/hiredis.h>
}

#include "RedisConnectionState.hpp"
#include <functional>

#include <Siv3D/StringView.hpp>
#include <Siv3D/String.hpp>
#include <Siv3D/Types.hpp>
#include <Siv3D/Optional.hpp>
#include <Siv3D/Duration.hpp>
#include <Siv3D/Timer.hpp>
#include <Siv3D/Stopwatch.hpp>

namespace MessageBus
{
	struct RedisConnectionOptions
	{
		s3d::StringView ip;
		s3d::uint16 port;
		s3d::Optional<s3d::StringView> password = s3d::none;
		s3d::Duration heartbeatInterval = s3d::Seconds{ 10 };
		std::function<void(redisAsyncContext*)> onConnect;
		std::function<void(redisAsyncContext*)> onReady;
		std::function<void()> onDisconnect;
		std::function<void(redisAsyncContext*, redisReply*)> onPush;
	};

	class RedisConnection
	{
	public:

		RedisConnection(RedisConnectionOptions options);

	public:

		const s3d::String& ip() const noexcept { return m_ip; }
		s3d::uint16 port() const noexcept { return m_port; }
		s3d::Optional<s3d::String> password() const noexcept { return m_password; }
		RedisConnectionState state() const noexcept { return m_state; }
		const s3d::String& error() const noexcept { return m_error; }
		bool isReconnecting() const noexcept { return m_isReconnecting; }
		redisAsyncContext* context() const noexcept { return m_context; }

		void tick();
		void disconnect();

	private:

		// 接続情報
		redisAsyncContext* m_context;
		s3d::String m_ip;
		s3d::uint16 m_port;
		s3d::Optional<s3d::String> m_password;
		s3d::Duration m_heartbeatInterval;

		// 接続状態
		RedisConnectionState m_state;
		s3d::String m_error;
		s3d::Timer m_reconnectTimer;
		int m_reconnectAttempts = 0;
		bool m_isReconnecting = false;

		// ハートビート監視
		s3d::Stopwatch m_heartbeatTimer;

		// コールバック
		std::function<void(redisAsyncContext*)> m_onConnect;
		std::function<void(redisAsyncContext*)> m_onReady;
		std::function<void()> m_onDisconnect;
		std::function<void(redisAsyncContext*, redisReply*)> m_onPush;

	private:

		void tryConnect();
		void setState(RedisConnectionState newState);
		void failure(s3d::StringView message, bool reconnect);

		// コマンド送信
		void sendHello();
		void sendAuth();
		void sendPing();

		// コールバック
		static void onConnectCallback(redisAsyncContext* ac, int status);
		static void onDisconnectCallback(const redisAsyncContext* ac, int status);
		static void onHelloCallback(redisAsyncContext* ac, redisReply* reply, RedisConnection* self);
		static void onAuthCallback(redisAsyncContext* ac, redisReply* reply, RedisConnection* self);
		static void onReady(redisAsyncContext* ac, RedisConnection* self);
		static void onPingCallback(redisAsyncContext* ac, redisReply* reply, RedisConnection* self);
		static void onPushCallback(redisAsyncContext* ac, redisReply* reply);

	public:

		~RedisConnection();
	};
}
