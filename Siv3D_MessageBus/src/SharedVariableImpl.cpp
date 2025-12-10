#include "MessageBus/detail/SharedVariableImpl.hpp"
#include <Siv3D/DateTime.hpp>
#include <Siv3D/Logger.hpp>
#include <Siv3D/Unicode.hpp>
#include <memory>

extern "C" {
#include <hiredis/async.h>
}

using namespace s3d;

namespace MessageBus::detail
{
	// Hiredisのコールバック呼び出し完了までSharedVariableImplの生存期間を延長するためのヘルパークラス
	struct SharedVariableImpl::RedisCommandHelper
	{
		std::shared_ptr<SharedVariableImpl> impl;

		static void OnSetCallbackProxy(redisAsyncContext* context, redisReply* reply, SharedVariableImpl::RedisCommandHelper* privdata)
		{
			if (!privdata) return;
			auto self = std::move(*privdata);
			delete privdata;

			self.impl->onSetCallback(context, reply);
		}

		static void OnGetCallbackProxy(redisAsyncContext* context, redisReply* reply, SharedVariableImpl::RedisCommandHelper* privdata)
		{
			if (!privdata) return;
			auto self = std::move(*privdata);
			delete privdata;

			self.impl->onGetCallback(context, reply);
		}
		
		static void OnSetNxGetCallbackProxy(redisAsyncContext* context, redisReply* reply, SharedVariableImpl::RedisCommandHelper* privdata)
		{
			if (!privdata) return;
			auto self = std::move(*privdata);
			delete privdata;

			self.impl->onSetNxGetCallback(context, reply);
		}

		
		static int SendSet(
			redisAsyncContext* context,
			std::shared_ptr<SharedVariableImpl> impl,
			std::string_view key,
			std::string_view value
		) {
			const char* argv[3];
			size_t argvlen[3];

			argv[0] = "SET";
			argvlen[0] = 3;
			argv[1] = key.data();
			argvlen[1] = key.size();
			argv[2] = value.data();
			argvlen[2] = value.size();
	
			return redisAsyncCommandArgv(
				context,
				reinterpret_cast<redisCallbackFn*>(RedisCommandHelper::OnSetCallbackProxy),
				new RedisCommandHelper{ impl },
				3, argv, argvlen
			);
		}

		static int SendSetNxGet(
			redisAsyncContext* context,
			std::shared_ptr<SharedVariableImpl> impl,
			std::string_view key,
			std::string_view value
		) {
			const char* argv[5];
			size_t argvlen[5];

			argv[0] = "SET";
			argvlen[0] = 3;
			argv[1] = key.data();
			argvlen[1] = key.size();
			argv[2] = value.data();
			argvlen[2] = value.size();
			argv[3] = "NX";
			argvlen[3] = 2;
			argv[4] = "GET";
			argvlen[4] = 3;

			return redisAsyncCommandArgv(
				context,
				reinterpret_cast<redisCallbackFn*>(RedisCommandHelper::OnSetNxGetCallbackProxy),
				new RedisCommandHelper{ impl },
				5, argv, argvlen
			);
		}

		static int SendGet(redisAsyncContext* context, std::shared_ptr<SharedVariableImpl> impl, std::string_view key)
		{
			const char* argv[2];
			size_t argvlen[2];

			argv[0] = "GET";
			argvlen[0] = 3;
			argv[1] = key.data();
			argvlen[1] = key.size();

			return redisAsyncCommandArgv(
				context,
				reinterpret_cast<redisCallbackFn*>(RedisCommandHelper::OnGetCallbackProxy),
				new RedisCommandHelper{ impl },
				2, argv, argvlen
			);
		}
	};
	
	SharedVariableImpl::SharedVariableImpl(std::string_view u8name, StringView u32name, const JSON& initialValue)
		: m_u8name(u8name)
		, m_u32name(u32name)
		, m_value(initialValue)
		, m_dirty(false)
		, m_sending(false)
		, m_initialized(false)
		, m_updatedAt(DateTime::Now())
	{
	}

	void SharedVariableImpl::setValueAsJSON(const JSON& value)
	{
		m_value = value;
		m_updatedAt = DateTime::Now();
	}

