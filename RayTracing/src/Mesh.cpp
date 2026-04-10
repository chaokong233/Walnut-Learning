#include "Mesh.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Walnut/myVulkan/myVulkanInclude.h"


TexturePool* g_texturePool = new TexturePool();


Triangle::Triangle(std::array<Vertex, 3> _verteies, std::shared_ptr<Material> material)
	:verteies(_verteies)
{
	this->material = material;
}

bvh_Vec3 Triangle::GetBvhCenter()
{
	return trans_Vec3_to_bvhVec3((verteies[0].position + verteies[1].position + verteies[2].position )/ 3.0f);
}

bvh_BBox Triangle::GetBvhBBox()
{
	bvh_Vec3 p1 = trans_Vec3_to_bvhVec3(verteies[0].position);
	bvh_Vec3 p2 = trans_Vec3_to_bvhVec3(verteies[1].position);
	bvh_Vec3 p3 = trans_Vec3_to_bvhVec3(verteies[2].position);
	return bvh_BBox(p1).extend(p2).extend(p3).reserveSafeExtent(0.0001f);
}

PreTriangle::PreTriangle(const Triangle& tri)
	:verteies(tri.verteies)
{
	this->material = tri.material;
	edge1_ = verteies[1].position - verteies[0].position;
	edge2_ = verteies[2].position - verteies[0].position;
}

bool PreTriangle::hit(const Ray& ray, double t_min, double t_max, HitResult& result)
{
	glm::vec3 b = ray.origin() - verteies[0].position;
	glm::vec3 e0 = ray.direction();
	float orig_detemenate = glm::dot(glm::cross(e0, edge2_), edge1_);
	if (orig_detemenate == 0)
	{
		result.isHit = false;
		return false;
	}
	float t = glm::dot(glm::cross(b, edge1_), edge2_) / orig_detemenate;
	float u = glm::dot(glm::cross(e0, edge2_), b) / orig_detemenate;
	float v = glm::dot(glm::cross(b, edge1_), e0) / orig_detemenate;
	if (u > 0 && v > 0 && (u + v) < 1 && t > t_min && t < t_max)
	{
		glm::vec3 normal = verteies[0].normal * (1 - u - v) + verteies[1].normal * v + verteies[2].normal * u;

		bool isFront = glm::dot(e0, normal) < 0;
		result.hitNormal = isFront ? normal : normal * -1.0f;
		result.isFrontFace = isFront;
		result.isHit = true;
		result.t = t;
		result.hitPosition = ray.at(t);
		result.material = material;
		return true;
	}
	result.isHit = false;
	return false;
}

void Model::LoadModel(const std::string& path)
{
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices); // | aiProcess_GenNormals);
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		std::cout << "[Error Model Importer] (The Path is invalid)" << std::endl;
		return;
	}
	loadDirectory_ = path.substr(0, path.find_last_of('/'));
	processNode(scene->mRootNode, scene);		// 将主节点输入processNode递归函数
}

void Model::processNode(aiNode* node, const aiScene* scene)
{
	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		meshes_.push_back(processMesh(mesh, scene));
	}

	for (unsigned int i = 0; i < node->mNumChildren; i++)
	{
		processNode(node->mChildren[i], scene);
	}
}

std::shared_ptr<Mesh> Model::processMesh(aiMesh* mesh, const aiScene* scene)
{
	auto newMesh = std::make_shared<Mesh>();

	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;
	constexpr float Scale = 0.01f;

	// Vertex
	for (unsigned int i = 0; i < mesh->mNumVertices; i++)
	{
		Vertex vertex;
		// position
		glm::vec3 vec;
		vec.x = mesh->mVertices[i].x * Scale;
		vec.y = mesh->mVertices[i].y * Scale;
		vec.z = mesh->mVertices[i].z * Scale;
		vertex.position = vec;
		// normal
		vec.x = mesh->mNormals[i].x;
		vec.y = mesh->mNormals[i].y;
		vec.z = mesh->mNormals[i].z;
		vertex.normal = vec;
		// tangent
		vec.x = mesh->mTangents[i].x;
		vec.y = mesh->mTangents[i].y;
		vec.z = mesh->mTangents[i].z;
		vertex.tangent = vec;
		// texcoord
		if (mesh->mTextureCoords[0])
		{
			glm::vec2 vec2;
			vec2.x = mesh->mTextureCoords[0][i].x;
			vec2.y = 1 - mesh->mTextureCoords[0][i].y;
			// vec2.y = mesh->mTextureCoords[0][i].y;
			vertex.texcoord = vec2;
		}
		else
		{
			vertex.texcoord = glm::vec2(0.0, 0.0);
		}

		vertices.push_back(vertex);
	}

	// Indices
	for (unsigned int i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++)
		{
			indices.push_back(face.mIndices[j]);
		}
	}
		
	// Permute To Triangle
	for (size_t i = 0; i < indices.size(); i += 3)
	{
		std::array<Vertex, 3> verteies;
		uint32_t index1 = indices[i];
		uint32_t index2 = indices[i+1];
		uint32_t index3 = indices[i+2];
		verteies[0] = vertices[index1];
		verteies[1] = vertices[index2];
		verteies[2] = vertices[index3];

		newMesh->triangles.emplace_back(verteies, material);
	}
	trianglesNums_ += newMesh->triangles.size();

	return newMesh;
}

