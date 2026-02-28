#include "MessageBus/detail/PlayerList.hpp"

#include <Siv3D/Random.hpp>
#include <sqids/sqids.hpp>

#include <cstdint>
#include <vector>

namespace MessageBus::detail
{
	PlayerList::PlayerList()
		: m_uidUtf8(generateUidUtf8())
	{
	}

	std::string PlayerList::generateUidUtf8()
	{
		const std::uint64_t a = s3d::RandomUint64();
		const std::uint64_t b = s3d::RandomUint64();

		static const sqidscxx::SqidsOptions sqidsOptions{
			.minLength = 8,
		};
		static const sqidscxx::Sqids<std::uint64_t> sqids{ sqidsOptions };

		return sqids.encode(std::vector<std::uint64_t>{ a, b });
	}
}
