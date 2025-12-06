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

	// SharedVariable 側の値が更新されていることを確認
	EXPECT_EQ(score.get(), 100);

	// Redis 側に反映された値が正しいことを確認
	auto [exitCode, output] = ExecRedisCli({ "GET", "score" });
	EXPECT_EQ(exitCode, 0);
	EXPECT_NE(output.find("100"), std::string::npos);
}

TEST_F(SharedVariableBasic, VariableSetDouble)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus, 10s);

	auto value = bus.variable<double>(U"value", 0.0);
	value.set(3.14);

	Sleep(bus, 0.5s);

	// SharedVariable 側の値が更新されていることを確認
	EXPECT_DOUBLE_EQ(value.get(), 3.14);

	// Redis 側に反映された値が正しいことを確認
	auto [exitCode, output] = ExecRedisCli({ "GET", "value" });
	EXPECT_EQ(exitCode, 0);
	EXPECT_NE(output.find("3.14"), std::string::npos);
}

TEST_F(SharedVariableBasic, VariableSetBool)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus, 10s);

	auto flag = bus.variable<bool>(U"flag", false);
	flag.set(true);

	Sleep(bus, 0.5s);

	// SharedVariable 側の値が更新されていることを確認
	EXPECT_TRUE(flag.get());

	// Redis 側に反映された値が正しいことを確認
	auto [exitCode, output] = ExecRedisCli({ "GET", "flag" });
	EXPECT_EQ(exitCode, 0);
	EXPECT_NE(output.find("true"), std::string::npos);
}

TEST_F(SharedVariableBasic, VariableSetString)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus, 10s);

	auto name = bus.variable<String>(U"name", U"");
	name.set(U"test");

	Sleep(bus, 0.5s);

	// SharedVariable 側の値が更新されていることを確認
	EXPECT_TRUE(name.get() == U"test");

	// Redis 側に反映された値が正しいことを確認
	auto [exitCode, output] = ExecRedisCli({ "GET", "name" });
	EXPECT_EQ(exitCode, 0);
	EXPECT_NE(output.find("test"), std::string::npos);
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
	EXPECT_NE(output.find("42"), std::string::npos);
}

TEST_F(SharedVariableBasic, VariableTypeMismatchThrowsErrorOnGet)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus, 10s);
	auto value = bus.variable<int32>(U"mismatch", 0);

	// Redis 側に数値として解釈できない文字列を設定
	ExecRedisCli({ "SET", "mismatch", "not_a_number" });

	Sleep(bus, 0.5s);

	EXPECT_THROW(value.get(), MessageBus::TypeMismatchError);
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

// ============================================================================
// SharedVariable Invalidateテスト
// ============================================================================

class SharedVariableInvalidate : public RedisDocker
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

TEST_F(SharedVariableInvalidate, UpdateValueOnInvalidate)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus, 10s);

	// 1. 変数を宣言（初期値0）
	auto remoteVar = bus.variable<int32>(U"remote_update", 0);

	// 初期化処理を完了させるために少し待つ
	Sleep(bus, 0.5s);
	EXPECT_EQ(remoteVar.get(), 0);

	// 2. Redis CLI経由で値を外部から更新
	// これによりRedisからinvalidateメッセージが送信されるはず
	auto [exitCode, output] = ExecRedisCli({ "SET", "remote_update", "123" });
	EXPECT_EQ(exitCode, 0);

	// 3. onInvalidateによる更新を待つ
	// 内部でGETを発行して更新するまでに少し時間がかかる
	Sleep(bus, 1.0s);

	// 4. 値が更新されていることを確認
	EXPECT_EQ(remoteVar.get(), 123);
}

TEST_F(SharedVariableInvalidate, IgnoreRemoteUpdateIfSending)
{
	MessageBus::MessageBus bus{ U"127.0.0.1", 6379, none };
	WaitForConnection(bus, 10s);

	// 1. 変数を宣言して初期化待機
	auto conflictVar = bus.variable<int32>(U"conflict_update", 0);
	Sleep(bus, 0.5s);

	// 2. ローカルで値を更新した直後に、Redis CLI経由で外部から更新（Invalidateを発生させる）
	//    ローカル更新(100) -> tick() で送信開始(sending=true) -> CLI SET 999 -> Invalidate受信
	//    sending中なのでリモート値(999)は無視され、ローカル値(100)がRedisに反映される
	conflictVar.set(100);
	ExecRedisCli({ "SET", "conflict_update", "999" });

	// 3. 全ての処理（送信完了、Invalidate処理）を待つ
	Sleep(bus, 1.0s);

	// 4. ローカル値(100)が維持され、Redis側も100になっていることを確認
	auto [exitCode, output] = ExecRedisCli({ "GET", "conflict_update" });
	EXPECT_EQ(exitCode, 0);
	EXPECT_EQ(conflictVar.get(), 100);
	EXPECT_NE(output.find("100"), std::string::npos);
}
