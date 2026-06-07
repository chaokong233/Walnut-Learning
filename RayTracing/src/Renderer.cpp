#include "Renderer.h"
#include "Walnut/Input/Input.h"
#include "walnut/Application.h"
#include "Walnut/myVulkan/myVulkanInclude.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

#include <functional>
#include "Mesh.h"
#include "Scene.h"

#include "util.hpp"

	Renderer::Renderer(const Scene& scene)
	{
		InitRayTracing(scene);
	}

	Renderer::~Renderer()
	{
		CleanUpRayTracing();
	}

	void Renderer::SetScene(const Scene& scene)
	{
		scene_ = &scene;
		syncSceneResources();
	}

	void Renderer::OnResize(uint32_t width, uint32_t height)
	{
		//
		if (outputFinalImage_)
		{
			if (outputFinalImage_->GetWidth() == width && outputFinalImage_->GetHeight() == height)
				return;
			
			outputFinalImage_->Resize(width, height);
			lastFrameVarianceImage_->Resize(width, height);
			nowFrameRadianceImage_->Resize(width, height);
			nowFrameVarianceImage_->Resize(width, height);
			nowFrameAlbedoImage_->Resize(width, height);
			nowFrameNormalImage_->Resize(width, height);
			nowFrameWorldPositionImage_->Resize(width, height);
			lastFrameFinalImage_->Resize(width, height);
			lastFrameAlbedoImage_->Resize(width, height);
			lastFrameNormalImage_->Resize(width, height);
			lastFrameWorldPositionImage_->Resize(width, height);
		}
		else
		{
			outputFinalImage_ = std::make_shared<Walnut::StorageImage>(width, height, Walnut::ImageFormat::RGBA32F, g_pCommandPool);
			lastFrameVarianceImage_ = std::make_shared<Walnut::StorageImage>(width, height, Walnut::ImageFormat::RGBA32F, g_pCommandPool);
			nowFrameRadianceImage_ = std::make_shared<Walnut::StorageImage>(width, height, Walnut::ImageFormat::RGBA32F, g_pCommandPool);
			nowFrameVarianceImage_ = std::make_shared<Walnut::StorageImage>(width, height, Walnut::ImageFormat::RGBA32F, g_pCommandPool);
			nowFrameAlbedoImage_ = std::make_shared<Walnut::StorageImage>(width, height, Walnut::ImageFormat::RGBA32F, g_pCommandPool);
			nowFrameNormalImage_ = std::make_shared<Walnut::StorageImage>(width, height, Walnut::ImageFormat::RGBA32F, g_pCommandPool);
			nowFrameWorldPositionImage_ = std::make_shared<Walnut::StorageImage>(width, height, Walnut::ImageFormat::RGBA32F, g_pCommandPool);
			lastFrameFinalImage_ = std::make_shared<Walnut::StorageImage>(width, height, Walnut::ImageFormat::RGBA32F, g_pCommandPool);
			lastFrameAlbedoImage_ = std::make_shared<Walnut::StorageImage>(width, height, Walnut::ImageFormat::RGBA32F, g_pCommandPool);
			lastFrameNormalImage_ = std::make_shared<Walnut::StorageImage>(width, height, Walnut::ImageFormat::RGBA32F, g_pCommandPool);
			lastFrameWorldPositionImage_ = std::make_shared<Walnut::StorageImage>(width, height, Walnut::ImageFormat::RGBA32F, g_pCommandPool);
		}

		updateDescriptorSets();

		isNeedTransition = false;

	}

	void Renderer::Render(Camera& camera, bool isAdaptiveNoise, bool isDenoise)
	{
		syncSceneResources();

		outputFinalImage_.swap(lastFrameFinalImage_);
		nowFrameRadianceImage_.swap(lastFrameVarianceImage_);
		nowFrameAlbedoImage_.swap(lastFrameAlbedoImage_);
		nowFrameNormalImage_.swap(lastFrameNormalImage_);
		nowFrameWorldPositionImage_.swap(lastFrameWorldPositionImage_);

		ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
		auto renderFunc = std::bind(&Renderer::buildCommandBuffers, this, wd, &camera);
		s_VulkanRenderFuncQueue.push_back(renderFunc);

	}


	void Renderer::InitRayTracing(const Scene& scene)
	{
		scene_ = &scene;
		rtBackend_ = std::make_unique<rt::VulkanRTBackend>(rt::VulkanRTBackend::CreateInfo{
			.device = g_Device,
			.physicalDevice = g_PhysicalDevice,
			.allocator = g_pVkMemoryAllocator,
			.commandPool = g_pCommandPool,
			.queue = g_Queue,
			.dynamicLoader = &g_dynamicLoader
		});

		frameDatas_.resize(g_MinImageCount);

		rebuildSceneResources(scene);
		createUniformBuffer();
		createRayTracingPipeline();
		createDenoisePipeline();
		createShaderBindingTable();
		createDescriptorSets();
	}

	void Renderer::syncSceneResources()
	{
		if (!scene_ || !rtBackend_)
		{
			return;
		}

		if (scene_->GetRevision() == uploadedSceneRevision_)
		{
			return;
		}

		vkDeviceWaitIdle(g_Device);
		rebuildSceneResources(*scene_);
		if (rtDescriptorSetLayout_ != VK_NULL_HANDLE && outputFinalImage_)
		{
			updateDescriptorSets();
		}
	}

	void Renderer::rebuildSceneResources(const Scene& scene)
	{
		releaseSceneResources();
		createBottomLevelAccelerationStructure(scene);
		createTopLevelAccelerationStructure();
		uploadedSceneRevision_ = scene.GetRevision();
	}

	void Renderer::releaseSceneResources()
	{
		if (rtBackend_)
		{
			rtBackend_->DestroyAccelerationStructure(bottomLevelAS_);
			rtBackend_->DestroyAccelerationStructure(topLevelAS_);
		}

		delete transformBuffer_;
		transformBuffer_ = nullptr;
		delete geometryNodeBuffer_;
		geometryNodeBuffer_ = nullptr;
		delete lightsBuffer_;
		lightsBuffer_ = nullptr;
		model_.reset();
	}

	void Renderer::createBottomLevelAccelerationStructure(const Scene& scene)
	{
		VkTransformMatrixKHR transformMatrix = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f
		};

		transformBuffer_ = g_pVkMemoryAllocator->createBuffer(sizeof(VkTransformMatrixKHR),
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
			VMA_MEMORY_USAGE_CPU_ONLY).release();
		transformBuffer_->uploadData(&transformMatrix, VK_WHOLE_SIZE);

		model_ = std::make_shared<RTModel>(g_pVkMemoryAllocator, g_pCommandPool, g_Queue);
		model_->UploadScene(scene);

		VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress{};
		VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress{};
		VkDeviceOrHostAddressConstKHR transformBufferDeviceAddress{};

		vertexBufferDeviceAddress.deviceAddress = rtBackend_->GetBufferDeviceAddress(model_->vertices.buffer->buffer());
		indexBufferDeviceAddress.deviceAddress = rtBackend_->GetBufferDeviceAddress(model_->indices.buffer->buffer());
		transformBufferDeviceAddress.deviceAddress = rtBackend_->GetBufferDeviceAddress(transformBuffer_->buffer());

		std::vector<uint32_t> maxPrimitiveCounts;
		std::vector<VkAccelerationStructureGeometryKHR> geometries;
		std::vector<VkAccelerationStructureBuildRangeInfoKHR> accelerationBuildStructureRangeInfos;
		std::vector<GeometryNode> geometriesNode;

		for(auto& mesh : model_->linerMeshes)
		{
			if(mesh->geometry.indexCount <= 0) continue;

			VkDeviceOrHostAddressConstKHR indexBufferDeviceAddressOffseted {};
			indexBufferDeviceAddressOffseted.deviceAddress = indexBufferDeviceAddress.deviceAddress + mesh->geometry.firstIndex * sizeof(uint32_t);

			VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
			accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
			accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
			accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
			accelerationStructureGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
			accelerationStructureGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;

			accelerationStructureGeometry.geometry.triangles.vertexData = vertexBufferDeviceAddress;
			accelerationStructureGeometry.geometry.triangles.maxVertex = model_->vertices.count;
			accelerationStructureGeometry.geometry.triangles.vertexStride = sizeof(Vertex);

			accelerationStructureGeometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
			accelerationStructureGeometry.geometry.triangles.indexData = indexBufferDeviceAddressOffseted;

			accelerationStructureGeometry.geometry.triangles.transformData = transformBufferDeviceAddress;

			geometries.push_back(accelerationStructureGeometry);

			uint32_t numTriangle = mesh->geometry.indexCount / 3;
			VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
			buildRangeInfo.primitiveCount = numTriangle;

			accelerationBuildStructureRangeInfos.push_back(buildRangeInfo);

			maxPrimitiveCounts.push_back(numTriangle);

			GeometryNode node;
			node.VertexBufferDeviceAddress = vertexBufferDeviceAddress.deviceAddress;
			node.IndexBufferDeviceAddress = indexBufferDeviceAddressOffseted.deviceAddress;

			node.BaseColor = mesh->material.BaseColor;
			node.EmissiveColor = mesh->material.EmissiveColor;
			node.SpecularTint = mesh->material.SpecularTint;
			node.Roughness = mesh->material.Roughness;
			node.Metallic = mesh->material.Metallic;
			node.Specular = mesh->material.Specular;
			node.Subsurface = mesh->material.Subsurface;
			node.Anisotropic = mesh->material.Anisotropic;

			node.BaseColorTextureID = mesh->material.BaseColorTextureID;
			node.IBLTextureID = mesh->material.IBLTextureID;

			geometriesNode.push_back(node);
			
		}
		vulkan::VulkanLocalBuffer::CopierCreateInfo copierInfo {
				.commandPool = g_pCommandPool,
				.transferQueue = g_Queue
		};
		VkDeviceSize bufferSize = geometriesNode.size() * sizeof(GeometryNode);
		// Geometry Node Buffer
		geometryNodeBuffer_ = new vulkan::VulkanLocalBuffer(g_pVkMemoryAllocator, bufferSize,
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, copierInfo);
		geometryNodeBuffer_->UploadMemory(geometriesNode.data(), bufferSize, 0);

		rt::AccelerationStructureBuildDesc buildDesc{};
		buildDesc.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		buildDesc.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		buildDesc.geometries = geometries;
		buildDesc.ranges = accelerationBuildStructureRangeInfos;
		buildDesc.primitiveCounts = maxPrimitiveCounts;
		bottomLevelAS_ = rtBackend_->BuildAccelerationStructure(buildDesc);

		UniformLightsData lightData{};
		lightData.areaLightCount = static_cast<uint32_t>(std::min<size_t>(scene.GetAreaLights().size(), MAX_AREA_LIGHT_NUM));
		for (uint32_t i = 0; i < lightData.areaLightCount; i++)
		{
			const AreaLight& source = scene.GetAreaLights()[i];
			AreaLightData& target = lightData.areaLightsData[i];
			target.beginPos = source.beginPos;
			target.u = source.u;
			target.v = source.v;
			target.color = source.color;
			target.rayDir = source.rayDir;
		}

		lightData.radiusLightCount = static_cast<uint32_t>(std::min<size_t>(scene.GetRadiusLights().size(), MAX_RADIUS_LIGHT_NUM));
		for (uint32_t i = 0; i < lightData.radiusLightCount; i++)
		{
			const RadiusLight& source = scene.GetRadiusLights()[i];
			RadiusLightData& target = lightData.radiusLightsData[i];
			target.centerPos = source.centerPos;
			target.color = source.color;
			target.radius = source.radius;
		}

		lightsBuffer_ = new vulkan::VulkanLocalBuffer(g_pVkMemoryAllocator, sizeof(UniformLightsData),
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, copierInfo);
		lightsBuffer_->UploadMemory(&lightData, sizeof(UniformLightsData), 0);

	}

	void Renderer::createTopLevelAccelerationStructure()
	{
		VkTransformMatrixKHR transformMatrix = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f };

		VkAccelerationStructureInstanceKHR instance{};
		instance.transform = transformMatrix;
		instance.instanceCustomIndex = 0;
		instance.mask = 0xFF;
		instance.instanceShaderBindingTableRecordOffset = 0;
		instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		instance.accelerationStructureReference = bottomLevelAS_.deviceAddress;

		vulkan::VulkanMemoryResource* instancesBuffer = g_pVkMemoryAllocator->createBuffer(sizeof(VkAccelerationStructureInstanceKHR),
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
			VMA_MEMORY_USAGE_CPU_ONLY).release();
		instancesBuffer->uploadData(&instance, VK_WHOLE_SIZE);

		VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress{};
		instanceDataDeviceAddress.deviceAddress = rtBackend_->GetBufferDeviceAddress(instancesBuffer->buffer());

		VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
		accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		accelerationStructureGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
		accelerationStructureGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
		accelerationStructureGeometry.geometry.instances.data = instanceDataDeviceAddress;

		VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
		accelerationStructureBuildRangeInfo.primitiveCount = 1;
		accelerationStructureBuildRangeInfo.primitiveOffset = 0;
		accelerationStructureBuildRangeInfo.firstVertex = 0;
		accelerationStructureBuildRangeInfo.transformOffset = 0;

		rt::AccelerationStructureBuildDesc buildDesc{};
		buildDesc.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		buildDesc.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		buildDesc.geometries = { accelerationStructureGeometry };
		buildDesc.ranges = { accelerationStructureBuildRangeInfo };
		buildDesc.primitiveCounts = { 1 };
		topLevelAS_ = rtBackend_->BuildAccelerationStructure(buildDesc);

		delete instancesBuffer;
	}

	void Renderer::createUniformBuffer()
	{
		// Frame Data
		CameraUniformData cameraData{};
		cameraData.samples = 4;
		cameraData.frame = 1;

		for (size_t i = 0; i < g_MinImageCount; i++)
		{
			// RT Pass
			 frameDatas_[i].RTUniformBuffer_ = g_pVkMemoryAllocator->createBuffer(sizeof(CameraUniformData),
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VMA_MEMORY_USAGE_AUTO_PREFER_HOST, VMA_ALLOCATION_CREATE_MAPPED_BIT).release();

			frameDatas_[i].RTUniformBuffer_->uploadData(&cameraData, sizeof(CameraUniformData), 0);

			// Denoise Camera
			frameDatas_[i].DenoiseUniformBuffer_ = g_pVkMemoryAllocator->createBuffer(sizeof(DenoiseCameraUniformData),
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VMA_MEMORY_USAGE_AUTO_PREFER_HOST, VMA_ALLOCATION_CREATE_MAPPED_BIT).release();

		}

		// Denosie pass
		DenoiseUniformData denoiseData{};	
		for (size_t i = 0; i < denoiseUniformBuffers_.size(); i++)
		{
			denoiseData.kernel_size = i + 1;
			// Denoise Pass
			denoiseUniformBuffers_[i] = g_pVkMemoryAllocator->createBuffer(sizeof(DenoiseUniformData),
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VMA_MEMORY_USAGE_AUTO_PREFER_HOST, VMA_ALLOCATION_CREATE_MAPPED_BIT).release();

			denoiseUniformBuffers_[i]->uploadData(&denoiseData, sizeof(DenoiseUniformData), 0);
		}
	}

	void Renderer::createRayTracingPipeline()
	{
		vulkan::DescriptorLayoutBuilder layoutBuilder1;
		// TLAS
        layoutBuilder1.AddBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr);
		// Image
        layoutBuilder1.AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr);
        layoutBuilder1.AddBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr);
        layoutBuilder1.AddBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr);
        layoutBuilder1.AddBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr);
		// Placeholder
        layoutBuilder1.AddBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr);
		// Camera Info
        layoutBuilder1.AddBinding(6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr);
		// Geometry Index
        layoutBuilder1.AddBinding(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr);
		// Light Info
        layoutBuilder1.AddBinding(8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr);
		// Texture
        layoutBuilder1.AddBinding(9, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr, g_texturePool->GetImageInfo()->size());
        layoutBuilder1.build(g_Device, rtDescriptorSetLayout_);

		VkPipelineLayoutCreateInfo pipelineLayoutCI{};
		pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCI.setLayoutCount = 1;
		pipelineLayoutCI.pSetLayouts = &rtDescriptorSetLayout_;
		vkCreatePipelineLayout(g_Device, &pipelineLayoutCI, nullptr, &rtPipelineLayout_);

		std::vector<rt::ShaderStageDesc> shaderStages = {
			{ "E:/Git/Walnut-Learning/Walnut-Learning/Walnut/src/Walnut/shaders/rt/raygen.rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR },
			{ "E:/Git/Walnut-Learning/Walnut-Learning/Walnut/src/Walnut/shaders/rt/miss.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR },
			{ "E:/Git/Walnut-Learning/Walnut-Learning/Walnut/src/Walnut/shaders/rt/shadow.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR },
			{ "E:/Git/Walnut-Learning/Walnut-Learning/Walnut/src/Walnut/shaders/rt/closesthit.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR }
		};

		std::vector<rt::ShaderGroupDesc> shaderGroups = {
			{ VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 0 },
			{ VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 1 },
			{ VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 2 },
			{ VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, 3 }
		};

		rtPipeline_ = rtBackend_->CreateRayTracingPipeline({
			.layout = rtPipelineLayout_,
			.maxPipelineRayRecursionDepth = 1,
			.shaderStages = shaderStages,
			.shaderGroups = shaderGroups
		});
	}

	void Renderer::createDenoisePipeline()
	{
		// Layout
		vulkan::DescriptorLayoutBuilder layoutBuilder1;
        layoutBuilder1.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, nullptr);
        layoutBuilder1.AddBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, nullptr);
        layoutBuilder1.AddBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, nullptr);
        layoutBuilder1.AddBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, nullptr);
        layoutBuilder1.AddBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, nullptr);
        layoutBuilder1.AddBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, nullptr);
        layoutBuilder1.AddBinding(6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, nullptr);
        layoutBuilder1.AddBinding(7, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, nullptr);
        layoutBuilder1.AddBinding(8, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, nullptr);
        layoutBuilder1.AddBinding(9, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, nullptr);
        layoutBuilder1.AddBinding(10, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, nullptr);
        layoutBuilder1.AddBinding(11, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, nullptr);
        layoutBuilder1.AddBinding(12, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, nullptr);

        layoutBuilder1.build(g_Device, denoiseDescriptorSetLayout_);

		VkPipelineLayoutCreateInfo pipelineLayoutCI{};
		pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCI.setLayoutCount = 1;
		pipelineLayoutCI.pSetLayouts = &denoiseDescriptorSetLayout_;
		vkCreatePipelineLayout(g_Device, &pipelineLayoutCI, nullptr, &denoisePipelineLayout_);

		// Shader
		VkShaderModule shaderModule = vulkan::loadShader("E:/Git/Walnut-Learning/Walnut-Learning/Walnut/src/Walnut/shaders/denoise/svgf.comp.spv", g_Device);
		VkPipelineShaderStageCreateInfo shaderStage{};
		shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		shaderStage.module = shaderModule;
		shaderStage.pName = "main";

		// Pipeline
		VkComputePipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipelineInfo.layout = denoisePipelineLayout_;
		pipelineInfo.stage = shaderStage;

		if (vkCreateComputePipelines(g_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &denoisePipeline_) != VK_SUCCESS) {
			throw std::runtime_error("failed to create compute pipeline!");
		}
		vkDestroyShaderModule(g_Device, shaderModule, nullptr);
	}

	void Renderer::createShaderBindingTable() {
		shaderBindingTable_ = rtBackend_->CreateShaderBindingTable(rtPipeline_, 1, 2, 1);
	}

	void Renderer::createDescriptorSets()
	{
		// Allocate
		for (size_t i = 0; i < g_MinImageCount; i++)
		{
			// RT Pass
			g_DescriptorAllocator->allocate(g_Device, rtDescriptorSetLayout_, frameDatas_[i].rtDescriptorSet_);
			
				// Write
			vulkan::DescriptorWriter writer1;
			writer1.write_structure(0, 1, &topLevelAS_.handle, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
			writer1.write_buffer(6, frameDatas_[i].RTUniformBuffer_->buffer(), sizeof(CameraUniformData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
			writer1.write_buffer(7, geometryNodeBuffer_->buffer(), geometryNodeBuffer_->getSize(), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
			writer1.write_buffer(8, lightsBuffer_->buffer(), lightsBuffer_->getSize(), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
			if (g_texturePool->GetImageCount() > 0)
			{
				writer1.write_image(9, g_texturePool->GetImageInfo()->data(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, g_texturePool->GetImageCount());
			}
			writer1.update_set(g_Device, frameDatas_[i].rtDescriptorSet_);

			for (size_t j = 0; j < frameDatas_[i].denoiseDescriptorSets_.size(); j++)
			{
				// denoise Pass
				vulkan::DescriptorWriter writer2;
				g_DescriptorAllocator->allocate(g_Device, denoiseDescriptorSetLayout_, frameDatas_[i].denoiseDescriptorSets_[j]);
				writer2.write_buffer(0, denoiseUniformBuffers_[j]->buffer(), sizeof(DenoiseUniformData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
				writer2.write_buffer(1, frameDatas_[i].DenoiseUniformBuffer_->buffer(), sizeof(DenoiseCameraUniformData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
				writer2.update_set(g_Device, frameDatas_[i].denoiseDescriptorSets_[j]);
			}
		}
	}
	
	void Renderer::updateDescriptorSets()
	{
		// Allocate
		for (size_t i = 0; i < g_MinImageCount; i++)
		{
				// Write
			vulkan::DescriptorWriter writer1;
			writer1.write_structure(0, 1, &topLevelAS_.handle, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
			writer1.write_image(1, nowFrameRadianceImage_->GetImageView(), nowFrameRadianceImage_->GetSampler(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
			writer1.write_image(2, nowFrameAlbedoImage_->GetImageView(), nowFrameAlbedoImage_->GetSampler(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
			writer1.write_image(3, nowFrameNormalImage_->GetImageView(), nowFrameNormalImage_->GetSampler(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
			writer1.write_image(4, nowFrameWorldPositionImage_->GetImageView(), nowFrameWorldPositionImage_->GetSampler(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
			writer1.write_buffer(6, frameDatas_[i].RTUniformBuffer_->buffer(), sizeof(CameraUniformData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
			writer1.write_buffer(7, geometryNodeBuffer_->buffer(), geometryNodeBuffer_->getSize(), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
			writer1.write_buffer(8, lightsBuffer_->buffer(), lightsBuffer_->getSize(), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
			if (g_texturePool->GetImageCount() > 0)
			{
				writer1.write_image(9, g_texturePool->GetImageInfo()->data(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, g_texturePool->GetImageCount());
			}
			writer1.update_set(g_Device, frameDatas_[i].rtDescriptorSet_);

			// Denoise Pass 
			for (size_t j = 0; j < frameDatas_[i].denoiseDescriptorSets_.size(); j++)
			{
				vulkan::DescriptorWriter writer2;
				writer2.write_image(4, nowFrameVarianceImage_->GetImageView(), nowFrameVarianceImage_->GetSampler(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
				writer2.write_image(5, nowFrameAlbedoImage_->GetImageView(), nowFrameAlbedoImage_->GetSampler(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
				writer2.write_image(6, nowFrameNormalImage_->GetImageView(), nowFrameNormalImage_->GetSampler(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
				writer2.write_image(7, nowFrameWorldPositionImage_->GetImageView(), nowFrameWorldPositionImage_->GetSampler(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
				writer2.write_image(8, lastFrameFinalImage_->GetImageView(), lastFrameFinalImage_->GetSampler(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
				writer2.write_image(9, lastFrameVarianceImage_->GetImageView(), lastFrameVarianceImage_->GetSampler(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
				writer2.write_image(10, lastFrameAlbedoImage_->GetImageView(), lastFrameAlbedoImage_->GetSampler(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
				writer2.write_image(11, lastFrameNormalImage_->GetImageView(), lastFrameNormalImage_->GetSampler(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
				writer2.write_image(12, lastFrameWorldPositionImage_->GetImageView(), lastFrameWorldPositionImage_->GetSampler(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

				if (j == 0 || j == 2 || j == 4)
				{
					writer2.write_image(2, outputFinalImage_->GetImageView(), outputFinalImage_->GetSampler(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
					writer2.write_image(3, nowFrameRadianceImage_->GetImageView(), nowFrameRadianceImage_->GetSampler(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
				}
				else
				{
					writer2.write_image(3, outputFinalImage_->GetImageView(), outputFinalImage_->GetSampler(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
					writer2.write_image(2, nowFrameRadianceImage_->GetImageView(), nowFrameRadianceImage_->GetSampler(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
				}
				writer2.update_set(g_Device, frameDatas_[i].denoiseDescriptorSets_[j]);
			}
		}
	}

	void Renderer::CleanUpRayTracing()
	{
		releaseSceneResources();
		for (size_t i = 0; i < frameDatas_.size(); i++)
		{
			delete frameDatas_[i].RTUniformBuffer_;
			delete frameDatas_[i].DenoiseUniformBuffer_;
		}
		shaderBindingTable_.Reset();

		for (size_t i = 0; i < denoiseUniformBuffers_.size(); i++)
		{
			delete denoiseUniformBuffers_[i];
		}

		rtPipeline_.Destroy();
		vkDestroyDescriptorSetLayout(g_Device, rtDescriptorSetLayout_, nullptr);
		vkDestroyPipelineLayout(g_Device, rtPipelineLayout_, nullptr);
	
		vkDestroyDescriptorSetLayout(g_Device, denoiseDescriptorSetLayout_, nullptr);
		vkDestroyPipelineLayout(g_Device, denoisePipelineLayout_, nullptr);
		vkDestroyPipeline(g_Device, denoisePipeline_, nullptr);
		rtBackend_.reset();
	}

	void Renderer::buildCommandBuffers(ImGui_ImplVulkanH_Window* wd, Camera* pCamera)
	{
		// Update
		updateDescriptorSets();

		// 
		uint32_t width = outputFinalImage_->GetWidth();
		uint32_t height = outputFinalImage_->GetHeight();

		// ===================================
		Camera& camera = *pCamera;
			// Update Uniform
		CameraUniformData cameraData{};
		cameraData.ViewMatrixInverse = glm::inverse(camera.GetViewMatrix());
		cameraData.ProjMatrixInverse = glm::inverse(camera.GetProjMatrix());
		cameraData.samples = max_render_samples_per_pixel_;
		cameraData.frame = nowFrameCount;
		nowFrameCount++;
		frameDatas_[wd->FrameIndex].RTUniformBuffer_->uploadData(&cameraData, sizeof(CameraUniformData), 0);

			// Last frame camera for denoise
		frameDatas_[wd->FrameIndex].DenoiseUniformBuffer_->uploadData(&lastFrameCameraVPMatrix_, sizeof(DenoiseCameraUniformData), 0);
		lastFrameCameraVPMatrix_ = camera.GetPreVPMatrix();

		// CMD
		ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];

		if (isNeedTransition)
		{
			// Transform: Blit -> RayTracing
			vulkan::VulkanImage::transitionImageLayout(fd->CommandBuffer, lastFrameFinalImage_->GetImage(),  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
				VK_IMAGE_ASPECT_COLOR_BIT);
		}

		rtBackend_->CmdTraceRays(fd->CommandBuffer, rtPipeline_, shaderBindingTable_, frameDatas_[wd->FrameIndex].rtDescriptorSet_, width, height, 1);

		// Transform: RayTracing -> Compute
		vulkan::VulkanImage::transitionImageLayout(fd->CommandBuffer, outputFinalImage_->GetImage(),  VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_ASPECT_COLOR_BIT);

		// Denosie Pass 
		vkCmdBindPipeline(fd->CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, denoisePipeline_);

		constexpr uint32_t Group_Size = 16;

		uint32_t groupCountX = (width + Group_Size - 1) / Group_Size;
		uint32_t groupCountY = (height + Group_Size - 1) / Group_Size;

		// Ping-Pong Blit
		for (size_t i = 0; i < frameDatas_[wd->FrameIndex].denoiseDescriptorSets_.size() - 1; i++)
		{
			vkCmdBindDescriptorSets(fd->CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, denoisePipelineLayout_, 0, 1, &frameDatas_[wd->FrameIndex].denoiseDescriptorSets_[i], 0, 0);
			vkCmdDispatch(fd->CommandBuffer, groupCountX, groupCountY, 1);
			
			if (i == 0 || i == 2)
			{
				vulkan::VulkanImage::waitComputeShaderComplete(fd->CommandBuffer, outputFinalImage_->GetImage(), VK_IMAGE_ASPECT_COLOR_BIT);
			}
			else
			{
				vulkan::VulkanImage::waitComputeShaderComplete(fd->CommandBuffer, nowFrameRadianceImage_->GetImage(), VK_IMAGE_ASPECT_COLOR_BIT);
			}
		}
		// Final Blit
		vkCmdBindDescriptorSets(fd->CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, denoisePipelineLayout_, 0, 1, &frameDatas_[wd->FrameIndex].denoiseDescriptorSets_[4], 0, 0);
		vkCmdDispatch(fd->CommandBuffer, groupCountX, groupCountY, 1);

		// Transform: Compute -> Blit
		vulkan::VulkanImage::transitionImageLayout(fd->CommandBuffer, outputFinalImage_->GetImage(),  VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_ASPECT_COLOR_BIT);

		isNeedTransition = true;
	}

Camera::Camera(glm::vec3 position, glm::vec3 front)
	:position_(position)
{
	front_ = glm::normalize(front);
	cachedYaw_ = angle_to_radius(std::asin(front_.z / glm::length(glm::vec2(front_.x, front_.z))));
	cachedYaw_ = front_.x > 0 ? cachedYaw_ : (180.0f - cachedYaw_);
	cachedPitch_ = angle_to_radius(std::asin(front_.y / glm::length(front_)));
}

void Camera::Tick(float ts, uint32_t width, uint32_t height)
{
	using namespace Walnut;

		// Update Camera
	auto cursorPos = Input::GetMousePosition();
	if (Input::IsKeyDown(KeyCode::LeftShift))
	{
			// Move
		float delta = ts * cameraMoveSpeed_;
		glm::vec3 dir(0);
		if (Input::IsKeyDown(KeyCode::A)) dir -= horizontal_;
		if (Input::IsKeyDown(KeyCode::D)) dir += horizontal_;
		if (Input::IsKeyDown(KeyCode::W)) dir += front_;
		if (Input::IsKeyDown(KeyCode::S)) dir -= front_;
		if (Input::IsKeyDown(KeyCode::Q)) dir -= up_;
		if (Input::IsKeyDown(KeyCode::E)) dir += up_;
		//
		if(!(dir == glm::vec3(0)))
			position_ += glm::normalize(dir) * delta;

			// Rotate
		glm::vec2 offset = cursorPos - glm::vec2(lastCursorX_, lastCursorY_);
		offset *= cameraRotateSpeed_ * ts;

		cachedYaw_ += offset.x;
		cachedPitch_ = std::clamp(cachedPitch_ - offset.y, -89.0f, 89.0f);

		glm::vec3 direction;
		direction.x = cos(glm::radians(cachedYaw_)) * cos(glm::radians(cachedPitch_));
		direction.y = sin(glm::radians(cachedPitch_));
		direction.z = sin(glm::radians(cachedYaw_)) * cos(glm::radians(cachedPitch_));
		front_ = normalize(direction);
	}
	lastCursorX_ = cursorPos.x;
	lastCursorY_ = cursorPos.y;

	front_ = normalize(front_);
	// Update Screen
	float focusMagnification = DOF_focus_distance_ / focus_distance_;
		// FOV
	horizontal_ = normalize(glm::cross(front_, up_));
	vertical_ = normalize(glm::cross(horizontal_, front_));
	horizontal_ *= (float)width / (float)height;
	screen_left_down_corner_ = position_ + front_ * focus_distance_ - (vertical_ + horizontal_) * .5f;
	relative_left_down_corner_ = front_ * focus_distance_ - (vertical_ + horizontal_) * .5f;

	focus_vertical_ = focusMagnification * vertical_;
	focus_horizontal_ = focusMagnification * horizontal_;

	focus_left_down_corner_ = (screen_left_down_corner_ - position_) * focusMagnification + position_;

	// Matrix
	ViewMatrix_ = glm::lookAt(position_, position_ + front_, up_);
	float sensorWidth = width == 0 ? 1.0f : (float)width / (float)height;
	float dialogue = std::sqrt(sensorWidth * sensorWidth + 1 * 1);
	float FOV = glm::degrees(2.0f * std::atan(dialogue / (2.0f * focus_distance_)));
	ProjMatrix_ = glm::perspective(glm::radians(FOV), sensorWidth, 0.1f, 100.0f);
	preVPMatrix_ = ProjMatrix_ * ViewMatrix_;
}

