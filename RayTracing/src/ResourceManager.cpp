#include "ResourceManager.h"

#include "ResourceConfig.h"
#include "Walnut/RuntimePath.h"
#include "Walnut/myVulkan/myVulkanInclude.h"

#include <json.hpp>

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unordered_set>

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

	std::string ReadString(const Json& json, const char* key, const std::string& fallback = {})
	{
		if (!json.contains(key) || !json[key].is_string())
		{
			return fallback;
		}
		return json[key].get<std::string>();
	}

	template<typename Asset>
	void ParseAssetMap(const Json& json, const char* key, const std::filesystem::path& baseDirectory, std::unordered_map<std::string, Asset>& target)
	{
		if (!json.contains(key) || !json[key].is_object())
		{
			return;
		}

		for (auto it = json[key].begin(); it != json[key].end(); ++it)
		{
			Asset asset;
			asset.id = it.key();
			if (it.value().is_string())
			{
				asset.path = it.value().get<std::string>();
			}
			else if (it.value().is_object())
			{
				asset.path = ReadString(it.value(), "path");
			}

			if (asset.path.empty())
			{
				continue;
			}

			Walnut::RuntimePath::TryResolveExistingPath(asset.path, asset.resolvedPath, nullptr, baseDirectory);
			target[asset.id] = asset;
		}
	}
}

bool ResourceManager::LoadAssetRegistry(const ResourceConfig& config, std::string* errorMessage)
{
	loaded_ = false;
	lastError_.clear();
	modelAssets_.clear();
	textureAssets_.clear();
	modelCache_.clear();
	assetsPath_ = config.GetAssetsPath();
	assetsDirectory_ = config.GetAssetsDirectory();

	if (!config.IsLoaded())
	{
		if (errorMessage)
		{
			*errorMessage = "resource config is not loaded";
		}
		lastError_ = errorMessage ? *errorMessage : "resource config is not loaded";
		return false;
	}

	if (!ParseAssetsJson(assetsPath_, errorMessage))
	{
		lastError_ = errorMessage ? *errorMessage : "failed to parse assets.json";
		return false;
	}

	loaded_ = true;
	if (errorMessage)
	{
		errorMessage->clear();
	}
	return true;
}

void ResourceManager::SetVulkanContext(vulkan::VulkanAllocator* allocator, vulkan::CommandPool* commandPool, VkQueue queue)
{
	allocator_ = allocator;
	commandPool_ = commandPool;
	queue_ = queue;
}

bool ResourceManager::TryGetModelAsset(const std::string& id, ModelAsset& asset, std::string* errorMessage) const
{
	auto it = modelAssets_.find(id);
	if (it == modelAssets_.end())
	{
		if (errorMessage)
		{
			*errorMessage = "model asset is not registered in assets.json: " + id;
		}
		return false;
	}

	asset = it->second;
	if (errorMessage)
	{
		errorMessage->clear();
	}
	return true;
}

bool ResourceManager::TryGetTextureAsset(const std::string& id, TextureAsset& asset, std::string* errorMessage) const
{
	auto it = textureAssets_.find(id);
	if (it == textureAssets_.end())
	{
		if (errorMessage)
		{
			*errorMessage = "texture asset is not registered in assets.json: " + id;
		}
		return false;
	}

	asset = it->second;
	if (errorMessage)
	{
		errorMessage->clear();
	}
	return true;
}

std::shared_ptr<Model> ResourceManager::LoadModelAsset(const std::string& id, std::string* errorMessage)
{
	ModelAsset asset;
	if (!TryGetModelAsset(id, asset, errorMessage))
	{
		return nullptr;
	}

	return LoadModelFile(asset.path, assetsDirectory_, errorMessage);
}

