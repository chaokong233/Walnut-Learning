#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

class ResourceConfig
{
public:
	enum class Shader
	{
		RayGen,
		Miss,
		ShadowMiss,
		ClosestHit,
		DenoiseSvgf
	};

	static std::filesystem::path GetDefaultConfigPath();

	bool Load(const std::filesystem::path& configPath, std::string* errorMessage = nullptr);
	bool IsLoaded() const { return loaded_; }

	const std::string& GetLastError() const { return lastError_; }
	const std::filesystem::path& GetConfigPath() const { return configPath_; }
	const std::filesystem::path& GetConfigDirectory() const { return configDirectory_; }
	const std::filesystem::path& GetAssetsPath() const { return assetsPath_; }
	const std::filesystem::path& GetScenesPath() const { return scenesPath_; }
	const std::filesystem::path& GetShadersPath() const { return shadersPath_; }
	const std::filesystem::path& GetAssetsDirectory() const { return assetsDirectory_; }
	const std::filesystem::path& GetSceneStorageDirectory() const { return sceneStorageDirectory_; }
	const std::filesystem::path& GetShadersDirectory() const { return shadersDirectory_; }

	bool TryGetShaderPath(Shader shader, std::string& path, std::string* errorMessage = nullptr) const;

private:
	bool LoadConfigIndex(const std::filesystem::path& configPath, std::unordered_map<std::string, std::string>& entries, std::string* errorMessage);
	bool LoadJsonDocuments(const std::unordered_map<std::string, std::string>& entries, std::string* errorMessage);
	bool ParseScenesConfig(std::string* errorMessage);
	bool ParseShaders(std::string* errorMessage);

	bool loaded_{ false };
	std::string lastError_;

	std::filesystem::path configPath_;
	std::filesystem::path configDirectory_;
	std::filesystem::path assetsPath_;
	std::filesystem::path scenesPath_;
	std::filesystem::path shadersPath_;
	std::filesystem::path assetsDirectory_;
	std::filesystem::path scenesDirectory_;
	std::filesystem::path sceneStorageDirectory_;
	std::filesystem::path shadersDirectory_;

	std::unordered_map<Shader, std::string> shaderPaths_;
};
