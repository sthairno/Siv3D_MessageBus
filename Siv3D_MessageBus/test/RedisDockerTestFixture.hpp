#pragma once
#include <gtest/gtest.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <WinSock2.h>
#endif
#include <boost/process.hpp>
#include <boost/asio/readable_pipe.hpp>
#include <boost/asio/read.hpp>
#include <boost/system/error_code.hpp>
#include <Siv3D.hpp>
#include <MessageBus/RedisConnection.hpp>
#include <MessageBus/RedisConnectionState.hpp>
#include <string>

namespace bp = boost::process::v2;
namespace asio = boost::asio;

constexpr auto REDIS_IMAGE = "redis:7-alpine";
constexpr auto REDIS_OLD_IMAGE = "redis:5-alpine"; // Not supported RESP3
constexpr auto REDIS_CONTAINER_NAME = "siv3d-messagebus-test";

namespace
{
	// dockerコマンドのフルパスを取得
	std::string GetDockerPath()
	{
		auto dockerPath = bp::environment::find_executable("docker");
		if (dockerPath.empty())
		{
			return "";
		}
		return dockerPath.string();
	}
}

// Redis用フィクスチャ
class RedisDocker : public ::testing::Test
{
protected:
	inline static bool s_started = false;
	inline static std::string s_dockerPath;
	inline static std::string s_password; // コンテナ起動時のパスワードを保持（未設定なら空）

	static void SetUpTestSuite()
	{
		s_dockerPath = GetDockerPath();
		if (s_dockerPath.empty())
		{
			FAIL() << "docker not found";
		}
	}

	static void TearDownTestSuite()
	{
		StopContainer();
	}

	virtual void SetUp()
	{
		if (not s_started)
		{
			StartContainer();
		}
		ExecRedisCli({ "FLUSHALL" });
	}

	static void WaitForContainerHealthy(Duration timeout)
	{
		Stopwatch sw{ StartImmediately::Yes };
		while (sw < timeout)
		{
			asio::io_context ctx;
			asio::readable_pipe pipe(ctx);

			bp::process proc(
				ctx,
				s_dockerPath,
				{"inspect", "-f", "{{.State.Health.Status}}", REDIS_CONTAINER_NAME},
				bp::process_stdio{{}, pipe, {}}
			);
			boost::system::error_code ec;
			std::string output;

			asio::read(pipe, asio::dynamic_buffer(output), ec);
			int exitCode = proc.wait();

			if (exitCode == 0 && output.find("healthy") != std::string::npos)
			{
				return;
			}
		}

		FAIL() << "Docker container did not become healthy in time";
	}

	static void StartContainer(const char* image = REDIS_IMAGE, const char* password = nullptr)
	{
		asio::io_context ctx;
		if (password)
		{
			Console << U"Starting Redis Docker container with auth...";
			std::string healthCmd = "redis-cli -a " + std::string(password) + " --raw incr ping";
			bp::process proc(
				ctx,
				s_dockerPath,
				{"run", "--rm", "-d", "--name", REDIS_CONTAINER_NAME,
				 "-p", "6379:6379", "--health-cmd", healthCmd,
				 "--health-interval", "1s", "--health-timeout", "3s",
				 "--health-retries", "5", image,
				 "redis-server", "--requirepass", password}
			);
			int exitCode = proc.wait();
			if (exitCode != 0)
			{
				FAIL() << "Failed to start Redis container with auth";
			}
		}
		else
		{
			Console << U"Starting Redis Docker container...";
			bp::process proc(
				ctx,
				s_dockerPath,
				{"run", "--rm", "-d", "--name", REDIS_CONTAINER_NAME,
				 "-p", "6379:6379", "--health-cmd", "redis-cli --raw incr ping",
				 "--health-interval", "1s", "--health-timeout", "3s",
				 "--health-retries", "5", image}
			);
			int exitCode = proc.wait();
			if (exitCode != 0)
			{
				FAIL() << "Failed to start Redis container";
			}
		}

		WaitForContainerHealthy(30s);

		// パスワードを保存（未設定なら空に）
		s_password = (password ? password : "");
		s_started = true;
	}

	static void StopContainer()
	{
		if (!s_started) return;
		Console << U"Stopping Redis Docker container...";

		asio::io_context ctx;
		bp::process proc(ctx, s_dockerPath, {"stop", REDIS_CONTAINER_NAME});
		proc.wait();
		s_started = false;
		s_password.clear();
	}

	// docker exec でコンテナ内の redis-cli を実行（RESP3 有効）
	// 返り値: <exitCode, stdout>
	static std::pair<int, std::string> ExecRedisCli(const std::vector<std::string>& cliArgs)
	{
		asio::io_context ctx;
		asio::readable_pipe pipe(ctx);
		std::vector<std::string> args;
		args.reserve(8 + cliArgs.size());
		args.push_back("exec");
		args.push_back("-i");
		args.push_back(REDIS_CONTAINER_NAME);
		args.push_back("redis-cli");
		args.push_back("-3");
		if (!s_password.empty())
		{
			args.push_back("-a");
			args.emplace_back(s_password);
		}
		for (const auto& a : cliArgs) args.push_back(a);

		bp::process proc(
			ctx,
			s_dockerPath,
			args,
			bp::process_stdio{{}, pipe, {}}
		);

		boost::system::error_code ec;
		std::string output;

		asio::read(pipe, asio::dynamic_buffer(output), ec);
		int exitCode = proc.wait();

		return { exitCode, std::move(output) };
	}

	// publish ヘルパー（docker exec で redis-cli -3 PUBLISH）
	static void Publish(std::string_view channel, std::string payload)
	{
#if SIV3D_PLATFORM(WINDOWS)
		// Windowsでは、payload内の " を \" にエスケープしないと正しく動作しない
		size_t pos = 0;
		while ((pos = payload.find("\"", pos)) != std::string::npos)
		{
			payload.replace(pos, 1, "\\\"");
			pos += 2;
		}

		// boost.process v1.89では空文字列を渡すと内部でエラーになるため、空文字列の場合は""に置き換える
		// https://github.com/boostorg/process/issues/495
		if (payload.empty())
		{
			payload = "\"\"";
		}
#endif
		asio::io_context ctx;

		bp::process proc(
			ctx,
			s_dockerPath,
			{ "exec", REDIS_CONTAINER_NAME, "redis-cli", "-3",
				"PUBLISH", channel.data(), payload.data() }
		);
		int exitCode = proc.wait();

		ASSERT_EQ(exitCode, 0);
	}
};
