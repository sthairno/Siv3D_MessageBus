#pragma once

#include <Siv3D/JSON.hpp>
#include <Siv3D/DateTime.hpp>
#include <string>

namespace MessageBus
{
	/// @brief SharedVariable の内部状態を管理するクラス
	class SharedVariableImpl
	{
	public:
		/// @brief コンストラクタ
		/// @param u8name 変数名（UTF-8）
		/// @param u32name 変数名（UTF-32）
		/// @param initialValue 初期値（JSON形式）
		SharedVariableImpl(std::string_view u8name, s3d::StringView u32name, const s3d::JSON& initialValue);

		/// @brief 変数名をUTF-8で取得します
		/// @return 変数名（UTF-8）
		[[nodiscard]]
		const std::string& u8name() const { return m_u8name; }

		/// @brief 変数名をUTF-32で取得します
		[[nodiscard]]
		const s3d::String& u32name() const { return m_u32name; }

		/// @brief JSON形式の値を取得します
		/// @return 現在の値（JSON形式）
		[[nodiscard]]
		const s3d::JSON& valueAsJSON() const;

		/// @brief JSON形式の値を設定します
		/// @param value 設定する値（JSON形式）
		void setValueAsJSON(const s3d::JSON& value);

		/// @brief dirtyフラグを取得します
		/// @return set()が呼ばれた場合 true
		[[nodiscard]]
		bool isDirty() const;

		/// @brief dirtyフラグをクリアします
		void markClean();

		/// @brief dirtyフラグを設定します
		void markDirty();

		/// @brief initializedフラグを取得します
		/// @return Redisに初回送信済みの場合 true
		[[nodiscard]]
		bool isInitialized() const;

		/// @brief initializedフラグを設定します
		void markInitialized();

		/// @brief initializedフラグをクリアします
		void markUninitialized();

		/// @brief 最終更新日時を取得します
		/// @return 最終更新日時
		[[nodiscard]]
		s3d::DateTime updatedAt() const;

	private:
		std::string m_u8name;
		s3d::String m_u32name;
		s3d::JSON m_value;
		s3d::JSON m_initialValue;
		bool m_dirty;
		bool m_initialized;
		s3d::DateTime m_updatedAt;
	};
}
