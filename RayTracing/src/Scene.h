#pragma once

#include "Mesh.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <filesystem>
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

enum class MaterialTextureSlot
{
	BaseColor,
	IBL
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
	static std::shared_ptr<Model> LoadFromFile(const std::string& path, const std::filesystem::path& relativeBase = {});
	static bool TryLoadFromFile(const std::string& path, std::shared_ptr<Model>& model, std::string* errorMessage = nullptr, const std::filesystem::path& relativeBase = {});
	static std::shared_ptr<Model> CreateMissingResourcePlaceholder(const std::string& sourcePath, const std::string& reason);

	inline const std::string& GetPath() const { return path_; }
	inline const std::string& GetResolvedPath() const { return resolvedPath_; }
	inline const std::string& GetLastError() const { return lastError_; }
	inline const std::vector<Mesh>& GetMeshes() const { return meshes_; }
	inline bool IsValid() const { return !meshes_.empty(); }
	void AddMesh(Mesh mesh) { meshes_.push_back(std::move(mesh)); }
	bool RebindSourcePath(const std::string& path, std::string* errorMessage = nullptr, const std::filesystem::path& relativeBase = {});
	bool RebindMaterialTexture(uint32_t meshIndex, MaterialTextureSlot slot, const std::string& path, std::string* errorMessage = nullptr);

private:
	std::string path_;
	std::string resolvedPath_;
	std::string lastError_;
	std::vector<Mesh> meshes_;
};

using Entity = uint32_t;
constexpr Entity InvalidEntity = 0;

struct TagComponent
{
	std::string name;
};

struct TransformComponent
{
	glm::vec3 translation{ 0.0f };
	glm::vec3 rotation{ 0.0f };
	glm::vec3 scale{ 1.0f };

	glm::mat4 GetMatrix() const;
};

struct MeshRendererComponent
{
	std::shared_ptr<Model> model;
	std::string modelAssetID;
	bool visible{ true };
};

struct RadiusLightComponent
{
	glm::vec3 color{ 1.0f };
	float intensity{ 1.0f };
	float radius{ 0.1f };
};

struct AreaLightComponent
{
	glm::vec3 color{ 1.0f };
	float intensity{ 1.0f };
	glm::vec3 direction{ 0.0f, -1.0f, 0.0f };
	float width{ 1.0f };
	float height{ 1.0f };
};

class Scene
{
public:
	std::shared_ptr<Model> LoadModel(const std::string& path);
	std::shared_ptr<Model> TryLoadModel(const std::string& path, std::string* errorMessage = nullptr, const std::filesystem::path& relativeBase = {});
	bool RebindModel(Entity entity, const std::string& path, std::string* errorMessage = nullptr, const std::filesystem::path& relativeBase = {});

	Entity CreateEntity(const std::string& name, const TransformComponent& transform = {});
	Entity CreateModelEntity(const std::string& name, std::shared_ptr<Model> model, const TransformComponent& transform = {}, const std::string& modelAssetID = {});

	MeshRendererComponent& AddMeshRenderer(Entity entity, std::shared_ptr<Model> model, bool visible = true, const std::string& modelAssetID = {});
	RadiusLightComponent& AddRadiusLight(Entity entity, const RadiusLightComponent& light = {});
	AreaLightComponent& AddAreaLight(Entity entity, const AreaLightComponent& light = {});

	inline const std::vector<Entity>& GetEntities() const { return entities_; }
	inline uint64_t GetRevision() const { return revision_; }

	bool IsValid(Entity entity) const;
	Entity GetFirstRadiusLightEntity() const;

	TagComponent* TryGetTag(Entity entity);
	TransformComponent* TryGetTransform(Entity entity);
	MeshRendererComponent* TryGetMeshRenderer(Entity entity);
	RadiusLightComponent* TryGetRadiusLight(Entity entity);
	AreaLightComponent* TryGetAreaLight(Entity entity);

	const TagComponent* TryGetTag(Entity entity) const;
	const TransformComponent* TryGetTransform(Entity entity) const;
	const MeshRendererComponent* TryGetMeshRenderer(Entity entity) const;
	const RadiusLightComponent* TryGetRadiusLight(Entity entity) const;
	const AreaLightComponent* TryGetAreaLight(Entity entity) const;

	TransformComponent& GetTransform(Entity entity);
	MeshRendererComponent& GetMeshRenderer(Entity entity);
	RadiusLightComponent& GetRadiusLight(Entity entity);
	AreaLightComponent& GetAreaLight(Entity entity);

	void Touch();

private:
	void RequireEntity(Entity entity) const;

	std::vector<Entity> entities_;
	std::unordered_map<Entity, TagComponent> tags_;
	std::unordered_map<Entity, TransformComponent> transforms_;
	std::unordered_map<Entity, MeshRendererComponent> meshRenderers_;
	std::unordered_map<Entity, RadiusLightComponent> radiusLights_;
	std::unordered_map<Entity, AreaLightComponent> areaLights_;
	std::unordered_map<std::string, std::shared_ptr<Model>> modelCache_;
	Entity nextEntityID_{ 1 };
	uint64_t revision_{ 1 };
};
