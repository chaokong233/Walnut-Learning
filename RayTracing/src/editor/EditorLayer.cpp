#include "EditorLayer.h"

#include "Walnut/Application.h"
#include "Walnut/Input/Input.h"
#include "Walnut/RuntimePath.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <unordered_set>

namespace
{
	using Json = nlohmann::json;

	EditorLayer* s_ActiveEditorLayer = nullptr;

	ResourceConfig LoadResourceConfig()
	{
		ResourceConfig resourceConfig;
		std::string error;
		if (!resourceConfig.Load(ResourceConfig::GetDefaultConfigPath(), &error))
		{
			std::cerr << "[Error ResourceConfig] " << error << std::endl;
		}
		return resourceConfig;
	}

	Entity FindFirstEditableEntity(Scene& scene)
	{
		for (Entity entity : scene.GetEntities())
		{
			if (scene.TryGetMeshRenderer(entity) || scene.TryGetRadiusLight(entity) || scene.TryGetAreaLight(entity))
			{
				return entity;
			}
		}
		return InvalidEntity;
	}

	Scene CreateFallbackScene(const std::string& reason)
	{
		Scene scene;
		auto placeholder = Model::CreateMissingResourcePlaceholder("scene loading failed", reason);
		scene.CreateModelEntity("Missing Scene", placeholder);

		TransformComponent lightTransform;
		lightTransform.translation = glm::vec3(0.0f, 0.8f, 0.0f);
		const Entity radiusLightEntity = scene.CreateEntity("Radius Light", lightTransform);
		RadiusLightComponent light;
		light.color = glm::vec3(1.15f, 0.8f, 0.27f);
		light.radius = 0.15f;
		scene.AddRadiusLight(radiusLightEntity, light);
		return scene;
	}

	void ApplyCameraSettings(Camera& camera, const SceneCameraSettings& settings)
	{
		camera.SetView(settings.position, settings.front);
		camera.SetFocusDistance(settings.focusDistance);
		camera.SetDOFFocusDistance(settings.dofFocusDistance);
		camera.SetDOFLensRadius(settings.lensRadius);
		camera.SetUseDOF(settings.useDOF);
	}

	SceneCameraSettings CaptureCameraSettings(const Camera& camera)
	{
		SceneCameraSettings settings;
		settings.position = camera.GetPosition();
		settings.front = camera.GetFront();
		settings.focusDistance = camera.GetFocusDistance();
		settings.dofFocusDistance = camera.GetDOFFocusDistance();
		settings.lensRadius = camera.GetDOFLensRadius();
		settings.useDOF = camera.IsDOFEnabled();
		return settings;
	}

