#include "MessageBus/MessageBus.hpp"
#include "MessageBus/detail/RedisConnection.hpp"
#include "MessageBus/SharedVariable.hpp"
#include "MessageBus/detail/SharedVariableImpl.hpp"
#include <Siv3D/Logger.hpp>
#include <Siv3D/Unicode.hpp>
#include <Siv3D/HashTable.hpp>
#include <memory>

extern "C" {
#include <hiredis/async.h>
}

using namespace s3d;

namespace MessageBus
{
	static bool ValidateChannelName(StringView channel)
	{
		return not channel.empty();
	}

	struct MessageBus::Impl
	{
		detail::RedisConnection conn;

		struct ChannelState
		{
			bool desired = false; // ユーザーの購読意図
			bool remote = false;  // サーバー側で購読確定
		};

		s3d::HashTable<std::string, ChannelState> channels;
		bool channelsDirty = false;

		s3d::Array<MessageBus::Event> eventsBuf;

		s3d::HashTable<std::string, std::shared_ptr<detail::SharedVariableImpl>> variables;

		Impl(s3d::StringView ip, s3d::uint16 port, s3d::Optional<s3d::StringView> password)
			: conn(detail::RedisConnectionOptions{
				.ip = ip,
				.port = port,
				.password = password,
				.heartbeatInterval = s3d::Seconds{ 10 },
				.onConnect = nullptr,
				.onReady = [this](redisAsyncContext* context) {
					reconcileSubscriptions(context);
					syncVariables(context);
				},
				.onDisconnect = [this]() {
					markAllUnsubscribed();
					resetAllVariables();
				},
				.onInvalidate = [this](redisAsyncContext* context, const s3d::Array<std::string>& keys) {
					handleInvalidate(context, keys);
				}
			})
		{
		}

		void clearEventsBuffer()
		{
			eventsBuf.clear();
		}

		static void onSubscriptionMessageReceive(redisAsyncContext*, redisReply* reply, Impl* self)
		{
			// 事前条件チェック
			if (!reply || reply->type != REDIS_REPLY_PUSH || reply->elements < 3) return;

			// 型チェック
			redisReply* kindElem = reply->element[0];
			redisReply* channelElem = reply->element[1];
			redisReply* payloadElem = reply->element[2];
			if (!kindElem ||
				kindElem->type != REDIS_REPLY_STRING ||
				!channelElem ||
				channelElem->type != REDIS_REPLY_STRING ||
				!payloadElem ||
				payloadElem->type != REDIS_REPLY_STRING)
			{
				return;
			}

			const std::string_view kind{ kindElem->str, kindElem->len };
			const std::string_view channelName{ channelElem->str, channelElem->len };
			const std::string_view payload{ payloadElem->str, payloadElem->len };

			// メッセージのみ処理
			if (kind != "message")
			{
				return;
			}

			// 購読中のチャンネルのみ処理
			auto channelItr = self->channels.find(channelName);
			if (channelItr == self->channels.end() ||
				!channelItr->second.desired)
			{
				return;
			}

			// イベントバッファに追加（空/失敗時は Invalid）
			self->eventsBuf.emplace_back(MessageBus::Event{
				.channel = Unicode::FromUTF8(channelName),
				.value = payload.empty() ? JSON::Invalid() : JSON::Parse(Unicode::FromUTF8(payload))
			});
		}

		void markAllUnsubscribed()
		{
			for (auto& [key, st] : channels)
			{
				st.remote = false;
			}
			channelsDirty = true;
		}

		void reconcileSubscriptions(redisAsyncContext* context)
		{
			if (!context) return;

			// コマンド構築
			std::vector<std::string_view> subscribeCommand{ {"SUBSCRIBE"} };
			std::vector<std::string_view> unsubscribeCommand{ {"UNSUBSCRIBE"} };
			for (const auto& [key, st] : channels)
			{
				if (st.desired && !st.remote)
				{
					subscribeCommand.push_back(key);
				}
				if (!st.desired && st.remote)
				{
					unsubscribeCommand.push_back(key);
				}
			}

			// コマンド送信
			auto sendCommand = [&, this](redisAsyncContext* context, redisCallbackFn* callback, const std::vector<std::string_view>& args) {
				if (args.size() == 1) return;
				const int argc = static_cast<int>(args.size());
				std::vector<const char*> argv(argc);
				std::vector<size_t> argvlen(argc);
				for (size_t i = 0; i < args.size(); ++i)
				{
					argv[i] = args[i].data();
					argvlen[i] = args[i].size();
				}
				redisAsyncCommandArgv(context, callback, this, argc, argv.data(), argvlen.data());
				};
			sendCommand(context, reinterpret_cast<redisCallbackFn*>(Impl::onSubscriptionMessageReceive), subscribeCommand);
			sendCommand(context, nullptr, unsubscribeCommand); // 失敗しても購読していないイベントはフィルターできるため無視

			// 状態を最新の状態に更新
			for (auto& [key, st] : channels)
			{
				st.remote = st.desired;
			}
			channelsDirty = false;
		}

