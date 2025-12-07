#include "MessageBus/SharedVariableImpl.hpp"
#include <Siv3D/DateTime.hpp>

using namespace s3d;

namespace MessageBus
{
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
}
