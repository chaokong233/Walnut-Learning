#include "SceneLoader.h"

#include "ResourceManager.h"
#include "Walnut/RuntimePath.h"

#include <json.hpp>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace
{
	using Json = nlohmann::json;

	bool ReadJsonFile(const std::filesystem::path& path, Json& json, std::string* errorMessage)
	{
		std::ifstream input(path);
		if (!input.is_open())
		{
			if (errorMessage)
			{
				*errorMessage = "failed to open scene file: " + path.generic_string();
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
				*errorMessage = "failed to parse scene file: " + path.generic_string() + " (" + e.what() + ")";
			}
			return false;
		}
	}

	std::string ReadString(const Json& json, const char* key, const std::string& fallback = {})
	{
		if (!json.contains(key) || !json[key].is_string())
		{
			return fallback;
		}
		return json[key].get<std::string>();
	}

	std::optional<float> ReadOptionalFloat(const Json& json, const char* key)
	{
		if (!json.contains(key) || !json[key].is_number())
		{
			return std::nullopt;
		}
		return json[key].get<float>();
	}

	glm::vec3 ReadVec3(const Json& json, const glm::vec3& fallback)
	{
		if (!json.is_array() || json.size() < 3)
		{
			return fallback;
		}

		return glm::vec3(
			json[0].get<float>(),
			json[1].get<float>(),
			json[2].get<float>());
	}

	std::optional<glm::vec3> ReadOptionalVec3(const Json& json, const char* key)
	{
		if (!json.contains(key) || !json[key].is_array() || json[key].size() < 3)
		{
			return std::nullopt;
		}
		return ReadVec3(json[key], glm::vec3(0.0f));
	}

	TransformComponent ReadTransform(const Json& json)
	{
		TransformComponent transform;
		const Json* source = &json;
		if (json.contains("transform") && json["transform"].is_object())
		{
			source = &json["transform"];
		}

		if (source->contains("translation"))
		{
			transform.translation = ReadVec3((*source)["translation"], transform.translation);
		}
		if (source->contains("rotation"))
		{
			transform.rotation = ReadVec3((*source)["rotation"], transform.rotation);
		}
		if (source->contains("scale"))
		{
			transform.scale = ReadVec3((*source)["scale"], transform.scale);
		}

		return transform;
	}

	SceneCameraSettings ReadCamera(const Json& sceneJson)
	{
		SceneCameraSettings camera;
		if (!sceneJson.contains("camera") || !sceneJson["camera"].is_object())
		{
			return camera;
		}

		const Json& cameraJson = sceneJson["camera"];
		if (cameraJson.contains("position"))
		{
			camera.position = ReadVec3(cameraJson["position"], camera.position);
		}
		if (cameraJson.contains("front"))
		{
			camera.front = ReadVec3(cameraJson["front"], camera.front);
		}
		camera.focusDistance = cameraJson.value("focusDistance", camera.focusDistance);
		camera.dofFocusDistance = cameraJson.value("dofFocusDistance", camera.dofFocusDistance);
		camera.lensRadius = cameraJson.value("lensRadius", camera.lensRadius);
		camera.useDOF = cameraJson.value("useDOF", camera.useDOF);
		return camera;
	}

	Json WriteVec3(const glm::vec3& value)
	{
		return Json::array({ value.x, value.y, value.z });
	}

	Json WriteTransform(const TransformComponent& transform)
	{
		Json json;
		json["translation"] = WriteVec3(transform.translation);
		json["rotation"] = WriteVec3(transform.rotation);
		json["scale"] = WriteVec3(transform.scale);
		return json;
	}

	Json WriteCamera(const SceneCameraSettings& camera)
	{
		Json json;
		json["position"] = WriteVec3(camera.position);
		json["front"] = WriteVec3(camera.front);
		json["focusDistance"] = camera.focusDistance;
		json["dofFocusDistance"] = camera.dofFocusDistance;
		json["lensRadius"] = camera.lensRadius;
		json["useDOF"] = camera.useDOF;
		return json;
	}

	std::string MakePathRelativeTo(const std::filesystem::path& path, const std::filesystem::path& base)
	{
		if (path.empty())
		{
			return {};
		}

		std::error_code error;
		std::filesystem::path relativePath = std::filesystem::relative(path, base, error);
		if (!error && !relativePath.empty())
		{
			return relativePath.generic_string();
		}

		return path.generic_string();
	}

	void ReadMaterialOverrides(const Json& entityJson, SceneModelEntityDesc& entity)
	{
		if (!entityJson.contains("materials") || !entityJson["materials"].is_array())
		{
			return;
		}

		for (const Json& materialJson : entityJson["materials"])
		{
			SceneMaterialTextureDesc material;
			material.meshIndex = materialJson.value("meshIndex", material.meshIndex);
			material.meshName = ReadString(materialJson, "meshName", ReadString(materialJson, "mesh"));
			material.baseColor = ReadOptionalVec3(materialJson, "baseColor");
			material.emissiveColor = ReadOptionalVec3(materialJson, "emissiveColor");
			material.specularTint = ReadOptionalVec3(materialJson, "specularTint");
			material.roughness = ReadOptionalFloat(materialJson, "roughness");
			material.metallic = ReadOptionalFloat(materialJson, "metallic");
			material.specular = ReadOptionalFloat(materialJson, "specular");
			material.subsurface = ReadOptionalFloat(materialJson, "subsurface");
			material.anisotropic = ReadOptionalFloat(materialJson, "anisotropic");
			material.hasBaseColorTexture = materialJson.contains("baseColorTexture");
			material.baseColorTexture = ReadString(materialJson, "baseColorTexture");
			material.baseColorTextureAsset = ReadString(materialJson, "baseColorTextureAsset");
			material.hasMetallicTexture = materialJson.contains("metallicTexture");
			material.metallicTexture = ReadString(materialJson, "metallicTexture");
			material.hasRoughnessTexture = materialJson.contains("roughnessTexture");
			material.roughnessTexture = ReadString(materialJson, "roughnessTexture");
			material.hasNormalTexture = materialJson.contains("normalTexture");
			material.normalTexture = ReadString(materialJson, "normalTexture");
			material.hasIBLTexture = materialJson.contains("iblTexture");
			material.iblTexture = ReadString(materialJson, "iblTexture");
			material.iblTextureAsset = ReadString(materialJson, "iblTextureAsset");
			entity.materials.push_back(std::move(material));
		}
	}

	bool IsJsonSceneFile(const std::filesystem::directory_entry& entry)
	{
		std::error_code error;
		return entry.is_regular_file(error) &&
			!error &&
			entry.path().extension() == ".json";
	}
}