	std::string ToLowerExtension(const std::filesystem::path& path)
	{
		std::string extension = path.extension().generic_string();
		std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c)
		{
			return static_cast<char>(std::tolower(c));
		});
		return extension;
	}

	bool IsModelExtension(const std::string& extension)
	{
		return extension == ".fbx" || extension == ".obj" || extension == ".gltf" || extension == ".glb";
	}

	bool IsTextureExtension(const std::string& extension)
	{
		return extension == ".png" || extension == ".jpg" || extension == ".jpeg";
	}

	const char* TextureSlotName(MaterialTextureSlot slot)
	{
		switch (slot)
		{
		case MaterialTextureSlot::BaseColor:
			return "Base Color";
		case MaterialTextureSlot::Metallic:
			return "Metallic";
		case MaterialTextureSlot::Roughness:
			return "Roughness";
		case MaterialTextureSlot::Normal:
			return "Normal";
		case MaterialTextureSlot::IBL:
			return "IBL";
		}

		return "Texture";
	}

	const char* RenderModeName(RenderMode mode)
	{
		return mode == RenderMode::Final ? "final" : "preview";
	}

	RenderMode ReadRenderMode(const std::string& value, RenderMode fallback)
	{
		if (value == "final")
		{
			return RenderMode::Final;
		}
		if (value == "preview")
		{
			return RenderMode::Preview;
		}
		return fallback;
	}

	const char* RenderOutputTypeName(RenderOutputType type)
	{
		switch (type)
		{
		case RenderOutputType::FinalColor:
			return "finalColor";
		case RenderOutputType::Albedo:
			return "albedo";
		case RenderOutputType::Normal:
			return "normal";
		}
		return "finalColor";
	}

	RenderOutputType ReadRenderOutputType(const std::string& value, RenderOutputType fallback)
	{
		if (value == "albedo")
		{
			return RenderOutputType::Albedo;
		}
		if (value == "normal")
		{
			return RenderOutputType::Normal;
		}
		if (value == "finalColor")
		{
			return RenderOutputType::FinalColor;
		}
		return fallback;
	}

	uint32_t ReadUInt(const Json& json, const char* key, uint32_t fallback)
	{
		if (!json.contains(key) || !json[key].is_number_integer())
		{
			return fallback;
		}
		const int value = json[key].get<int>();
		return value < 0 ? fallback : static_cast<uint32_t>(value);
	}

	RenderSettings ReadRenderSettings(const Json& json, RenderSettings fallback)
	{
		if (!json.is_object())
		{
			return fallback;
		}

		fallback.maxSamples = ReadUInt(json, "maxSamples", fallback.maxSamples);
		fallback.minSamples = ReadUInt(json, "minSamples", fallback.minSamples);
		fallback.maxBounceCount = ReadUInt(json, "maxBounceCount", fallback.maxBounceCount);
		fallback.noiseThreshold = json.value("noiseThreshold", fallback.noiseThreshold);
		fallback.adaptiveNoise = json.value("adaptiveNoise", fallback.adaptiveNoise);
		fallback.denoise = json.value("denoise", fallback.denoise);
		return fallback;
	}

	Json WriteRenderSettings(const RenderSettings& settings)
	{
		Json json;
		json["maxSamples"] = settings.maxSamples;
		json["minSamples"] = settings.minSamples;
		json["maxBounceCount"] = settings.maxBounceCount;
		json["noiseThreshold"] = settings.noiseThreshold;
		json["adaptiveNoise"] = settings.adaptiveNoise;
		json["denoise"] = settings.denoise;
		return json;
	}

	struct Ray
	{
		glm::vec3 origin{ 0.0f };
		glm::vec3 direction{ 0.0f, 0.0f, -1.0f };
	};

	bool IntersectTriangle(const Ray& ray, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, float& distance)
	{
		constexpr float epsilon = 1e-6f;
		const glm::vec3 edge1 = v1 - v0;
		const glm::vec3 edge2 = v2 - v0;
		const glm::vec3 p = glm::cross(ray.direction, edge2);
		const float determinant = glm::dot(edge1, p);
		if (std::abs(determinant) < epsilon)
		{
			return false;
		}

		const float invDet = 1.0f / determinant;
		const glm::vec3 t = ray.origin - v0;
		const float u = glm::dot(t, p) * invDet;
		if (u < 0.0f || u > 1.0f)
		{
			return false;
		}

		const glm::vec3 q = glm::cross(t, edge1);
		const float v = glm::dot(ray.direction, q) * invDet;
		if (v < 0.0f || u + v > 1.0f)
		{
			return false;
		}

		const float hitDistance = glm::dot(edge2, q) * invDet;
		if (hitDistance <= epsilon)
		{
			return false;
		}

		distance = hitDistance;
		return true;
	}

	bool IntersectSphere(const Ray& ray, const glm::vec3& center, float radius, float& distance)
	{
		const glm::vec3 oc = ray.origin - center;
		const float a = glm::dot(ray.direction, ray.direction);
		const float b = 2.0f * glm::dot(oc, ray.direction);
		const float c = glm::dot(oc, oc) - radius * radius;
		const float discriminant = b * b - 4.0f * a * c;
		if (discriminant < 0.0f)
		{
			return false;
		}

		const float root = std::sqrt(discriminant);
		const float t0 = (-b - root) / (2.0f * a);
		const float t1 = (-b + root) / (2.0f * a);
		distance = t0 > 0.0f ? t0 : t1;
		return distance > 0.0f;
	}

	bool BuildViewportRay(const Camera& camera, const EditorState& state, const glm::vec2& viewportPosition, Ray& ray)
	{
		if (state.viewportWidth == 0 || state.viewportHeight == 0)
		{
			return false;
		}

		const float u = std::clamp(viewportPosition.x / static_cast<float>(state.viewportWidth), 0.0f, 1.0f);
		const float v = 1.0f - std::clamp(viewportPosition.y / static_cast<float>(state.viewportHeight), 0.0f, 1.0f);
		ray.origin = camera.GetPosition();
		ray.direction = glm::normalize(camera.relative_left_down_corner_ + camera.horizontal_ * u + camera.vertical_ * v);
		return true;
	}

	void DropCallback(GLFWwindow*, int count, const char** paths)
	{
		if (!s_ActiveEditorLayer)
		{
			return;
		}

		for (int i = 0; i < count; i++)
		{
			s_ActiveEditorLayer->HandleDroppedFile(paths[i]);
		}
	}

	void WindowCloseCallback(GLFWwindow* window)
	{
		if (s_ActiveEditorLayer)
		{
			s_ActiveEditorLayer->RequestClose();
			if (s_ActiveEditorLayer)
			{
				glfwSetWindowShouldClose(window, GLFW_FALSE);
			}
		}
	}
}

