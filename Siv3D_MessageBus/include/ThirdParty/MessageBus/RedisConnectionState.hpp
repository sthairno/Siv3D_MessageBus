#pragma once

#include <Siv3D/FormatData.hpp>
#include <fmt/format.h>

namespace MessageBus
{
	// 接続状態の列挙型
	enum class RedisConnectionState
	{
		Disconnected,      // 未接続
		Connecting,        // TCP接続中
		HelloSent,         // HELLO 3送信済み、応答待ち
		AuthSent,          // AUTH送信済み、応答待ち（パスワードありの場合）
		Connected,         // 接続確立完了
		Failed             // 接続失敗（認証エラー等）
	};

	// Siv3D Formatter 対応
	inline void Formatter(s3d::FormatData& formatData, const RedisConnectionState& value)
	{
		switch (value)
		{
		case RedisConnectionState::Disconnected: formatData.string += U"Disconnected"; break;
		case RedisConnectionState::Connecting:   formatData.string += U"Connecting"; break;
		case RedisConnectionState::HelloSent:    formatData.string += U"HelloSent"; break;
		case RedisConnectionState::AuthSent:     formatData.string += U"AuthSent"; break;
		case RedisConnectionState::Connected:    formatData.string += U"Connected"; break;
		case RedisConnectionState::Failed:       formatData.string += U"Failed"; break;
		}
	}
}

// fmt 対応（テンプレートで任意の CharType に対応、ASCII を逐次書き出し）
template <class Char>
struct fmt::formatter<MessageBus::RedisConnectionState, Char>
{
	template <class ParseContext>
	constexpr auto parse(ParseContext& ctx)
	{
		return ctx.begin();
	}

	template <class FormatContext>
	auto format(const MessageBus::RedisConnectionState& value, FormatContext& ctx) const
	{
		const char* ascii = "";
		switch (value)
		{
		case MessageBus::RedisConnectionState::Disconnected: ascii = "Disconnected"; break;
		case MessageBus::RedisConnectionState::Connecting:   ascii = "Connecting";   break;
		case MessageBus::RedisConnectionState::HelloSent:    ascii = "HelloSent";    break;
		case MessageBus::RedisConnectionState::AuthSent:     ascii = "AuthSent";     break;
		case MessageBus::RedisConnectionState::Connected:    ascii = "Connected";    break;
		case MessageBus::RedisConnectionState::Failed:       ascii = "Failed";       break;
		}

		auto out = ctx.out();
		for (const char* p = ascii; *p; ++p)
		{
			*out++ = static_cast<Char>(*p);
		}
		return out;
	}
};
