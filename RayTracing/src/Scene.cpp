#include "Scene.h"

#include "Walnut/RuntimePath.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace
{
	constexpr float ModelImportScale = 0.01f;

	std::string GetDirectory(const std::string& path)
	{
		const size_t slash = path.find_last_of("/\\");
		if (slash == std::string::npos)
		{
			return ".";
		}
		return path.substr(0, slash);
	}

	glm::vec3 ToVec3(const aiColor3D& color)
	{
		return glm::vec3(color.r, color.g, color.b);
	}

	std::string LoadMaterialTexturePath(aiMaterial* material, aiTextureType type, const std::string& directory)
	{
		if (!material || material->GetTextureCount(type) == 0)
		{
			return {};
		}

		aiString textureName;
		if (material->GetTexture(type, 0, &textureName) != aiReturn_SUCCESS)
		{
			return {};
		}

		std::filesystem::path resolvedPath;
		std::string error;
		if (Walnut::RuntimePath::TryResolveExistingPath(textureName.C_Str(), resolvedPath, &error, directory))
		{
			return resolvedPath.generic_string();
		}

		std::filesystem::path reboundPath = std::filesystem::path(directory) / textureName.C_Str();
		std::cerr << "[Warning Model Importer] " << error << std::endl;
		return reboundPath.lexically_normal().generic_string();
	}

	Material LoadMaterial(aiMaterial* material, const std::string& directory)
	{
		Material result;
		if (!material)
		{
			return result;
		}

		aiColor3D color;
		if (material->Get(AI_MATKEY_BASE_COLOR, color) == aiReturn_SUCCESS)
		{
			result.BaseColor = ToVec3(color);
		}
		if (material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == aiReturn_SUCCESS)
		{
			result.BaseColor = ToVec3(color);
		}
		if (material->Get(AI_MATKEY_COLOR_EMISSIVE, color) == aiReturn_SUCCESS)
		{
			result.EmissiveColor = ToVec3(color);
		}
		if (material->Get(AI_MATKEY_COLOR_SPECULAR, color) == aiReturn_SUCCESS)
		{
			result.SpecularTint = ToVec3(color);
		}

		float value = 0.0f;
		if (material->Get(AI_MATKEY_SPECULAR_FACTOR, value) == aiReturn_SUCCESS)
		{
			result.Specular = value;
		}
		if (material->Get(AI_MATKEY_OPACITY, value) == aiReturn_SUCCESS)
		{
			result.Roughness = value;
		}
		if (material->Get(AI_MATKEY_REFLECTIVITY, value) == aiReturn_SUCCESS)
		{
			result.Metallic = value;
		}
		if (material->Get(AI_MATKEY_ANISOTROPY_FACTOR, value) == aiReturn_SUCCESS)
		{
			result.Anisotropic = value;
		}

		result.BaseColorTexturePath = LoadMaterialTexturePath(material, aiTextureType_DIFFUSE, directory);
		result.IBLTexturePath = LoadMaterialTexturePath(material, aiTextureType_NORMALS, directory);
		return result;
	}

	Mesh ProcessMesh(aiMesh* mesh, const aiScene* scene, const std::string& directory)
	{
		Mesh result;
		result.name = mesh->mName.C_Str();

		result.vertices.reserve(mesh->mNumVertices);
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
		{
			Vertex vertex{};

			vertex.position = glm::vec3(
				mesh->mVertices[i].x * ModelImportScale,
				mesh->mVertices[i].y * ModelImportScale,
				mesh->mVertices[i].z * ModelImportScale);

			if (mesh->HasNormals())
			{
				vertex.normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
			}
			else
			{
				vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
			}

			if (mesh->HasTangentsAndBitangents())
			{
				vertex.tangent = glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
			}
			else
			{
				vertex.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
			}

			if (mesh->mTextureCoords[0])
			{
				vertex.texcoord = glm::vec2(mesh->mTextureCoords[0][i].x, 1.0f - mesh->mTextureCoords[0][i].y);
			}
			else
			{
				vertex.texcoord = glm::vec2(0.0f);
			}

			result.vertices.push_back(vertex);
		}

		for (unsigned int i = 0; i < mesh->mNumFaces; i++)
		{
			const aiFace& face = mesh->mFaces[i];
			for (unsigned int j = 0; j < face.mNumIndices; j++)
			{
				result.indices.push_back(face.mIndices[j]);
			}
		}

		if (mesh->mMaterialIndex >= 0)
		{
			result.material = LoadMaterial(scene->mMaterials[mesh->mMaterialIndex], directory);
		}

		return result;
	}

	void ProcessNode(Model& model, aiNode* node, const aiScene* scene, const std::string& directory)
	{
		for (unsigned int i = 0; i < node->mNumMeshes; i++)
		{
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			model.AddMesh(ProcessMesh(mesh, scene, directory));
		}

		for (unsigned int i = 0; i < node->mNumChildren; i++)
		{
			ProcessNode(model, node->mChildren[i], scene, directory);
		}
	}
}

