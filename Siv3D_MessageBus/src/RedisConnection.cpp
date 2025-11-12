#include "MessageBus/RedisConnection.hpp"
#include "MessageBus/GeneratedLicenses.hpp"

extern "C"
{
#include <hiredis/adapters/poll.h>
#include <hiredis/async.h>
}

#include <Siv3D/Logger.hpp>
#include <Siv3D/Unicode.hpp>
#include <Siv3D/FormatLiteral.hpp>
#include <Siv3D/Duration.hpp>
#include <Siv3D/Timer.hpp>
#include <Siv3D/Stopwatch.hpp>
#include <Siv3D/Math.hpp>
#include <Siv3D/LicenseManager.hpp>

using namespace s3d;

namespace MessageBus
{
	constexpr int MAX_RECONNECT_INTERVAL_SEC = 60;
	constexpr int MAX_RECONNECT_ATTEMPTS = 10;

	RedisConnection::RedisConnection(RedisConnectionOptions options)
		: m_context(nullptr), m_ip(options.ip), m_port(options.port),
		m_password(options.password
					? MakeOptional<String>(options.password.value())
					: Optional<String>{}),
		m_reconnectTimer(Seconds{ 0 }, StartImmediately::No),
		m_heartbeatInterval(options.heartbeatInterval),
		m_state(RedisConnectionState::Disconnected),
		m_onConnect(options.onConnect), m_onReady(options.onReady),
		m_onDisconnect(options.onDisconnect), m_onPush(options.onPush)
	{
		const auto& licenses = LicenseManager::EnumLicenses();
		const auto& hiredisLicense = Generated::HiredisLicense();
		const bool exists = licenses.contains_if([&](const LicenseInfo& li) { return li.title == hiredisLicense.title; });
		if (!exists)
		{
			LicenseManager::AddLicense(hiredisLicense);
		}

		tryConnect();
	}

	RedisConnection::~RedisConnection()
	{
		if (m_context)
		{
			redisAsyncFree(m_context);
			m_context = nullptr;
		}
	}

	void RedisConnection::tick()
	{
		if (m_context)
		{
			redisPollTick(m_context, 0.0);
		}

		if (m_state == RedisConnectionState::Disconnected ||
			m_state == RedisConnectionState::Failed)
		{
			// 再接続処理
			if (m_isReconnecting && m_reconnectTimer.reachedZero())
			{
				tryConnect();
			}
		}
		if (m_state == RedisConnectionState::Connected)
		{
			// ハートビート処理
			if (m_heartbeatTimer.elapsed() >= m_heartbeatInterval)
			{
				sendPing();
			}
		}
	}

	void RedisConnection::disconnect()
	{
		// 手動切断では再接続を抑止
		m_isReconnecting = false;
		if (m_context)
		{
			redisAsyncDisconnect(m_context);
		}
	}

	void RedisConnection::tryConnect()
	{
		setState(RedisConnectionState::Connecting);

		Logger << U"[Redis][INFO] ip=" << m_ip << U", port=" << m_port;

		// 非同期接続オプションを設定
		redisOptions options = { 0 };
		std::string ipStr = m_ip.narrow();
		REDIS_OPTIONS_SET_TCP(&options, ipStr.c_str(), m_port);
		options.async_push_cb = reinterpret_cast<redisAsyncPushFn*>(RedisConnection::onPushCallback);

		m_context = redisAsyncConnectWithOptions(&options);
		if (!m_context)
		{
			failure(U"Initialization Error: Failed to initialize Redis client", false);
			return;
		}

		if (m_context->err)
		{
			failure(
				U"Initialization Error: {}"_fmt(Unicode::FromUTF8(m_context->errstr)),
				false);
			redisAsyncFree(m_context);
			m_context = nullptr;
			return;
		}

		// クラス参照を設定
		m_context->data = this;
		m_context->dataCleanup = nullptr;

		// poll.hアダプタをアタッチ
		if (redisPollAttach(m_context) != REDIS_OK)
		{
			failure(U"Initialization Error: Failed to attach poll adapter", false);
			redisAsyncFree(m_context);
			m_context = nullptr;
			return;
		}

		// コールバック登録
		redisAsyncSetConnectCallbackNC(m_context, onConnectCallback);
		redisAsyncSetDisconnectCallback(m_context, onDisconnectCallback);

		setState(RedisConnectionState::Connecting);
	}

	void RedisConnection::setState(RedisConnectionState newState)
	{
		if (m_state != newState)
		{
			Logger << U"[Redis][INFO] " << m_state << U" → " << newState;
			m_state = newState;
		}
	}

	void RedisConnection::failure(s3d::StringView message, bool reconnect)
	{
		Logger << U"[Redis][ERROR] " << message;
		m_error = message;
		setState(RedisConnectionState::Failed);

		if (!reconnect)
		{
			m_isReconnecting = false;
			return;
		}

		m_reconnectAttempts++;

		if (m_reconnectAttempts > MAX_RECONNECT_ATTEMPTS)
		{
			if (!message.isEmpty())
			{
				m_error = U"Connection Error: Exceeded maximum reconnect attempts (last error: {})"_fmt(message);
			}
			else
			{
				m_error = U"Connection Error: Exceeded maximum reconnect attempts";
			}
			m_isReconnecting = false;
			return;
		}

		int delay =
			Min(5 * (1 << (m_reconnectAttempts - 1)), MAX_RECONNECT_INTERVAL_SEC);

		Logger << U"[Redis][INFO] Reconnect in " << delay << U"s ("
			<< m_reconnectAttempts << U"/" << MAX_RECONNECT_ATTEMPTS << U")";

		m_reconnectTimer.restart(Seconds{ delay });
		m_isReconnecting = true;
	}

