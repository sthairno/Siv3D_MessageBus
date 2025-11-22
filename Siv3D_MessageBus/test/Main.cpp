#define NO_S3D_USING

#include <gtest/gtest.h>
#include <Siv3D.hpp>
#include <MessageBus/RedisConnection.hpp>

SIV3D_SET(s3d::EngineOption::Renderer::Headless)

#if SIV3D_PLATFORM(WINDOWS)
/// @see https://discord.com/channels/443310697397354506/998714158621147237/1303965339045855232
class AttachToParentConsole
{
public:
	AttachToParentConsole()
	{
		if (::AttachConsole(ATTACH_PARENT_PROCESS))
		{
			::freopen_s(&m_fpOut, "CONOUT$", "w", stdout);
			::freopen_s(&m_fpErr, "CONOUT$", "w", stderr);
		}
		else
		{
			s3d::Print << U"Failed to attach to parent console";
			s3d::Console.open();
		}
	}

	~AttachToParentConsole()
	{
		if (m_fpOut)
		{
			::fclose(m_fpOut);
		}
		if (m_fpErr)
		{
			::fclose(m_fpErr);
		}

		::FreeConsole();
	}

private:
	FILE* m_fpOut = nullptr;
	FILE* m_fpErr = nullptr;
};
#else
class AttachToParentConsole
{
public:
	AttachToParentConsole() = default;
};
#endif

void OutputToGitHubActions(int code)
{
	const auto outputPath = s3d::EnvironmentVariable::Get(U"GITHUB_OUTPUT");

	if (outputPath.isEmpty())
	{
		return;
	}

	s3d::TextWriter writer{ outputPath, s3d::OpenMode::Append, s3d::TextEncoding::UTF8_NO_BOM };

	if (not writer.isOpen())
	{
		return;
	}

	writer.writeln(fmt::format(U"SIV3D_EXIT_CODE={}", code));
}

void Main()
{
	const bool isInGitHubActions = s3d::EnvironmentVariable::Get(U"GITHUB_ACTIONS") == U"true";

	// Attach to parent console for PowerShell output
	const AttachToParentConsole console{};

	auto argc = s3d::System::GetArgc();
	::testing::InitGoogleTest(&argc, s3d::System::GetArgv());

	auto code = RUN_ALL_TESTS();

#if SIV3D_PLATFORM(WINDOWS)
	// Windowsでは別スレッドでMain()関数が実行されるため、スレッドセーフではないstd::exit()は呼び出さない
	if (isInGitHubActions)
	{
		OutputToGitHubActions(code);
	}
#else
	std::exit(code);
#endif
}
