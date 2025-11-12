#define NO_S3D_USING

#include <gtest/gtest.h>
#include <Siv3D.hpp>
#include <Siv3D/Windows/Windows.hpp>
#include <MessageBus/RedisConnection.hpp>

SIV3D_SET(s3d::EngineOption::Renderer::Headless)

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

void Main()
{
	// Attach to parent console for PowerShell output
	const AttachToParentConsole console{};

	auto argc = s3d::System::GetArgc();
	::testing::InitGoogleTest(&argc, s3d::System::GetArgv());

	RUN_ALL_TESTS();
}
