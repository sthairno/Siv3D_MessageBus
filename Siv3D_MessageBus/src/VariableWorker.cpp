#include "MessageBus/detail/VariableWorker.hpp"

#include "MessageBus/detail/RedisConnection.hpp"
#include "MessageBus/detail/SharedVariableImpl.hpp"

#include <Siv3D/Logger.hpp>
#include <Siv3D/Unicode.hpp>
#include <utility>

namespace MessageBus::detail
{
	VariableWorker::VariableWorker() = default;

	void VariableWorker::onConnect(RedisConnection& conn)
	{
		syncAll(conn.context());
		m_isReady = m_data.empty();
	}

	void VariableWorker::beforeTick(RedisConnection& conn)
	{
		if (conn.state() == RedisConnectionState::Connected)
		{
			syncAll(conn.context());
		}
	}

	void VariableWorker::afterTick()
	{
		if (not m_isReady)
		{
			m_isReady = true;
			for (auto& [_, impl] : m_data)
			{
				if (not impl->isInitialized() || impl->isSending())
				{
					m_isReady = false;
					break;
				}
			}
		}
	}

	void VariableWorker::beforeDisconnect(RedisConnection&)
	{
	}

	void VariableWorker::onDisconnect()
	{
		resetAll();
		m_isReady = false;
	}

	void VariableWorker::onInvalidate(redisAsyncContext* context, const s3d::Array<std::string>& keys)
	{
		if (!context)
		{
			return;
		}

		for (const auto& key : keys)
		{
			auto itr = m_data.find(key);
			if (itr != m_data.end())
			{
				s3d::Logger << U"[MessageBus][DEBUG] Invalidated key found, refreshing: " << s3d::Unicode::FromUTF8(key);
				itr->second->fetchFromRemote(context);
			}
		}
	}

	bool VariableWorker::handlePubSubMessage(std::string_view, std::string_view)
	{
		return false;
	}

	void VariableWorker::resetAll()
	{
		for (auto& [name, varImpl] : m_data)
		{
			varImpl->reset();
		}
	}

	void VariableWorker::syncAll(redisAsyncContext* context)
	{
		for (auto& [name, varImpl] : m_data)
		{
			varImpl->syncToRemote(context);
		}
	}

	std::shared_ptr<SharedVariableImpl> VariableWorker::getOrCreateVariable(std::string_view u8name, s3d::StringView u32name, const s3d::JSON& initialValue)
	{
		std::string key{ u8name };
		auto itr = m_data.find(key);
		if (itr != m_data.end())
		{
			return itr->second;
		}

		auto varImpl = std::make_shared<SharedVariableImpl>(u8name, u32name, initialValue);
		m_data.emplace(std::move(key), varImpl);
		return varImpl;
	}
}

