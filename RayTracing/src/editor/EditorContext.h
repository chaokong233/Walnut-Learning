#pragma once

#include "../Renderer.h"
#include "../ResourceConfig.h"
#include "../ResourceManager.h"
#include "../Scene.h"
#include "../SceneLoader.h"

#include <filesystem>
#include <functional>
#include <string>

#include <glm/glm.hpp>

struct EditorTextureDropTarget
{
	Entity entity{ InvalidEntity };
	uint32_t meshIndex{ 0 };
	MaterialTextureSlot slot{ MaterialTextureSlot::BaseColor };

	bool IsValid() const { return entity != InvalidEntity; }
	void Clear() { entity = InvalidEntity; meshIndex = 0; slot = MaterialTextureSlot::BaseColor; }
};

struct EditorState
{
	Entity selectedEntity{ InvalidEntity };
	bool dirty{ false };
	bool overlayEnabled{ true };
	RenderMode activeRenderMode{ RenderMode::Preview };
	RenderOutputType outputType{ RenderOutputType::FinalColor };
	uint32_t viewportWidth{ 0 };
	uint32_t viewportHeight{ 0 };
	glm::vec2 viewportMin{ 0.0f };
	glm::vec2 viewportMax{ 0.0f };
	float fps{ 0.0f };
	float frameTimeMs{ 0.0f };
	RenderSettings previewSettings;
	RenderSettings finalSettings;
	EditorTextureDropTarget textureDropTarget;
	std::string status;
};

struct EditorActions
{
	std::function<void()> MarkSceneEdited;
	std::function<void()> MarkCameraEdited;
	std::function<bool()> SaveScene;
	std::function<void(const std::filesystem::path&)> RequestLoadScene;
	std::function<void(PrimitiveType)> CreatePrimitive;
	std::function<void()> CreateRadiusLight;
	std::function<void()> CreateAreaLight;
	std::function<void()> DeleteSelected;
	std::function<void()> ToggleRenderMode;
	std::function<void()> SubmitRenderIfNeeded;
	std::function<void(Entity, uint32_t, MaterialTextureSlot, const std::string&)> ApplyMaterialTexture;
	std::function<void(const glm::vec2&)> PickViewport;
};

struct EditorContext
{
	ResourceConfig* resourceConfig{ nullptr };
	ResourceManager* resourceManager{ nullptr };
	SceneManager* sceneManager{ nullptr };
	Scene* scene{ nullptr };
	Renderer* renderer{ nullptr };
	Camera* camera{ nullptr };
	EditorState* state{ nullptr };
	EditorActions* actions{ nullptr };
};
