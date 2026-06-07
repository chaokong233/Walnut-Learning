#pragma once

#include "Scene.h"

#include <filesystem>
#include <string>
#include <vector>

class ResourceManager;

struct SceneCameraSettings
{
	glm::vec3 position{ 0.0f, 0.0f, 4.0f };
	glm::vec3 front{ 0.0f, 0.0f, -1.0f };
	float focusDistance{ 2.0f };
	float dofFocusDistance{ 6.0f };
	float lensRadius{ 0.02f };
	bool useDOF{ true };
};

struct SceneMaterialTextureDesc
{
	uint32_t meshIndex{ 0 };
	std::string meshName;
	std::string baseColorTexture;
	std::string baseColorTextureAsset;
	std::string iblTexture;
	std::string iblTextureAsset;
};

struct SceneModelEntityDesc
{
	std::string name;
	std::string modelAssetID;
	std::string modelPath;
	bool visible{ true };
	TransformComponent transform;
	std::vector<SceneMaterialTextureDesc> materials;
};

struct SceneRadiusLightDesc
{
	std::string name;
	TransformComponent transform;
	RadiusLightComponent light;
};

struct SceneAreaLightDesc
{
	std::string name;
	TransformComponent transform;
	AreaLightComponent light;
};

struct SceneDocument
{
	SceneCameraSettings camera;
	std::vector<SceneModelEntityDesc> modelEntities;
	std::vector<SceneRadiusLightDesc> radiusLights;
	std::vector<SceneAreaLightDesc> areaLights;
};

class SceneLoader
{
public:
	static bool Load(const std::filesystem::path& scenePath, SceneDocument& document, std::string* errorMessage = nullptr);
	static bool Save(const std::filesystem::path& scenePath, const Scene& scene, const SceneCameraSettings& camera, std::string* errorMessage = nullptr);
};

class SceneManager
{
public:
	explicit SceneManager(ResourceManager& resourceManager);

	bool SetSceneDirectory(const std::filesystem::path& sceneDirectory, std::string* errorMessage = nullptr);
	bool LoadFirstScene(Scene& scene, SceneCameraSettings* camera, std::string* errorMessage = nullptr);
	bool LoadScene(const std::filesystem::path& scenePath, Scene& scene, SceneCameraSettings* camera, std::string* errorMessage = nullptr);
	bool SaveActiveScene(const Scene& scene, const SceneCameraSettings& camera, std::string* errorMessage = nullptr) const;

	const std::filesystem::path& GetSceneDirectory() const { return sceneDirectory_; }
	const std::filesystem::path& GetActiveScenePath() const { return activeScenePath_; }

private:
	bool ApplyDocument(const SceneDocument& document, const std::filesystem::path& sceneDirectory, Scene& scene, std::string* errorMessage);

	ResourceManager* resourceManager_{ nullptr };
	std::filesystem::path sceneDirectory_;
	std::filesystem::path activeScenePath_;
};