std::shared_ptr<Model> ResourceManager::LoadModelFile(const std::string& path, const std::filesystem::path& relativeBase, std::string* errorMessage)
{
	std::filesystem::path resolvedPath;
	std::string resolveError;
	if (!Walnut::RuntimePath::TryResolveExistingPath(path, resolvedPath, &resolveError, relativeBase))
	{
		if (errorMessage)
		{
			*errorMessage = resolveError;
		}
		return nullptr;
	}

	const std::string cacheKey = resolvedPath.generic_string();
	auto it = modelCache_.find(cacheKey);
	if (it != modelCache_.end())
	{
		if (errorMessage)
		{
			errorMessage->clear();
		}
		return it->second;
	}

	std::shared_ptr<Model> model;
	if (!Model::TryLoadFromFile(path, model, errorMessage, relativeBase))
	{
		return nullptr;
	}

	RegisterModelTextures(model);
	modelCache_[cacheKey] = model;
	return model;
}

bool ResourceManager::RebindModel(Scene& scene, Entity entity, const std::string& path, std::string* errorMessage, const std::filesystem::path& relativeBase)
{
	if (!scene.IsValid(entity))
	{
		if (errorMessage)
		{
			*errorMessage = "invalid scene entity";
		}
		return false;
	}

	MeshRendererComponent* meshRenderer = scene.TryGetMeshRenderer(entity);
	if (!meshRenderer)
	{
		if (errorMessage)
		{
			*errorMessage = "entity does not have MeshRendererComponent";
		}
		return false;
	}

	std::shared_ptr<Model> model = LoadModelFile(path, relativeBase, errorMessage);
	if (!model)
	{
		return false;
	}

	meshRenderer->model = model;
	meshRenderer->modelAssetID.clear();
	scene.Touch();
	if (errorMessage)
	{
		errorMessage->clear();
	}
	return true;
}

void ResourceManager::PruneModelCacheForScene(const Scene& scene)
{
	std::unordered_set<std::string> liveModelPaths;
	for (Entity entity : scene.GetEntities())
	{
		const MeshRendererComponent* meshRenderer = scene.TryGetMeshRenderer(entity);
		if (!meshRenderer || !meshRenderer->model)
		{
			continue;
		}

		const std::string& resolvedPath = meshRenderer->model->GetResolvedPath();
		if (!resolvedPath.empty())
		{
			liveModelPaths.insert(resolvedPath);
		}
	}

	for (auto it = modelCache_.begin(); it != modelCache_.end();)
	{
		if (liveModelPaths.find(it->first) == liveModelPaths.end())
		{
			it = modelCache_.erase(it);
		}
		else
		{
			++it;
		}
	}
}

bool ResourceManager::ResolveTexturePath(const std::string& path, std::filesystem::path& resolvedPath, std::string* errorMessage, const std::filesystem::path& relativeBase) const
{
	if (path.empty())
	{
		resolvedPath.clear();
		if (errorMessage)
		{
			errorMessage->clear();
		}
		return true;
	}

	return Walnut::RuntimePath::TryResolveExistingPath(path, resolvedPath, errorMessage, relativeBase);
}

bool ResourceManager::ApplyMaterialTexture(Model& model, uint32_t meshIndex, MaterialTextureSlot slot, const std::string& path, std::string* errorMessage, const std::filesystem::path& relativeBase) const
{
	if (path.empty())
	{
		return model.RebindMaterialTexture(meshIndex, slot, {}, errorMessage);
	}

	std::filesystem::path resolvedPath;
	if (!ResolveTexturePath(path, resolvedPath, errorMessage, relativeBase))
	{
		return false;
	}

	return model.RebindMaterialTexture(meshIndex, slot, resolvedPath.generic_string(), errorMessage);
}

