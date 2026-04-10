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
#ifdef RandomScene
		const float Max_Radius = .8f;
		const float Min_Radius = 0.15f;
		const float Ground_Radius = 1000.0f;
		const int widthIter = 3;

		auto ground = std::make_shared<Sphere>(glm::vec3(0, -Ground_Radius - Max_Radius, 0), Ground_Radius);
		ground->material = std::make_shared<Lambertian>(Color(.2f,.2f,.3f,1.0f));
		scene_.Add(ground);

		auto rectLit = std::make_shared<Rect>();
		rectLit->SetPosition(glm::vec3(0, 6, 0));
		rectLit->SetScale(glm::vec3(8));
		rectLit->CalculateTrans();
		rectLit->UpdateNode(glm::mat4(1));
		rectLit->Pricompute();
		auto lit = std::make_shared<Diffuse_Light>(Color(1,1,1,1));
		lit->SetIntensity(2.0f);
		rectLit->material = lit;
		scene_.Add(rectLit);

		for (int i = -widthIter; i < widthIter + 1; i++)
		{
			for (int j = -widthIter; j < widthIter + 1; j++)
			{
				auto r = std::pow((Random::Float(Min_Radius, Max_Radius) - Min_Radius) / Max_Radius, 1.5) * Max_Radius + Min_Radius;
				auto x = Max_Radius * 2 * i;
				auto z = Max_Radius * 2 * j;
				//float y = (std::sqrt((Ground_Radius + r) * (Ground_Radius + r) - (x * x + z * z)) - Ground_Radius) - Max_Radius;
				float y = Random::Float(0, 4);
				std::shared_ptr<Hittable> mod;
				uint32_t model = Random::UInt(0, 3);
				{
					std::shared_ptr<Material> m;
					uint32_t mat = Random::UInt(0, 4);
					{
						if(mat < 3) m = std::make_shared<Lambertian>(Color(Random::Vec3(.3, 1)));
						else if(mat == 4 && model == 4)	m = std::make_shared<PureRefraction>(Color(Random::Vec3(.6, 1)));
						else m = std::make_shared<Metal>(Color(Random::Vec3(.4, 1)));
					}
					if (model < 3)
					{
						auto s = std::make_shared<Sphere>(glm::vec3(x, y, z), r);
						s->material = m;
						mod = s;
					}
					else
					{
						auto b = std::make_shared<Box>(m);
						b->SetPosition(glm::vec3(x, y, z));
						b->SetScale(glm::vec3(r * 1.6));
						b->CalculateTrans();	
						b->UpdateNode(glm::mat4(1));
						b->Update();
						mod = b;
					}
				}
				
				scene_.Add(mod);
			}
		}
#endif

#ifdef useCornellBox
		auto red = std::make_shared<Lambertian>(Color(0.65, 0.05, 0.05, 1));
		auto white = std::make_shared<Lambertian>(Color(0.73, 0.73, 0.73, 1));
		auto green = std::make_shared<Lambertian>(Color(0.12, 0.45, 0.15, 1));
		auto light = std::make_shared<Diffuse_Emissive>(Color(1.15, .8f, .27f, 1), 2);

		auto rectL = std::make_shared<Rect>(light, glm::vec3(0, 1.84f / 2.0f, 0), glm::vec3(0, 0, 180), glm::vec3(.43f, 1.0f, .35f));
		// material, position, rotation, scale
        scene_.AddObject(std::make_shared<Rect>(red, glm::vec3(1.85f / 2.0f,0,0), glm::vec3(0,0,90), glm::vec3(1.85f,1.85f,1.85f)));
        scene_.AddObject(std::make_shared<Rect>(green, glm::vec3(-1.85f / 2.0f,0,0), glm::vec3(0,0,-90), glm::vec3(1.85f,1.85f,1.85f)));
        scene_.AddObject(rectL);
        scene_.AddObject(std::make_shared<Rect>(white, glm::vec3(0,1.85f / 2.0f,0), glm::vec3(0,0,180), glm::vec3(1.85f,1.85f,1.85f)));
        scene_.AddObject(std::make_shared<Rect>(white, glm::vec3(0,-1.85f / 2.0f,0), glm::vec3(0,0,0), glm::vec3(1.85f,1.85f,1.85f)));
        scene_.AddObject(std::make_shared<Rect>(white, glm::vec3(0,0,-1.85f / 2.0f), glm::vec3(90,0,0), glm::vec3(1.85f,1.85f,1.85f)));
		// 165, 330
        scene_.AddObject(std::make_shared<Box>(white, glm::vec3(.2f, (- 1.85f + 1.1f) / 2.0f, -.33f), glm::vec3(0,15,0), glm::vec3(.55f,1.1f,.55f)));
		scene_.AddObject(std::make_shared<Box>(white, glm::vec3(.216f, (-1.85f + .55f) / 2.0f, .433f), glm::vec3(0, -18, 0), glm::vec3(.55f, .55f, .55f)));

		auto areaL = std::make_shared<AreaLight>(*(rectL.get()), Color(1.15, .8f, .27f, 1), 5.0f);

		light_ = areaL;
		scene_.AddLight(areaL);
#endif
		//std::shared_ptr<Material> metal = std::make_shared<Metal>(Color(0.85, 0.85, 0.85, 1));
		//auto red = std::make_shared<Lambertian>(Color(0.65, 0.05, 0.05, 1));
		//auto model1 = std::make_shared<Model>(metal);
		//model1->LoadModel("assets/model/Monkey.fbx");
		//scene_.AddModel(model1);
		//sph = scene_.GetObject(3);
	}

	virtual void OnUIRender() override
	{
		ImGui::Begin("Ray Tracing");
		// Renderer
		ImGui::LabelText("", "Ray Tracing Info:");

		// 计算帧率，自适应时间
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
		// Sphere 
#ifdef shpereUI
		ImGui::LabelText("", "\nSphere Info:");
		if (ImGui::ColorEdit3("Sphere Color", sphereCol_))
		{
			dynamic_cast<Metal*>((dynamic_cast<Sphere*>(sph.get())->material).get())->SetAlbedo({ sphereCol_[0], sphereCol_[1], sphereCol_[2], 1});
		}

		if (ImGui::SliderFloat("Sphere Roughness", &rough_, 0, 1, "%.2f"))
		{
			dynamic_cast<Metal*>((dynamic_cast<Sphere*>(sph.get())->material).get())->SetRoughness(rough_);
		}

		if (ImGui::SliderFloat("Sphere Refractive Index", &refractive_, 1, 5, "%.2f"))
		{
			//dynamic_cast<Metal*>((dynamic_cast<Sphere*>(sph.get())->material).get())->SetRefractive(refractive_);
		}
#endif
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