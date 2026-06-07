#include "ResourceConfig.h"

#include "Walnut/RuntimePath.h"

#include <json.hpp>

#include <cctype>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <stdexcept>

namespace
{
	using Json = nlohmann::json;

	Json g_ScenesJson;
	Json g_ShadersJson;

	std::string Trim(const std::string& value)
	{
		size_t begin = 0;
		while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])))
		{
			begin++;
		}

		size_t end = value.size();
		while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])))
		{
			end--;
		}

		return value.substr(begin, end - begin);
	}

	std::string NormalizeKey(std::string key)
	{
		key = Trim(key);
		std::string result;
		result.reserve(key.size());
		for (char c : key)
		{
			if (c == '_' || c == '-' || std::isspace(static_cast<unsigned char>(c)))
			{
				continue;
			}
			result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
		}
		return result;
	}

	bool GetConfigEntry(const std::unordered_map<std::string, std::string>& entries, std::initializer_list<const char*> keys, std::string& value)
	{
		for (const char* key : keys)
		{
			auto it = entries.find(NormalizeKey(key));
			if (it != entries.end())
			{
				value = it->second;
				return true;
			}
		}
		return false;
	}

	std::string ReadString(const Json& json, const char* key, const std::string& fallback = {})
	{
		if (!json.contains(key) || !json[key].is_string())
		{
			return fallback;
		}
		return json[key].get<std::string>();
	}

	bool ReadJsonFile(const std::filesystem::path& path, Json& json, std::string* errorMessage)
	{
		std::ifstream input(path);
		if (!input.is_open())
		{
			if (errorMessage)
			{
				*errorMessage = "failed to open json file: " + path.generic_string();
			}
			return false;
		}

		try
		{
			input >> json;
			return true;
		}
		catch (const std::exception& e)
		{
			if (errorMessage)
			{
				*errorMessage = "failed to parse json file: " + path.generic_string() + " (" + e.what() + ")";
			}
			return false;
		}
	}

	bool ResolveConfigFile(const std::filesystem::path& path, const std::filesystem::path& base, std::filesystem::path& resolvedPath, std::string* errorMessage)
	{
		return Walnut::RuntimePath::TryResolveExistingPath(path, resolvedPath, errorMessage, base);
	}
}

std::filesystem::path ResourceConfig::GetDefaultConfigPath()
{
	return Walnut::RuntimePath::ResolveFromExecutableDirectory("config/config.ini");
}

bool ResourceConfig::Load(const std::filesystem::path& configPath, std::string* errorMessage)
{
	loaded_ = false;
	lastError_.clear();
	shaderPaths_.clear();
	configPath_.clear();
	configDirectory_.clear();
	assetsPath_.clear();
	scenesPath_.clear();
	shadersPath_.clear();
	assetsDirectory_.clear();
	scenesDirectory_.clear();
	sceneStorageDirectory_.clear();
	shadersDirectory_.clear();
	g_ScenesJson = Json();
	g_ShadersJson = Json();

	std::unordered_map<std::string, std::string> entries;
	if (!LoadConfigIndex(configPath, entries, errorMessage))
	{
		lastError_ = errorMessage ? *errorMessage : "failed to load config.ini";
		return false;
	}

	if (!LoadJsonDocuments(entries, errorMessage))
	{
		lastError_ = errorMessage ? *errorMessage : "failed to load json config files";
		return false;
	}

	if (!ParseScenesConfig(errorMessage) || !ParseShaders(errorMessage))
	{
		lastError_ = errorMessage ? *errorMessage : "failed to parse resource config";
		return false;
	}

	loaded_ = true;
	if (errorMessage)
	{
		errorMessage->clear();
	}
	return true;
}

bool ResourceConfig::TryGetShaderPath(Shader shader, std::string& path, std::string* errorMessage) const
{
	auto it = shaderPaths_.find(shader);
	if (it == shaderPaths_.end() || it->second.empty())
	{
		if (errorMessage)
		{
			*errorMessage = "shader path is not configured";
		}
		return false;
	}

	std::filesystem::path resolvedPath;
	if (!Walnut::RuntimePath::TryResolveExistingPath(it->second, resolvedPath, errorMessage, shadersDirectory_))
	{
		return false;
	}

	path = resolvedPath.generic_string();
	return true;
}