	// コールバック実装
	void RedisConnection::onConnectCallback(redisAsyncContext* ac, int status)
	{
		RedisConnection* self = static_cast<RedisConnection*>(ac->data);

		if (status == REDIS_OK)
		{
			// TCP接続確立を通知
			if (self->m_onConnect)
				self->m_onConnect(ac);

			self->m_error = U"";
			self->m_reconnectAttempts = 0;

			if (self->m_password.has_value())
			{
				self->sendAuth();
			}
			else
			{
				self->sendHello();
			}
		}
		else
		{
			self->failure(U"Connection Error: {}"_fmt(Unicode::FromUTF8(ac->errstr)), true);
			self->m_context = nullptr;
		}
	}

	void RedisConnection::onDisconnectCallback(const redisAsyncContext* ac, int status)
	{
		RedisConnection* self = static_cast<RedisConnection*>(ac->data);

		bool disconnectedByError =
			self->m_state == RedisConnectionState::Failed && status == REDIS_OK;

		if (!disconnectedByError)
		{
			switch (status)
			{
			case REDIS_OK: {
				self->setState(RedisConnectionState::Disconnected);
				self->m_error = U"";
				break;
			}
			case REDIS_ERR_PROTOCOL:
			case REDIS_ERR_OOM: {
				// リトライしても改善する見込みがないエラーは再接続しない
				self->failure(U"Connection Error: {}"_fmt(Unicode::FromUTF8(ac->errstr)), false);
				break;
			}
			default: {
				self->failure(U"Connection Error: {}"_fmt(Unicode::FromUTF8(ac->errstr)), true);
				break;
			}
			}
		}

		// ユーザー向けコールバック
		if (self->m_onDisconnect)
			self->m_onDisconnect();

		self->m_context = nullptr;
	}

	void RedisConnection::onAuthCallback(redisAsyncContext* ac, redisReply* reply, RedisConnection* self)
	{
		if (!reply)
			return;

		if (reply->type == REDIS_REPLY_ERROR)
		{
			self->failure(U"Auth Error: {}"_fmt(Unicode::FromUTF8(reply->str)), false);
			redisAsyncDisconnect(ac);
		}
		else
		{
			self->sendHello();
		}
	}

	void RedisConnection::onHelloCallback(redisAsyncContext* ac, redisReply* reply, RedisConnection* self)
	{
		if (!reply)
			return;

		if (reply->type == REDIS_REPLY_ERROR)
		{
			String message = Unicode::FromUTF8(reply->str);
			if (message.contains(U"unknown command"))
			{
				message =
					U"Protocol Error: {} (NOTE: This client requires Redis >= 7.2 or Valkey)"_fmt(
						message);
			}
			else if (message.contains(U"NOAUTH"))
			{
				message = U"Auth Error: {}"_fmt(message);
			}
			else
			{
				message = U"Protocol Error: {}"_fmt(message);
			}
			self->failure(message, false);
			redisAsyncDisconnect(ac);
		}
		else
		{
			self->onReady(ac, self);
		}
	}

	void RedisConnection::onReady(redisAsyncContext* ac, RedisConnection* self)
	{
		self->setState(RedisConnectionState::Connected);
		self->m_heartbeatTimer.restart();
		self->m_isReconnecting = false;

		if (self->m_onReady)
			self->m_onReady(ac);
	}

	void RedisConnection::onPingCallback(redisAsyncContext*, redisReply* reply, RedisConnection* self)
	{
		if (!reply)
			return;

		// ここでのエラーハンドリングはhiredis側で行われるため特に何もしない
		// エラー応答だった場合はonDisconnectCallbackが呼ばれるはず

		self->m_heartbeatTimer.restart();
	}

	void RedisConnection::onPushCallback(redisAsyncContext* ac, redisReply* reply)
	{
		if (!ac || !reply) return;
		RedisConnection* self = reinterpret_cast<RedisConnection*>(ac->data);
		if (!self || !self->m_onPush) return;
		self->m_onPush(ac, reply);
	}

	// ===== アクション実装 =====
	void RedisConnection::sendHello()
	{
		redisAsyncCommand(
			m_context,
			reinterpret_cast<redisCallbackFn*>(onHelloCallback), this,
			"HELLO 3"
		);
		setState(RedisConnectionState::HelloSent);
	}

	void RedisConnection::sendAuth()
	{
		redisAsyncCommand(
			m_context,
			reinterpret_cast<redisCallbackFn*>(onAuthCallback), this,
			"AUTH %s", m_password->narrow().c_str()
		);
		setState(RedisConnectionState::AuthSent);
	}

	void RedisConnection::sendPing()
	{
		redisAsyncCommand(
			m_context,
			reinterpret_cast<redisCallbackFn*>(onPingCallback), this,
			"PING"
		);
	}
}