void RTModel::LoadModel(const std::string& path)
{
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices);
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		std::cout << "[Error Model Importer] (The Path is invalid)" << std::endl;
		return;
	}
	loadDirectory_ = path.substr(0, path.find_last_of('/'));

	indices.scratchArray.clear();
	vertices.scratchArray.clear();
	//
	processNode(scene->mRootNode, scene);		// 将主节点输入processNode递归函数

	// ================ Process ================
	vertices.count = vertices.scratchArray.size();
	indices.count = indices.scratchArray.size();

	// Create Buffer
	vulkan::VulkanLocalBuffer::CopierCreateInfo copierInfo {
			.commandPool = commandPool_,
			.transferQueue = queue_
	};

	// Vertex
	VkDeviceSize VerticesSize = vertices.count * sizeof(Vertex);
	vertices.buffer = std::make_shared<vulkan::VulkanLocalBuffer>(allocator_, VerticesSize,
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, copierInfo);
	vertices.buffer->UploadMemory(vertices.scratchArray.data(), VerticesSize, 0);

	vertices.scratchArray.clear();

	// Index
	VkDeviceSize IndicesSize = indices.count * sizeof(uint32_t);
	indices.buffer = std::make_shared<vulkan::VulkanLocalBuffer>(allocator_, IndicesSize,
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, copierInfo);
	indices.buffer->UploadMemory(indices.scratchArray.data(), IndicesSize, 0);

	indices.scratchArray.clear();

}

void RTModel::processNode(aiNode* node, const aiScene* scene)
{
	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		linerMeshes.push_back(processMesh(mesh, scene));
	}

	for (unsigned int i = 0; i < node->mNumChildren; i++)
	{
		processNode(node->mChildren[i], scene);
	}
}

std::shared_ptr<RTMesh> RTModel::processMesh(aiMesh* mesh, const aiScene* scene)
{
	auto newMesh = std::make_shared<RTMesh>();
	newMesh->geometry.firstVertex = vertices.scratchArray.size();
	newMesh->geometry.firstIndex = indices.scratchArray.size();


	constexpr float Scale = 0.01f;

	// Vertex
	for (unsigned int i = 0; i < mesh->mNumVertices; i++)
	{
		Vertex vertex;
		// position
		glm::vec3 vec;
		vec.x = mesh->mVertices[i].x * Scale;
		vec.y = mesh->mVertices[i].y * Scale;
		vec.z = mesh->mVertices[i].z * Scale;
		vertex.position = vec;
		// normal
		vec.x = mesh->mNormals[i].x;
		vec.y = mesh->mNormals[i].y;
		vec.z = mesh->mNormals[i].z;
		vertex.normal = vec;
		// tangent
		vec.x = mesh->mTangents[i].x;
		vec.y = mesh->mTangents[i].y;
		vec.z = mesh->mTangents[i].z;
		vertex.tangent = vec;
		// texcoord
		if (mesh->mTextureCoords[0])
		{
			glm::vec2 vec2;
			vec2.x = mesh->mTextureCoords[0][i].x;
			vec2.y = 1 - mesh->mTextureCoords[0][i].y;
			// vec2.y = mesh->mTextureCoords[0][i].y;
			vertex.texcoord = vec2;
		}
		else
		{
			vertex.texcoord = glm::vec2(0.0, 0.0);
		}
		// 
		vertices.scratchArray.push_back(vertex);
	}

	// Indices
	for (unsigned int i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++)
		{
			// 
			indices.scratchArray.push_back(face.mIndices[j] + newMesh->geometry.firstVertex);
		}
	}

	newMesh->geometry.vertexCount = vertices.scratchArray.size() - newMesh->geometry.firstVertex;
	newMesh->geometry.indexCount = indices.scratchArray.size() - newMesh->geometry.firstIndex;

	// Load Color
	if (mesh->mMaterialIndex >= 0)
	{
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		newMesh->material = loadMaterial(material);
	}

	return newMesh;
}

inline static glm::vec3 toVec3(aiColor3D& color)
{
	return glm::vec3(color.r, color.g, color.b);
}

