#pragma once

#include "Walnut/myVulkan/myVulkanInclude.h"

#include <memory>
#include <string>
#include <vector>

namespace rt
{
	struct AccelerationStructure
	{
		VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
		uint64_t deviceAddress = 0;
		VkAccelerationStructureTypeKHR type = VK_ACCELERATION_STRUCTURE_TYPE_MAX_ENUM_KHR;
		std::unique_ptr<vulkan::VulkanMemoryResource> buffer;
	};

	struct AccelerationStructureBuildDesc
	{
		VkAccelerationStructureTypeKHR type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		std::vector<VkAccelerationStructureGeometryKHR> geometries;
		std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges;
		std::vector<uint32_t> primitiveCounts;
	};

	struct ShaderStageDesc
	{
		std::string filePath;
		VkShaderStageFlagBits stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
		std::string entryPoint = "main";
	};

	struct ShaderGroupDesc
	{
		VkRayTracingShaderGroupTypeKHR type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		uint32_t generalShader = VK_SHADER_UNUSED_KHR;
		uint32_t closestHitShader = VK_SHADER_UNUSED_KHR;
		uint32_t anyHitShader = VK_SHADER_UNUSED_KHR;
		uint32_t intersectionShader = VK_SHADER_UNUSED_KHR;
	};

	struct RTPipelineCreateInfo
	{
		VkPipelineLayout layout = VK_NULL_HANDLE;
		uint32_t maxPipelineRayRecursionDepth = 1;
		std::vector<ShaderStageDesc> shaderStages;
		std::vector<ShaderGroupDesc> shaderGroups;
	};

	class RTPipeline
	{
	public:
		RTPipeline() = default;
		~RTPipeline();

		RTPipeline(const RTPipeline&) = delete;
		RTPipeline& operator=(const RTPipeline&) = delete;
		RTPipeline(RTPipeline&& other) noexcept;
		RTPipeline& operator=(RTPipeline&& other) noexcept;

		void Destroy();

		inline VkPipeline handle() const { return pipeline_; }
		inline VkPipelineLayout layout() const { return layout_; }
		inline uint32_t shaderGroupCount() const { return shaderGroupCount_; }

	private:
		friend class VulkanRTBackend;

		VkDevice device_ = VK_NULL_HANDLE;
		VkPipeline pipeline_ = VK_NULL_HANDLE;
		VkPipelineLayout layout_ = VK_NULL_HANDLE;
		uint32_t shaderGroupCount_ = 0;
	};

	class ShaderBindingTable
	{
	public:
		ShaderBindingTable() = default;
		~ShaderBindingTable() = default;

		ShaderBindingTable(const ShaderBindingTable&) = delete;
		ShaderBindingTable& operator=(const ShaderBindingTable&) = delete;
		ShaderBindingTable(ShaderBindingTable&&) noexcept = default;
		ShaderBindingTable& operator=(ShaderBindingTable&&) noexcept = default;

		void Reset();

		inline const VkStridedDeviceAddressRegionKHR& raygenRegion() const { return raygenRegion_; }
		inline const VkStridedDeviceAddressRegionKHR& missRegion() const { return missRegion_; }
		inline const VkStridedDeviceAddressRegionKHR& hitRegion() const { return hitRegion_; }
		inline const VkStridedDeviceAddressRegionKHR& callableRegion() const { return callableRegion_; }

	private:
		friend class VulkanRTBackend;

		std::unique_ptr<vulkan::VulkanMemoryResource> raygenBuffer_;
		std::unique_ptr<vulkan::VulkanMemoryResource> missBuffer_;
		std::unique_ptr<vulkan::VulkanMemoryResource> hitBuffer_;
		std::unique_ptr<vulkan::VulkanMemoryResource> callableBuffer_;

		VkStridedDeviceAddressRegionKHR raygenRegion_{};
		VkStridedDeviceAddressRegionKHR missRegion_{};
		VkStridedDeviceAddressRegionKHR hitRegion_{};
		VkStridedDeviceAddressRegionKHR callableRegion_{};
	};

	class VulkanRTBackend
	{
	public:
		struct CreateInfo
		{
			VkDevice device = VK_NULL_HANDLE;
			VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
			vulkan::VulkanAllocator* allocator = nullptr;
			vulkan::CommandPool* commandPool = nullptr;
			VkQueue queue = VK_NULL_HANDLE;
			DynamicLoader* dynamicLoader = nullptr;
		};

		explicit VulkanRTBackend(const CreateInfo& createInfo);

		VulkanRTBackend(const VulkanRTBackend&) = delete;
		VulkanRTBackend& operator=(const VulkanRTBackend&) = delete;

		inline const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rayTracingPipelineProperties() const { return rayTracingPipelineProperties_; }
		inline const VkPhysicalDeviceAccelerationStructureFeaturesKHR& accelerationStructureFeatures() const { return accelerationStructureFeatures_; }

		uint64_t GetBufferDeviceAddress(VkBuffer buffer) const;
		uint64_t GetAccelerationStructureDeviceAddress(VkAccelerationStructureKHR accelerationStructure) const;

		AccelerationStructure BuildAccelerationStructure(const AccelerationStructureBuildDesc& buildDesc) const;
		void DestroyAccelerationStructure(AccelerationStructure& accelerationStructure) const;

		RTPipeline CreateRayTracingPipeline(const RTPipelineCreateInfo& createInfo) const;
		ShaderBindingTable CreateShaderBindingTable(const RTPipeline& pipeline, uint32_t raygenGroupCount, uint32_t missGroupCount, uint32_t hitGroupCount, uint32_t callableGroupCount = 0) const;

		void CmdTraceRays(VkCommandBuffer commandBuffer, const RTPipeline& pipeline, const ShaderBindingTable& shaderBindingTable, VkDescriptorSet descriptorSet, uint32_t width, uint32_t height, uint32_t depth = 1) const;

	private:
		struct ScratchBuffer
		{
			uint64_t deviceAddress = 0;
			std::unique_ptr<vulkan::VulkanMemoryResource> buffer;
		};

		PFN_vkVoidFunction LoadDeviceFunction(const char* name) const;
		void QueryRayTracingDeviceInfo();
		ScratchBuffer CreateScratchBuffer(VkDeviceSize size) const;
		void ValidateBuildDesc(const AccelerationStructureBuildDesc& buildDesc) const;

	private:
		VkDevice device_ = VK_NULL_HANDLE;
		VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
		vulkan::VulkanAllocator* allocator_ = nullptr;
		vulkan::CommandPool* commandPool_ = nullptr;
		VkQueue queue_ = VK_NULL_HANDLE;
		DynamicLoader* dynamicLoader_ = nullptr;

		VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties_{};
		VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures_{};
	};
}
