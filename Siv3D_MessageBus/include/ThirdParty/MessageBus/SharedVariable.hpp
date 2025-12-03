#pragma once

#include "SharedVariableImpl.hpp"
#include <memory>
#include <Siv3D/String.hpp>
#include <Siv3D/DateTime.hpp>
#include <Siv3D/JSON.hpp>
#include <Siv3D/Error.hpp>

namespace MessageBus
{
	/// @brief 型変換エラーを表現する型
	class TypeConversionError final : public s3d::Error
	{
	public:
		using Error::Error;

		[[nodiscard]]
		s3d::StringView type() const noexcept override
		{
			return U"TypeConversionError";
		}
	};

	/// @brief 共有変数を表すテンプレートクラス
	/// @tparam Type 変数の型（int32, double, bool, String, JSON のみサポート）
	template<class Type>
	class SharedVariable
	{
	public:
		/// @brief 変数名を取得します
		/// @return 変数名
		[[nodiscard]]
		const s3d::String& name() const;

		/// @brief 値を設定します
		/// @param value 設定する値
		void set(const Type& value);

		/// @brief 値を取得します
		/// @return 現在の値
		[[nodiscard]]
		Type get() const;

		/// @brief 最終更新日時を取得します（後フェーズで実装）
		/// @return 最終更新日時
		[[nodiscard]]
		s3d::DateTime updatedAt() const;

	private:
		friend class MessageBus;

		explicit SharedVariable(std::shared_ptr<SharedVariableImpl> impl);

		std::shared_ptr<SharedVariableImpl> m_impl;
	};
}
