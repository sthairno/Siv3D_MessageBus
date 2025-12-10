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
		, m_initialValue(initialValue)
		, m_dirty(false)
		, m_sending(false)
		, m_initialized(false)
		, m_updatedAt(DateTime::Now())
	{
	}

	const JSON& SharedVariableImpl::valueAsJSON() const
	{
		return m_value;
	}

	void SharedVariableImpl::setValueAsJSON(const JSON& value)
	{
		m_value = value;
		m_updatedAt = DateTime::Now();
	}

	bool SharedVariableImpl::isDirty() const
	{
		return m_dirty;
	}

	void SharedVariableImpl::markClean()
	{
		m_dirty = false;
	}

	void SharedVariableImpl::markDirty()
	{
		m_dirty = true;
	}

	bool SharedVariableImpl::isSending() const
	{
		return m_sending;
	}

	void SharedVariableImpl::markSending()
	{
		m_sending = true;
	}

	void SharedVariableImpl::markSent()
	{
		m_sending = false;
	}

	bool SharedVariableImpl::isInitialized() const
	{
		return m_initialized;
	}

	void SharedVariableImpl::markInitialized()
	{
		m_initialized = true;
	}

	void SharedVariableImpl::markUninitialized()
	{
		m_initialized = false;
	}

	DateTime SharedVariableImpl::updatedAt() const
	{
		return m_updatedAt;
	}

	void SharedVariableImpl::reconcile(redisAsyncContext* context)
	{
		if (!context) return;

		// 送信中は次の送信を行わない
		if (isSending())
		{
			return;
		}

		// 前回の値から変更されている場合は送信
		if (isDirty())
		{
			sendSet(context);
			markInitialized();
			markClean();
			return;
		}

		// 接続直後の場合は変更されていなくても送信して初期化
		if (not isInitialized())
		{
			sendSetNxGet(context);
			markInitialized();
			return;
		}
	}

	void SharedVariableImpl::refresh(redisAsyncContext* context)
	{
		if (!context) return;
		sendGet(context);
	}

	void SharedVariableImpl::sendGet(redisAsyncContext* context)
	{
		if (!context) return;

		RedisCommandHelper::SendGet(context, shared_from_this(), m_u8name);
	}

	void SharedVariableImpl::sendSet(redisAsyncContext* context)
	{
		if (!context) return;

		markSending();

		const std::string valueJson = valueAsJSON().formatUTF8Minimum();

		RedisCommandHelper::SendSet(context, shared_from_this(), m_u8name, valueJson);
	}

	void SharedVariableImpl::sendSetNxGet(redisAsyncContext* context)
	{
		if (!context) return;

		markSending();

		const std::string valueJson = valueAsJSON().formatUTF8Minimum();

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
			
			if (isSending() || isDirty())
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
		markSent();

		if (!reply) return;

		if (reply->type == REDIS_REPLY_ERROR)
		{
			Logger << U"[MessageBus][ERROR] SET failed: " << Unicode::FromUTF8(std::string_view{ reply->str, reply->len });
		}
	}

	void SharedVariableImpl::onSetNxGetCallback(redisAsyncContext*, redisReply* reply)
	{
		// 送信完了したのでフラグをクリア
		markSent();

		if (!reply) return;

		if (reply->type == REDIS_REPLY_ERROR)
		{
			Logger << U"[MessageBus][ERROR] SET NX GET failed for key: " << u32name()
				<< U" - " << Unicode::FromUTF8(std::string_view{ reply->str, reply->len });
			return;
		}

		// GET で取得した値があれば更新 ただし、次のtickで更新される予定の場合は無視
		// nil の場合は何もしない（初期値のまま）
		if (reply->type == REDIS_REPLY_STRING && not isDirty())
		{
			const std::string_view valueStr{ reply->str, reply->len };
			const auto jsonValue = JSON::Parse(Unicode::FromUTF8(valueStr));
			setValueAsJSON(jsonValue);
		}
	}
}