EditorLayer::EditorLayer()
	: resourceConfig_(LoadResourceConfig()), sceneManager_(resourceManager_)
{
	InitializeRenderSettings();
	LoadStartupScene();
	LoadEditorSettings();
	renderer_ = std::make_unique<Renderer>(scene_, resourceConfig_, resourceManager_);
	state_.selectedEntity = FindFirstEditableEntity(scene_);
	if (!startupStatus_.empty())
	{
		state_.status = startupStatus_;
	}
}

EditorLayer::~EditorLayer()
{
	SaveEditorSettings();
	ReleaseVulkanResources();
}

void EditorLayer::OnAttach()
{
	s_ActiveEditorLayer = this;
	GLFWwindow* window = Walnut::Application::Get().GetWindowHandle();
	glfwSetDropCallback(window, DropCallback);
	glfwSetWindowCloseCallback(window, WindowCloseCallback);
}

void EditorLayer::OnDetach()
{
	SaveEditorSettings();
	if (s_ActiveEditorLayer == this)
	{
		GLFWwindow* window = Walnut::Application::Get().GetWindowHandle();
		glfwSetDropCallback(window, nullptr);
		glfwSetWindowCloseCallback(window, nullptr);
		s_ActiveEditorLayer = nullptr;
	}
	ReleaseVulkanResources();
}

void EditorLayer::OnUpdate(float timestep)
{
	const glm::vec3 previousPosition = camera_.GetPosition();
	const glm::vec3 previousFront = camera_.GetFront();
	camera_.Tick(timestep, std::max(state_.viewportWidth, 1u), std::max(state_.viewportHeight, 1u));

	if (glm::distance(previousPosition, camera_.GetPosition()) > 1e-5f ||
		glm::distance(previousFront, camera_.GetFront()) > 1e-5f)
	{
		MarkCameraEdited();
	}
}

void EditorLayer::OnUIRender()
{
	UpdateFrameStats();
	HandleShortcuts();

	EditorActions actions = BuildActions();
	EditorContext context = BuildContext();
	context.actions = &actions;

	sceneHierarchyPanel_.OnUIRender(context);
	inspectorPanel_.OnUIRender(context);
	renderSettingsPanel_.OnUIRender(context);
	infoPanel_.OnUIRender(context);
	viewportPanel_.OnUIRender(context);

	RenderPendingModals();
}

void EditorLayer::RenderMainMenu(Walnut::Application&)
{
	if (ImGui::BeginMenu("File"))
	{
		if (ImGui::MenuItem("Save", "Ctrl+S"))
		{
			SaveScene();
		}
		if (ImGui::MenuItem("Exit"))
		{
			RequestClose();
		}
		ImGui::EndMenu();
	}
}

