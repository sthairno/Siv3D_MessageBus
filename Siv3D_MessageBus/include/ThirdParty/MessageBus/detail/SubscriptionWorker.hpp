#pragma once

#include "MessageBus/MessageBus.hpp"

#include <Siv3D/Array.hpp>
#include <Siv3D/HashTable.hpp>
#include <Siv3D/StringView.hpp>

#include <functional>
#include <string>
#include <string_view>

extern "C" {
struct redisAsyncContext;
struct redisReply;
}

namespace MessageBus::detail
{
	class RedisConnection;

	class SubscriptionWorker
	{
	public:
		using PubSubMessageHandler = std::function<bool(std::string_view channel, std::string_view payload)>;

		SubscriptionWorker();

		void onConnect(RedisConnection& conn);

		void beforeTick(RedisConnection& conn);

		void afterTick();

		void beforeDisconnect(RedisConnection& conn);

		void onDisconnect();

		void setPubSubMessageHandler(PubSubMessageHandler handler);

		bool handlePubSubMessage(std::string_view channel, std::string_view payload);

		void clearEventsBuffer();

		bool subscribe(s3d::StringView channel);

		bool unsubscribe(s3d::StringView channel);

		[[nodiscard]]
		const s3d::Array<MessageBus::Event>& events() const noexcept { return m_eventsBuf; }

	private:
		struct ChannelState
		{
			bool desired = false; // ユーザーの購読意図
			bool remote = false;  // サーバー側で購読確定
		};

		s3d::HashTable<std::string, ChannelState> m_channels;
		bool m_channelsDirty = false;
		s3d::Array<MessageBus::Event> m_eventsBuf;
		PubSubMessageHandler m_externalPubSubMessageHandler;

		static void onSubscriptionMessageReceive(redisAsyncContext* context, redisReply* reply, SubscriptionWorker* self);

		void markAllUnsubscribed();

		void syncSubscriptions(redisAsyncContext* context);
	};
}
