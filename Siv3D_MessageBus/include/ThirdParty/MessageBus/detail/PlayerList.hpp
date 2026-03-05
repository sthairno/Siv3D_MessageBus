#pragma once

#include "MessageBus/detail/RedisConnection.hpp"

#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>
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
			s3d::Duration playerListPollInterval = s3d::Seconds{ 30 };
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

		void afterTick();

		void beforeDisconnect(RedisConnection& conn);

		void onDisconnect();

		bool handlePubSubMessage(std::string_view channel, std::string_view payload);

		[[nodiscard]]
		std::string_view uidUtf8() const noexcept { return m_uidUtf8; }
		
		[[nodiscard]]
		SessionStatus sessionStatus() const noexcept { return m_sessionStatus; }

		[[nodiscard]]
		const std::unordered_set<std::string>& connectedPlayerUidsUtf8() const noexcept { return m_connectedPlayerUidsUtf8; }

		[[nodiscard]]
		const std::vector<std::string>& addedPlayerUidsUtf8() const noexcept { return m_addedPlayerUidsUtf8; }

		[[nodiscard]]
		const std::vector<std::string>& removedPlayerUidsUtf8() const noexcept { return m_removedPlayerUidsUtf8; }

	private:

		Options m_options;

		std::string m_uidUtf8;

		std::string m_sessionKeyUtf8;

		s3d::Stopwatch m_timeSinceUpdate;

		bool m_sessionUpdatetInFlight;

		SessionStatus m_sessionStatus;

		std::unordered_set<std::string> m_connectedPlayerUidsUtf8;

		std::unordered_set<std::string> m_previousConnectedPlayerUidsUtf8;
		std::vector<std::string> m_addedPlayerUidsUtf8;
		std::vector<std::string> m_removedPlayerUidsUtf8;

		bool m_refreshInFlight;

		s3d::Stopwatch m_timeSinceLastRefresh;

		[[nodiscard]]
		static std::string generateUidUtf8();

		void publishPlayerJoin(redisAsyncContext* context);

		void publishPlayerLeft(redisAsyncContext* context);

		void updateSession(redisAsyncContext* context);

		static void onSessionUpdateCallback(redisAsyncContext* context, redisReply* reply, PlayerList* self);

		void deleteSession(redisAsyncContext* context);

		void fetchPlayerList(redisAsyncContext* context);

		static void fetchPlayerListCallback(redisAsyncContext* context, redisReply* reply, PlayerList* self);
	};
}
