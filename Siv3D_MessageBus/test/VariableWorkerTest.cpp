#include "RedisDockerTestFixture.hpp"
#include "Utility.hpp"

#include <MessageBus/detail/RedisConnection.hpp>
#include <MessageBus/detail/SharedVariableImpl.hpp>
#include <MessageBus/detail/VariableWorker.hpp>

// ============================================================================
// VariableWorker 単体テスト
// ============================================================================

TEST(VariableWorkerUnit, GetOrCreateVariableReturnsSameInstance)
{
	MessageBus::detail::VariableWorker worker;

	auto var1 = worker.getOrCreateVariable("worker_shared", U"worker_shared", JSON(0));
	auto var2 = worker.getOrCreateVariable("worker_shared", U"worker_shared", JSON(100));

	EXPECT_EQ(var1, var2);
}

TEST(VariableWorkerUnit, HandlePubSubMessageReturnsFalse)
{
	MessageBus::detail::VariableWorker worker;

	EXPECT_FALSE(worker.handlePubSubMessage("channel", "payload"));
}

// ============================================================================
// VariableWorker 基本テスト
// ============================================================================

class VariableWorkerBasic : public RedisDocker
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

TEST_F(VariableWorkerBasic, VariableGetExistingValue)
{
	MessageBus::detail::VariableWorker worker;
	MessageBus::detail::RedisConnection conn{ {
		.ip = U"127.0.0.1",
		.port = 6379,
		.password = none,
		.heartbeatInterval = 1s,
		.onReady = [&](redisAsyncContext*) {
			worker.onConnect(conn);
		},
		.onDisconnect = [&]() {
			worker.onDisconnect();
		},
		.onInvalidate = [&](redisAsyncContext* context, const s3d::Array<std::string>& keys) {
			worker.onInvalidate(context, keys);
		},
	} };
	ASSERT_TRUE(WaitForConnection(conn, 10s));

	// 最初に Redis に値を設定
	ExecRedisCli({ "SET", "existing", "42" });

	// 既存の値がある場合、初期値で上書きされないことを確認
	auto value = worker.getOrCreateVariable("existing", U"existing", JSON(0))->asSharedVariable<int32>();
	Sleep(worker, conn, 0.5s);
	EXPECT_EQ(value.get(), 42);

	// Redis の値は 42 のままであることを確認
	auto [exitCode, output] = ExecRedisCli({ "GET", "existing" });
	EXPECT_EQ(exitCode, 0);
	EXPECT_NE(output.find("42"), std::string::npos);
}

TEST_F(VariableWorkerBasic, VariableSameNameReturnsSameInstance)
{
	MessageBus::detail::VariableWorker worker;
	MessageBus::detail::RedisConnection conn{ {
		.ip = U"127.0.0.1",
		.port = 6379,
		.password = none,
		.heartbeatInterval = 1s,
		.onReady = [&](redisAsyncContext*) {
			worker.onConnect(conn);
		},
		.onDisconnect = [&]() {
			worker.onDisconnect();
		},
		.onInvalidate = [&](redisAsyncContext* context, const s3d::Array<std::string>& keys) {
			worker.onInvalidate(context, keys);
		},
	} };
	ASSERT_TRUE(WaitForConnection(conn, 10s));

	auto var1 = worker.getOrCreateVariable("shared", U"shared", JSON(0))->asSharedVariable<int32>();
	auto var2 = worker.getOrCreateVariable("shared", U"shared", JSON(100))->asSharedVariable<int32>();

	EXPECT_EQ(var1.name(), var2.name());

	var1.set(50);
	var2.set(75);

	EXPECT_EQ(var1.get(), var2.get());
}

TEST(VariableWorkerWithoutConnection, VariableWorksWithoutConnection)
{
	MessageBus::detail::VariableWorker worker;

	// connが空でもVariableWorkerは変数を作成可能
	auto var1 = worker.getOrCreateVariable("test_var", U"test_var", JSON(42))->asSharedVariable<int32>();
	EXPECT_EQ(var1.get(), 42);

	auto var2 = worker.getOrCreateVariable("test_string", U"test_string", JSON(U"hello"))->asSharedVariable<String>();
	EXPECT_EQ(var2.get(), U"hello");
}

