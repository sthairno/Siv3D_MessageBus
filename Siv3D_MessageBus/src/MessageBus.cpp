#include "MessageBus/MessageBus.hpp"
#include "MessageBus/ConnectNotAllowedError.hpp"
#include "MessageBus/InvalidNameError.hpp"
#include "MessageBus/detail/RedisConnection.hpp"
#include "MessageBus/SharedVariable.hpp"
#include "MessageBus/detail/SharedVariableImpl.hpp"
#include "MessageBus/detail/PlayerList.hpp"
#include "MessageBus/detail/SubscriptionWorker.hpp"
#include "MessageBus/detail/VariableWorker.hpp"
#include <Siv3D/Logger.hpp>
#include <Siv3D/Unicode.hpp>
#include <Siv3D/FormatLiteral.hpp>
#include <memory>
#include <thread>

extern "C" {
#include <hiredis/async.h>
}

using namespace s3d;

namespace MessageBus
{
	static StringView SystemKeyPrefix = U"s3d-mbus:";

	static bool ValidateVariableName(StringView name)
	{
		return 
			not name.empty() && 
			not name.starts_with(SystemKeyPrefix);
	}

	static bool ValidateChannelName(StringView channel)
	{
		return
			not channel.empty() && 
			not channel.starts_with(SystemKeyPrefix);
	}

	struct MessageBus::Impl
	{
		std::unique_ptr<detail::RedisConnection> conn;

		detail::SubscriptionWorker subscriptionWorker;
		detail::PlayerList playerList{{ }};
		detail::VariableWorker variableWorker;

		String playerIdUtf32;

		Array<String> playerListUtf32;

		Impl()
		{
			setupSubscriptionWorker();
		}

		Impl(s3d::StringView ip, s3d::uint16 port, s3d::Optional<s3d::StringView> password)
		{
			setupSubscriptionWorker();
			createConnection(ip, port, password);
		}

		~Impl()
		{
			conn.reset();
		}

		void setupSubscriptionWorker()
		{
			subscriptionWorker.setPubSubMessageHandler([this](std::string_view channel, std::string_view payload) {
				if (playerList.handlePubSubMessage(channel, payload))
				{
					return true;
				}

				if (variableWorker.handlePubSubMessage(channel, payload))
				{
					return true;
				}

				return false;
			});
		}

		void createConnection(s3d::StringView ip, s3d::uint16 port, s3d::Optional<s3d::StringView> password)
		{
			conn = std::make_unique<detail::RedisConnection>(detail::RedisConnectionOptions{
				.ip = ip,
				.port = port,
				.password = password,
				.heartbeatInterval = s3d::Seconds{ 10 },
				.onConnect = nullptr,
				.onReady = [this](redisAsyncContext*) {
					subscriptionWorker.onConnect(*conn);
					playerList.onConnect(*conn);
					variableWorker.onConnect(*conn);
				},
				.onDisconnect = [this]() {
					subscriptionWorker.onDisconnect();
					playerList.onDisconnect();
					variableWorker.onDisconnect();
				},
				.onInvalidate = [this](redisAsyncContext* context, const s3d::Array<std::string>& keys) {
					variableWorker.onInvalidate(context, keys);
				}
			});
		}

		void syncPlayerList()
		{
			const auto& src = playerList.connectedPlayerUidsUtf8();
			auto& dst = playerListUtf32;

			dst.clear();
			dst.reserve(src.size());
			for (std::string_view utf8 : src)
			{
				dst.push_back(Unicode::FromUTF8(utf8));
			}
		}

		static void onPublishCallback(redisAsyncContext*, redisReply* reply, Impl*)
		{
			if (!reply) return;
			if (reply->type == REDIS_REPLY_INTEGER)
			{
				Logger << U"[MessageBus][INFO] PUBLISH delivered=" << reply->integer;
			}
			else if (reply->type == REDIS_REPLY_ERROR)
			{
				Logger << U"[MessageBus][ERROR] PUBLISH failed: " << Unicode::FromUTF8(std::string_view{ reply->str, reply->len });
			}
		}

		bool emit(StringView channel, Optional<JSON> payload)
		{
			if (not conn)
			{
				return false;
			}

			if (conn->state() != detail::RedisConnectionState::Connected)
			{
				return false;
			}

			auto* context = conn->context();
			if (!context)
			{
				return false;
			}

			const std::string u8channel = Unicode::ToUTF8(channel);
			const std::string payloadJson = payload.has_value()
				? payload->formatUTF8Minimum()
				: std::string{};

			const char* argv[3];
			size_t argvlen[3];
			argv[0] = "PUBLISH";           argvlen[0] = 7;
			argv[1] = u8channel.c_str();   argvlen[1] = u8channel.size();
			argv[2] = payloadJson.c_str(); argvlen[2] = payloadJson.size();

			const int rc = redisAsyncCommandArgv(
				context,
				reinterpret_cast<redisCallbackFn*>(Impl::onPublishCallback),
				this,
				3, argv, argvlen
			);
			return (rc == REDIS_OK);
		}

	};

	MessageBus::MessageBus()
		: m_impl(std::make_unique<Impl>())
	{
	}

	MessageBus::MessageBus(s3d::StringView ip, s3d::uint16 port, s3d::Optional<s3d::StringView> password)
		: m_impl(std::make_unique<Impl>(ip, port, password))
	{
	}

	MessageBus::~MessageBus() = default;

	void MessageBus::connect(s3d::StringView ip, s3d::uint16 port, s3d::Optional<s3d::StringView> password)
	{
		if (m_impl->conn)
		{
			throw ConnectNotAllowedError();
		}

		m_impl->createConnection(ip, port, password);
	}

