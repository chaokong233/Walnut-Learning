#pragma once
#include "Walnut/Application.h"

#include <cmath>
#include <algorithm>

#include "Walnut/Image.h"
#include "Ray_Hittable.h"
#include "Denoiser.h"

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

struct AreaLightData
{
    alignas(16) glm::vec3 beginPos;
    alignas(16) glm::vec3 u;
    alignas(16) glm::vec3 v;
    alignas(16) glm::vec3 color;
    alignas(16) glm::vec3 rayDir;
};

struct UniformAreaLightData
{
    alignas(16) uint32_t lightCount{ 0 };
    AreaLightData data[MAX_AREA_LIGHT_NUM];
};


struct TracingColors
{
	Color albedoColor{0,0,0,1};
	Color normalColor{0,0,0,1};
	Color finalColor{0,0,0,1};

	TracingColors() = default;

	TracingColors(const TracingColors& other)
	{
		albedoColor = other.albedoColor;
		finalColor = other.finalColor;
		normalColor = other.normalColor;
	}

	TracingColors& operator=(const TracingColors& other)
	{
		albedoColor = other.albedoColor;
		finalColor = other.finalColor;
		normalColor = other.normalColor;
		return *this;
	}

	TracingColors& Mul(TracingColors& other)
	{
		albedoColor = albedoColor.MulWithoutAlpha(other.albedoColor);
		normalColor = normalColor.MulWithoutAlpha(other.normalColor);
		finalColor = finalColor.MulWithoutAlpha(other.finalColor);
		return *this;
	}

	TracingColors operator+(TracingColors&& other)
	{
		TracingColors res(*this);
		res.albedoColor = res.albedoColor + other.albedoColor;
		res.normalColor = res.normalColor + other.normalColor;
		res.finalColor = res.finalColor + other.finalColor;
		return res;
	}

	TracingColors operator/(float para)
	{
		TracingColors res(*this);
		res.albedoColor = res.albedoColor / para;
		res.normalColor = res.normalColor / para;
		res.finalColor = res.finalColor / para;
		return res;
	}

	TracingColors operator*(float para)
	{
		TracingColors res(*this);
		res.albedoColor = res.albedoColor * para;
		res.normalColor = res.normalColor * para;
		res.finalColor = res.finalColor * para;
		return res;
	}
};

#ifndef VULKAN_RT
class Renderer
{
public:
	Renderer();
	Renderer(uint32_t width, uint32_t height);
	~Renderer();

	void OnResize(uint32_t width, uint32_t height);
	void Render(Camera& camera, RenderScene& scene, bool isAdaptiveNoise = true, bool isDenoise = false);
	TracingColors Ray_Colors(const Ray& ray, int depth, RenderScene& scene, bool isFirstHit = false);
	bool CalculateDirectLightAttenuation(const LightSample& lightSample, const Ray& ray_in, const HitResult& hitres, RenderScene& scene, Color& resColor);

	inline std::shared_ptr<Walnut::Image> GetFinalImage() const { return finalImage_; }
	inline std::shared_ptr<Walnut::Image> GetNormalImage() const { return normalImage_; }
	inline std::shared_ptr<Walnut::Image> GetAlbedoImage() const { return albedoImage_; }
	inline void SetMaxRenderSampleCount(uint32_t count) { max_render_samples_per_pixel_ = count; }
	inline void SetMinRenderSampleCount(uint32_t count) { min_render_samples_per_pixel_ = count; }
	inline void SetMinRenderNoiseThreshold(double threshold) { min_render_noise_threshold = threshold; }
	inline void SetMaxPreviewSampleCount(uint32_t count) { max_preview_samples_per_pixel_ = count; }
	inline void SetMaxBounceCount(uint32_t count) { max_bounce_count_ = count; }
	inline void SetUseMT(bool use) { use_MT_Acceleration_ = use; }

private:
	// For Denoise
	std::shared_ptr<Walnut::Image> albedoImage_;
	std::shared_ptr<Walnut::Image> normalImage_;
	std::shared_ptr<Walnut::Image> finalImage_;

	uint32_t* finalImageData_ {nullptr};
	uint32_t* normalImageData_ {nullptr};
	uint32_t* albedoImageData_ {nullptr};

	Denoiser denoiser_;
	// 
	uint32_t max_bounce_count_ = 4;
	uint32_t max_render_samples_per_pixel_ = Default_Max_Render_Samples_Per_Pixel;
	uint32_t min_render_samples_per_pixel_ = Default_Min_Render_Samples_Per_Pixel;
	uint32_t max_preview_samples_per_pixel_ = Default_Max_Preview_Samples_Per_Pixel;
	double min_render_noise_threshold = Default_Render_Noise_Threshold;
	// MT
	bool use_MT_Acceleration_ = true;
	std::vector<uint32_t> MT_Vertical_Iter;
};