std::shared_ptr<Model> Model::LoadFromFile(const std::string& path, const std::filesystem::path& relativeBase)
{
	std::filesystem::path resolvedPath;
	std::string error;
	if (!Walnut::RuntimePath::TryResolveExistingPath(path, resolvedPath, &error, relativeBase))
	{
		throw std::runtime_error(error);
	}

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(resolvedPath.generic_string(), aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices);
	if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
	{
		throw std::runtime_error("failed to load model: " + resolvedPath.generic_string() + " (" + importer.GetErrorString() + ")");
	}

	auto model = std::shared_ptr<Model>(new Model());
	model->path_ = path;
	model->resolvedPath_ = resolvedPath.generic_string();
	ProcessNode(*model, scene->mRootNode, scene, GetDirectory(model->resolvedPath_));

	if (!model->IsValid())
	{
		std::cout << "[Warning Model Importer] Model has no meshes: " << model->resolvedPath_ << std::endl;
	}

	return model;
}

bool Model::TryLoadFromFile(const std::string& path, std::shared_ptr<Model>& model, std::string* errorMessage, const std::filesystem::path& relativeBase)
{
	try
	{
		model = LoadFromFile(path, relativeBase);
		if (errorMessage)
		{
			errorMessage->clear();
		}
		return true;
	}
	catch (const std::exception& e)
	{
		model.reset();
		if (errorMessage)
		{
			*errorMessage = e.what();
		}
		return false;
	}
}

std::shared_ptr<Model> Model::CreateMissingResourcePlaceholder(const std::string& sourcePath, const std::string& reason)
{
	auto model = std::shared_ptr<Model>(new Model());
	model->path_ = sourcePath;
	model->lastError_ = reason;

	Mesh mesh;
	mesh.name = "Missing Resource Placeholder";
	mesh.material.BaseColor = glm::vec3(1.0f, 0.0f, 1.0f);
	mesh.material.EmissiveColor = glm::vec3(0.15f, 0.0f, 0.15f);
	mesh.vertices = {
		{ glm::vec3(-0.75f, -0.75f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f) },
		{ glm::vec3(0.75f, -0.75f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 0.0f) },
		{ glm::vec3(0.75f, 0.75f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 1.0f) },
		{ glm::vec3(-0.75f, 0.75f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 1.0f) }
	};
	mesh.indices = { 0, 1, 2, 2, 3, 0 };
	model->AddMesh(std::move(mesh));

	return model;
}

bool Model::RebindSourcePath(const std::string& path, std::string* errorMessage, const std::filesystem::path& relativeBase)
{
	std::shared_ptr<Model> loadedModel;
	if (!TryLoadFromFile(path, loadedModel, errorMessage, relativeBase))
	{
		if (errorMessage)
		{
			lastError_ = *errorMessage;
		}
		return false;
	}

	path_ = loadedModel->path_;
	resolvedPath_ = loadedModel->resolvedPath_;
	lastError_.clear();
	meshes_ = std::move(loadedModel->meshes_);
	return true;
}

