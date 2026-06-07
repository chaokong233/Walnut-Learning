#pragma once
#include "Walnut/Application.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

#include "Walnut/Image.h"
#include "VulkanRTBackend.h"


// Default
#define Default_Max_Render_Samples_Per_Pixel 1
#define Default_Min_Render_Samples_Per_Pixel 4
#define Default_Max_Preview_Samples_Per_Pixel 128
#define Default_Max_Bounce_Count_Per_Ray_Render 6
#define Default_Max_Bounce_Count_Per_Ray_Preview 6
#define Default_Render_Noise_Threshold 0.008

class Camera;
class ImGui_ImplVulkanH_Window;
class RTModel;
class Scene;

// Camera Uniform Data
struct CameraUniformData {
	glm::mat4 ViewMatrixInverse;
	glm::mat4 ProjMatrixInverse;
	float samples{1};
	uint32_t frame{0};
};

struct DenoiseUniformData
{
	float kernel_size = 1;
};
struct DenoiseCameraUniformData
{
	glm::mat4 LastFrameVPMatrix;
};

const uint32_t MAX_AREA_LIGHT_NUM = 5;
const uint32_t MAX_RADIUS_LIGHT_NUM = 5;

struct AreaLightData
{
    alignas(16) glm::vec3 beginPos;
    alignas(16) glm::vec3 u;
    alignas(16) glm::vec3 v;
    alignas(16) glm::vec3 color;
    alignas(16) glm::vec3 rayDir;
};

struct RadiusLightData
{
    alignas(16) glm::vec3 centerPos;
    alignas(16) glm::vec3 color;
    alignas(16) float radius;
};

struct UniformLightsData 
{
    alignas(16) uint32_t areaLightCount{ 0 };
     AreaLightData areaLightsData[MAX_AREA_LIGHT_NUM];
	alignas(16) uint32_t radiusLightCount{ 0 };
     RadiusLightData radiusLightsData[MAX_RADIUS_LIGHT_NUM];
};



class Renderer
{
public:
	explicit Renderer(const Scene& scene);
	~Renderer();

	void SetScene(const Scene& scene);
	void OnResize(uint32_t width, uint32_t height);
	void Render(Camera& camera, bool isAdaptiveNoise = true, bool isDenoise = false);

	inline std::shared_ptr<Walnut::StorageImage> GetFinalImage() const { return outputFinalImage_; }
	inline std::shared_ptr<Walnut::StorageImage> GetNormalImage() const { return nowFrameNormalImage_; }
	inline std::shared_ptr<Walnut::StorageImage> GetAlbedoImage() const { return nowFrameAlbedoImage_; }
	inline void SetMaxRenderSampleCount(uint32_t count) { max_render_samples_per_pixel_ = count; }
	inline void SetMinRenderSampleCount(uint32_t count) { min_render_samples_per_pixel_ = count; }
	inline void SetMinRenderNoiseThreshold(double threshold) { min_render_noise_threshold = threshold; }
	inline void SetMaxPreviewSampleCount(uint32_t count) { max_preview_samples_per_pixel_ = count; }
	inline void SetMaxBounceCount(uint32_t count) { max_bounce_count_ = count; }


private:
	// Image

	std::shared_ptr<Walnut::StorageImage> outputFinalImage_;
	std::shared_ptr<Walnut::StorageImage> nowFrameRadianceImage_;
	std::shared_ptr<Walnut::StorageImage> nowFrameVarianceImage_;
	std::shared_ptr<Walnut::StorageImage> nowFrameAlbedoImage_;
	std::shared_ptr<Walnut::StorageImage> nowFrameNormalImage_;
	std::shared_ptr<Walnut::StorageImage> nowFrameWorldPositionImage_;

	std::shared_ptr<Walnut::StorageImage> lastFrameFinalImage_;
	std::shared_ptr<Walnut::StorageImage> lastFrameVarianceImage_;
	std::shared_ptr<Walnut::StorageImage> lastFrameAlbedoImage_;
	std::shared_ptr<Walnut::StorageImage> lastFrameNormalImage_;
	std::shared_ptr<Walnut::StorageImage> lastFrameWorldPositionImage_;

	// 
	uint32_t max_bounce_count_ = 4;
	uint32_t max_render_samples_per_pixel_ = Default_Max_Render_Samples_Per_Pixel;
	uint32_t min_render_samples_per_pixel_ = Default_Min_Render_Samples_Per_Pixel;
	uint32_t max_preview_samples_per_pixel_ = Default_Max_Preview_Samples_Per_Pixel;
	double min_render_noise_threshold = Default_Render_Noise_Threshold;

private:
	const Scene* scene_{ nullptr };
	uint64_t uploadedSceneRevision_{ 0 };
	std::shared_ptr<RTModel> model_;

