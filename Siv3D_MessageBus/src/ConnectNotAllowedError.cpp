#include "MessageBus/ConnectNotAllowedError.hpp"

using namespace s3d;

namespace MessageBus
{
	ConnectNotAllowedError::ConnectNotAllowedError()
		: Error(U"connect() is not allowed because connection settings are already initialized")
	{
	}

	StringView ConnectNotAllowedError::type() const noexcept
	{
		return U"ConnectNotAllowedError";
	}
}