	void MessageBus::disconnect()
	{
		if (!m_impl->conn)
		{
			return;
		}

		m_impl->subscriptionWorker.beforeDisconnect(*m_impl->conn);
		m_impl->playerList.beforeDisconnect(*m_impl->conn);
		m_impl->variableWorker.beforeDisconnect(*m_impl->conn);
		m_impl->conn->disconnect();
	}

	void MessageBus::shutdown()
	{
		if (!m_impl->conn)
		{
			return;
		}

		auto state = m_impl->conn->state();
		if (state != detail::RedisConnectionState::Disconnected &&
			state != detail::RedisConnectionState::Disconnecting)
		{
			disconnect();
		}

		while (m_impl->conn->state() != detail::RedisConnectionState::Disconnected)
		{
			std::this_thread::yield();
			m_impl->subscriptionWorker.beforeTick(*m_impl->conn);
			m_impl->playerList.beforeTick(*m_impl->conn);
			m_impl->variableWorker.beforeTick(*m_impl->conn);
			m_impl->conn->tick();
			m_impl->subscriptionWorker.afterTick();
			m_impl->playerList.afterTick();
			m_impl->variableWorker.afterTick();
		}
	}

	void MessageBus::update()
	{
		m_impl->subscriptionWorker.clearEventsBuffer();

		if (!m_impl->conn)
		{
			return;
		}

		m_impl->subscriptionWorker.beforeTick(*m_impl->conn);
		m_impl->playerList.beforeTick(*m_impl->conn);
		m_impl->variableWorker.beforeTick(*m_impl->conn);
		m_impl->conn->tick();
		m_impl->subscriptionWorker.afterTick();
		m_impl->playerList.afterTick();
		m_impl->variableWorker.afterTick();

		if (!m_impl->playerList.addedPlayerUidsUtf8().empty() ||
			!m_impl->playerList.removedPlayerUidsUtf8().empty())
		{
			m_impl->syncPlayerList();
		}
	}

	bool MessageBus::isConnected() const
	{
		if (!m_impl->conn)
		{
			return false;
		}

		if (m_impl->conn->state() != detail::RedisConnectionState::Connected)
		{
			return false;
		}

		// 前準備が完了するまで connected 扱いにしない
		return m_impl->subscriptionWorker.isReady()
			&& m_impl->playerList.isReady()
			&& m_impl->variableWorker.isReady();
	}

	bool MessageBus::isDisconnecting() const
	{
		if (!m_impl->conn)
		{
			return false;
		}
		return m_impl->conn->state() == detail::RedisConnectionState::Disconnecting;
	}

	const s3d::String& MessageBus::error() const
	{
		if (!m_impl->conn)
		{
			static const s3d::String empty;
			return empty;
		}
		return m_impl->conn->error();
	}

	bool MessageBus::subscribe(s3d::StringView channel)
	{
		if (not ValidateChannelName(channel))
		{
			Logger << U"[MessageBus][ERROR] Invalid channel name: \"" << channel << U"\"";
			throw InvalidNameError(UR"(Invalid channel name: "{0}")"_fmt(channel));
		}

		return m_impl->subscriptionWorker.subscribe(channel);
	}

	bool MessageBus::unsubscribe(s3d::StringView channel)
	{
		if (not ValidateChannelName(channel))
		{
			Logger << U"[MessageBus][ERROR] Invalid channel name: \"" << channel << U"\"";
			throw InvalidNameError(UR"(Invalid channel name: "{0}")"_fmt(channel));
		}

		return m_impl->subscriptionWorker.unsubscribe(channel);
	}

	const s3d::Array<MessageBus::Event>& MessageBus::events() const
	{
		return m_impl->subscriptionWorker.events();
	}

	bool MessageBus::emit(s3d::StringView channel, s3d::Optional<s3d::JSON> payload)
	{
		if (not ValidateChannelName(channel))
		{
			Logger << U"[MessageBus][ERROR] Invalid channel name: \"" << channel << U"\"";
			throw InvalidNameError(UR"(Invalid channel name: "{0}")"_fmt(channel));
		}

		return m_impl->emit(channel, payload);
	}

	template<class Type>
	SharedVariable<Type> MessageBus::variable(s3d::StringView name, const Type& defaultValue)
	{
		if (not ValidateVariableName(name))
		{
			Logger << U"[MessageBus][ERROR] Invalid variable name: \"" << name << U"\"";
			throw InvalidNameError(UR"(Invalid variable name: "{0}")"_fmt(name));
		}

		const std::string u8name = Unicode::ToUTF8(name);
		const JSON initialJson(defaultValue);
		auto varImpl = m_impl->variableWorker.getOrCreateVariable(u8name, name, initialJson);
		return varImpl->asSharedVariable<Type>();
	}

	// 明示的インスタンス化
	template SharedVariable<int32> MessageBus::variable<int32>(s3d::StringView, const int32&);
	template SharedVariable<double> MessageBus::variable<double>(s3d::StringView, const double&);
	template SharedVariable<bool> MessageBus::variable<bool>(s3d::StringView, const bool&);
	template SharedVariable<String> MessageBus::variable<String>(s3d::StringView, const String&);
	template SharedVariable<JSON> MessageBus::variable<JSON>(s3d::StringView, const JSON&);

	const String& MessageBus::id() const
	{
		if (not m_impl->playerIdUtf32.empty())
		{
			return m_impl->playerIdUtf32;
		}

		m_impl->playerIdUtf32 = Unicode::FromUTF8(m_impl->playerList.uidUtf8());
		return m_impl->playerIdUtf32;
	}

	const Array<String>& MessageBus::onlineIdList() const
	{
		return m_impl->playerListUtf32;
	}
}