bool SceneLoader::Load(const std::filesystem::path& scenePath, SceneDocument& document, std::string* errorMessage)
{
	document = SceneDocument();

	Json sceneJson;
	if (!ReadJsonFile(scenePath, sceneJson, errorMessage))
	{
		return false;
	}

	document.camera = ReadCamera(sceneJson);

	if (sceneJson.contains("entities") && sceneJson["entities"].is_array())
	{
		for (const Json& entityJson : sceneJson["entities"])
		{
			SceneModelEntityDesc entity;
			entity.name = ReadString(entityJson, "name", "Entity");
			entity.modelAssetID = ReadString(entityJson, "model", ReadString(entityJson, "modelAsset"));
			entity.modelPath = ReadString(entityJson, "modelPath", ReadString(entityJson, "path"));
			const std::string primitiveName = ReadString(entityJson, "primitive");
			PrimitiveType primitiveType;
			if (!primitiveName.empty() && TryParsePrimitiveType(primitiveName, primitiveType))
			{
				entity.primitive = primitiveType;
			}
			entity.visible = entityJson.value("visible", entity.visible);
			entity.transform = ReadTransform(entityJson);
			ReadMaterialOverrides(entityJson, entity);
			if (!entity.modelAssetID.empty() || !entity.modelPath.empty() || entity.primitive.has_value())
			{
				document.modelEntities.push_back(std::move(entity));
			}
		}
	}

	if (sceneJson.contains("radiusLights") && sceneJson["radiusLights"].is_array())
	{
		for (const Json& lightJson : sceneJson["radiusLights"])
		{
			SceneRadiusLightDesc light;
			light.name = ReadString(lightJson, "name", "Radius Light");
			light.transform = ReadTransform(lightJson);
			if (lightJson.contains("color"))
			{
				light.light.color = ReadVec3(lightJson["color"], light.light.color);
			}
			light.light.intensity = lightJson.value("intensity", light.light.intensity);
			light.light.radius = lightJson.value("radius", light.light.radius);
			document.radiusLights.push_back(light);
		}
	}

	if (sceneJson.contains("areaLights") && sceneJson["areaLights"].is_array())
	{
		for (const Json& lightJson : sceneJson["areaLights"])
		{
			SceneAreaLightDesc light;
			light.name = ReadString(lightJson, "name", "Area Light");
			light.transform = ReadTransform(lightJson);
			if (lightJson.contains("color"))
			{
				light.light.color = ReadVec3(lightJson["color"], light.light.color);
			}
			if (lightJson.contains("direction"))
			{
				light.light.direction = ReadVec3(lightJson["direction"], light.light.direction);
			}
			light.light.intensity = lightJson.value("intensity", light.light.intensity);
			light.light.width = lightJson.value("width", light.light.width);
			light.light.height = lightJson.value("height", light.light.height);
			document.areaLights.push_back(light);
		}
	}

	if (document.modelEntities.empty())
	{
		if (errorMessage)
		{
			*errorMessage = scenePath.filename().generic_string() + " must contain at least one entity with model or modelPath";
		}
		return false;
	}

	if (errorMessage)
	{
		errorMessage->clear();
	}
	return true;
}