	void SharedVariableImpl::syncToRemote(redisAsyncContext* context)
	{
		assert(context);
		
		// 送信中は次の送信を行わない
		if (m_sending)
		{
			return;
		}

		// 前回の値から変更されている場合は送信
		if (m_dirty)
		{
			sendSet(context);
			m_initialized = true;
			m_dirty = false;
			return;
		}

		// 接続直後の場合は変更されていなくても送信して初期化
		if (not m_initialized)
		{
			sendSetNxGet(context);
			m_initialized = true;
			return;
		}
	}

	void SharedVariableImpl::fetchFromRemote(redisAsyncContext* context)
	{
		assert(context);

		sendGet(context);
	}

	void SharedVariableImpl::reset()
	{
		m_initialized = false;
		m_sending = false;
	}

	void SharedVariableImpl::sendGet(redisAsyncContext* context)
	{
		if (!context) return;

		RedisCommandHelper::SendGet(context, shared_from_this(), m_u8name);
	}

	void SharedVariableImpl::sendSet(redisAsyncContext* context)
	{
		if (!context) return;

		m_sending = true;

		const std::string valueJson = valueAsString();

		RedisCommandHelper::SendSet(context, shared_from_this(), m_u8name, valueJson);
	}

	void SharedVariableImpl::sendSetNxGet(redisAsyncContext* context)
	{
		if (!context) return;

		m_sending = true;

		const std::string valueJson = valueAsString();

		RedisCommandHelper::SendSetNxGet(context, shared_from_this(), m_u8name, valueJson);
	}

	void SharedVariableImpl::onGetCallback(redisAsyncContext*, redisReply* reply)
	{
		if (!reply) return;

		if (reply->type == REDIS_REPLY_ERROR)
		{
			Logger << U"[MessageBus][ERROR] GET failed for key: " << u32name()
				<< U" - " << Unicode::FromUTF8(std::string_view{ reply->str, reply->len });
			return;
		}

		// nil の場合は何もしない（キャッシュは更新しない）
		if (reply->type == REDIS_REPLY_NIL)
		{
			Logger << U"[MessageBus][WARN] GET returned nil for key: " << u32name();
			return;
		}

		// 文字列値が取得できた場合のみ更新
		if (reply->type == REDIS_REPLY_STRING)
		{
			const std::string_view valueStr{ reply->str, reply->len };
			const auto jsonValue = JSON::Parse(Unicode::FromUTF8(valueStr));
			
			if (m_sending || m_dirty)
			{
				// 送信中か、次のtickで更新される予定の場合は無視
				Logger << U"[MessageBus][WARN] Remote value update was ignored (possible data conflict)";
			}
			else if (jsonValue.isEmpty())
			{
				setValueAsJSON(JSON::Invalid());
				Logger << U"[MessageBus][WARN] Failed to parse JSON for key: " << u32name();
			}
			else
			{
				setValueAsJSON(jsonValue);
			}
		}
	}

	void SharedVariableImpl::onSetCallback(redisAsyncContext*, redisReply* reply)
	{
		// 送信完了したのでフラグをクリア
		m_sending = false;

		if (!reply) return;

		if (reply->type == REDIS_REPLY_ERROR)
		{
			Logger << U"[MessageBus][ERROR] SET failed: " << Unicode::FromUTF8(std::string_view{ reply->str, reply->len });
		}
	}

	void SharedVariableImpl::onSetNxGetCallback(redisAsyncContext*, redisReply* reply)
	{
		// 送信完了したのでフラグをクリア
		m_sending = false;

		if (!reply) return;

		if (reply->type == REDIS_REPLY_ERROR)
		{
			Logger << U"[MessageBus][ERROR] SET NX GET failed for key: " << u32name()
				<< U" - " << Unicode::FromUTF8(std::string_view{ reply->str, reply->len });
			return;
		}

		// GET で取得した値があれば更新 ただし、次のtickで更新される予定の場合は無視
		// nil の場合は何もしない（初期値のまま）
		if (reply->type == REDIS_REPLY_STRING && not m_dirty)
		{
			const std::string_view valueStr{ reply->str, reply->len };
			const auto jsonValue = JSON::Parse(Unicode::FromUTF8(valueStr));
			if (jsonValue.isEmpty())
			{
				Logger << U"[MessageBus][WARN] Failed to parse JSON for key: " << u32name();
				setValueAsJSON(JSON::Invalid());
			}
			else
			{
				setValueAsJSON(jsonValue);
			}
		}
	}
}
