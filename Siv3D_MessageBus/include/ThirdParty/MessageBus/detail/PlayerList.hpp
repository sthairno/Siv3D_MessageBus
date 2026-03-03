#pragma once

#include "MessageBus/detail/RedisConnection.hpp"

#include <string>
#include <string_view>
#include <Siv3D/Duration.hpp>
#include <Siv3D/Stopwatch.hpp>

namespace MessageBus::detail
{
	class PlayerList
	{
    public:

		struct Options
		{
			int sessionTtlMs = 60 * 1000;
			s3d::Duration sessionRefreshInterval = s3d::Seconds{ 55 };
		};

		enum class SessionStatus
		{
			InactiveOrExpired,
			Active,
			Error
		};

		explicit PlayerList(Options options);

		void onConnect(RedisConnection& conn);

		void beforeTick(RedisConnection& conn);

		void beforeDisconnect(RedisConnection& conn);

		void onDisconnect();

		[[nodiscard]]
		std::string_view uidUtf8() const noexcept { return m_uidUtf8; }
		
		[[nodiscard]]
		SessionStatus sessionStatus() const noexcept { return m_sessionStatus; }
		
	private:

		Options m_options;

		std::string m_uidUtf8;

		std::string m_sessionKeyUtf8;

		s3d::Stopwatch m_timeSinceUpdate;

		bool m_sessionUpdatetInFlight;

		SessionStatus m_sessionStatus;

		[[nodiscard]]
		static std::string generateUidUtf8();

		void publishPlayerJoin(redisAsyncContext* context);

		void publishPlayerLeft(redisAsyncContext* context);

		void updateSession(redisAsyncContext* context);

		static void onSessionUpdateCallback(redisAsyncContext* context, redisReply* reply, PlayerList* self);

		void deleteSession(redisAsyncContext* context);
	};
}