bool SceneLoader::Save(const std::filesystem::path& scenePath, const Scene& scene, const SceneCameraSettings& camera, std::string* errorMessage)
{
	Json sceneJson;
	sceneJson["camera"] = WriteCamera(camera);
	sceneJson["entities"] = Json::array();
	sceneJson["radiusLights"] = Json::array();
	sceneJson["areaLights"] = Json::array();

	const std::filesystem::path sceneDirectory = scenePath.parent_path();
	for (Entity entity : scene.GetEntities())
	{
		const TagComponent* tag = scene.TryGetTag(entity);
		const TransformComponent* transform = scene.TryGetTransform(entity);

		if (const MeshRendererComponent* meshRenderer = scene.TryGetMeshRenderer(entity))
		{
			Json entityJson;
			entityJson["name"] = tag ? tag->name : "Entity";
			if (!meshRenderer->modelAssetID.empty())
			{
				entityJson["model"] = meshRenderer->modelAssetID;
			}
			else if (meshRenderer->model && meshRenderer->model->IsPrimitive())
			{
				entityJson["primitive"] = PrimitiveTypeToString(meshRenderer->model->GetPrimitiveType());
			}
			else if (meshRenderer->model)
			{
				const std::string& resolvedPath = meshRenderer->model->GetResolvedPath();
				entityJson["modelPath"] = resolvedPath.empty()
					? meshRenderer->model->GetPath()
					: MakePathRelativeTo(resolvedPath, sceneDirectory);
			}
			entityJson["visible"] = meshRenderer->visible;
			if (transform)
			{
				entityJson["transform"] = WriteTransform(*transform);
			}

			if (meshRenderer->model)
			{
				Json materialsJson = Json::array();
				const std::vector<Mesh>& meshes = meshRenderer->model->GetMeshes();
				for (uint32_t i = 0; i < meshes.size(); i++)
				{
					const Mesh& mesh = meshes[i];
					Json materialJson;
					materialJson["meshIndex"] = i;
					if (!mesh.name.empty())
					{
						materialJson["meshName"] = mesh.name;
					}
					materialJson["baseColor"] = WriteVec3(mesh.material.BaseColor);
					materialJson["emissiveColor"] = WriteVec3(mesh.material.EmissiveColor);
					materialJson["specularTint"] = WriteVec3(mesh.material.SpecularTint);
					materialJson["roughness"] = mesh.material.Roughness;
					materialJson["metallic"] = mesh.material.Metallic;
					materialJson["specular"] = mesh.material.Specular;
					materialJson["subsurface"] = mesh.material.Subsurface;
					materialJson["anisotropic"] = mesh.material.Anisotropic;
					materialJson["baseColorTexture"] = MakePathRelativeTo(mesh.material.BaseColorTexturePath, sceneDirectory);
					materialJson["metallicTexture"] = MakePathRelativeTo(mesh.material.MetallicTexturePath, sceneDirectory);
					materialJson["roughnessTexture"] = MakePathRelativeTo(mesh.material.RoughnessTexturePath, sceneDirectory);
					materialJson["normalTexture"] = MakePathRelativeTo(mesh.material.NormalTexturePath, sceneDirectory);
					materialJson["iblTexture"] = MakePathRelativeTo(mesh.material.IBLTexturePath, sceneDirectory);
					materialsJson.push_back(std::move(materialJson));
				}
				if (!materialsJson.empty())
				{
					entityJson["materials"] = std::move(materialsJson);
				}
			}

			sceneJson["entities"].push_back(std::move(entityJson));
		}

		if (const RadiusLightComponent* radiusLight = scene.TryGetRadiusLight(entity))
		{
			Json lightJson;
			lightJson["name"] = tag ? tag->name : "Radius Light";
			if (transform)
			{
				lightJson["transform"] = WriteTransform(*transform);
			}
			lightJson["color"] = WriteVec3(radiusLight->color);
			lightJson["intensity"] = radiusLight->intensity;
			lightJson["radius"] = radiusLight->radius;
			sceneJson["radiusLights"].push_back(std::move(lightJson));
		}

		if (const AreaLightComponent* areaLight = scene.TryGetAreaLight(entity))
		{
			Json lightJson;
			lightJson["name"] = tag ? tag->name : "Area Light";
			if (transform)
			{
				lightJson["transform"] = WriteTransform(*transform);
			}
			lightJson["color"] = WriteVec3(areaLight->color);
			lightJson["intensity"] = areaLight->intensity;
			lightJson["direction"] = WriteVec3(areaLight->direction);
			lightJson["width"] = areaLight->width;
			lightJson["height"] = areaLight->height;
			sceneJson["areaLights"].push_back(std::move(lightJson));
		}
	}

	std::error_code error;
	std::filesystem::create_directories(scenePath.parent_path(), error);
	if (error)
	{
		if (errorMessage)
		{
			*errorMessage = "failed to create scene directory: " + scenePath.parent_path().generic_string();
		}
		return false;
	}

	std::ofstream output(scenePath);
	if (!output.is_open())
	{
		if (errorMessage)
		{
			*errorMessage = "failed to open scene file for writing: " + scenePath.generic_string();
		}
		return false;
	}

	output << std::setw(2) << sceneJson << std::endl;
	if (errorMessage)
	{
		errorMessage->clear();
	}
	return true;
}

