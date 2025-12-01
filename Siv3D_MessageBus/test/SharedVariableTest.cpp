#include "RedisDockerTestFixture.hpp"
#include "Utility.hpp"

// ============================================================================
// SharedVariable基本テスト
// ============================================================================

class SharedVariableBasic : public RedisDocker
{
protected:
	static void SetUpTestSuite()
	{
		RedisDocker::SetUpTestSuite();
		StartContainer();
	}

	static void TearDownTestSuite()
	{
		RedisDocker::TearDownTestSuite();
	}
};

TEST_F(SharedVariableBasic, VariableSetInt32)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus, 10s);

	auto score = bus.variable<int32>(U"score", 0);
	score.set(100);

	Sleep(bus, 0.5s);

	// Redis から値を取得して確認
	auto [exitCode, output] = ExecRedisCli({ "GET", "score" });
	EXPECT_EQ(exitCode, 0);
	EXPECT_TRUE(output.find("100") != std::string::npos);
}

TEST_F(SharedVariableBasic, VariableSetDouble)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus, 10s);

	auto value = bus.variable<double>(U"value", 0.0);
	value.set(3.14);

	Sleep(bus, 0.5s);

	auto [exitCode, output] = ExecRedisCli({ "GET", "value" });
	EXPECT_EQ(exitCode, 0);
	EXPECT_TRUE(output.find("3.14") != std::string::npos);
}

TEST_F(SharedVariableBasic, VariableSetBool)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus, 10s);

	auto flag = bus.variable<bool>(U"flag", false);
	flag.set(true);

	Sleep(bus, 0.5s);

	auto [exitCode, output] = ExecRedisCli({ "GET", "flag" });
	EXPECT_EQ(exitCode, 0);
	EXPECT_TRUE(output.find("true") != std::string::npos);
}

TEST_F(SharedVariableBasic, VariableSetString)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus, 10s);

	auto name = bus.variable<String>(U"name", U"");
	name.set(U"test");

	Sleep(bus, 0.5s);

	auto [exitCode, output] = ExecRedisCli({ "GET", "name" });
	EXPECT_EQ(exitCode, 0);
	EXPECT_TRUE(output.find("test") != std::string::npos);
}

TEST_F(SharedVariableBasic, VariableGetExistingValue)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus, 10s);

	// 最初に Redis に値を設定
	ExecRedisCli({ "SET", "existing", "42" });

	// 既存の値がある場合、初期値で上書きされないことを確認
	auto value = bus.variable<int32>(U"existing", 0);
	Sleep(bus, 0.5s);
	EXPECT_EQ(value.get(), 42);

	// Redis の値は 42 のままであることを確認
	auto [exitCode, output] = ExecRedisCli({ "GET", "existing" });
	EXPECT_EQ(exitCode, 0);
	EXPECT_TRUE(output == "42");
}

TEST_F(SharedVariableBasic, VariableSameNameReturnsSameInstance)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus, 10s);

	auto var1 = bus.variable<int32>(U"shared", 0);
	auto var2 = bus.variable<int32>(U"shared", 100);

	EXPECT_EQ(var1.name(), var2.name());

	var1.set(50);
	var2.set(75);

	EXPECT_EQ(var1.get(), var2.get());
}

