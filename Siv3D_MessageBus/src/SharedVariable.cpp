#include "MessageBus/SharedVariable.hpp"
#include "MessageBus/detail/SharedVariableImpl.hpp"
#include "MessageBus/TypeMismatchError.hpp"

using namespace s3d;

namespace MessageBus
{
	template<class Type>
	SharedVariable<Type>::SharedVariable(std::shared_ptr<detail::SharedVariableImpl> impl)
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
		m_impl->markDirty();
	}

	template<class Type>
	Type SharedVariable<Type>::get() const
	{
		const auto& jsonValue = m_impl->valueAsJSON();
		const auto optValue = jsonValue.getOpt<Type>();

		if (not optValue.has_value())
		{
			StringView variableType, jsonType;
			if constexpr (std::is_same_v<Type, s3d::int32>)
			{
				variableType = U"Int32";
			}
			else if constexpr (std::is_same_v<Type, double>)
			{
				variableType = U"Double";
			}
			else if constexpr (std::is_same_v<Type, bool>)
			{
				variableType = U"Boolean";
			}
			else if constexpr (std::is_same_v<Type, s3d::String>)
			{
				variableType = U"String";
			}

			switch (jsonValue.getType())
			{
			case JSONValueType::Null: jsonType = U"Null"; break;
			case JSONValueType::Object: jsonType = U"Object"; break;
			case JSONValueType::Array: jsonType = U"Array"; break;
			case JSONValueType::String: jsonType = U"String"; break;
			case JSONValueType::Number:	jsonType = U"Number"; break;
			case JSONValueType::Bool: jsonType = U"Boolean"; break;
			default: jsonType = U"Unknown"; break;
			}

			throw TypeMismatchError(m_impl->u32name(), variableType, jsonType);
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

	template<class Type>
	SharedVariable<Type>::~SharedVariable()
	{
	}

	template class SharedVariable<s3d::int32>;
	template class SharedVariable<double>;
	template class SharedVariable<bool>;
	template class SharedVariable<s3d::String>;
	template class SharedVariable<s3d::JSON>;
}
