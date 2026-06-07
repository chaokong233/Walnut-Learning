#include "Scene.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

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

	bool IsAbsolutePath(const std::string& path)
	{
		if (path.size() > 1 && path[1] == ':')
		{
			return true;
		}
		return !path.empty() && (path[0] == '/' || path[0] == '\\');
	}

	std::string ResolveAssetPath(const std::string& directory, const std::string& relativePath)
	{
		if (relativePath.empty() || IsAbsolutePath(relativePath))
		{
			return relativePath;
		}
		return directory + "/" + relativePath;
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

		return ResolveAssetPath(directory, textureName.C_Str());
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

std::shared_ptr<Model> Model::LoadFromFile(const std::string& path)
{
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices);
	if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
	{
		throw std::runtime_error("failed to load model: " + path);
	}

	auto model = std::shared_ptr<Model>(new Model());
	model->path_ = path;
	ProcessNode(*model, scene->mRootNode, scene, GetDirectory(path));

	if (!model->IsValid())
	{
		std::cout << "[Warning Model Importer] Model has no meshes: " << path << std::endl;
	}

	return model;
}

glm::mat4 Transform::GetMatrix() const
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
	auto it = modelCache_.find(path);
	if (it != modelCache_.end())
	{
		return it->second;
	}

	auto model = Model::LoadFromFile(path);
	modelCache_[path] = model;
	return model;
}

Entity& Scene::CreateEntity(const std::string& name, std::shared_ptr<Model> model, const Transform& transform)
{
	Entity& entity = entities_.emplace_back();
	entity.id = nextEntityID_++;
	entity.name = name;
	entity.model = std::move(model);
	entity.transform = transform;
	Touch();
	return entity;
}

void Scene::AddAreaLight(const AreaLight& light)
{
	areaLights_.push_back(light);
	Touch();
}

void Scene::AddRadiusLight(const RadiusLight& light)
{
	radiusLights_.push_back(light);
	Touch();
}

void Scene::Touch()
{
	revision_++;
}
