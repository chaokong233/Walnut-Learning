#include "RuntimePath.h"

#include <sstream>
#include <system_error>

#ifdef WL_PLATFORM_WINDOWS
#define NOMINMAX
#include <Windows.h>
#endif

namespace Walnut
{
	namespace
	{
		struct RuntimePathState
		{
			bool initialized{ false };
			std::filesystem::path executablePath;
			std::filesystem::path executableDirectory;
			std::filesystem::path workingDirectory;
		};

		RuntimePathState s_State;

		std::filesystem::path NormalizePath(const std::filesystem::path& path)
		{
			std::error_code error;
			std::filesystem::path normalized = std::filesystem::weakly_canonical(path, error);
			if (!error)
			{
				return normalized;
			}

			normalized = std::filesystem::absolute(path, error);
			if (!error)
			{
				return normalized.lexically_normal();
			}

			return path.lexically_normal();
		}

		std::filesystem::path GetExecutablePathFromSystem()
		{
#ifdef WL_PLATFORM_WINDOWS
			std::wstring buffer(MAX_PATH, L'\0');
			for (;;)
			{
				const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
				if (length == 0)
				{
					break;
				}

				if (length < buffer.size() - 1)
				{
					buffer.resize(length);
					return std::filesystem::path(buffer);
				}

				buffer.resize(buffer.size() * 2);
			}
#endif
			return {};
		}

		std::filesystem::path ResolvePath(const std::filesystem::path& path, const std::filesystem::path& base)
		{
			if (path.is_absolute())
			{
				return NormalizePath(path);
			}
			return NormalizePath(base / path);
		}

		void SetErrorMessage(std::string* errorMessage, const std::filesystem::path& storedPath, const std::filesystem::path& resolvedPath)
		{
			if (!errorMessage)
			{
				return;
			}

			std::ostringstream stream;
			stream << "resource path is invalid: " << storedPath.generic_string();
			if (!resolvedPath.empty())
			{
				stream << " (resolved to " << resolvedPath.generic_string() << ")";
			}
			*errorMessage = stream.str();
		}
	}

	void RuntimePath::Initialize(int argc, char** argv)
	{
		if (s_State.initialized)
		{
			return;
		}

		std::error_code error;
		s_State.workingDirectory = std::filesystem::current_path(error);
		if (error)
		{
			s_State.workingDirectory = std::filesystem::path();
		}

		s_State.executablePath = GetExecutablePathFromSystem();
		if (s_State.executablePath.empty() && argc > 0 && argv && argv[0])
		{
			s_State.executablePath = argv[0];
		}

		if (!s_State.executablePath.empty())
		{
			s_State.executablePath = NormalizePath(s_State.executablePath);
			s_State.executableDirectory = s_State.executablePath.parent_path();
		}

		if (s_State.executableDirectory.empty())
		{
			s_State.executableDirectory = s_State.workingDirectory;
		}

		s_State.initialized = true;
	}

	bool RuntimePath::IsInitialized()
	{
		return s_State.initialized;
	}

	const std::filesystem::path& RuntimePath::GetExecutablePath()
	{
		return s_State.executablePath;
	}

	const std::filesystem::path& RuntimePath::GetExecutableDirectory()
	{
		return s_State.executableDirectory;
	}

	const std::filesystem::path& RuntimePath::GetWorkingDirectory()
	{
		return s_State.workingDirectory;
	}

	std::filesystem::path RuntimePath::ResolveFromExecutableDirectory(const std::filesystem::path& path)
	{
		if (path.is_absolute())
		{
			return NormalizePath(path);
		}
		return ResolvePath(path, s_State.executableDirectory);
	}

	bool RuntimePath::TryResolveExistingPath(const std::filesystem::path& storedPath, std::filesystem::path& resolvedPath, std::string* errorMessage, const std::filesystem::path& relativeBase)
	{
		if (storedPath.empty())
		{
			SetErrorMessage(errorMessage, storedPath, {});
			return false;
		}

		const std::filesystem::path base = relativeBase.empty() ? s_State.executableDirectory : relativeBase;
		resolvedPath = ResolvePath(storedPath, base);

		std::error_code error;
		if (std::filesystem::exists(resolvedPath, error) && !error)
		{
			if (errorMessage)
			{
				errorMessage->clear();
			}
			return true;
		}

		SetErrorMessage(errorMessage, storedPath, resolvedPath);
		return false;
	}
}