	struct FrameData
	{
		vulkan::VulkanMemoryResource* RTUniformBuffer_{ nullptr };
		vulkan::VulkanMemoryResource* DenoiseUniformBuffer_{ nullptr };
		VkDescriptorSet rtDescriptorSet_{ VK_NULL_HANDLE };
		std::array<VkDescriptorSet, 5> denoiseDescriptorSets_{};
	};


	bool isNeedTransition = false;
	uint32_t nowFrameCount{ 0 };

	std::unique_ptr<rt::VulkanRTBackend> rtBackend_;

	rt::AccelerationStructure bottomLevelAS_{};
	rt::AccelerationStructure topLevelAS_{};

	vulkan::VulkanMemoryResource* transformBuffer_{ nullptr };
	vulkan::VulkanLocalBuffer* geometryNodeBuffer_{ nullptr };
	vulkan::VulkanLocalBuffer* lightsBuffer_{ nullptr };
	// 
	std::vector<FrameData> frameDatas_;
	
	rt::ShaderBindingTable shaderBindingTable_;

	// Ray Tracing Pass
	rt::RTPipeline rtPipeline_;
	VkPipelineLayout rtPipelineLayout_{ VK_NULL_HANDLE };
	VkDescriptorSetLayout rtDescriptorSetLayout_{ VK_NULL_HANDLE };

	// Denoise Pass
	VkPipeline denoisePipeline_{ VK_NULL_HANDLE };
	VkPipelineLayout denoisePipelineLayout_{ VK_NULL_HANDLE };
	VkDescriptorSetLayout denoiseDescriptorSetLayout_{ VK_NULL_HANDLE };

	std::array<vulkan::VulkanMemoryResource*, 5> denoiseUniformBuffers_{};
	glm::mat4 lastFrameCameraVPMatrix_;

	void InitRayTracing(const Scene& scene);
	void rebuildSceneResources(const Scene& scene);
	void releaseSceneResources();
	void syncSceneResources();
	void createBottomLevelAccelerationStructure(const Scene& scene);
	void createTopLevelAccelerationStructure();
	void createUniformBuffer();

	void createRayTracingPipeline();
	void createDenoisePipeline();

	void createShaderBindingTable();
	void createDescriptorSets();
	void updateDescriptorSets();
	void CleanUpRayTracing();
	void buildCommandBuffers(ImGui_ImplVulkanH_Window* wd, Camera* camera);

};


class Camera
{
public:
	Camera(glm::vec3 position, glm::vec3 front);

	void Tick(float ts, uint32_t width, uint32_t height);

	void SetFocusDistance(float dis) { focus_distance_ = dis; }
	void SetDOFFocusDistance(float dis) { DOF_focus_distance_ = dis; }
	void SetDOFLensRadius(float radius) { lens_radius_ = radius; }
	void SetUseDOF(bool use) { useDOF_ = use; }

	inline glm::mat4 GetPreVPMatrix() const { return preVPMatrix_; }
	inline glm::mat4 GetViewMatrix() const { return ViewMatrix_; }
	inline glm::mat4 GetProjMatrix() const { return ProjMatrix_; }

	glm::vec3 position_;
	glm::vec3 vertical_;	
	glm::vec3 horizontal_;
	glm::vec3 relative_left_down_corner_;
private:
	glm::vec3 front_;
	glm::vec3 up_ = glm::vec3(0, 1, 0);
	float focus_distance_ = 2;

	uint32_t* cachedAccumulateImageData_ {nullptr};

	glm::vec3 screen_left_down_corner_;
	// DOF
	bool useDOF_ = true;
	float DOF_focus_distance_ = 6;
	float lens_radius_ = 0.02f;

	glm::vec3 focus_vertical_;
	glm::vec3 focus_horizontal_;
	glm::vec3 focus_left_down_corner_;
	
	glm::mat4 ViewMatrix_;
	glm::mat4 ProjMatrix_;
	glm::mat4 preVPMatrix_;

	float cameraMoveSpeed_{ 4.0f };
	float cameraRotateSpeed_{ 30.0f };
	float lastCursorX_;
	float lastCursorY_;
	float cachedYaw_;
	float cachedPitch_;
};
