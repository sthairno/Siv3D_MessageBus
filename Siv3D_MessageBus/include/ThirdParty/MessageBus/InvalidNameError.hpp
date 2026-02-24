#pragma once
#include <Siv3D/Error.hpp>
#include <Siv3D/String.hpp>
#include <Siv3D/StringView.hpp>

namespace MessageBus
{
	/// @brief 無効な名前（チャンネル名/変数名など）が指定されたときのエラー
	class InvalidNameError final : public s3d::Error
	{
	public:
		using Error::Error;

		explicit InvalidNameError(s3d::StringView message);

		[[nodiscard]]
		s3d::StringView type() const noexcept override;
	};
}