SceneManager::SceneManager(ResourceManager& resourceManager)
	: resourceManager_(&resourceManager)
{
}

bool SceneManager::SetSceneDirectory(const std::filesystem::path& sceneDirectory, std::string* errorMessage)
{
	if (sceneDirectory.empty())
	{
		if (errorMessage)
		{
			*errorMessage = "scene directory is empty";
		}
		return false;
	}

	std::error_code error;
	if (!std::filesystem::is_directory(sceneDirectory, error) || error)
	{
		if (errorMessage)
		{
			*errorMessage = "scene directory is not valid: " + sceneDirectory.generic_string();
		}
		return false;
	}

	sceneDirectory_ = sceneDirectory;
	if (errorMessage)
	{
		errorMessage->clear();
	}
	return true;
}

bool SceneManager::LoadFirstScene(Scene& scene, SceneCameraSettings* camera, std::string* errorMessage)
{
	if (sceneDirectory_.empty())
	{
		if (errorMessage)
		{
			*errorMessage = "scene directory is not configured";
		}
		return false;
	}

	std::error_code error;
	std::vector<std::filesystem::path> sceneFiles;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(sceneDirectory_, error))
	{
		if (error)
		{
			break;
		}
		if (IsJsonSceneFile(entry))
		{
			sceneFiles.push_back(entry.path());
		}
	}

	if (error)
	{
		if (errorMessage)
		{
			*errorMessage = "failed to enumerate scene directory: " + sceneDirectory_.generic_string();
		}
		return false;
	}

	std::sort(sceneFiles.begin(), sceneFiles.end(), [](const std::filesystem::path& a, const std::filesystem::path& b)
	{
		return a.filename().generic_string() < b.filename().generic_string();
	});

	if (sceneFiles.empty())
	{
		if (errorMessage)
		{
			*errorMessage = "scene directory does not contain any json scene files: " + sceneDirectory_.generic_string();
		}
		return false;
	}

	return LoadScene(sceneFiles.front(), scene, camera, errorMessage);
}

