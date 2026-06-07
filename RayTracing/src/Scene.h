#pragma once

#include "Mesh.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct Material
{
	glm::vec3 BaseColor{ glm::vec3(1.0f) };
	glm::vec3 EmissiveColor{ glm::vec3(0.0f) };
	glm::vec3 SpecularTint{ glm::vec3(0.2f) };
	float Roughness{ 0.5f };
	float Metallic{ 0.0f };
	float Specular{ 0.5f };
	float Subsurface{ 0.0f };
	float Anisotropic{ 0.0f };

	std::string BaseColorTexturePath;
	std::string IBLTexturePath;
};

struct Mesh
{
	std::string name;
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	Material material;
};

class Model
{
public:
	static std::shared_ptr<Model> LoadFromFile(const std::string& path);

	inline const std::string& GetPath() const { return path_; }
	inline const std::vector<Mesh>& GetMeshes() const { return meshes_; }
	inline bool IsValid() const { return !meshes_.empty(); }
	void AddMesh(Mesh mesh) { meshes_.push_back(std::move(mesh)); }

private:
	std::string path_;
	std::vector<Mesh> meshes_;
};

struct Transform
{
	glm::vec3 translation{ 0.0f };
	glm::vec3 rotation{ 0.0f };
	glm::vec3 scale{ 1.0f };

	glm::mat4 GetMatrix() const;
};

struct Entity
{
	uint32_t id{ 0 };
	std::string name;
	Transform transform;
	std::shared_ptr<Model> model;
	bool visible{ true };
};

struct AreaLight
{
	glm::vec3 beginPos{ 0.0f };
	glm::vec3 u{ 1.0f, 0.0f, 0.0f };
	glm::vec3 v{ 0.0f, 0.0f, 1.0f };
	glm::vec3 color{ 1.0f };
	glm::vec3 rayDir{ 0.0f, -1.0f, 0.0f };
};

struct RadiusLight
{
	glm::vec3 centerPos{ 0.0f };
	glm::vec3 color{ 1.0f };
	float radius{ 0.1f };
};

class Scene
{
public:
	std::shared_ptr<Model> LoadModel(const std::string& path);

	Entity& CreateEntity(const std::string& name, std::shared_ptr<Model> model, const Transform& transform = {});
	void AddAreaLight(const AreaLight& light);
	void AddRadiusLight(const RadiusLight& light);

	inline const std::vector<Entity>& GetEntities() const { return entities_; }
	inline const std::vector<AreaLight>& GetAreaLights() const { return areaLights_; }
	inline const std::vector<RadiusLight>& GetRadiusLights() const { return radiusLights_; }
	inline uint64_t GetRevision() const { return revision_; }

	void Touch();

private:
	std::vector<Entity> entities_;
	std::vector<AreaLight> areaLights_;
	std::vector<RadiusLight> radiusLights_;
	std::unordered_map<std::string, std::shared_ptr<Model>> modelCache_;
	uint32_t nextEntityID_{ 1 };
	uint64_t revision_{ 1 };
};