void EditorLayer::RequestClose()
{
	if (state_.dirty)
	{
		pendingCloseModal_ = true;
		return;
	}

	Walnut::Application::Get().Close();
}

void EditorLayer::HandleDroppedFile(const std::filesystem::path& path)
{
	const std::string extension = ToLowerExtension(path);
	if (IsModelExtension(extension))
	{
		std::string error;
		const std::filesystem::path base = sceneManager_.GetActiveScenePath().parent_path();
		std::shared_ptr<Model> model = resourceManager_.LoadModelFile(path.generic_string(), base, &error);
		if (!model)
		{
			state_.status = error;
			return;
		}

		TransformComponent transform;
		const Entity entity = scene_.CreateModelEntity(path.stem().generic_string(), model, transform);
		state_.selectedEntity = entity;
		MarkSceneEdited();
		state_.status = "Imported model: " + path.generic_string();
		return;
	}

	if (IsTextureExtension(extension))
	{
		if (!state_.textureDropTarget.IsValid())
		{
			state_.status = "Select a material texture slot with Use Drop before dropping an image.";
			return;
		}

		ApplyMaterialTexture(
			state_.textureDropTarget.entity,
			state_.textureDropTarget.meshIndex,
			state_.textureDropTarget.slot,
			path.generic_string());
		return;
	}

	state_.status = "Unsupported dropped file: " + path.generic_string();
}

void EditorLayer::InitializeRenderSettings()
{
	state_.previewSettings.mode = RenderMode::Preview;
	state_.previewSettings.maxSamples = Default_Max_Preview_Samples_Per_Pixel;
	state_.previewSettings.minSamples = 1;
	state_.previewSettings.maxBounceCount = Default_Max_Bounce_Count_Per_Ray_Preview;
	state_.previewSettings.noiseThreshold = Default_Render_Noise_Threshold;
	state_.previewSettings.adaptiveNoise = false;
	state_.previewSettings.denoise = false;

	state_.finalSettings.mode = RenderMode::Final;
	state_.finalSettings.maxSamples = Default_Max_Render_Samples_Per_Pixel;
	state_.finalSettings.minSamples = Default_Min_Render_Samples_Per_Pixel;
	state_.finalSettings.maxBounceCount = Default_Max_Bounce_Count_Per_Ray_Render;
	state_.finalSettings.noiseThreshold = Default_Render_Noise_Threshold;
	state_.finalSettings.adaptiveNoise = true;
	state_.finalSettings.denoise = true;
}

void EditorLayer::LoadStartupScene()
{
	std::string error;
	if (!resourceConfig_.IsLoaded())
	{
		startupStatus_ = resourceConfig_.GetLastError();
		scene_ = CreateFallbackScene(startupStatus_);
		return;
	}

	if (!resourceManager_.LoadAssetRegistry(resourceConfig_, &error))
	{
		startupStatus_ = error;
		scene_ = CreateFallbackScene(startupStatus_);
		return;
	}

	SceneCameraSettings cameraSettings;
	if (!sceneManager_.SetSceneDirectory(resourceConfig_.GetSceneStorageDirectory(), &error) ||
		!sceneManager_.LoadFirstScene(scene_, &cameraSettings, &error))
	{
		startupStatus_ = error;
		scene_ = CreateFallbackScene(startupStatus_);
		return;
	}

	ApplyCameraSettings(camera_, cameraSettings);
	if (!error.empty())
	{
		startupStatus_ = error;
	}
}

void EditorLayer::ReleaseVulkanResources()
{
	if (vulkanResourcesReleased_)
	{
		return;
	}

	if (g_Device != VK_NULL_HANDLE)
	{
		vkDeviceWaitIdle(g_Device);
	}

	renderer_.reset();
	if (g_texturePool)
	{
		g_texturePool->Clear();
	}

	vulkanResourcesReleased_ = true;
}

