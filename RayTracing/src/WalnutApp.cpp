#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

#include "Walnut/Image.h"
#include "Walnut/Timer.h"
#include "Renderer.h"
#include "ResourceConfig.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "SceneLoader.h"

#include <cstdio>
#include <iostream>
#include <memory>
#include <string>

using namespace Walnut;

namespace
{
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

	Entity FindFirstMeshRendererEntity(Scene& scene)
	{
		for (Entity entity : scene.GetEntities())
		{
			if (scene.TryGetMeshRenderer(entity))
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
}

class ExampleLayer : public Walnut::Layer
{
public:
	ExampleLayer()
		: resourceConfig_(LoadResourceConfig()), sceneManager_(resourceManager_)
	{
		std::string error;
		if (!resourceConfig_.IsLoaded())
		{
			resourceStatus_ = resourceConfig_.GetLastError();
			scene_ = CreateFallbackScene(resourceStatus_);
		}
		else if (!resourceManager_.LoadAssetRegistry(resourceConfig_, &error))
		{
			resourceStatus_ = error;
			scene_ = CreateFallbackScene(resourceStatus_);
		}
		else if (!sceneManager_.SetSceneDirectory(resourceConfig_.GetSceneStorageDirectory(), &error) ||
			!sceneManager_.LoadFirstScene(scene_, &cameraSettings_, &error))
		{
			resourceStatus_ = error;
			scene_ = CreateFallbackScene(resourceStatus_);
		}
		else if (!error.empty())
		{
			resourceStatus_ = error;
		}

		ApplyCameraSettings(camera_, cameraSettings_);
		focus_distance_ = cameraSettings_.focusDistance;
		DOF_focus_distance_ = cameraSettings_.dofFocusDistance;
		lens_radius_ = cameraSettings_.lensRadius;
		isDOF_ = cameraSettings_.useDOF;
		renderer_ = std::make_unique<Renderer>(scene_, resourceConfig_, resourceManager_);

		editableRadiusLight_ = scene_.GetFirstRadiusLightEntity();
		editableModelEntity_ = FindFirstMeshRendererEntity(scene_);
		if (const MeshRendererComponent* meshRenderer = scene_.TryGetMeshRenderer(editableModelEntity_))
		{
			if (meshRenderer->model)
			{
				std::snprintf(modelPathBuffer_, sizeof(modelPathBuffer_), "%s", meshRenderer->model->GetPath().c_str());
				if (!meshRenderer->model->GetLastError().empty())
				{
					resourceStatus_ = meshRenderer->model->GetLastError();
				}
			}
		}
	}

	~ExampleLayer() override
	{
		ReleaseVulkanResources();
	}

	virtual void OnDetach() override
	{
		ReleaseVulkanResources();
	}

	virtual void OnUIRender() override
	{
		ImGui::Begin("Ray Tracing");
		// Renderer
		ImGui::LabelText("", "Ray Tracing Info:");

		float elapsed = timer.ElapsedMillis();
		accumulateTime += elapsed;
		accumulateFrameCount++;
		static float msPerCal = 1000.0f;
		if (accumulateTime > msPerCal)
		{
			lastRenderTime_ = msPerCal / accumulateFrameCount;
			accumulateTime -= msPerCal;
			if (accumulateFrameCount < 4)
			{
				msPerCal *= 2.0f;
			}
			else if(accumulateFrameCount > 10)
			{
				msPerCal = std::max(msPerCal / 2.0f, 500.0f);
			}
			fps_ = accumulateFrameCount / (msPerCal / 1000.0f);
			accumulateFrameCount = 0;
		}
		timer.Reset();
		 // 

		ImGui::Text("Frame Rate: %.1ffps ", fps_);
		ImGui::Text("Frame Render Cost: %.3fms (%.1fs)", lastRenderTime_, lastRenderTime_ / 1000.0f);

		ImGui::Text("Now Viewport Size: %d x %d", viewportWidth_, viewportHeight_);
		ImGui::LabelText("", "	Scene:");
		if (TransformComponent* transform = scene_.TryGetTransform(editableRadiusLight_))
		{
			bool lightChanged = false;
			lightChanged |= ImGui::InputFloat("Radius Light X", &transform->translation.x, 0.01f, 0.1f, "%.3f");
			lightChanged |= ImGui::InputFloat("Radius Light Y", &transform->translation.y, 0.01f, 0.1f, "%.3f");
			lightChanged |= ImGui::InputFloat("Radius Light Z", &transform->translation.z, 0.01f, 0.1f, "%.3f");
			if (lightChanged)
			{
				scene_.Touch();
				isLockImage = false;
			}
		}
		ImGui::InputText("Model Path", modelPathBuffer_, sizeof(modelPathBuffer_));
		if (ImGui::Button("Rebind Model"))
		{
			std::string error;
			if (resourceManager_.RebindModel(scene_, editableModelEntity_, modelPathBuffer_, &error, resourceConfig_.GetAssetsDirectory()))
			{
				resourceStatus_ = "Model rebound.";
				isLockImage = false;
			}
			else
			{
				resourceStatus_ = error;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Save Scene"))
		{
			std::string error;
			if (sceneManager_.SaveActiveScene(scene_, CaptureCameraSettings(camera_), &error))
			{
				resourceStatus_ = "Scene saved: " + sceneManager_.GetActiveScenePath().generic_string();
			}
			else
			{
				resourceStatus_ = error;
			}
		}
		if (!resourceStatus_.empty())
		{
			ImGui::TextWrapped("%s", resourceStatus_.c_str());
		}
		// Preview
		ImGui::LabelText("", "	Preview:");
		ImGui::InputFloat("Preview Max Accumulate Sample Count", &max_preview_sample_count_, 1, 0, "%.0f");
		// Render
		ImGui::LabelText("", "	Render:");
		ImGui::InputFloat("Render Max Sample Count", &max_render_sample_count_, 1, 0, "%.0f");
		ImGui::InputFloat("Render Min Sample Count", &min_render_sample_count_, 1, 0, "%.0f");
		ImGui::Checkbox("Use Denoise?", &isDenoise_);
		ImGui::Combo("Output Type", &imageType_, imageTypes_, IM_ARRAYSIZE(imageTypes_));

		if (ImGui::SliderFloat("Focus Distance", &focus_distance_, 0.05, 3, "%.1f"))
		{
			camera_.SetFocusDistance(focus_distance_);
		}
			// DOF
		ImGui::LabelText("", "	DOF:");
		if (ImGui::SliderFloat("DOF Focus Distance", &DOF_focus_distance_, 0.05, 14, "%.1f"))
		{
			camera_.SetDOFFocusDistance(DOF_focus_distance_);
		}
		if (ImGui::SliderFloat("Lens Radius", &lens_radius_, 0, 1, "%.2f"))
		{
			camera_.SetDOFLensRadius(lens_radius_);
		}
		if (ImGui::Checkbox("Use DOF?", &isDOF_))
		{
			camera_.SetUseDOF(isDOF_);
		}

		// Render
		if (ImGui::Button("Render"))
		{
			Render();
		}
		if (!isLockImage)
		{
			RenderPreview();
		}
		

		if (ImGui::Button("Unlock"))
		{
			isLockImage = false;
		}

		ImGui::End();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::Begin("ViewPort");

		viewportWidth_ = ImGui::GetContentRegionAvail().x;
		viewportHeight_ = ImGui::GetContentRegionAvail().y;

		std::shared_ptr<StorageImage> image;
		switch (imageType_)
		{
		case 0:
			image = renderer_->GetFinalImage();
			break;
		case 1:
			image = renderer_->GetAlbedoImage();
			break;
		case 2:
			image = renderer_->GetNormalImage();
			break;
		}

		if (image)
			ImGui::Image(image->GetDescriptorSet(), { (float)viewportWidth_, (float)viewportHeight_ });

		ImGui::End();
		ImGui::PopStyleVar();
	}

	virtual void OnUpdate(float ts) override
	{
		camera_.Tick(ts, viewportWidth_, viewportHeight_);
	}

	void Render()
	{
		// Resize
		renderer_->OnResize(viewportWidth_, viewportHeight_);
		renderer_->SetMaxRenderSampleCount(max_render_sample_count_);
		renderer_->SetMinRenderSampleCount(min_render_sample_count_);
		renderer_->SetMaxBounceCount(Default_Max_Bounce_Count_Per_Ray_Render);
		// Render
		renderer_->Render(camera_, true, isDenoise_);
		isLockImage = true;

	}
	
	void RenderPreview()
	{
		if (viewportWidth_ == 0 || viewportHeight_ == 0) return;

		// Resize
		renderer_->OnResize(viewportWidth_, viewportHeight_);
		renderer_->SetMaxRenderSampleCount(max_render_sample_count_);
		renderer_->SetMaxPreviewSampleCount(max_preview_sample_count_);
		renderer_->SetMaxBounceCount(Default_Max_Bounce_Count_Per_Ray_Preview);
		// Render
		renderer_->Render(camera_, false, false);

	}

	void ReleaseVulkanResources()
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

private:
	ResourceConfig resourceConfig_;
	ResourceManager resourceManager_;
	SceneManager sceneManager_;
	Scene scene_;
	std::unique_ptr<Renderer> renderer_;
	SceneCameraSettings cameraSettings_;
	Entity editableRadiusLight_{ InvalidEntity };
	Entity editableModelEntity_{ InvalidEntity };
	Camera camera_ {Camera(glm::vec3(0, 0, 4), glm::vec3(0, 0,-1))};

	Timer timer;
	float accumulateTime = 0;
	uint32_t accumulateFrameCount = 0;

	uint32_t viewportWidth_ = 0;
	uint32_t viewportHeight_ = 0;
	// UI
	float lastRenderTime_ = 0;
	float fps_ = 0;
	float max_render_sample_count_ = Default_Max_Render_Samples_Per_Pixel;
	float min_render_sample_count_ = Default_Min_Render_Samples_Per_Pixel;
	float max_preview_sample_count_ = Default_Max_Preview_Samples_Per_Pixel;
	float focus_distance_ = 2;
	float DOF_focus_distance_ = 6;
	float lens_radius_ = .02f;
	bool isLockImage {false};
	const char* imageTypes_[3] = { "FinalColor", "Albedo", "Normal"};
	int imageType_ = 0;
	bool isDenoise_ = true;
	bool isDOF_ = true;
	bool vulkanResourcesReleased_{ false };
	char modelPathBuffer_[1024]{};
	std::string resourceStatus_;
};

Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
	Walnut::ApplicationSpecification spec;
	spec.Name = "Ray Tracing";

	Walnut::Application* app = new Walnut::Application(spec);
	app->PushLayer<ExampleLayer>();
	app->SetMenubarCallback([app]()
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Exit"))
			{
				app->Close();
			}
			ImGui::EndMenu();
		}
	});
	return app;
}
