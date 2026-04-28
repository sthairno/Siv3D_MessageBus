#pragma once

#include "MessageBus/SharedVariable.hpp"

#include <Siv3D/JSON.hpp>
#include <Siv3D/DateTime.hpp>
#include <string>
#include <memory>

extern "C" {
struct redisAsyncContext;
struct redisReply;
}

namespace MessageBus::detail
{
	/// @brief SharedVariable の内部状態を管理するクラス
	class SharedVariableImpl : public std::enable_shared_from_this<SharedVariableImpl>
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
		const s3d::JSON& valueAsJSON() const { return m_value; }

		/// @brief JSON形式の値をUTF-8文字列で取得します
		/// @return 現在の値（UTF-8文字列）
		[[nodiscard]]
		std::string valueAsString() const { return m_value.formatUTF8Minimum(); }

		/// @brief JSON形式の値を設定します
		/// @param value 設定する値（JSON形式）
		void setValueAsJSON(const s3d::JSON& value);

		/// @brief dirtyフラグを設定します
		void markDirty() { m_dirty = true; }

		/// @brief dirtyフラグを取得します
		/// @return set()が呼ばれた場合 true
		[[nodiscard]]
		bool isDirty() const { return m_dirty; }

		/// @brief sendingフラグを取得します
		/// @return Redisへ送信中の場合 true
		[[nodiscard]]
		bool isSending() const { return m_sending; }

		/// @brief initializedフラグを取得します
		/// @return Redisに初回送信済みの場合 true
		[[nodiscard]]
		bool isInitialized() const { return m_initialized; }

		/// @brief 最終更新日時を取得します
		/// @return 最終更新日時
		[[nodiscard]]
		s3d::DateTime updatedAt() const { return m_updatedAt; }

		/// @brief この実装を参照する SharedVariable を作成します
		template<class Type>
		[[nodiscard]]
		SharedVariable<Type> asSharedVariable()
		{
			return SharedVariable<Type>(shared_from_this());
		}

		/// @brief 変数の状態を確認して必要に応じて Redis に送信します
		/// @param context Redis 非同期コンテキスト
		/// @param self 自身の shared_ptr（コールバック用）
		void syncToRemote(redisAsyncContext* context);

		/// @brief Redis から値を再取得します（無効化された場合など）
		/// @param context Redis 非同期コンテキスト
		/// @param self 自身の shared_ptr（コールバック用）
		void fetchFromRemote(redisAsyncContext* context);

		/// @brief 変数の状態をリセットします
		void reset();

	private:
		
		struct RedisCommandHelper;
		
		std::string m_u8name;
		s3d::String m_u32name;
		s3d::JSON m_value;
		bool m_dirty;
		bool m_sending;
		bool m_initialized;
		s3d::DateTime m_updatedAt;
		
		// コマンド送信
		void sendGet(redisAsyncContext* context);
		void sendSet(redisAsyncContext* context);
		void sendSetNxGet(redisAsyncContext* context);

		// コールバック関数
		void onGetCallback(redisAsyncContext*, redisReply* reply);
		void onSetCallback(redisAsyncContext*, redisReply* reply);
		void onSetNxGetCallback(redisAsyncContext*, redisReply* reply);
	};
}