void EditorLayer::UpdateFrameStats()
{
	const float elapsed = frameTimer_.ElapsedMillis();
	accumulatedFrameTimeMs_ += elapsed;
	accumulatedFrameCount_++;
	if (accumulatedFrameTimeMs_ > statsWindowMs_)
	{
		state_.frameTimeMs = statsWindowMs_ / static_cast<float>(std::max(accumulatedFrameCount_, 1u));
		accumulatedFrameTimeMs_ -= statsWindowMs_;
		if (accumulatedFrameCount_ < 4)
		{
			statsWindowMs_ *= 2.0f;
		}
		else if (accumulatedFrameCount_ > 10)
		{
			statsWindowMs_ = std::max(statsWindowMs_ / 2.0f, 500.0f);
		}
		state_.fps = static_cast<float>(accumulatedFrameCount_) / (statsWindowMs_ / 1000.0f);
		accumulatedFrameCount_ = 0;
	}
	frameTimer_.Reset();
}

void EditorLayer::HandleShortcuts()
{
	const bool ctrlDown = Walnut::Input::IsKeyDown(Walnut::KeyCode::LeftControl) ||
		Walnut::Input::IsKeyDown(Walnut::KeyCode::RightControl);
	const bool saveDown = Walnut::Input::IsKeyDown(Walnut::KeyCode::S);
	if (ctrlDown && saveDown && !ctrlSSavedLastFrame_)
	{
		SaveScene();
	}
	ctrlSSavedLastFrame_ = ctrlDown && saveDown;
}

void EditorLayer::SubmitRenderIfNeeded()
{
	if (!renderer_ || state_.viewportWidth == 0 || state_.viewportHeight == 0)
	{
		return;
	}

	if (std::shared_ptr<Walnut::StorageImage> image = renderer_->GetFinalImage())
	{
		if (image->GetWidth() != state_.viewportWidth || image->GetHeight() != state_.viewportHeight)
		{
			renderer_->OnResize(state_.viewportWidth, state_.viewportHeight);
		}
	}

	RenderSettings settings = state_.activeRenderMode == RenderMode::Final
		? state_.finalSettings
		: state_.previewSettings;
	settings.mode = state_.activeRenderMode;
	renderer_->Render(BuildRenderPacket(settings));
}

