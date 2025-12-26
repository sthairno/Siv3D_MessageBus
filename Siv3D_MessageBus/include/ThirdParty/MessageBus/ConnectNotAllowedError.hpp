#pragma once
#include <Siv3D/Error.hpp>

namespace MessageBus
{
	/// @brief connect() が許可されない状態で呼ばれたときのエラー
	class ConnectNotAllowedError final : public s3d::Error
	{
	public:
		using Error::Error;

		ConnectNotAllowedError();

		[[nodiscard]]
		s3d::StringView type() const noexcept override;
	};
}