// ============================================================================
// VariableWorker Invalidateテスト
// ============================================================================

class VariableWorkerInvalidate : public RedisDocker
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

TEST_F(VariableWorkerInvalidate, UpdateValueOnInvalidate)
{
	MessageBus::detail::VariableWorker worker;
	MessageBus::detail::RedisConnection conn{ {
		.ip = U"127.0.0.1",
		.port = 6379,
		.password = none,
		.heartbeatInterval = 1s,
		.onReady = [&](redisAsyncContext*) {
			worker.onConnect(conn);
		},
		.onDisconnect = [&]() {
			worker.onDisconnect();
		},
		.onInvalidate = [&](redisAsyncContext* context, const s3d::Array<std::string>& keys) {
			worker.onInvalidate(context, keys);
		},
	} };
	ASSERT_TRUE(WaitForConnection(conn, 10s));

	// 1. 変数を宣言（初期値0）
	auto remoteVar = worker.getOrCreateVariable("remote_update", U"remote_update", JSON(0))->asSharedVariable<int32>();

	// 初期化処理を完了させるために少し待つ
	Sleep(worker, conn, 0.5s);
	EXPECT_EQ(remoteVar.get(), 0);

	// 2. Redis CLI経由で値を外部から更新
	// これによりRedisからinvalidateメッセージが送信されるはず
	auto [exitCode, output] = ExecRedisCli({ "SET", "remote_update", "123" });
	EXPECT_EQ(exitCode, 0);

	// 3. onInvalidateによる更新を待つ
	// 内部でGETを発行して更新するまでに少し時間がかかる
	EXPECT_TRUE(WaitUntil(worker, conn, [&] { return remoteVar.get() == 123; }, 1.0s));

	// 4. 値が更新されていることを確認
	EXPECT_EQ(remoteVar.get(), 123);
}

TEST_F(VariableWorkerInvalidate, IgnoreRemoteUpdateIfSending)
{
	MessageBus::detail::VariableWorker worker;
	MessageBus::detail::RedisConnection conn{ {
		.ip = U"127.0.0.1",
		.port = 6379,
		.password = none,
		.heartbeatInterval = 1s,
		.onReady = [&](redisAsyncContext*) {
			worker.onConnect(conn);
		},
		.onDisconnect = [&]() {
			worker.onDisconnect();
		},
		.onInvalidate = [&](redisAsyncContext* context, const s3d::Array<std::string>& keys) {
			worker.onInvalidate(context, keys);
		},
	} };
	ASSERT_TRUE(WaitForConnection(conn, 10s));

	// 1. 変数を宣言して初期化待機
	auto conflictVar = worker.getOrCreateVariable("conflict_update", U"conflict_update", JSON(0))->asSharedVariable<int32>();
	Sleep(worker, conn, 0.5s);

	// 2. ローカルで値を更新した直後に、Redis CLI経由で外部から更新（Invalidateを発生させる）
	//    ローカル更新(100) -> update() で送信開始(sending=true) -> CLI SET 999 -> Invalidate受信
	//    sending中なのでリモート値(999)は無視され、ローカル値(100)がRedisに反映される
	conflictVar.set(100);
	ExecRedisCli({ "SET", "conflict_update", "999" });

	// 3. 全ての処理（送信完了、Invalidate処理）を待つ
	Sleep(worker, conn, 1.0s);

	// 4. ローカル値(100)が維持され、Redis側も100になっていることを確認
	auto [exitCode, output] = ExecRedisCli({ "GET", "conflict_update" });
	EXPECT_EQ(exitCode, 0);
	EXPECT_EQ(conflictVar.get(), 100);
	EXPECT_NE(output.find("100"), std::string::npos);
}
