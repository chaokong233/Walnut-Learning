#pragma once

#include "Scene.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

namespace vulkan
{
	class CommandPool;
	class VulkanAllocator;
}

class ResourceConfig;

class ResourceManager
{
public:
	struct ModelAsset
	{
		std::string id;
		std::string path;
		std::filesystem::path resolvedPath;
	};

	struct TextureAsset
	{
		std::string id;
		std::string path;
		std::filesystem::path resolvedPath;
	};

	bool LoadAssetRegistry(const ResourceConfig& config, std::string* errorMessage = nullptr);
	bool IsLoaded() const { return loaded_; }
	const std::string& GetLastError() const { return lastError_; }
	const std::filesystem::path& GetAssetsDirectory() const { return assetsDirectory_; }

	void SetVulkanContext(vulkan::VulkanAllocator* allocator, vulkan::CommandPool* commandPool, VkQueue queue);

	bool TryGetModelAsset(const std::string& id, ModelAsset& asset, std::string* errorMessage = nullptr) const;
	bool TryGetTextureAsset(const std::string& id, TextureAsset& asset, std::string* errorMessage = nullptr) const;

	std::shared_ptr<Model> LoadModelAsset(const std::string& id, std::string* errorMessage = nullptr);
	std::shared_ptr<Model> LoadModelFile(const std::string& path, const std::filesystem::path& relativeBase = {}, std::string* errorMessage = nullptr);
	bool RebindModel(Scene& scene, Entity entity, const std::string& path, std::string* errorMessage = nullptr, const std::filesystem::path& relativeBase = {});

	bool ResolveTexturePath(const std::string& path, std::filesystem::path& resolvedPath, std::string* errorMessage = nullptr, const std::filesystem::path& relativeBase = {}) const;
	bool ApplyMaterialTexture(Model& model, uint32_t meshIndex, MaterialTextureSlot slot, const std::string& path, std::string* errorMessage = nullptr, const std::filesystem::path& relativeBase = {}) const;
	int LoadTextureForGPU(const std::string& path, std::string* errorMessage = nullptr);

private:
	bool ParseAssetsJson(const std::filesystem::path& assetsPath, std::string* errorMessage);
	void RegisterModelTextures(const std::shared_ptr<Model>& model) const;

	bool loaded_{ false };
	std::string lastError_;
	std::filesystem::path assetsPath_;
	std::filesystem::path assetsDirectory_;

	std::unordered_map<std::string, ModelAsset> modelAssets_;
	std::unordered_map<std::string, TextureAsset> textureAssets_;
	std::unordered_map<std::string, std::shared_ptr<Model>> modelCache_;

	vulkan::VulkanAllocator* allocator_{ nullptr };
	vulkan::CommandPool* commandPool_{ nullptr };
	VkQueue queue_{ VK_NULL_HANDLE };
};