		void resetAllVariables()
		{
			for (auto& [name, varImpl] : variables)
			{
				varImpl->reset();
			}
		}

		void syncVariables(redisAsyncContext* context)
		{
			for (auto& [name, varImpl] : variables)
			{
				varImpl->syncToRemote(context);
			}
		}

		void handleInvalidate(redisAsyncContext* context, const s3d::Array<std::string>& keys)
		{
			if (!context) return;

			for (const auto& key : keys)
			{
				auto varItr = variables.find(key);
				if (varItr != variables.end())
				{
					Logger << U"[MessageBus][DEBUG] Invalidated key found, refreshing: " << Unicode::FromUTF8(key);
					varItr->second->fetchFromRemote(context);
				}
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
			if (not ValidateChannelName(channel) ||
				conn.state() != detail::RedisConnectionState::Connected)
			{
				return false;
			}

			auto* context = conn.context();
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

		bool subscribe(StringView channel)
		{
			if (not ValidateChannelName(channel)) return false;

			// 購読している→成功
			// 購読していない→成功

			auto u8channel = Unicode::ToUTF8(channel);
			auto channelItr = channels.find(u8channel);
			if (channelItr == channels.end())
			{
				channels.emplace(
					u8channel,
					ChannelState{
						.desired = true,
						.remote = false
					}
				);
				channelsDirty = true;
			}
			else
			{
				channelsDirty |= channelItr->second.desired == false;
				channelItr->second.desired = true;
			}

			return true;
		}

		bool unsubscribe(StringView channel)
		{
			if (not ValidateChannelName(channel)) return false;

			// 購読している→成功
			// 購読していない→失敗

			auto u8channel = Unicode::ToUTF8(channel);
			auto channelItr = channels.find(u8channel);
			if (channelItr == channels.end())
			{
				return false;
			}

			if (not channelItr->second.desired)
			{
				return false;
			}

			channelsDirty = true;
			channelItr->second.desired = false;
			return true;
		}
	};

	MessageBus::MessageBus(s3d::StringView ip, s3d::uint16 port, s3d::Optional<s3d::StringView> password)
		: m_impl(std::make_unique<Impl>(ip, port, password))
	{
	}

	MessageBus::~MessageBus() = default;

	void MessageBus::close()
	{
		m_impl->conn.disconnect();
	}

	void MessageBus::tick()
	{
		m_impl->clearEventsBuffer();

		// conn.tick の直前に差分バッチ送信
		if (m_impl->conn.state() == detail::RedisConnectionState::Connected)
		{
			if (m_impl->channelsDirty)
			{
				m_impl->reconcileSubscriptions(m_impl->conn.context());
			}
			m_impl->syncVariables(m_impl->conn.context());
		}

		m_impl->conn.tick();
	}

	bool MessageBus::isConnected() const
	{
		return m_impl->conn.state() == detail::RedisConnectionState::Connected;
	}

	const s3d::String& MessageBus::error() const
	{
		return m_impl->conn.error();
	}

	bool MessageBus::subscribe(s3d::StringView channel)
	{
		return m_impl->subscribe(channel);
	}

	bool MessageBus::unsubscribe(s3d::StringView channel)
	{
		return m_impl->unsubscribe(channel);
	}

	const s3d::Array<MessageBus::Event>& MessageBus::events() const
	{
		return m_impl->eventsBuf;
	}

	bool MessageBus::emit(s3d::StringView channel, s3d::Optional<s3d::JSON> payload)
	{
		return m_impl->emit(channel, payload);
	}

	template<class Type>
	SharedVariable<Type> MessageBus::variable(s3d::StringView name, const Type& defaultValue)
	{
		if (name.empty())
		{
			Logger << U"[MessageBus][ERROR] variable name cannot be empty";
			throw std::invalid_argument("variable name cannot be empty");
		}

		const std::string u8name = Unicode::ToUTF8(name);
		auto& variables = m_impl->variables;
		auto varItr = variables.find(u8name);
		if (varItr != variables.end())
		{
			// 既存の変数を返す
			return SharedVariable<Type>(varItr->second);
		}

		// 新しい変数を作成
		const JSON initialJson(defaultValue);
		auto varImpl = std::make_shared<detail::SharedVariableImpl>(u8name, name, initialJson);
		variables.emplace(u8name, varImpl);

		Logger << U"[MessageBus][INFO] variable created: " << name;

		return SharedVariable<Type>(varImpl);
	}

	// 明示的インスタンス化
	template SharedVariable<int32> MessageBus::variable<int32>(s3d::StringView, const int32&);
	template SharedVariable<double> MessageBus::variable<double>(s3d::StringView, const double&);
	template SharedVariable<bool> MessageBus::variable<bool>(s3d::StringView, const bool&);
	template SharedVariable<String> MessageBus::variable<String>(s3d::StringView, const String&);
	template SharedVariable<JSON> MessageBus::variable<JSON>(s3d::StringView, const JSON&);
}
