#pragma once

#include "SharedVariableImpl.hpp"
#include "TypeMismatchError.hpp"

#include <memory>
#include <Siv3D/String.hpp>
#include <Siv3D/DateTime.hpp>
#include <Siv3D/JSON.hpp>

namespace MessageBus
{
	/// @brief 共有変数を表すテンプレートクラス
	/// @tparam Type 変数の型（int32, double, bool, String, JSON のみサポート）
	template<class Type>
	class SharedVariable
	{
	public:
		static_assert(
			std::is_same_v<Type, s3d::int32> ||
			std::is_same_v<Type, double> ||
			std::is_same_v<Type, bool> ||
			std::is_same_v<Type, s3d::String> ||
			std::is_same_v<Type, s3d::JSON>,
			"Type must be int32, double, bool, String, or JSON"
		);

		explicit SharedVariable(std::shared_ptr<SharedVariableImpl> impl);

	private:
		friend class MessageBus;

		std::shared_ptr<SharedVariableImpl> m_impl;

	public:
		/// @brief 変数名を取得します
		/// @return 変数名
		[[nodiscard]]
		const s3d::String& name() const;

		/// @brief 値を設定します
		/// @param value 設定する値
		void set(const Type& value);

		/// @brief 値を取得します
		/// @throws TypeMismatchError Redisに保存されている型とTypeが一致していない場合
		/// @return 現在の値
		[[nodiscard]]
		Type get() const;

		/// @brief 最終更新日時を取得します（後フェーズで実装）
		/// @return 最終更新日時
		[[nodiscard]]
		s3d::DateTime updatedAt() const;

		~SharedVariable();
	};
}
