#pragma once
#include <gtest/gtest.h>
#include <boost/process/v1.hpp>
#include <Siv3D.hpp>
#include <MessageBus/RedisConnection.hpp>
#include <MessageBus/RedisConnectionState.hpp>
#include <string>

namespace bp = boost::process::v1;

constexpr auto REDIS_IMAGE = "redis:7-alpine";
constexpr auto REDIS_OLD_IMAGE = "redis:5-alpine"; // Not supported RESP3
constexpr auto REDIS_CONTAINER_NAME = "siv3d-messagebus-test";

namespace
{
	// dockerコマンドのフルパスを取得
	std::string GetDockerPath()
	{
		try
		{
			auto dockerPath = bp::search_path("docker");
			if (dockerPath.empty())
			{
				return "";
			}
			return dockerPath.string();
		}
		catch (const std::exception&)
		{
			return "";
		}
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

	static void WaitForContainerHealthy(Duration timeout)
	{
		Stopwatch sw{ StartImmediately::Yes };
		while (sw < timeout)
		{
			bp::ipstream pipe_stream;
			bp::child c(
				s_dockerPath, "inspect",
				"-f", "{{.State.Health.Status}}",
				REDIS_CONTAINER_NAME,
				bp::std_out > pipe_stream
			);
			std::string output(
				(std::istreambuf_iterator<char>(pipe_stream)),
				std::istreambuf_iterator<char>()
			);
			c.wait();

			if (c.exit_code() == 0 && output.find("healthy") != std::string::npos)
			{
				return;
			}
		}

		FAIL() << "Docker container did not become healthy in time";
	}

	static void StartContainer(const char* image = REDIS_IMAGE, const char* password = nullptr)
	{
		try
		{
			if (password)
			{
				Console << U"Starting Redis Docker container with auth...";
				std::string healthCmd = "redis-cli -a " + std::string(password) + " --raw incr ping";
				bp::child c(
					s_dockerPath, "run",
					"--rm",
					"-d",
					"--name", REDIS_CONTAINER_NAME,
					"-p", "6379:6379",
					"--health-cmd", healthCmd,
					"--health-interval", "1s",
					"--health-timeout", "3s",
					"--health-retries", "5",
					image,
					"redis-server", "--requirepass", password
				);
				c.wait();
				if (c.exit_code() != 0)
				{
					FAIL() << "Failed to start Redis container with auth";
				}
			}
			else
			{
				Console << U"Starting Redis Docker container...";
				bp::child c(
					s_dockerPath, "run",
					"--rm",
					"-d",
					"--name", REDIS_CONTAINER_NAME,
					"-p", "6379:6379",
					"--health-cmd", "redis-cli --raw incr ping",
					"--health-interval", "1s",
					"--health-timeout", "3s",
					"--health-retries", "5",
					image
				);
				c.wait();
				if (c.exit_code() != 0)
				{
					FAIL() << "Failed to start Redis container";
				}
			}

			WaitForContainerHealthy(30s);

			// パスワードを保存（未設定なら空に）
			s_password = (password ? password : "");
			s_started = true;
		}
		catch (const std::exception& e)
		{
			FAIL() << "Exception starting Redis container: " << e.what();
		}
	}

	static void StopContainer()
	{
		if (!s_started) return;
		try
		{
			Console << U"Stopping Redis Docker container...";

			bp::child c(s_dockerPath, "stop", REDIS_CONTAINER_NAME);
			c.wait();
		}
		catch (const std::exception&)
		{
			// コンテナが存在しない場合は無視
		}
		s_started = false;
		s_password.clear();
	}

	// docker exec でコンテナ内の redis-cli を実行（RESP3 有効）
	// 返り値: <exit_code, stdout>
	static std::pair<int, std::string> ExecRedisCli(const std::vector<std::string>& cliArgs)
	{
		bp::ipstream pipe_stream;
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

		// 起動
		bp::child c(
			s_dockerPath,
			bp::args(args),
			bp::std_out > pipe_stream
		);

		// 出力取得
		std::string output(
			(std::istreambuf_iterator<char>(pipe_stream)),
			std::istreambuf_iterator<char>()
		);
		c.wait();
		return { c.exit_code(), std::move(output) };
	}

	// publish ヘルパー（docker exec で redis-cli -3 PUBLISH）
	static void Publish(std::string_view channel, std::string payload)
	{
		size_t pos = 0;
		while ((pos = payload.find("\"", pos)) != std::string::npos)
		{
			payload.replace(pos, 1, "\\\"");
			pos += 2;
		}

		bp::child c(
			s_dockerPath,
			"exec", REDIS_CONTAINER_NAME,
			"redis-cli", "-3",
			"PUBLISH", channel.data(), payload.data()
		);
		c.wait();
		ASSERT_EQ(c.exit_code(), 0);
	}
};
