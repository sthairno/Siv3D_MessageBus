#include "MessageBus/SharedVariable.hpp"
#include <Siv3D/Unicode.hpp>

using namespace s3d;

namespace MessageBus
{
	template<class Type>
	SharedVariable<Type>::SharedVariable(std::shared_ptr<SharedVariableImpl> impl)
		: m_impl(std::move(impl))
	{
	}

	template<class Type>
	const s3d::String& SharedVariable<Type>::name() const
	{
		return m_impl->u32name();
	}

	template<class Type>
	void SharedVariable<Type>::set(const Type& value)
	{
		m_impl->setValueAsJSON(JSON(value));
	}

	template<class Type>
	Type SharedVariable<Type>::get() const
	{
		const auto& jsonValue = m_impl->valueAsJSON();
		const auto optValue = jsonValue.getOpt<Type>();

		if (not optValue.has_value())
		{
			throw TypeConversionError(U"Failed to convert JSON value to requested type: {}"_fmt(m_impl->u32name()));
		}

		return optValue.value();
	}

	template<>
	s3d::JSON SharedVariable<s3d::JSON>::get() const
	{
		return m_impl->valueAsJSON();
	}

	template<class Type>
	DateTime SharedVariable<Type>::updatedAt() const
	{
		return m_impl->updatedAt();
	}

	template class SharedVariable<s3d::int32>;
	template class SharedVariable<double>;
	template class SharedVariable<bool>;
	template class SharedVariable<s3d::String>;
	template class SharedVariable<s3d::JSON>;
}
