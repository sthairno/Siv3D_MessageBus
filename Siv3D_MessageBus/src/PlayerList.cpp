#include "MessageBus/detail/PlayerList.hpp"

#include <Siv3D/Random.hpp>
#include <Siv3D/Logger.hpp>
#include <Siv3D/Unicode.hpp>
#include <sqids/sqids.hpp>

#include <cstdint>
#include <vector>

extern "C" {
#include <hiredis/async.h>
}

namespace MessageBus::detail
{
	namespace
	{
		constexpr std::string_view PlayerJoinChannelUtf8 = "s3d-mbus:player-join";
		constexpr std::string_view PlayerLeftChannelUtf8 = "s3d-mbus:player-left";
		constexpr std::string_view PlayerKeyPrefix = "s3d-mbus:player:";
		constexpr std::string_view PlayerKeysPattern = "s3d-mbus:player:*";
	}

	PlayerList::PlayerList(Options options)
		: m_options(options)
		, m_uidUtf8(generateUidUtf8())
		, m_sessionKeyUtf8(std::string(PlayerKeyPrefix) + m_uidUtf8)
		, m_timeSinceUpdate(s3d::StartImmediately::No)
		, m_sessionUpdatetInFlight(false)
		, m_sessionStatus(SessionStatus::InactiveOrExpired)
		, m_refreshInFlight(false)
		, m_timeSinceLastRefresh(s3d::StartImmediately::No)
	{
	}

	void PlayerList::onConnect(RedisConnection& conn)
	{
		m_timeSinceUpdate.restart();
		m_sessionUpdatetInFlight = false;
		m_sessionStatus = SessionStatus::InactiveOrExpired;

		updateSession(conn.context());

		m_timeSinceLastRefresh.restart();
		fetchPlayerList(conn.context());
	}

	void PlayerList::beforeTick(RedisConnection& conn)
	{
		if (conn.state() == RedisConnectionState::Connected &&
			not m_sessionUpdatetInFlight &&
			m_sessionStatus != SessionStatus::Error &&
			m_timeSinceUpdate.elapsed() > m_options.sessionRefreshInterval)
		{
			updateSession(conn.context());
		}

		if (m_sessionStatus == SessionStatus::Active &&
			m_timeSinceUpdate.ms64() > m_options.sessionTtlMs)
		{
			m_sessionStatus = SessionStatus::InactiveOrExpired;
		}

		if (conn.state() == RedisConnectionState::Connected &&
			not m_refreshInFlight &&
			m_timeSinceLastRefresh.elapsed() >= m_options.playerListPollInterval)
		{
			fetchPlayerList(conn.context());
		}
	}

	void PlayerList::afterTick()
	{
		m_addedPlayerUidsUtf8.clear();
		m_removedPlayerUidsUtf8.clear();

		for (const std::string& uid : m_connectedPlayerUidsUtf8)
		{
			if (m_previousConnectedPlayerUidsUtf8.find(uid) == m_previousConnectedPlayerUidsUtf8.end())
			{
				m_addedPlayerUidsUtf8.push_back(uid);
			}
		}

		for (const std::string& uid : m_previousConnectedPlayerUidsUtf8)
		{
			if (m_connectedPlayerUidsUtf8.find(uid) == m_connectedPlayerUidsUtf8.end())
			{
				m_removedPlayerUidsUtf8.push_back(uid);
			}
		}

		m_previousConnectedPlayerUidsUtf8 = m_connectedPlayerUidsUtf8;
	}

	void PlayerList::beforeDisconnect(RedisConnection& conn)
	{
		publishPlayerLeft(conn.context());
		deleteSession(conn.context());
	}

	void PlayerList::onDisconnect()
	{
		m_timeSinceUpdate.reset();
		m_sessionUpdatetInFlight = false;
		m_sessionStatus = SessionStatus::InactiveOrExpired;

		m_refreshInFlight = false;
		m_timeSinceLastRefresh.reset();

		m_connectedPlayerUidsUtf8.clear();
		m_previousConnectedPlayerUidsUtf8.clear();
		m_addedPlayerUidsUtf8.clear();
		m_removedPlayerUidsUtf8.clear();
	}

	bool PlayerList::handlePubSubMessage(std::string_view channel, std::string_view payload)
	{
		if (channel == PlayerJoinChannelUtf8)
		{
			if (!payload.empty())
			{
				m_connectedPlayerUidsUtf8.emplace(payload);
			}
			return true;
		}

		if (channel == PlayerLeftChannelUtf8)
		{
			if (!payload.empty())
			{
				m_connectedPlayerUidsUtf8.erase(std::string(payload));
			}
			return true;
		}

		return false;
	}

	std::string PlayerList::generateUidUtf8()
	{
		const std::uint64_t a = s3d::RandomUint64();
		const std::uint64_t b = s3d::RandomUint64();

		static const sqidscxx::SqidsOptions sqidsOptions{
			.minLength = 8,
		};
		static const sqidscxx::Sqids<std::uint64_t> sqids{ sqidsOptions };

		return sqids.encode(std::vector<std::uint64_t>{ a, b });
	}

