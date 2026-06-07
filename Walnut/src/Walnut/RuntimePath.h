#pragma once

#include <filesystem>
#include <string>

namespace Walnut
{
	class RuntimePath
	{
	public:
		static void Initialize(int argc, char** argv);
		static bool IsInitialized();

		static const std::filesystem::path& GetExecutablePath();
		static const std::filesystem::path& GetExecutableDirectory();
		static const std::filesystem::path& GetWorkingDirectory();

		static std::filesystem::path ResolveFromExecutableDirectory(const std::filesystem::path& path);
		static bool TryResolveExistingPath(const std::filesystem::path& storedPath, std::filesystem::path& resolvedPath, std::string* errorMessage = nullptr, const std::filesystem::path& relativeBase = {});
	};
}
