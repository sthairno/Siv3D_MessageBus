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
	const Type& SharedVariable<Type>::get() const
	{
		// 後フェーズで実装
		static Type dummy{};
		return dummy;
	}

	template<class Type>
	DateTime SharedVariable<Type>::updatedAt() const
	{
		return m_impl->updatedAt();
	}

	template<class Type>
	void SharedVariable<Type>::set(const Type& value)
	{
		m_impl->setValueAsJSON(JSON(value));
	}

	template<>
	void SharedVariable<JSON>::set(const JSON& value)
	{
		m_impl->setValueAsJSON(value);
	}

	template class SharedVariable<s3d::int32>;
	template class SharedVariable<double>;
	template class SharedVariable<bool>;
	template class SharedVariable<s3d::String>;
	template class SharedVariable<s3d::JSON>;
}
