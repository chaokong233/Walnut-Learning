#pragma once

#include "EditorContext.h"
#include "EditorPanels.h"

#include "Walnut/Layer.h"
#include "Walnut/Timer.h"

#include <memory>
#include <string>

namespace Walnut
{
	class Application;
}

class EditorLayer : public Walnut::Layer
{
public:
	EditorLayer();
	~EditorLayer() override;

	void OnAttach() override;
	void OnDetach() override;
	void OnUpdate(float timestep) override;
	void OnUIRender() override;

	void RenderMainMenu(Walnut::Application& app);
	void RequestClose();
	void HandleDroppedFile(const std::filesystem::path& path);

private:
	void InitializeRenderSettings();
	void LoadStartupScene();
	void ReleaseVulkanResources();
	void UpdateFrameStats();
	void HandleShortcuts();
	void SubmitRenderIfNeeded();
	void RenderPendingModals();
	std::filesystem::path GetEditorSettingsPath() const;
	void LoadEditorSettings();
	void SaveEditorSettings() const;

	void MarkSceneEdited();
	void MarkCameraEdited();
	bool SaveScene();
	void RequestLoadScene(const std::filesystem::path& scenePath);
	bool LoadSceneNow(const std::filesystem::path& scenePath);
	void CreatePrimitive(PrimitiveType type);
	void CreateRadiusLight();
	void CreateAreaLight();
	void DeleteSelected();
	void ToggleRenderMode();
	void ApplyMaterialTexture(Entity entity, uint32_t meshIndex, MaterialTextureSlot slot, const std::string& path);
	void PickViewport(const glm::vec2& viewportPosition);
	void PruneResourcesForActiveScene();

	EditorContext BuildContext();
	EditorActions BuildActions();
	RenderPacket BuildRenderPacket(const RenderSettings& settings) const;

	ResourceConfig resourceConfig_;
	ResourceManager resourceManager_;
	SceneManager sceneManager_;
	Scene scene_;
	std::unique_ptr<Renderer> renderer_;
	Camera camera_{ glm::vec3(0.0f, 0.0f, 4.0f), glm::vec3(0.0f, 0.0f, -1.0f) };
	EditorState state_;

	SceneHierarchyPanel sceneHierarchyPanel_;
	InspectorPanel inspectorPanel_;
	RenderSettingsPanel renderSettingsPanel_;
	InfoPanel infoPanel_;
	ViewportPanel viewportPanel_;

	Walnut::Timer frameTimer_;
	float accumulatedFrameTimeMs_{ 0.0f };
	uint32_t accumulatedFrameCount_{ 0 };
	float statsWindowMs_{ 1000.0f };
	bool vulkanResourcesReleased_{ false };
	bool ctrlSSavedLastFrame_{ false };
	bool pendingCloseModal_{ false };
	bool pendingSceneSwitchModal_{ false };
	std::filesystem::path pendingScenePath_;
	std::string startupStatus_;
};