void EditorLayer::RenderPendingModals()
{
	if (pendingCloseModal_)
	{
		ImGui::OpenPopup("Unsaved Changes##Close");
	}
	if (ImGui::BeginPopupModal("Unsaved Changes##Close", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("The current scene has unsaved changes.");
		if (ImGui::Button("Save and Exit"))
		{
			if (SaveScene())
			{
				pendingCloseModal_ = false;
				ImGui::CloseCurrentPopup();
				Walnut::Application::Get().Close();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Discard"))
		{
			state_.dirty = false;
			pendingCloseModal_ = false;
			ImGui::CloseCurrentPopup();
			Walnut::Application::Get().Close();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			pendingCloseModal_ = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (pendingSceneSwitchModal_)
	{
		ImGui::OpenPopup("Unsaved Changes##SceneSwitch");
	}
	if (ImGui::BeginPopupModal("Unsaved Changes##SceneSwitch", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Save changes before loading another scene?");
		if (ImGui::Button("Save and Load"))
		{
			if (SaveScene() && LoadSceneNow(pendingScenePath_))
			{
				pendingSceneSwitchModal_ = false;
				pendingScenePath_.clear();
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Discard"))
		{
			state_.dirty = false;
			LoadSceneNow(pendingScenePath_);
			pendingSceneSwitchModal_ = false;
			pendingScenePath_.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			pendingSceneSwitchModal_ = false;
			pendingScenePath_.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

std::filesystem::path EditorLayer::GetEditorSettingsPath() const
{
	if (resourceConfig_.IsLoaded() && !resourceConfig_.GetConfigDirectory().empty())
	{
		return resourceConfig_.GetConfigDirectory() / "editor_settings.json";
	}

	return Walnut::RuntimePath::ResolveFromExecutableDirectory("config/editor_settings.json");
}

void EditorLayer::LoadEditorSettings()
{
	const std::filesystem::path settingsPath = GetEditorSettingsPath();
	std::ifstream input(settingsPath);
	if (!input.is_open())
	{
		return;
	}

	Json json;
	try
	{
		input >> json;
	}
	catch (const std::exception& e)
	{
		state_.status = "Failed to parse editor settings: " + std::string(e.what());
		return;
	}

	if (!json.is_object())
	{
		return;
	}

	state_.overlayEnabled = json.value("overlayEnabled", state_.overlayEnabled);
	state_.outputType = ReadRenderOutputType(json.value("outputType", std::string(RenderOutputTypeName(state_.outputType))), state_.outputType);
	state_.activeRenderMode = ReadRenderMode(json.value("activeRenderMode", std::string(RenderModeName(state_.activeRenderMode))), state_.activeRenderMode);

	if (json.contains("preview") && json["preview"].is_object())
	{
		state_.previewSettings = ReadRenderSettings(json["preview"], state_.previewSettings);
		state_.previewSettings.mode = RenderMode::Preview;
	}
	if (json.contains("final") && json["final"].is_object())
	{
		state_.finalSettings = ReadRenderSettings(json["final"], state_.finalSettings);
		state_.finalSettings.mode = RenderMode::Final;
	}
}

void EditorLayer::SaveEditorSettings() const
{
	const std::filesystem::path settingsPath = GetEditorSettingsPath();
	std::error_code error;
	std::filesystem::create_directories(settingsPath.parent_path(), error);
	if (error)
	{
		return;
	}

	Json json;
	json["version"] = 1;
	json["overlayEnabled"] = state_.overlayEnabled;
	json["outputType"] = RenderOutputTypeName(state_.outputType);
	json["activeRenderMode"] = RenderModeName(state_.activeRenderMode);
	json["preview"] = WriteRenderSettings(state_.previewSettings);
	json["final"] = WriteRenderSettings(state_.finalSettings);

	std::ofstream output(settingsPath);
	if (!output.is_open())
	{
		return;
	}
	output << std::setw(2) << json << std::endl;
}

void EditorLayer::MarkSceneEdited()
{
	scene_.Touch();
	state_.dirty = true;
}

void EditorLayer::MarkCameraEdited()
{
	state_.dirty = true;
}

bool EditorLayer::SaveScene()
{
	std::string error;
	if (sceneManager_.SaveActiveScene(scene_, CaptureCameraSettings(camera_), &error))
	{
		state_.dirty = false;
		state_.status = "Scene saved: " + sceneManager_.GetActiveScenePath().generic_string();
		return true;
	}

	state_.status = error;
	return false;
}

void EditorLayer::RequestLoadScene(const std::filesystem::path& scenePath)
{
	if (state_.dirty)
	{
		pendingScenePath_ = scenePath;
		pendingSceneSwitchModal_ = true;
		return;
	}

	LoadSceneNow(scenePath);
}

bool EditorLayer::LoadSceneNow(const std::filesystem::path& scenePath)
{
	std::string error;
	SceneCameraSettings cameraSettings;
	if (!sceneManager_.LoadScene(scenePath, scene_, &cameraSettings, &error))
	{
		state_.status = error;
		return false;
	}

	ApplyCameraSettings(camera_, cameraSettings);
	state_.selectedEntity = FindFirstEditableEntity(scene_);
	state_.dirty = false;
	state_.textureDropTarget.Clear();
	PruneResourcesForActiveScene();
	renderer_->SetScene(scene_);
	state_.status = error.empty() ? ("Scene loaded: " + sceneManager_.GetActiveScenePath().generic_string()) : error;
	return true;
}

void EditorLayer::CreatePrimitive(PrimitiveType type)
{
	const Entity entity = scene_.CreateModelEntity(PrimitiveTypeToString(type), Model::CreatePrimitive(type));
	state_.selectedEntity = entity;
	MarkSceneEdited();
}

void EditorLayer::CreateRadiusLight()
{
	TransformComponent transform;
	transform.translation = glm::vec3(0.0f, 0.8f, 0.0f);
	const Entity entity = scene_.CreateEntity("Radius Light", transform);
	RadiusLightComponent light;
	light.radius = 0.15f;
	scene_.AddRadiusLight(entity, light);
	state_.selectedEntity = entity;
	MarkSceneEdited();
}

void EditorLayer::CreateAreaLight()
{
	TransformComponent transform;
	transform.translation = glm::vec3(0.0f, 1.0f, 0.0f);
	const Entity entity = scene_.CreateEntity("Area Light", transform);
	AreaLightComponent light;
	light.width = 1.0f;
	light.height = 1.0f;
	scene_.AddAreaLight(entity, light);
	state_.selectedEntity = entity;
	MarkSceneEdited();
}

void EditorLayer::DeleteSelected()
{
	if (scene_.DestroyEntity(state_.selectedEntity))
	{
		state_.selectedEntity = InvalidEntity;
		state_.dirty = true;
		PruneResourcesForActiveScene();
	}
}

void EditorLayer::ToggleRenderMode()
{
	state_.activeRenderMode = state_.activeRenderMode == RenderMode::Preview
		? RenderMode::Final
		: RenderMode::Preview;
}

void EditorLayer::ApplyMaterialTexture(Entity entity, uint32_t meshIndex, MaterialTextureSlot slot, const std::string& path)
{
	if (!scene_.IsValid(entity))
	{
		state_.status = "Texture target entity is no longer valid.";
		state_.textureDropTarget.Clear();
		return;
	}

	MeshRendererComponent* meshRenderer = scene_.TryGetMeshRenderer(entity);
	if (!meshRenderer || !meshRenderer->model)
	{
		state_.status = "Texture target entity has no model.";
		state_.textureDropTarget.Clear();
		return;
	}

	std::string error;
	const std::filesystem::path base = sceneManager_.GetActiveScenePath().parent_path();
	if (!resourceManager_.ApplyMaterialTexture(*meshRenderer->model, meshIndex, slot, path, &error, base))
	{
		state_.status = error;
		return;
	}

	MarkSceneEdited();
	PruneResourcesForActiveScene();
	state_.status = std::string("Texture applied to ") + TextureSlotName(slot) + ".";
}

void EditorLayer::PickViewport(const glm::vec2& viewportPosition)
{
	Ray ray;
	if (!BuildViewportRay(camera_, state_, viewportPosition, ray))
	{
		return;
	}

	Entity bestEntity = InvalidEntity;
	float bestDistance = std::numeric_limits<float>::max();

	for (Entity entity : scene_.GetEntities())
	{
		const TransformComponent* transform = scene_.TryGetTransform(entity);
		if (!transform)
		{
			continue;
		}

		if (const MeshRendererComponent* meshRenderer = scene_.TryGetMeshRenderer(entity))
		{
			if (!meshRenderer->visible || !meshRenderer->model)
			{
				continue;
			}

			const glm::mat4 modelMatrix = transform->GetMatrix();
			for (const Mesh& mesh : meshRenderer->model->GetMeshes())
			{
				for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
				{
					const glm::vec3 v0 = glm::vec3(modelMatrix * glm::vec4(mesh.vertices[mesh.indices[i]].position, 1.0f));
					const glm::vec3 v1 = glm::vec3(modelMatrix * glm::vec4(mesh.vertices[mesh.indices[i + 1]].position, 1.0f));
					const glm::vec3 v2 = glm::vec3(modelMatrix * glm::vec4(mesh.vertices[mesh.indices[i + 2]].position, 1.0f));
					float distance = 0.0f;
					if (IntersectTriangle(ray, v0, v1, v2, distance) && distance < bestDistance)
					{
						bestDistance = distance;
						bestEntity = entity;
					}
				}
			}
		}

		if (const RadiusLightComponent* light = scene_.TryGetRadiusLight(entity))
		{
			float distance = 0.0f;
			if (IntersectSphere(ray, transform->translation, std::max(light->radius, 0.08f), distance) && distance < bestDistance)
			{
				bestDistance = distance;
				bestEntity = entity;
			}
		}

		if (const AreaLightComponent* light = scene_.TryGetAreaLight(entity))
		{
			float distance = 0.0f;
			const float proxyRadius = std::max(0.12f, std::max(light->width, light->height) * 0.15f);
			if (IntersectSphere(ray, transform->translation, proxyRadius, distance) && distance < bestDistance)
			{
				bestDistance = distance;
				bestEntity = entity;
			}
		}
	}

	state_.selectedEntity = bestEntity;
}

void EditorLayer::PruneResourcesForActiveScene()
{
	if (g_Device != VK_NULL_HANDLE)
	{
		vkDeviceWaitIdle(g_Device);
	}

	resourceManager_.PruneModelCacheForScene(scene_);

	std::unordered_set<std::string> liveTexturePaths;
	for (Entity entity : scene_.GetEntities())
	{
		const MeshRendererComponent* meshRenderer = scene_.TryGetMeshRenderer(entity);
		if (!meshRenderer || !meshRenderer->model)
		{
			continue;
		}

		for (const Mesh& mesh : meshRenderer->model->GetMeshes())
		{
			if (!mesh.material.BaseColorTexturePath.empty())
			{
				liveTexturePaths.insert(mesh.material.BaseColorTexturePath);
			}
			if (!mesh.material.MetallicTexturePath.empty())
			{
				liveTexturePaths.insert(mesh.material.MetallicTexturePath);
			}
			if (!mesh.material.RoughnessTexturePath.empty())
			{
				liveTexturePaths.insert(mesh.material.RoughnessTexturePath);
			}
			if (!mesh.material.NormalTexturePath.empty())
			{
				liveTexturePaths.insert(mesh.material.NormalTexturePath);
			}
			if (!mesh.material.IBLTexturePath.empty())
			{
				liveTexturePaths.insert(mesh.material.IBLTexturePath);
			}
		}
	}

	if (g_texturePool)
	{
		g_texturePool->RetainOnly(liveTexturePaths);
	}
}

EditorContext EditorLayer::BuildContext()
{
	EditorContext context;
	context.resourceConfig = &resourceConfig_;
	context.resourceManager = &resourceManager_;
	context.sceneManager = &sceneManager_;
	context.scene = &scene_;
	context.renderer = renderer_.get();
	context.camera = &camera_;
	context.state = &state_;
	return context;
}

EditorActions EditorLayer::BuildActions()
{
	EditorActions actions;
	actions.MarkSceneEdited = [this]() { MarkSceneEdited(); };
	actions.MarkCameraEdited = [this]() { MarkCameraEdited(); };
	actions.SaveScene = [this]() { return SaveScene(); };
	actions.RequestLoadScene = [this](const std::filesystem::path& path) { RequestLoadScene(path); };
	actions.CreatePrimitive = [this](PrimitiveType type) { CreatePrimitive(type); };
	actions.CreateRadiusLight = [this]() { CreateRadiusLight(); };
	actions.CreateAreaLight = [this]() { CreateAreaLight(); };
	actions.DeleteSelected = [this]() { DeleteSelected(); };
	actions.ToggleRenderMode = [this]() { ToggleRenderMode(); };
	actions.SubmitRenderIfNeeded = [this]() { SubmitRenderIfNeeded(); };
	actions.ApplyMaterialTexture = [this](Entity entity, uint32_t meshIndex, MaterialTextureSlot slot, const std::string& path)
	{
		ApplyMaterialTexture(entity, meshIndex, slot, path);
	};
	actions.PickViewport = [this](const glm::vec2& viewportPosition) { PickViewport(viewportPosition); };
	return actions;
}

RenderPacket EditorLayer::BuildRenderPacket(const RenderSettings& settings) const
{
	RenderPacket packet;
	packet.viewportWidth = state_.viewportWidth;
	packet.viewportHeight = state_.viewportHeight;
	packet.sceneRevision = scene_.GetRevision();
	packet.settings = settings;
	packet.camera = camera_.CaptureSnapshot();
	return packet;
}