bool ResourceConfig::LoadConfigIndex(const std::filesystem::path& configPath, std::unordered_map<std::string, std::string>& entries, std::string* errorMessage)
{
	std::filesystem::path resolvedConfigPath;
	if (!Walnut::RuntimePath::TryResolveExistingPath(configPath, resolvedConfigPath, errorMessage))
	{
		return false;
	}

	std::ifstream input(resolvedConfigPath);
	if (!input.is_open())
	{
		if (errorMessage)
		{
			*errorMessage = "failed to open config.ini: " + resolvedConfigPath.generic_string();
		}
		return false;
	}

	configPath_ = resolvedConfigPath;
	configDirectory_ = configPath_.parent_path();

	std::string line;
	while (std::getline(input, line))
	{
		line = Trim(line);
		if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[')
		{
			continue;
		}

		const size_t equal = line.find('=');
		if (equal == std::string::npos)
		{
			continue;
		}

		const std::string key = NormalizeKey(line.substr(0, equal));
		const std::string value = Trim(line.substr(equal + 1));
		if (!key.empty() && !value.empty())
		{
			entries[key] = value;
		}
	}

	return true;
}

bool ResourceConfig::LoadJsonDocuments(const std::unordered_map<std::string, std::string>& entries, std::string* errorMessage)
{
	std::string assetsPath;
	std::string scenesPath;
	std::string shadersPath;
	if (!GetConfigEntry(entries, { "assets", "assetsJson", "assets_json" }, assetsPath) ||
		!GetConfigEntry(entries, { "scenes", "scenesJson", "scenes_json", "scene", "sceneJson", "scene_json" }, scenesPath) ||
		!GetConfigEntry(entries, { "shaders", "shadersJson", "shaders_json" }, shadersPath))
	{
		if (errorMessage)
		{
			*errorMessage = "config.ini must define assets, scenes, and shaders json paths";
		}
		return false;
	}

	if (!ResolveConfigFile(assetsPath, configDirectory_, assetsPath_, errorMessage) ||
		!ResolveConfigFile(scenesPath, configDirectory_, scenesPath_, errorMessage) ||
		!ResolveConfigFile(shadersPath, configDirectory_, shadersPath_, errorMessage))
	{
		return false;
	}

	assetsDirectory_ = assetsPath_.parent_path();
	scenesDirectory_ = scenesPath_.parent_path();
	shadersDirectory_ = shadersPath_.parent_path();

	return ReadJsonFile(scenesPath_, g_ScenesJson, errorMessage) &&
		ReadJsonFile(shadersPath_, g_ShadersJson, errorMessage);
}

bool ResourceConfig::ParseScenesConfig(std::string* errorMessage)
{
	const std::string sceneDirectory = ReadString(g_ScenesJson, "sceneDirectory", ReadString(g_ScenesJson, "path"));
	if (sceneDirectory.empty())
	{
		if (errorMessage)
		{
			*errorMessage = "scenes.json must define sceneDirectory";
		}
		return false;
	}

	return Walnut::RuntimePath::TryResolveExistingPath(sceneDirectory, sceneStorageDirectory_, errorMessage, scenesDirectory_);
}

bool ResourceConfig::ParseShaders(std::string* errorMessage)
{
	if (!g_ShadersJson.contains("rayTracing") || !g_ShadersJson["rayTracing"].is_object())
	{
		if (errorMessage)
		{
			*errorMessage = "shaders.json must contain a rayTracing object";
		}
		return false;
	}

	const Json& rayTracing = g_ShadersJson["rayTracing"];
	shaderPaths_[Shader::RayGen] = ReadString(rayTracing, "raygen");
	shaderPaths_[Shader::Miss] = ReadString(rayTracing, "miss");
	shaderPaths_[Shader::ShadowMiss] = ReadString(rayTracing, "shadow");
	shaderPaths_[Shader::ClosestHit] = ReadString(rayTracing, "closesthit", ReadString(rayTracing, "closestHit"));

	if (g_ShadersJson.contains("denoise") && g_ShadersJson["denoise"].is_object())
	{
		shaderPaths_[Shader::DenoiseSvgf] = ReadString(g_ShadersJson["denoise"], "svgf");
	}

	for (Shader shader : { Shader::RayGen, Shader::Miss, Shader::ShadowMiss, Shader::ClosestHit, Shader::DenoiseSvgf })
	{
		auto it = shaderPaths_.find(shader);
		if (it == shaderPaths_.end() || it->second.empty())
		{
			if (errorMessage)
			{
				*errorMessage = "shaders.json is missing a required shader path";
			}
			return false;
		}
	}

	return true;
}
