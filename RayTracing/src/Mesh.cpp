#include "Mesh.h"

#include "Scene.h"
#include "Walnut/myVulkan/myVulkanInclude.h"

#include <glm/gtc/matrix_inverse.hpp>

#include <stdexcept>

TexturePool* g_texturePool = new TexturePool();

namespace
{
	glm::vec3 NormalizeOr(const glm::vec3& value, const glm::vec3& fallback)
	{
		const float length = glm::length(value);
		if (length <= 1e-6f)
		{
			return fallback;
		}
		return value / length;
	}

	RTMesh::Material ToRTMaterial(const Material& material, RTModel& uploader)
	{
		RTMesh::Material result;
		result.BaseColor = material.BaseColor;
		result.EmissiveColor = material.EmissiveColor;
		result.SpecularTint = material.SpecularTint;
		result.Roughness = material.Roughness;
		result.Metallic = material.Metallic;
		result.Specular = material.Specular;
		result.Subsurface = material.Subsurface;
		result.Anisotropic = material.Anisotropic;
		result.BaseColorTextureID = uploader.loadTexture(material.BaseColorTexturePath);
		result.IBLTextureID = uploader.loadTexture(material.IBLTexturePath);
		return result;
	}
}

void RTModel::UploadScene(const Scene& scene)
{
	linerMeshes.clear();
	indices.scratchArray.clear();
	vertices.scratchArray.clear();
	indices.buffer.reset();
	vertices.buffer.reset();

	for (const Entity& entity : scene.GetEntities())
	{
		if (!entity.visible || !entity.model)
		{
			continue;
		}

		const glm::mat4 transform = entity.transform.GetMatrix();
		const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(transform));

		for (const Mesh& sourceMesh : entity.model->GetMeshes())
		{
			auto newMesh = std::make_shared<RTMesh>();
			newMesh->geometry.firstVertex = static_cast<uint32_t>(vertices.scratchArray.size());
			newMesh->geometry.firstIndex = static_cast<uint32_t>(indices.scratchArray.size());

			for (Vertex vertex : sourceMesh.vertices)
			{
				vertex.position = glm::vec3(transform * glm::vec4(vertex.position, 1.0f));
				vertex.normal = NormalizeOr(normalMatrix * vertex.normal, glm::vec3(0.0f, 1.0f, 0.0f));
				vertex.tangent = NormalizeOr(normalMatrix * vertex.tangent, glm::vec3(1.0f, 0.0f, 0.0f));
				vertices.scratchArray.push_back(vertex);
			}

			for (uint32_t index : sourceMesh.indices)
			{
				indices.scratchArray.push_back(index + newMesh->geometry.firstVertex);
			}

			newMesh->geometry.vertexCount = static_cast<uint32_t>(vertices.scratchArray.size()) - newMesh->geometry.firstVertex;
			newMesh->geometry.indexCount = static_cast<uint32_t>(indices.scratchArray.size()) - newMesh->geometry.firstIndex;
			newMesh->material = ToRTMaterial(sourceMesh.material, *this);

			linerMeshes.push_back(newMesh);
		}
	}

	vertices.count = static_cast<uint32_t>(vertices.scratchArray.size());
	indices.count = static_cast<uint32_t>(indices.scratchArray.size());

	if (vertices.count == 0 || indices.count == 0)
	{
		throw std::runtime_error("scene contains no renderable geometry");
	}

	vulkan::VulkanLocalBuffer::CopierCreateInfo copierInfo{
		.commandPool = commandPool_,
		.transferQueue = queue_
	};

	const VkDeviceSize verticesSize = vertices.count * sizeof(Vertex);
	vertices.buffer = std::make_shared<vulkan::VulkanLocalBuffer>(allocator_, verticesSize,
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, copierInfo);
	vertices.buffer->UploadMemory(vertices.scratchArray.data(), verticesSize, 0);
	vertices.scratchArray.clear();

	const VkDeviceSize indicesSize = indices.count * sizeof(uint32_t);
	indices.buffer = std::make_shared<vulkan::VulkanLocalBuffer>(allocator_, indicesSize,
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, copierInfo);
	indices.buffer->UploadMemory(indices.scratchArray.data(), indicesSize, 0);
	indices.scratchArray.clear();
}

int RTModel::loadTexture(const std::string& path)
{
	if (path.empty())
	{
		return -1;
	}

	using namespace vulkan;

	const int existingID = g_texturePool->FindID(path);
	if (existingID >= 0)
	{
		return existingID;
	}

	VulkanLoadedTexture::CopierCreateInfo info;
	info.commandPool = commandPool_;
	info.transferQueue = queue_;

	auto texture = new VulkanLoadedTexture(allocator_, path, info, VK_IMAGE_USAGE_SAMPLED_BIT);

	SingleTimeCommands cmd(info.commandPool);
	VulkanImage::transitionImageLayout(cmd, *texture,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1);

	auto sampler = new VulkanSampler(allocator_->getDevice());
	return g_texturePool->Add(path, vulkanSampleImage(texture, sampler));
}

TexturePool::~TexturePool()
{
	for (auto pair : sampleImage)
	{
		delete pair.first;
		delete pair.second;
	}
}

int TexturePool::Add(const std::string& path, vulkanSampleImage image)
{
	if (isExisted(path))
	{
		return -1;
	}

	path_to_imageID_map[path] = static_cast<uint32_t>(sampleImage.size());
	sampleImage.push_back(image);
	imageInfo_.push_back({ image.second->handle(), image.first->getView(), VK_IMAGE_LAYOUT_GENERAL });
	return static_cast<int>(sampleImage.size() - 1);
}

bool TexturePool::isExisted(const std::string& path)
{
	return path_to_imageID_map.find(path) != path_to_imageID_map.end();
}

int TexturePool::FindID(const std::string& path)
{
	if (isExisted(path))
	{
		return static_cast<int>(path_to_imageID_map[path]);
	}
	return -1;
}
