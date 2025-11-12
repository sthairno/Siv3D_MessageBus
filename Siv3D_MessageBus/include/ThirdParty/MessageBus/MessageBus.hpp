#pragma once

#include "WindowsLibrary.hpp"
#include <memory>

#include <Siv3D/StringView.hpp>
#include <Siv3D/String.hpp>
#include <Siv3D/Optional.hpp>
#include <Siv3D/JSON.hpp>
#include <Siv3D/Array.hpp>

namespace MessageBus
{
	class MessageBus
	{
	public:
		/// @brief MessageBusを初期化します
		/// @param ip 接続先のIPアドレス
		/// @param port 接続先のポート番号
		/// @param password 認証パスワード（オプション）
		MessageBus(s3d::StringView ip, s3d::uint16 port, s3d::Optional<s3d::StringView> password = s3d::none);

		/// @brief MessageBusを終了します
		void close();

		/// @brief イベント処理を行います（メインループで毎フレーム呼び出す）
		void tick();

		/// @brief 接続状態を取得します
		/// @return 接続済みの場合 true
		[[nodiscard]]
		bool isConnected() const;

		/// @brief 接続状態を取得します
		/// @return 接続済みの場合 true
		[[nodiscard]]
		explicit operator bool() const { return isConnected(); }

		/// @brief エラーメッセージを取得します
		/// @return エラーメッセージ文字列
		[[nodiscard]]
		const s3d::String& error() const;

		// ================================
		// イベント処理
		// ================================

		struct Event
		{
			s3d::String channel;
			s3d::JSON value;
		};

		/// @brief チャンネルを購読します
		bool subscribe(s3d::StringView channel);

		/// @brief チャンネルの購読を解除します
		bool unsubscribe(s3d::StringView channel);

		/// @brief イベントを送信します
		/// @param channel 送信先チャンネル名
		/// @param payload イベントに含めるJSON
		/// @return イベント送信が成功した場合 true
		bool emit(s3d::StringView channel, s3d::Optional<s3d::JSON> payload = s3d::none);

		/// @brief 受信済みイベント
		[[nodiscard]]
		const s3d::Array<Event>& events() const;

	private:

		struct Impl;

		std::unique_ptr<Impl> m_impl;

	public:
		~MessageBus();
	};
}
