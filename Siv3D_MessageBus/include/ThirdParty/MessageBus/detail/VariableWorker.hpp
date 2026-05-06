#pragma once

#include <Siv3D/Array.hpp>
#include <Siv3D/HashTable.hpp>
#include <Siv3D/JSON.hpp>
#include <Siv3D/StringView.hpp>
#include <memory>
#include <string>
#include <string_view>

extern "C" {
struct redisAsyncContext;
}

namespace MessageBus::detail
{
	class RedisConnection;
	class SharedVariableImpl;

	class VariableWorker
	{
	public:
		VariableWorker();

		void onConnect(RedisConnection& conn);

		void beforeTick(RedisConnection& conn);

		void afterTick();

		void beforeDisconnect(RedisConnection& conn);

		void onDisconnect();

		void onInvalidate(redisAsyncContext* context, const s3d::Array<std::string>& keys);

		void resetAll();

		void syncAll(redisAsyncContext* context);

		std::shared_ptr<detail::SharedVariableImpl> getOrCreateVariable(std::string_view u8name, s3d::StringView u32name, const s3d::JSON& initialValue);

		[[nodiscard]]
		bool isReady() const noexcept { return m_isReady; }

	private:
		s3d::HashTable<std::string, std::shared_ptr<detail::SharedVariableImpl>> m_data;
		bool m_isReady = false;
	};
}

