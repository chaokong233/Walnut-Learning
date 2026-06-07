#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

#include "Walnut/Random.h"
#include "Walnut/Image.h"
#include "Walnut/Timer.h"
#include "Renderer.h"
#include "Ray.h"
#include "Mesh.h"

using namespace Walnut;

class ExampleLayer : public Walnut::Layer
{
public:
	ExampleLayer()
	{

	}

	virtual void OnUIRender() override
	{
		ImGui::Begin("Ray Tracing");
		// Renderer
		ImGui::LabelText("", "Ray Tracing Info:");

		// ����֡�ʣ�����Ӧʱ��
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
		// Preview
		ImGui::LabelText("", "	Preview:");
		ImGui::InputFloat("Preview Max Accumulate Sample Count", &max_preview_sample_count_, 1, 0, "%.0f");
		// Render
		ImGui::LabelText("", "	Render:");
		ImGui::InputFloat("Render Max Sample Count", &max_render_sample_count_, 1, 0, "%.0f");
		ImGui::InputFloat("Render Min Sample Count", &min_render_sample_count_, 1, 0, "%.0f");
		if (ImGui::Checkbox("Use MT Acceleration?", &isUseMT_))
		{
			// renderer_.SetUseMT(isUseMT_);
		}
		if (ImGui::Checkbox("Use Bvh Acceleration?", &isUseBvh_))
		{
			scene_.SetUseBvh(isUseBvh_);
		}
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

		ImGui::LabelText("", "\nLight Info:");
		if(ImGui::InputFloat("Light Intensity", &lightIntensity_, 1, 0, "%.0f"))
		{
			light_->SetPower(lightIntensity_);
		}

		// Render
		if (ImGui::Button("Render"))
		{
			// Render();
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

#ifdef VULKAN_RT
		std::shared_ptr<StorageImage> image;
#else
		std::shared_ptr<Image> image;
#endif
		switch (imageType_)
		{
		case 0:
			image = renderer_.GetFinalImage();
			break;
		case 1:
			image = renderer_.GetAlbedoImage();
			break;
		case 2:
			image = renderer_.GetNormalImage();
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
		scene_.Update(ts);
	}

	void Render()
	{
		// Resize
		renderer_.OnResize(viewportWidth_, viewportHeight_);
		renderer_.SetMaxRenderSampleCount(max_render_sample_count_);
		renderer_.SetMinRenderSampleCount(min_render_sample_count_);
		renderer_.SetMaxBounceCount(Default_Max_Bounce_Count_Per_Ray_Render);
		// Render
		renderer_.Render(camera_, scene_, true, isDenoise_);
		isLockImage = true;

	}
	
	void RenderPreview()
	{
		if (viewportWidth_ == 0 || viewportHeight_ == 0) return;

		// Resize
		renderer_.OnResize(viewportWidth_, viewportHeight_);
		renderer_.SetMaxRenderSampleCount(max_render_sample_count_);
		renderer_.SetMaxPreviewSampleCount(max_preview_sample_count_);
		renderer_.SetMaxBounceCount(Default_Max_Bounce_Count_Per_Ray_Preview);
		// Render
		renderer_.Render(camera_, scene_, false, false);

	}

private:
	Renderer renderer_;
	RenderScene scene_;
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
	bool isUseMT_ = true;
	bool isUseBvh_ = true;
	bool isDenoise_ = true;
	bool isDOF_ = true;

		// Sphere 
	std::shared_ptr<AreaLight> light_;
	float sphereCol_[3] = { 0.6f, 0.6f, 0.6f };
	float rough_ = 0;
	float refractive_ = 1.3f;
	float lightIntensity_ = 5.0f;
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