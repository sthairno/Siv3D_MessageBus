#include <gtest/gtest.h>

#include <MessageBus/detail/PlayerList.hpp>

#include <cctype>
#include <string_view>

TEST(PlayerListTest, UidIsGeneratedInConstructor)
{
	MessageBus::detail::PlayerList list;
	const std::string_view uid = list.uidUtf8();
	EXPECT_FALSE(uid.empty());

	for (const unsigned char ch : uid)
	{
		EXPECT_TRUE(std::isalnum(ch)) << "uid=" << uid;
	}
}

TEST(PlayerListTest, UidIsUniqueAcrossInstances)
{
	MessageBus::detail::PlayerList a;
	MessageBus::detail::PlayerList b;

	EXPECT_NE(a.uidUtf8(), b.uidUtf8());
}