int ResourceManager::LoadTextureForGPU(const std::string& path, std::string* errorMessage)
{
	if (path.empty())
	{
		if (errorMessage)
		{
			errorMessage->clear();
		}
		return -1;
	}

	if (!allocator_ || !commandPool_ || queue_ == VK_NULL_HANDLE)
	{
		if (errorMessage)
		{
			*errorMessage = "vulkan texture loading context is not initialized";
		}
		return -1;
	}

	std::filesystem::path resolvedPath;
	if (!Walnut::RuntimePath::TryResolveExistingPath(path, resolvedPath, errorMessage))
	{
		return -1;
	}

	const std::string texturePath = resolvedPath.generic_string();
	const int existingID = g_texturePool->FindID(texturePath);
	if (existingID >= 0)
	{
		if (errorMessage)
		{
			errorMessage->clear();
		}
		return existingID;
	}

	using namespace vulkan;
	VulkanLoadedTexture::CopierCreateInfo info;
	info.commandPool = commandPool_;
	info.transferQueue = queue_;

	auto texture = new VulkanLoadedTexture(allocator_, texturePath, info, VK_IMAGE_USAGE_SAMPLED_BIT);
	if (!texture->getVulkanImageHandle())
	{
		if (errorMessage)
		{
			*errorMessage = "failed to load texture: " + texturePath;
		}
		delete texture;
		return -1;
	}

	SingleTimeCommands cmd(info.commandPool);
	VulkanImage::transitionImageLayout(cmd, *texture,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1);

	auto sampler = new VulkanSampler(allocator_->getDevice());
	const int textureID = g_texturePool->Add(texturePath, vulkanSampleImage(texture, sampler));
	if (errorMessage)
	{
		errorMessage->clear();
	}
	return textureID;
}

bool ResourceManager::ParseAssetsJson(const std::filesystem::path& assetsPath, std::string* errorMessage)
{
	Json assetsJson;
	if (!ReadJsonFile(assetsPath, assetsJson, errorMessage))
	{
		return false;
	}

	ParseAssetMap<ModelAsset>(assetsJson, "models", assetsDirectory_, modelAssets_);
	ParseAssetMap<TextureAsset>(assetsJson, "textures", assetsDirectory_, textureAssets_);

	if (modelAssets_.empty())
	{
		if (errorMessage)
		{
			*errorMessage = "assets.json must register at least one model";
		}
		return false;
	}

	return true;
}

void ResourceManager::RegisterModelTextures(const std::shared_ptr<Model>& model) const
{
	if (!model)
	{
		return;
	}

	for (const Mesh& mesh : model->GetMeshes())
	{
		if (!mesh.material.BaseColorTexturePath.empty())
		{
			std::filesystem::path resolvedPath;
			std::string error;
			if (!ResolveTexturePath(mesh.material.BaseColorTexturePath, resolvedPath, &error))
			{
				std::cerr << "[Warning Texture] " << error << std::endl;
			}
		}
		if (!mesh.material.MetallicTexturePath.empty())
		{
			std::filesystem::path resolvedPath;
			std::string error;
			if (!ResolveTexturePath(mesh.material.MetallicTexturePath, resolvedPath, &error))
			{
				std::cerr << "[Warning Texture] " << error << std::endl;
			}
		}
		if (!mesh.material.RoughnessTexturePath.empty())
		{
			std::filesystem::path resolvedPath;
			std::string error;
			if (!ResolveTexturePath(mesh.material.RoughnessTexturePath, resolvedPath, &error))
			{
				std::cerr << "[Warning Texture] " << error << std::endl;
			}
		}
		if (!mesh.material.NormalTexturePath.empty())
		{
			std::filesystem::path resolvedPath;
			std::string error;
			if (!ResolveTexturePath(mesh.material.NormalTexturePath, resolvedPath, &error))
			{
				std::cerr << "[Warning Texture] " << error << std::endl;
			}
		}
		if (!mesh.material.IBLTexturePath.empty())
		{
			std::filesystem::path resolvedPath;
			std::string error;
			if (!ResolveTexturePath(mesh.material.IBLTexturePath, resolvedPath, &error))
			{
				std::cerr << "[Warning Texture] " << error << std::endl;
			}
		}
	}
}
