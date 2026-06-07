#pragma once

#include "Walnut/Application.h"

#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vulkan
{
	class CommandPool;
	class VulkanAllocator;
	class VulkanLocalBuffer;
}

class TexturePool;
class Scene;
extern TexturePool* g_texturePool;

struct Vertex
{
	alignas(16) glm::vec3 position;
	alignas(16) glm::vec3 normal;
	alignas(16) glm::vec3 tangent;
	alignas(16) glm::vec2 texcoord;
};

class RTMesh;
class RTModel;
using vulkanSampleImage = std::pair<vulkan::VulkanLoadedTexture*, vulkan::VulkanSampler*>;

struct GeometryNode
{
	uint64_t VertexBufferDeviceAddress;
	uint64_t IndexBufferDeviceAddress;
	alignas(16) glm::vec3 BaseColor;
	alignas(16) glm::vec3 EmissiveColor;
	alignas(16) glm::vec3 SpecularTint;
	float Roughness;
	float Metallic;
	float Specular;
	float Subsurface;
	float Anisotropic;
	int BaseColorTextureID;
	int IBLTextureID;
};

class RTNode
{
public:
	std::vector<std::shared_ptr<RTMesh>> children;
};

class RTMesh : public RTNode
{
public:
	struct Geometry
	{
		uint32_t firstVertex;
		uint32_t vertexCount;
		uint32_t firstIndex;
		uint32_t indexCount;
	} geometry;

	struct Material
	{
		glm::vec3 BaseColor{ glm::vec3(1) };
		glm::vec3 EmissiveColor{ glm::vec3(0) };
		glm::vec3 SpecularTint{ glm::vec3(0.2f) };
		float Roughness{ 0.5f };
		float Metallic{ 0.0f };
		float Specular{ 0.5f };
		float Subsurface{ 0.0f };
		float Anisotropic{ 0.0f };
		int BaseColorTextureID{ -1 };
		int IBLTextureID{ -1 };
	} material;
};

class RTModel : public RTNode
{
public:
	RTModel(vulkan::VulkanAllocator* allocator, vulkan::CommandPool* commandPool, VkQueue queue)
		: allocator_(allocator), commandPool_(commandPool), queue_(queue) {}

	std::vector<std::shared_ptr<RTMesh>> linerMeshes;

	struct Vertices
	{
		uint32_t count{0};
		std::shared_ptr<vulkan::VulkanLocalBuffer> buffer{nullptr};
		std::vector<Vertex> scratchArray;
	} vertices;

	struct Indices
	{
		uint32_t count{0};
		std::shared_ptr<vulkan::VulkanLocalBuffer> buffer{nullptr};
		std::vector<uint32_t> scratchArray;
	} indices;

	void UploadScene(const Scene& scene);
	int loadTexture(const std::string& path);

private:
	vulkan::VulkanAllocator* allocator_;
	vulkan::CommandPool* commandPool_;
	VkQueue queue_;
};

class TexturePool
{
private:
	std::vector<vulkanSampleImage> sampleImage{};
	std::unordered_map<std::string, uint32_t> path_to_imageID_map;
	std::vector<VkDescriptorImageInfo> imageInfo_{};

public:
	TexturePool() = default;
	~TexturePool();

	int Add(const std::string&, vulkanSampleImage);
	bool isExisted(const std::string&);
	int FindID(const std::string&);

	inline std::vector<VkDescriptorImageInfo>* GetImageInfo() { return &imageInfo_; }
	inline int GetImageCount() { return static_cast<int>(imageInfo_.size()); }
};
