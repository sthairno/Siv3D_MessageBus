#include "MessageBus/detail/SubscriptionWorker.hpp"

#include "MessageBus/detail/RedisConnection.hpp"

#include <Siv3D/JSON.hpp>
#include <Siv3D/Unicode.hpp>

#include <utility>
#include <vector>

extern "C" {
#include <hiredis/async.h>
}

namespace MessageBus::detail
{
	SubscriptionWorker::SubscriptionWorker() = default;

	void SubscriptionWorker::onConnect(RedisConnection& conn)
	{
		syncSubscriptions(conn.context());
	}

	void SubscriptionWorker::beforeTick(RedisConnection& conn)
	{
		if (conn.state() == RedisConnectionState::Connected &&
			m_channelsDirty)
		{
			syncSubscriptions(conn.context());
		}
	}

	void SubscriptionWorker::afterTick()
	{
	}

	void SubscriptionWorker::beforeDisconnect(RedisConnection&)
	{
	}

	void SubscriptionWorker::onDisconnect()
	{
		m_subscribeAckReceived = false;
		markAllUnsubscribed();
	}

	void SubscriptionWorker::setPubSubMessageHandler(PubSubMessageHandler handler)
	{
		m_externalPubSubMessageHandler = std::move(handler);
	}

	bool SubscriptionWorker::handlePubSubMessage(std::string_view channel, std::string_view payload)
	{
		auto channelItr = m_channels.find(channel);
		if (channelItr == m_channels.end() ||
			!channelItr->second.desired)
		{
			return false;
		}

		m_eventsBuf.emplace_back(MessageBus::Event{
			.channel = s3d::Unicode::FromUTF8(channel),
			.value = payload.empty() ? s3d::JSON::Invalid() : s3d::JSON::Parse(s3d::Unicode::FromUTF8(payload))
		});
		return true;
	}

	void SubscriptionWorker::clearEventsBuffer()
	{
		m_eventsBuf.clear();
	}

	bool SubscriptionWorker::subscribe(s3d::StringView channel)
	{
		const std::string u8channel = s3d::Unicode::ToUTF8(channel);
		auto channelItr = m_channels.find(u8channel);
		if (channelItr == m_channels.end())
		{
			m_channels.emplace(
				u8channel,
				ChannelState{
					.desired = true,
					.remote = false
				}
			);
			m_channelsDirty = true;
		}
		else
		{
			m_channelsDirty |= channelItr->second.desired == false;
			channelItr->second.desired = true;
		}

		return true;
	}

	bool SubscriptionWorker::unsubscribe(s3d::StringView channel)
	{
		const std::string u8channel = s3d::Unicode::ToUTF8(channel);
		auto channelItr = m_channels.find(u8channel);
		if (channelItr == m_channels.end())
		{
			return false;
		}

		if (not channelItr->second.desired)
		{
			return false;
		}

		m_channelsDirty = true;
		channelItr->second.desired = false;
		return true;
	}

	void SubscriptionWorker::onSubscriptionMessageReceive(redisAsyncContext*, redisReply* reply, SubscriptionWorker* self)
	{
		if (!self ||
			!reply ||
			reply->type != REDIS_REPLY_PUSH ||
			reply->elements < 3)
		{
			return;
		}

		redisReply* kindElem = reply->element[0];
		redisReply* channelElem = reply->element[1];
		redisReply* payloadElem = reply->element[2];
		if (!kindElem ||
			kindElem->type != REDIS_REPLY_STRING ||
			!channelElem ||
			channelElem->type != REDIS_REPLY_STRING ||
			!payloadElem)
		{
			return;
		}

		const std::string_view kind{ kindElem->str, kindElem->len };
		const std::string_view channelName{ channelElem->str, channelElem->len };

		if (kind == "subscribe")
		{
			self->m_subscribeAckReceived = true;
		} else if (kind == "message" && payloadElem->type == REDIS_REPLY_STRING) {
			const std::string_view payload{ payloadElem->str, payloadElem->len };

			if (self->m_externalPubSubMessageHandler &&
				self->m_externalPubSubMessageHandler(channelName, payload))
			{
				return;
			}
			self->handlePubSubMessage(channelName, payload);
		}
	}

	void SubscriptionWorker::markAllUnsubscribed()
	{
		for (auto& [key, st] : m_channels)
		{
			st.remote = false;
		}
		m_channelsDirty = true;
	}

	void SubscriptionWorker::syncSubscriptions(redisAsyncContext* context)
	{
		if (!context)
		{
			return;
		}

		std::vector<std::string_view> subscribeCommand{ {"SUBSCRIBE"} };
		std::vector<std::string_view> unsubscribeCommand{ {"UNSUBSCRIBE"} };
		for (const auto& [key, st] : m_channels)
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

		// 購読対象が無い場合、Redis から subscribe ACK は来ないため ready 扱いにする
		m_subscribeAckReceived |= (subscribeCommand.size() == 1);

		auto sendCommand = [this](redisAsyncContext* context, redisCallbackFn* callback, const std::vector<std::string_view>& args) {
			if (args.size() == 1)
			{
				return;
			}

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
		sendCommand(context, reinterpret_cast<redisCallbackFn*>(SubscriptionWorker::onSubscriptionMessageReceive), subscribeCommand);
		sendCommand(context, nullptr, unsubscribeCommand);

		for (auto& [key, st] : m_channels)
		{
			st.remote = st.desired;
		}
		m_channelsDirty = false;
	}
}