	void PlayerList::updateSession(redisAsyncContext* context)
	{
		assert(context);

		// SET <key> 1 EX <ttl> GET
		const static std::string_view value = "1";
		const std::string ttl = std::to_string(m_options.sessionTtlMs);

		const char* argv[7];
		size_t argvlen[7];
		argv[0] = "SET";      argvlen[0] = 3;
		argv[1] = m_sessionKeyUtf8.data();
		argvlen[1] = m_sessionKeyUtf8.size();
		argv[2] = value.data();
		argvlen[2] = value.size();
		argv[3] = "PX";       argvlen[3] = 2;
		argv[4] = ttl.data(); argvlen[4] = ttl.size();
		argv[5] = "GET";      argvlen[5] = 3;
		argv[6] = nullptr;    argvlen[6] = 0;

		const int rc = redisAsyncCommandArgv(
			context,
			reinterpret_cast<redisCallbackFn*>(PlayerList::onSessionUpdateCallback),
			this,
			6, argv, argvlen
		);

		if (rc == REDIS_OK)
		{
			m_sessionUpdatetInFlight = true;
		}
		else
		{
			s3d::Logger << U"[MessageBus][ERROR] Failed to update the player session: Command execution was failed";
			m_sessionStatus = SessionStatus::Error;
		}
	}

	void PlayerList::onSessionUpdateCallback(redisAsyncContext* context, redisReply* reply, PlayerList* self)
	{
		if (!self)
		{
			return;
		}

		self->m_sessionUpdatetInFlight = false;

		if (!reply)
		{
			return;
		}

		if (reply->type == REDIS_REPLY_ERROR)
		{
			std::string_view message{ reply->str, reply->len };
			s3d::Logger
				<< U"[MessageBus][ERROR] Failed to update the player session: "
				<< s3d::Unicode::FromUTF8(message);
			
			self->m_sessionStatus = SessionStatus::Error;

			return;
		}

		self->m_sessionStatus = SessionStatus::Active;

		// If GET was specified, one of the following:
		// - Null reply:
		//   The key didn't exist before the SET operation,
		//   whether the key was created of not.
		// - Bulk string reply:
		//   The previous value of the key, whether the key was set or not.

		if (reply->type == REDIS_REPLY_NIL)
		{
			self->publishPlayerJoin(context);
		}
	}

	void PlayerList::publishPlayerJoin(redisAsyncContext* context)
	{
		if (!context) { return; }

		const char* argv[3];
		size_t argvlen[3];
		argv[0] = "PUBLISH";
		argvlen[0] = 7;
		argv[1] = PlayerJoinChannelUtf8.data();
		argvlen[1] = PlayerJoinChannelUtf8.size();
		argv[2] = m_uidUtf8.data();
		argvlen[2] = m_uidUtf8.size();

		redisAsyncCommandArgv(
			context,
			nullptr,
			nullptr,
			3, argv, argvlen
		);
	}

	void PlayerList::publishPlayerLeft(redisAsyncContext* context)
	{
		if (!context) { return; }

		const char* argv[3];
		size_t argvlen[3];
		argv[0] = "PUBLISH";
		argvlen[0] = 7;
		argv[1] = PlayerLeftChannelUtf8.data();
		argvlen[1] = PlayerLeftChannelUtf8.size();
		argv[2] = m_uidUtf8.data();
		argvlen[2] = m_uidUtf8.size();

		redisAsyncCommandArgv(
			context,
			nullptr,
			nullptr,
			3, argv, argvlen
		);
	}

	void PlayerList::deleteSession(redisAsyncContext* context)
	{
		assert(context);

		const char* argv[2];
		size_t argvlen[2];
		argv[0] = "DEL";
		argvlen[0] = 3;
		argv[1] = m_sessionKeyUtf8.data();
		argvlen[1] = m_sessionKeyUtf8.size();

		redisAsyncCommandArgv(
			context,
			nullptr,
			nullptr,
			2, argv, argvlen
		);
	}

	void PlayerList::fetchPlayerList(redisAsyncContext* context)
	{
		if (!context) { return; }

		const char* argv[2];
		size_t argvlen[2];
		argv[0] = "KEYS";
		argvlen[0] = 4;
		argv[1] = PlayerKeysPattern.data();
		argvlen[1] = PlayerKeysPattern.size();

		const int rc = redisAsyncCommandArgv(
			context,
			reinterpret_cast<redisCallbackFn*>(PlayerList::fetchPlayerListCallback),
			this,
			2, argv, argvlen
		);

		if (rc == REDIS_OK)
		{
			m_refreshInFlight = true;
		}
		else
		{
			s3d::Logger << U"[MessageBus][ERROR] Failed to fetch connected player list: Command execution was failed";
		}
	}

	void PlayerList::fetchPlayerListCallback(redisAsyncContext* context, redisReply* reply, PlayerList* self)
	{
		if (!self)
		{
			return;
		}

		self->m_refreshInFlight = false;

		if (!reply)
		{
			return;
		}

		if (reply->type == REDIS_REPLY_ERROR)
		{
			std::string_view message{ reply->str, reply->len };
			s3d::Logger
				<< U"[MessageBus][ERROR] Failed to fetch connected player list: "
				<< s3d::Unicode::FromUTF8(message);
			return;
		}

		if (reply->type != REDIS_REPLY_ARRAY)
		{
			return;
		}

		self->m_connectedPlayerUidsUtf8.clear();
		self->m_connectedPlayerUidsUtf8.reserve(static_cast<size_t>(reply->elements));

		for (size_t i = 0; i < static_cast<size_t>(reply->elements); ++i)
		{
			redisReply* elem = reply->element[i];
			if (!elem || elem->type != REDIS_REPLY_STRING || !elem->str || elem->len == 0)
			{
				continue;
			}

			std::string_view key{ elem->str, static_cast<size_t>(elem->len) };
			if (!key.starts_with(PlayerKeyPrefix))
			{
				continue;
			}

			std::string_view uid = key.substr(PlayerKeyPrefix.size());
			self->m_connectedPlayerUidsUtf8.emplace(uid);
		}

		self->m_timeSinceLastRefresh.restart();
	}
}
