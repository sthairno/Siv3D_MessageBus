#pragma once

#include <string>
#include <string_view>

namespace MessageBus::detail
{
	class PlayerList
	{
    public:

		PlayerList();

		[[nodiscard]]
		std::string_view uidUtf8() const noexcept
		{
			return m_uidUtf8;
		}

	private:

		std::string m_uidUtf8;

		[[nodiscard]]
		static std::string generateUidUtf8();
	};
}