bool SceneManager::LoadScene(const std::filesystem::path& scenePath, Scene& scene, SceneCameraSettings* camera, std::string* errorMessage)
{
	std::filesystem::path resolvedPath;
	if (!Walnut::RuntimePath::TryResolveExistingPath(scenePath, resolvedPath, errorMessage, sceneDirectory_))
	{
		return false;
	}

	SceneDocument document;
	if (!SceneLoader::Load(resolvedPath, document, errorMessage))
	{
		return false;
	}

	Scene loadedScene;
	if (!ApplyDocument(document, resolvedPath.parent_path(), loadedScene, errorMessage))
	{
		return false;
	}

	scene = std::move(loadedScene);
	activeScenePath_ = resolvedPath;
	if (camera)
	{
		*camera = document.camera;
	}
	if (errorMessage)
	{
		errorMessage->clear();
	}
	return true;
}

bool SceneManager::SaveActiveScene(const Scene& scene, const SceneCameraSettings& camera, std::string* errorMessage) const
{
	if (activeScenePath_.empty())
	{
		if (errorMessage)
		{
			*errorMessage = "no active scene path is available";
		}
		return false;
	}

	return SceneLoader::Save(activeScenePath_, scene, camera, errorMessage);
}

bool SceneManager::ApplyDocument(const SceneDocument& document, const std::filesystem::path& sceneDirectory, Scene& scene, std::string* errorMessage)
{
	bool hasRenderableEntity = false;
	std::ostringstream warnings;

	for (const SceneModelEntityDesc& entityDesc : document.modelEntities)
	{
		std::shared_ptr<Model> model;
		std::string loadError;
		if (!entityDesc.modelAssetID.empty())
		{
			model = resourceManager_->LoadModelAsset(entityDesc.modelAssetID, &loadError);
			if (!model)
			{
				warnings << "[Warning Resource] " << loadError << "\n";
				model = Model::CreateMissingResourcePlaceholder(entityDesc.modelAssetID, loadError);
			}
		}
		else if (!entityDesc.modelPath.empty())
		{
			model = resourceManager_->LoadModelFile(entityDesc.modelPath, sceneDirectory, &loadError);
			if (!model)
			{
				warnings << "[Warning Resource] " << loadError << "\n";
				model = Model::CreateMissingResourcePlaceholder(entityDesc.modelPath, loadError);
			}
		}
		else if (entityDesc.primitive.has_value())
		{
			model = Model::CreatePrimitive(*entityDesc.primitive);
		}

		if (!model)
		{
			continue;
		}

		for (const SceneMaterialTextureDesc& materialDesc : entityDesc.materials)
		{
			if (materialDesc.meshIndex < model->GetMeshes().size())
			{
				Material& material = model->GetMeshes()[materialDesc.meshIndex].material;
				if (materialDesc.baseColor)
				{
					material.BaseColor = *materialDesc.baseColor;
				}
				if (materialDesc.emissiveColor)
				{
					material.EmissiveColor = *materialDesc.emissiveColor;
				}
				if (materialDesc.specularTint)
				{
					material.SpecularTint = *materialDesc.specularTint;
				}
				if (materialDesc.roughness)
				{
					material.Roughness = *materialDesc.roughness;
				}
				if (materialDesc.metallic)
				{
					material.Metallic = *materialDesc.metallic;
				}
				if (materialDesc.specular)
				{
					material.Specular = *materialDesc.specular;
				}
				if (materialDesc.subsurface)
				{
					material.Subsurface = *materialDesc.subsurface;
				}
				if (materialDesc.anisotropic)
				{
					material.Anisotropic = *materialDesc.anisotropic;
				}
			}

			if (!materialDesc.baseColorTextureAsset.empty())
			{
				ResourceManager::TextureAsset textureAsset;
				if (resourceManager_->TryGetTextureAsset(materialDesc.baseColorTextureAsset, textureAsset, &loadError))
				{
					resourceManager_->ApplyMaterialTexture(*model, materialDesc.meshIndex, MaterialTextureSlot::BaseColor, textureAsset.path, &loadError, resourceManager_->GetAssetsDirectory());
				}
			}
			else if (materialDesc.hasBaseColorTexture)
			{
				resourceManager_->ApplyMaterialTexture(*model, materialDesc.meshIndex, MaterialTextureSlot::BaseColor, materialDesc.baseColorTexture, &loadError, sceneDirectory);
			}

			if (materialDesc.hasMetallicTexture)
			{
				resourceManager_->ApplyMaterialTexture(*model, materialDesc.meshIndex, MaterialTextureSlot::Metallic, materialDesc.metallicTexture, &loadError, sceneDirectory);
			}

			if (materialDesc.hasRoughnessTexture)
			{
				resourceManager_->ApplyMaterialTexture(*model, materialDesc.meshIndex, MaterialTextureSlot::Roughness, materialDesc.roughnessTexture, &loadError, sceneDirectory);
			}

			if (materialDesc.hasNormalTexture)
			{
				resourceManager_->ApplyMaterialTexture(*model, materialDesc.meshIndex, MaterialTextureSlot::Normal, materialDesc.normalTexture, &loadError, sceneDirectory);
			}

			if (!materialDesc.iblTextureAsset.empty())
			{
				ResourceManager::TextureAsset textureAsset;
				if (resourceManager_->TryGetTextureAsset(materialDesc.iblTextureAsset, textureAsset, &loadError))
				{
					resourceManager_->ApplyMaterialTexture(*model, materialDesc.meshIndex, MaterialTextureSlot::IBL, textureAsset.path, &loadError, resourceManager_->GetAssetsDirectory());
				}
			}
			else if (materialDesc.hasIBLTexture)
			{
				resourceManager_->ApplyMaterialTexture(*model, materialDesc.meshIndex, MaterialTextureSlot::IBL, materialDesc.iblTexture, &loadError, sceneDirectory);
			}
		}

		const Entity entity = scene.CreateModelEntity(entityDesc.name, model, entityDesc.transform, entityDesc.modelAssetID);
		if (MeshRendererComponent* meshRenderer = scene.TryGetMeshRenderer(entity))
		{
			meshRenderer->visible = entityDesc.visible;
		}
		hasRenderableEntity = true;
	}

	for (const SceneRadiusLightDesc& radiusLight : document.radiusLights)
	{
		const Entity entity = scene.CreateEntity(radiusLight.name, radiusLight.transform);
		scene.AddRadiusLight(entity, radiusLight.light);
	}

	for (const SceneAreaLightDesc& areaLight : document.areaLights)
	{
		const Entity entity = scene.CreateEntity(areaLight.name, areaLight.transform);
		scene.AddAreaLight(entity, areaLight.light);
	}

	if (!hasRenderableEntity)
	{
		if (errorMessage)
		{
			*errorMessage = "scene contains no renderable model entities";
		}
		return false;
	}

	if (errorMessage)
	{
		*errorMessage = warnings.str();
	}
	return true;
}
