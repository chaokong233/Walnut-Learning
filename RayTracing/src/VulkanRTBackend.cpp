#include "VulkanRTBackend.h"

#include "Walnut/tool.hpp"

#include <cassert>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace rt
{
	namespace
	{
		VkRayTracingShaderGroupCreateInfoKHR ToVulkanShaderGroup(const ShaderGroupDesc& desc)
		{
			VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
			shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			shaderGroup.type = desc.type;
			shaderGroup.generalShader = desc.generalShader;
			shaderGroup.closestHitShader = desc.closestHitShader;
			shaderGroup.anyHitShader = desc.anyHitShader;
			shaderGroup.intersectionShader = desc.intersectionShader;
			return shaderGroup;
		}

		std::vector<uint8_t> BuildShaderBindingTableSegment(const std::vector<uint8_t>& shaderHandleStorage, uint32_t firstGroup, uint32_t groupCount, uint32_t handleSize, uint32_t handleSizeAligned)
		{
			std::vector<uint8_t> segment(handleSizeAligned * groupCount);
			for (uint32_t group = 0; group < groupCount; group++)
			{
				const uint8_t* src = shaderHandleStorage.data() + (firstGroup + group) * handleSize;
				uint8_t* dst = segment.data() + group * handleSizeAligned;
				std::memcpy(dst, src, handleSize);
			}
			return segment;
		}
	}

	RTPipeline::~RTPipeline()
	{
		Destroy();
	}

	RTPipeline::RTPipeline(RTPipeline&& other) noexcept
	{
		*this = std::move(other);
	}

	RTPipeline& RTPipeline::operator=(RTPipeline&& other) noexcept
	{
		if (this != &other)
		{
			Destroy();

			device_ = other.device_;
			pipeline_ = other.pipeline_;
			layout_ = other.layout_;
			shaderGroupCount_ = other.shaderGroupCount_;

			other.device_ = VK_NULL_HANDLE;
			other.pipeline_ = VK_NULL_HANDLE;
			other.layout_ = VK_NULL_HANDLE;
			other.shaderGroupCount_ = 0;
		}

		return *this;
	}

	void RTPipeline::Destroy()
	{
		if (pipeline_ != VK_NULL_HANDLE)
		{
			vkDestroyPipeline(device_, pipeline_, nullptr);
			pipeline_ = VK_NULL_HANDLE;
		}
		shaderGroupCount_ = 0;
	}

	void ShaderBindingTable::Reset()
	{
		raygenBuffer_.reset();
		missBuffer_.reset();
		hitBuffer_.reset();
		callableBuffer_.reset();

		raygenRegion_ = {};
		missRegion_ = {};
		hitRegion_ = {};
		callableRegion_ = {};
	}

	VulkanRTBackend::VulkanRTBackend(const CreateInfo& createInfo)
		: device_(createInfo.device), physicalDevice_(createInfo.physicalDevice), allocator_(createInfo.allocator),
		  commandPool_(createInfo.commandPool), queue_(createInfo.queue), dynamicLoader_(createInfo.dynamicLoader)
	{
		if (device_ == VK_NULL_HANDLE || physicalDevice_ == VK_NULL_HANDLE || !allocator_ || !commandPool_ || queue_ == VK_NULL_HANDLE || !dynamicLoader_)
		{
			throw std::runtime_error("VulkanRTBackend was created with an incomplete context");
		}

		QueryRayTracingDeviceInfo();
	}

	uint64_t VulkanRTBackend::GetBufferDeviceAddress(VkBuffer buffer) const
	{
		VkBufferDeviceAddressInfoKHR bufferDeviceAddressInfo{};
		bufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		bufferDeviceAddressInfo.buffer = buffer;

		auto func = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(LoadDeviceFunction("vkGetBufferDeviceAddressKHR"));
		return func(device_, &bufferDeviceAddressInfo);
	}

	uint64_t VulkanRTBackend::GetAccelerationStructureDeviceAddress(VkAccelerationStructureKHR accelerationStructure) const
	{
		VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
		accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		accelerationDeviceAddressInfo.accelerationStructure = accelerationStructure;

		auto func = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(LoadDeviceFunction("vkGetAccelerationStructureDeviceAddressKHR"));
		return func(device_, &accelerationDeviceAddressInfo);
	}

	AccelerationStructure VulkanRTBackend::BuildAccelerationStructure(const AccelerationStructureBuildDesc& buildDesc) const
	{
		ValidateBuildDesc(buildDesc);

		VkAccelerationStructureBuildGeometryInfoKHR sizeGeometryInfo{};
		sizeGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		sizeGeometryInfo.type = buildDesc.type;
		sizeGeometryInfo.flags = buildDesc.flags;
		sizeGeometryInfo.geometryCount = static_cast<uint32_t>(buildDesc.geometries.size());
		sizeGeometryInfo.pGeometries = buildDesc.geometries.data();

		VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{};
		buildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

		auto getBuildSizes = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(LoadDeviceFunction("vkGetAccelerationStructureBuildSizesKHR"));
		getBuildSizes(
			device_,
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&sizeGeometryInfo,
			buildDesc.primitiveCounts.data(),
			&buildSizesInfo);

		AccelerationStructure accelerationStructure{};
		accelerationStructure.type = buildDesc.type;
		accelerationStructure.buffer = allocator_->createBuffer(
			buildSizesInfo.accelerationStructureSize,
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

		VkAccelerationStructureCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		createInfo.buffer = accelerationStructure.buffer->buffer();
		createInfo.size = buildSizesInfo.accelerationStructureSize;
		createInfo.type = buildDesc.type;

		auto createAccelerationStructure = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(LoadDeviceFunction("vkCreateAccelerationStructureKHR"));
		VkResult result = createAccelerationStructure(device_, &createInfo, nullptr, &accelerationStructure.handle);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create acceleration structure");
		}

		ScratchBuffer scratchBuffer = CreateScratchBuffer(buildSizesInfo.buildScratchSize);

		VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo = sizeGeometryInfo;
		buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		buildGeometryInfo.dstAccelerationStructure = accelerationStructure.handle;
		buildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

		std::vector<const VkAccelerationStructureBuildRangeInfoKHR*> rangeInfos(buildDesc.ranges.size());
		for (size_t i = 0; i < buildDesc.ranges.size(); i++)
		{
			rangeInfos[i] = &buildDesc.ranges[i];
		}

		{
			vulkan::SingleTimeCommands cmd(commandPool_);
			auto cmdBuildAccelerationStructures = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(LoadDeviceFunction("vkCmdBuildAccelerationStructuresKHR"));
			cmdBuildAccelerationStructures(
				cmd.getBuffer(),
				1,
				&buildGeometryInfo,
				rangeInfos.data());
			cmd.Submit(queue_);
		}

		accelerationStructure.deviceAddress = GetAccelerationStructureDeviceAddress(accelerationStructure.handle);
		return accelerationStructure;
	}

	void VulkanRTBackend::DestroyAccelerationStructure(AccelerationStructure& accelerationStructure) const
	{
		if (accelerationStructure.handle != VK_NULL_HANDLE)
		{
			auto destroyAccelerationStructure = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(LoadDeviceFunction("vkDestroyAccelerationStructureKHR"));
			destroyAccelerationStructure(device_, accelerationStructure.handle, nullptr);
		}

		accelerationStructure.handle = VK_NULL_HANDLE;
		accelerationStructure.deviceAddress = 0;
		accelerationStructure.type = VK_ACCELERATION_STRUCTURE_TYPE_MAX_ENUM_KHR;
		accelerationStructure.buffer.reset();
	}

	RTPipeline VulkanRTBackend::CreateRayTracingPipeline(const RTPipelineCreateInfo& createInfo) const
	{
		if (createInfo.layout == VK_NULL_HANDLE)
		{
			throw std::runtime_error("ray tracing pipeline needs a valid pipeline layout");
		}
		if (createInfo.shaderStages.empty() || createInfo.shaderGroups.empty())
		{
			throw std::runtime_error("ray tracing pipeline needs shader stages and shader groups");
		}

		std::vector<VkShaderModule> shaderModules;
		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
		shaderModules.reserve(createInfo.shaderStages.size());
		shaderStages.reserve(createInfo.shaderStages.size());

		for (const ShaderStageDesc& stageDesc : createInfo.shaderStages)
		{
			VkShaderModule shaderModule = vulkan::loadShader(stageDesc.filePath.c_str(), device_);
			if (shaderModule == VK_NULL_HANDLE)
			{
				throw std::runtime_error("failed to load ray tracing shader: " + stageDesc.filePath);
			}

			shaderModules.push_back(shaderModule);

			VkPipelineShaderStageCreateInfo shaderStage{};
			shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStage.stage = stageDesc.stage;
			shaderStage.module = shaderModule;
			shaderStage.pName = stageDesc.entryPoint.c_str();
			shaderStages.push_back(shaderStage);
		}

		std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;
		shaderGroups.reserve(createInfo.shaderGroups.size());
		for (const ShaderGroupDesc& groupDesc : createInfo.shaderGroups)
		{
			shaderGroups.push_back(ToVulkanShaderGroup(groupDesc));
		}

		VkRayTracingPipelineCreateInfoKHR pipelineCreateInfo{};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
		pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCreateInfo.pStages = shaderStages.data();
		pipelineCreateInfo.groupCount = static_cast<uint32_t>(shaderGroups.size());
		pipelineCreateInfo.pGroups = shaderGroups.data();
		pipelineCreateInfo.maxPipelineRayRecursionDepth = createInfo.maxPipelineRayRecursionDepth;
		pipelineCreateInfo.layout = createInfo.layout;

		VkPipeline pipeline = VK_NULL_HANDLE;
		auto createRayTracingPipelines = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(LoadDeviceFunction("vkCreateRayTracingPipelinesKHR"));
		VkResult result = createRayTracingPipelines(device_, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline);

		for (VkShaderModule shaderModule : shaderModules)
		{
			vkDestroyShaderModule(device_, shaderModule, nullptr);
		}

		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create ray tracing pipeline");
		}

		RTPipeline rtPipeline{};
		rtPipeline.device_ = device_;
		rtPipeline.pipeline_ = pipeline;
		rtPipeline.layout_ = createInfo.layout;
		rtPipeline.shaderGroupCount_ = static_cast<uint32_t>(shaderGroups.size());
		return rtPipeline;
	}

	ShaderBindingTable VulkanRTBackend::CreateShaderBindingTable(const RTPipeline& pipeline, uint32_t raygenGroupCount, uint32_t missGroupCount, uint32_t hitGroupCount, uint32_t callableGroupCount) const
	{
		const uint32_t groupCount = pipeline.shaderGroupCount();
		if (groupCount == 0 || raygenGroupCount + missGroupCount + hitGroupCount + callableGroupCount > groupCount)
		{
			throw std::runtime_error("invalid shader binding table group counts");
		}

		const uint32_t handleSize = rayTracingPipelineProperties_.shaderGroupHandleSize;
		const uint32_t handleSizeAligned = tool::roundUp(rayTracingPipelineProperties_.shaderGroupHandleSize, rayTracingPipelineProperties_.shaderGroupHandleAlignment);
		const uint32_t shaderHandleStorageSize = groupCount * handleSize;

		std::vector<uint8_t> shaderHandleStorage(shaderHandleStorageSize);
		auto getShaderGroupHandles = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(LoadDeviceFunction("vkGetRayTracingShaderGroupHandlesKHR"));
		VkResult result = getShaderGroupHandles(device_, pipeline.handle(), 0, groupCount, shaderHandleStorageSize, shaderHandleStorage.data());
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to get ray tracing shader group handles");
		}

		ShaderBindingTable shaderBindingTable{};
		uint32_t firstGroup = 0;

		auto createSegment = [&](uint32_t segmentGroupCount, std::unique_ptr<vulkan::VulkanMemoryResource>& buffer, VkStridedDeviceAddressRegionKHR& region)
		{
			if (segmentGroupCount == 0)
			{
				region = {};
				return;
			}

			std::vector<uint8_t> segmentData = BuildShaderBindingTableSegment(shaderHandleStorage, firstGroup, segmentGroupCount, handleSize, handleSizeAligned);
			const VkDeviceSize segmentSize = segmentData.size();
			buffer = allocator_->createBuffer(
				segmentSize,
				VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				VMA_MEMORY_USAGE_CPU_ONLY);
			buffer->uploadData(segmentData.data(), segmentSize);

			region.deviceAddress = GetBufferDeviceAddress(buffer->buffer());
			region.stride = handleSizeAligned;
			region.size = segmentSize;
			firstGroup += segmentGroupCount;
		};

		createSegment(raygenGroupCount, shaderBindingTable.raygenBuffer_, shaderBindingTable.raygenRegion_);
		createSegment(missGroupCount, shaderBindingTable.missBuffer_, shaderBindingTable.missRegion_);
		createSegment(hitGroupCount, shaderBindingTable.hitBuffer_, shaderBindingTable.hitRegion_);
		createSegment(callableGroupCount, shaderBindingTable.callableBuffer_, shaderBindingTable.callableRegion_);

		return shaderBindingTable;
	}

	void VulkanRTBackend::CmdTraceRays(VkCommandBuffer commandBuffer, const RTPipeline& pipeline, const ShaderBindingTable& shaderBindingTable, VkDescriptorSet descriptorSet, uint32_t width, uint32_t height, uint32_t depth) const
	{
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline.handle());
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline.layout(), 0, 1, &descriptorSet, 0, nullptr);

		auto cmdTraceRays = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(LoadDeviceFunction("vkCmdTraceRaysKHR"));
		cmdTraceRays(
			commandBuffer,
			&shaderBindingTable.raygenRegion(),
			&shaderBindingTable.missRegion(),
			&shaderBindingTable.hitRegion(),
			&shaderBindingTable.callableRegion(),
			width,
			height,
			depth);
	}

	PFN_vkVoidFunction VulkanRTBackend::LoadDeviceFunction(const char* name) const
	{
		PFN_vkVoidFunction function = dynamicLoader_->GetDeviceProc(name);
		if (!function)
		{
			throw std::runtime_error(std::string("failed to load Vulkan device function: ") + name);
		}
		return function;
	}

	void VulkanRTBackend::QueryRayTracingDeviceInfo()
	{
		rayTracingPipelineProperties_ = {};
		rayTracingPipelineProperties_.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

		VkPhysicalDeviceProperties2 deviceProperties{};
		deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		deviceProperties.pNext = &rayTracingPipelineProperties_;
		vkGetPhysicalDeviceProperties2(physicalDevice_, &deviceProperties);

		accelerationStructureFeatures_ = {};
		accelerationStructureFeatures_.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

		VkPhysicalDeviceFeatures2 deviceFeatures{};
		deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		deviceFeatures.pNext = &accelerationStructureFeatures_;
		vkGetPhysicalDeviceFeatures2(physicalDevice_, &deviceFeatures);
	}

	VulkanRTBackend::ScratchBuffer VulkanRTBackend::CreateScratchBuffer(VkDeviceSize size) const
	{
		ScratchBuffer scratchBuffer{};
		scratchBuffer.buffer = allocator_->createBuffer(
			size,
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
		scratchBuffer.deviceAddress = GetBufferDeviceAddress(scratchBuffer.buffer->buffer());
		return scratchBuffer;
	}

	void VulkanRTBackend::ValidateBuildDesc(const AccelerationStructureBuildDesc& buildDesc) const
	{
		const size_t geometryCount = buildDesc.geometries.size();
		if (geometryCount == 0)
		{
			throw std::runtime_error("acceleration structure build needs at least one geometry");
		}
		if (buildDesc.ranges.size() != geometryCount || buildDesc.primitiveCounts.size() != geometryCount)
		{
			throw std::runtime_error("acceleration structure build arrays must match geometry count");
		}
	}
}
