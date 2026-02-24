#include "MessageBus/InvalidNameError.hpp"

using namespace s3d;

namespace MessageBus
{
	InvalidNameError::InvalidNameError(StringView message)
		: Error(message)
	{
	}

	StringView InvalidNameError::type() const noexcept
	{
		return U"InvalidNameError";
	}
}