bool Model::RebindMaterialTexture(uint32_t meshIndex, MaterialTextureSlot slot, const std::string& path, std::string* errorMessage)
{
	if (meshIndex >= meshes_.size())
	{
		if (errorMessage)
		{
			*errorMessage = "mesh index is out of range";
		}
		return false;
	}

	std::string storedPath;
	if (!path.empty())
	{
		std::filesystem::path resolvedPath;
		const std::filesystem::path modelDirectory = resolvedPath_.empty() ? std::filesystem::path{} : std::filesystem::path(resolvedPath_).parent_path();
		if (!Walnut::RuntimePath::TryResolveExistingPath(path, resolvedPath, errorMessage, modelDirectory))
		{
			return false;
		}
		storedPath = resolvedPath.generic_string();
	}

	Material& material = meshes_[meshIndex].material;
	switch (slot)
	{
	case MaterialTextureSlot::BaseColor:
		material.BaseColorTexturePath = storedPath;
		break;
	case MaterialTextureSlot::IBL:
		material.IBLTexturePath = storedPath;
		break;
	}

	if (errorMessage)
	{
		errorMessage->clear();
	}
	return true;
}

glm::mat4 TransformComponent::GetMatrix() const
{
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation);
	transform = glm::rotate(transform, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	transform = glm::rotate(transform, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	transform = glm::rotate(transform, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
	transform = glm::scale(transform, scale);
	return transform;
}

std::shared_ptr<Model> Scene::LoadModel(const std::string& path)
{
	std::string error;
	std::shared_ptr<Model> model = TryLoadModel(path, &error);
	if (!model)
	{
		throw std::runtime_error(error);
	}
	return model;
}

std::shared_ptr<Model> Scene::TryLoadModel(const std::string& path, std::string* errorMessage, const std::filesystem::path& relativeBase)
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

	modelCache_[cacheKey] = model;
	return model;
}

bool Scene::RebindModel(Entity entity, const std::string& path, std::string* errorMessage, const std::filesystem::path& relativeBase)
{
	RequireEntity(entity);

	MeshRendererComponent* meshRenderer = TryGetMeshRenderer(entity);
	if (!meshRenderer)
	{
		if (errorMessage)
		{
			*errorMessage = "entity does not have MeshRendererComponent";
		}
		return false;
	}

	std::shared_ptr<Model> model = TryLoadModel(path, errorMessage, relativeBase);
	if (!model)
	{
		return false;
	}

	meshRenderer->model = model;
	meshRenderer->modelAssetID.clear();
	Touch();
	return true;
}

Entity Scene::CreateEntity(const std::string& name, const TransformComponent& transform)
{
	const Entity entity = nextEntityID_++;
	entities_.push_back(entity);
	tags_[entity] = TagComponent{ name };
	transforms_[entity] = transform;
	Touch();
	return entity;
}

Entity Scene::CreateModelEntity(const std::string& name, std::shared_ptr<Model> model, const TransformComponent& transform, const std::string& modelAssetID)
{
	const Entity entity = CreateEntity(name, transform);
	AddMeshRenderer(entity, std::move(model), true, modelAssetID);
	return entity;
}

MeshRendererComponent& Scene::AddMeshRenderer(Entity entity, std::shared_ptr<Model> model, bool visible, const std::string& modelAssetID)
{
	RequireEntity(entity);
	MeshRendererComponent& component = meshRenderers_[entity];
	component.model = std::move(model);
	component.modelAssetID = modelAssetID;
	component.visible = visible;
	Touch();
	return component;
}

RadiusLightComponent& Scene::AddRadiusLight(Entity entity, const RadiusLightComponent& light)
{
	RequireEntity(entity);
	RadiusLightComponent& component = radiusLights_[entity];
	component = light;
	Touch();
	return component;
}

AreaLightComponent& Scene::AddAreaLight(Entity entity, const AreaLightComponent& light)
{
	RequireEntity(entity);
	AreaLightComponent& component = areaLights_[entity];
	component = light;
	Touch();
	return component;
}

bool Scene::IsValid(Entity entity) const
{
	return entity != InvalidEntity && transforms_.find(entity) != transforms_.end();
}

Entity Scene::GetFirstRadiusLightEntity() const
{
	for (Entity entity : entities_)
	{
		if (radiusLights_.find(entity) != radiusLights_.end())
		{
			return entity;
		}
	}
	return InvalidEntity;
}

TagComponent* Scene::TryGetTag(Entity entity)
{
	auto it = tags_.find(entity);
	return it == tags_.end() ? nullptr : &it->second;
}

TransformComponent* Scene::TryGetTransform(Entity entity)
{
	auto it = transforms_.find(entity);
	return it == transforms_.end() ? nullptr : &it->second;
}

MeshRendererComponent* Scene::TryGetMeshRenderer(Entity entity)
{
	auto it = meshRenderers_.find(entity);
	return it == meshRenderers_.end() ? nullptr : &it->second;
}

RadiusLightComponent* Scene::TryGetRadiusLight(Entity entity)
{
	auto it = radiusLights_.find(entity);
	return it == radiusLights_.end() ? nullptr : &it->second;
}

AreaLightComponent* Scene::TryGetAreaLight(Entity entity)
{
	auto it = areaLights_.find(entity);
	return it == areaLights_.end() ? nullptr : &it->second;
}

const TagComponent* Scene::TryGetTag(Entity entity) const
{
	auto it = tags_.find(entity);
	return it == tags_.end() ? nullptr : &it->second;
}

const TransformComponent* Scene::TryGetTransform(Entity entity) const
{
	auto it = transforms_.find(entity);
	return it == transforms_.end() ? nullptr : &it->second;
}

const MeshRendererComponent* Scene::TryGetMeshRenderer(Entity entity) const
{
	auto it = meshRenderers_.find(entity);
	return it == meshRenderers_.end() ? nullptr : &it->second;
}

const RadiusLightComponent* Scene::TryGetRadiusLight(Entity entity) const
{
	auto it = radiusLights_.find(entity);
	return it == radiusLights_.end() ? nullptr : &it->second;
}

const AreaLightComponent* Scene::TryGetAreaLight(Entity entity) const
{
	auto it = areaLights_.find(entity);
	return it == areaLights_.end() ? nullptr : &it->second;
}

TransformComponent& Scene::GetTransform(Entity entity)
{
	RequireEntity(entity);
	return transforms_.at(entity);
}

MeshRendererComponent& Scene::GetMeshRenderer(Entity entity)
{
	RequireEntity(entity);
	auto it = meshRenderers_.find(entity);
	if (it == meshRenderers_.end())
	{
		throw std::runtime_error("entity does not have MeshRendererComponent");
	}
	return it->second;
}

RadiusLightComponent& Scene::GetRadiusLight(Entity entity)
{
	RequireEntity(entity);
	auto it = radiusLights_.find(entity);
	if (it == radiusLights_.end())
	{
		throw std::runtime_error("entity does not have RadiusLightComponent");
	}
	return it->second;
}

AreaLightComponent& Scene::GetAreaLight(Entity entity)
{
	RequireEntity(entity);
	auto it = areaLights_.find(entity);
	if (it == areaLights_.end())
	{
		throw std::runtime_error("entity does not have AreaLightComponent");
	}
	return it->second;
}

void Scene::Touch()
{
	revision_++;
}

void Scene::RequireEntity(Entity entity) const
{
	if (!IsValid(entity))
	{
		throw std::runtime_error("invalid scene entity");
	}
}