#else

class Renderer
{
public:
	Renderer();
	~Renderer();

	void OnResize(uint32_t width, uint32_t height);
	void Render(Camera& camera, RenderScene& scene, bool isAdaptiveNoise = true, bool isDenoise = false);

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
	std::shared_ptr<RTModel> model_;
	// Utils
	//PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
	//PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
	//PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
	//PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
	//PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
	//PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
	//PFN_vkBuildAccelerationStructuresKHR vkBuildAccelerationStructuresKHR;
	//PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
	//PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
	//PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;

	struct FrameData
	{
		vulkan::VulkanMemoryResource* RTUniformBuffer_;
		vulkan::VulkanMemoryResource* DenoiseUniformBuffer_;
		VkDescriptorSet rtDescriptorSet_;
		std::array<VkDescriptorSet, 5> denoiseDescriptorSets_;
	};


	bool isNeedTransition = false;
	uint32_t nowFrameCount{ 0 };

	VkPhysicalDeviceRayTracingPipelinePropertiesKHR  rayTracingPipelineProperties_{};
	VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures_{};

	AccelerationStructure bottomLevelAS_{};
	AccelerationStructure topLevelAS_{};

	vulkan::VulkanMemoryResource* transformBuffer_;
	vulkan::VulkanLocalBuffer* geometryNodeBuffer_;
	vulkan::VulkanLocalBuffer* areaLightBuffer_;
	// 
	std::vector<FrameData> frameDatas_;
	
	std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups_ {};
	vulkan::VulkanMemoryResource* raygenShaderBindingTable_;
	vulkan::VulkanMemoryResource* missShaderBindingTable_;
	vulkan::VulkanMemoryResource* hitShaderBindingTable_;

	// Ray Tracing Pass
	VkPipeline rtPipeline_;
	VkPipelineLayout rtPipelineLayout_;
	VkDescriptorSetLayout rtDescriptorSetLayout_;

	// Denoise Pass
	VkPipeline denoisePipeline_;
	VkPipelineLayout denoisePipelineLayout_;
	VkDescriptorSetLayout denoiseDescriptorSetLayout_;

	std::array<vulkan::VulkanMemoryResource*, 5> denoiseUniformBuffers_;
	glm::mat4 lastFrameCameraVPMatrix_;

	// temp
	std::vector<VkShaderModule> shaderModules_;

	void InitRayTracing();
	void createBottomLevelAccelerationStructure();
	void createTopLevelAccelerationStructure();
	void createUniformBuffer();

	void createRayTracingPipeline();
	void createDenoisePipeline();

	void createShaderBindingTable();
	void createDescriptorSets();
	void updateDescriptorSets();
	void CleanUpRayTracing();
	void buildCommandBuffers(ImGui_ImplVulkanH_Window* wd, Camera* camera);
	//
	uint64_t getBufferDeviceAddress(VkBuffer buffer);
	uint64_t getAccelerationStructureDeviceAddress(VkAccelerationStructureKHR accelerationStructure);
	void destoryAccelerationStructure(VkAccelerationStructureKHR handle);
	RayTracingScratchBuffer createScratchBuffer(VkDeviceSize size);
	void deleteScratchBuffer(RayTracingScratchBuffer& scratchBuffer);
	void createAccelerationStructureBuffer(AccelerationStructure& accelerationStructure, VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo);
	VkPipelineShaderStageCreateInfo loadShader(std::string fileName, VkShaderStageFlagBits stage);

};

#endif

class Camera
{
public:
	Camera(glm::vec3 position, glm::vec3 front);

	void Tick(float ts, uint32_t width, uint32_t height);


	Ray GetRay(float u, float v);
	Ray GetNormalizedRay(float u, float v);

	void SetFocusDistance(float dis) { focus_distance_ = dis; }
	void SetDOFFocusDistance(float dis) { DOF_focus_distance_ = dis; }	// �������ھ���Ľ���
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
	float focus_distance_ = 2;	// Ӱ��FOV�Ľ���

	// 
	uint32_t* cachedAccumulateImageData_ {nullptr};

	// ����������任������ϵ
	glm::vec3 screen_left_down_corner_;
	// DOF
	bool useDOF_ = true;
	float DOF_focus_distance_ = 6;	// ���ھ���Ľ���
	float lens_radius_ = 0.02f;

	// �����ڽ�ƽ���ϵ�����ϵ�����ڼ������յ�ray target 
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