RTMesh::Material RTModel::loadMaterial(aiMaterial* material)
{
	RTMesh::Material mat;

	// Base Color
	aiColor3D BaseColor;
	if (material->Get(AI_MATKEY_BASE_COLOR, BaseColor) == aiReturn_SUCCESS)
	{
		mat.BaseColor = toVec3(BaseColor);
	}
	if (material->Get(AI_MATKEY_COLOR_DIFFUSE, BaseColor) == aiReturn_SUCCESS)
	{
		mat.BaseColor = toVec3(BaseColor);
	}

	// Emissive Color
	aiColor3D emissiveColor;
	if (material->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor) == aiReturn_SUCCESS)
	{
		mat.EmissiveColor = toVec3(emissiveColor);
	}

	// Specular
	aiColor3D SpecularTint;
	if (material->Get(AI_MATKEY_COLOR_SPECULAR, SpecularTint) == aiReturn_SUCCESS)
	{
		mat.SpecularTint = toVec3(SpecularTint);
	}

	float Specular;
	if (material->Get(AI_MATKEY_SPECULAR_FACTOR, Specular) == aiReturn_SUCCESS)
	{
		mat.Specular = Specular;
	}

	// Roughness
	float Roughness;
	if (material->Get(AI_MATKEY_OPACITY, Roughness) == aiReturn_SUCCESS)
	{
		mat.Roughness = Roughness;
	}
	// Metallic
	float Metallic;
	if (material->Get(AI_MATKEY_REFLECTIVITY, Metallic) == aiReturn_SUCCESS)
	{
		mat.Metallic = Metallic;
	}

	// Anisotropic
	float Anisotropic;
	if (material->Get(AI_MATKEY_ANISOTROPY_FACTOR, Anisotropic) == aiReturn_SUCCESS)
	{
		mat.Anisotropic = Anisotropic;
	}

	//// Subsurface
	//float Subsurface;
	//if (material->Get(AI_MATKEY_ROUGHNESS_FACTOR, Subsurface) == aiReturn_SUCCESS)
	//{
	//	mat.Subsurface = Subsurface;
	//}

	// Texture
	
	// BaseColor
	auto BaseColorTexID = loadMaterialTextures(material, aiTextureType_DIFFUSE);
	if (BaseColorTexID >= 0)
	{
		mat.BaseColorTextureID = BaseColorTexID;
	}
	// IBL
	auto IBLTexID = loadMaterialTextures(material, aiTextureType_NORMALS);
	if (IBLTexID >= 0)
	{
		mat.IBLTextureID = IBLTexID;
	}

	return mat;
}

int RTModel::loadMaterialTextures(aiMaterial* material, aiTextureType type)
{
	using namespace vulkan;

	//for (unsigned int i = 0; i < material->GetTextureCount(type); i++)
	//{		做简化，只导入一张
		int count = material->GetTextureCount(type);
		if (material->GetTextureCount(type) == 0) return -1;

		aiString texname;
		material->GetTexture(type, 0, &texname);
		auto imageLoadedID = g_texturePool->FindID(texname.data);
		if (imageLoadedID >= 0)
		{
			return imageLoadedID;
		}
		else
		{
			std::string path = loadDirectory_ + "/" + std::string(texname.data);

			VulkanLoadedTexture::CopierCreateInfo info;
			info.commandPool = g_pCommandPool;
			info.transferQueue = g_Queue;

			auto tex_ = new VulkanLoadedTexture(g_pVkMemoryAllocator, path, info, VK_IMAGE_USAGE_SAMPLED_BIT);

			SingleTimeCommands cmd(info.commandPool);

			VulkanImage::transitionImageLayout(cmd, *tex_,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1);

			// Sampler
			auto sampler_ = new VulkanSampler(g_Device);

			uint32_t id = g_texturePool->Add(texname.data, vulkanSampleImage(tex_, sampler_));
			return id;
		}
	//}
}

TexturePool::~TexturePool()
{
	for(auto pair : sampleImage)
	{
		delete pair.first;
		delete pair.second;
	}
}

int TexturePool::Add(const std::string& path, vulkanSampleImage image)
{
	if (isExisted(path))
		return -1;
	std::string str = path;
	path_to_imageID_map[str] = sampleImage.size();
	sampleImage.push_back(image);
	imageInfo_.push_back({ image.second->handle(), image.first->getView(), VK_IMAGE_LAYOUT_GENERAL });
	return sampleImage.size() - 1;
}

bool TexturePool::isExisted(const std::string& path)
{
	return path_to_imageID_map.find(path) != path_to_imageID_map.end();
}

int TexturePool::FindID(const std::string& path)
{
	if (isExisted(path))
	{
		return path_to_imageID_map[path];
	}
	return -1;
}
