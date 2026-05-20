#include "Renderer.h"
#include "Walnut/Random.h"
#include "Walnut/Input/Input.h"
#include "walnut/Application.h"
#include "walnut/tool.hpp"
#include "Walnut/myVulkan/myVulkanInclude.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

#include <format>
#include <execution>
#include "iostream"
#include <functional>
#include "Mesh.h"

#include "util.hpp"

const Color g_Background_Color_Up(0.5, 0.7, 1.0, 1.0f);
const Color g_Background_Color_Down(1.0f, 1.0f, 1.0f, 1.0f);



#ifndef VULKAN_RT

Renderer::Renderer()
{
}

Renderer::Renderer(uint32_t width, uint32_t height)
{
}

Renderer::~Renderer()
{
	delete[] finalImageData_;
}

void Renderer::OnResize(uint32_t width, uint32_t height)
{
	//
	if (finalImage_)
	{
		if (finalImage_->GetWidth() == width && finalImage_->GetHeight() == height)
			return;
		finalImage_->Resize(width, height);
	}
	else
	{
		finalImage_ = std::make_shared<Walnut::Image>(width, height, Walnut::ImageFormat::RGBA);
	}
	// Albedo
	if (albedoImage_)
	{
		if (albedoImage_->GetWidth() == width && albedoImage_->GetHeight() == height)
			return;
		albedoImage_->Resize(width, height);
	}
	else
	{
		albedoImage_ = std::make_shared<Walnut::Image>(width, height, Walnut::ImageFormat::RGBA);
	}
	// Normal
	if (normalImage_)
	{
		if (normalImage_->GetWidth() == width && normalImage_->GetHeight() == height)
			return;
		normalImage_->Resize(width, height);
	}
	else
	{
		normalImage_ = std::make_shared<Walnut::Image>(width, height, Walnut::ImageFormat::RGBA);
	}
	// Data
	delete[] finalImageData_;
	finalImageData_ = new uint32_t[width * height];
	delete[] normalImageData_;
	normalImageData_ = new uint32_t[width * height];
	delete[] albedoImageData_;
	albedoImageData_ = new uint32_t[width * height];

	// MT
	if (use_MT_Acceleration_)
	{
		MT_Vertical_Iter.resize(height);
		for (uint32_t i = 0; i < height; i++)
			MT_Vertical_Iter[i] = i;
	}
}

void Renderer::Render(Camera& camera, RenderScene& scene, bool isAdaptiveNoise, bool isDenoise)
{
	uint32_t width = finalImage_->GetWidth();
	uint32_t height = finalImage_->GetHeight();

	// MT
	if(use_MT_Acceleration_)
	{ 
		std::for_each(std::execution::par, MT_Vertical_Iter.begin(), MT_Vertical_Iter.end(),
			[this, width, height, isAdaptiveNoise, &camera, &scene](uint32_t y)
			{
				for (uint32_t x = 0; x < width; x++)
				{
					TracingColors averageColors;
					uint32_t index = y * width + x;
					// 
					double accumulateVariance = 0;
					// Samples
					for (int sample_count = 1; sample_count < max_render_samples_per_pixel_ + 1; sample_count++)
					{
						// Base Context
							// Random Sample
						float u = ((float)x + Walnut::Random::Float()) / (float)width;
						float v = ((float)y + Walnut::Random::Float()) / (float)height;
						// Ray
						Ray ray = camera.GetNormalizedRay(u, v);

						// Shader
						TracingColors finalCols = Ray_Colors(ray, max_bounce_count_, scene, true);

						// Accumulate
						averageColors = finalCols / (float)sample_count + averageColors * (1.0f - 1.0f / (float)sample_count);
						// Adaptive
						if (isAdaptiveNoise)
						{
							double diffSqr = (finalCols.finalColor - averageColors.finalColor).NormSqr();
							accumulateVariance += diffSqr;
							// Debug
							//if (x == width / 2 && y == height / 2) std::cout << std::format("r:{:.4f} g:{:.4f} b:{:.4f} ", finalCols.finalColor.r,finalCols.finalColor.g, finalCols.finalColor.b)
							//	<< std::format("average--r:{:.4f} g:{:.4f} b:{:.4f} ", averageColors.finalColor.r,averageColors.finalColor.g, averageColors.finalColor.b)
							//		<< std::format(" noise:{:.8f}", diffSqr) << std::endl;
							
							// Noise
							if (sample_count >= min_render_samples_per_pixel_ && sample_count > 1)
							{
								double noise = accumulateVariance / ((float)(sample_count - 1) * std::sqrt(sample_count));
								double norm = averageColors.finalColor.Norm();
								double normalizeNoise = norm < 1e-8 ? 0 : noise / norm;
								// Debug
								//if (x == width / 2 && y == height / 2) std::cout << std::format("NoiseValue: {:.6f}", normalizeNoise) << std::endl;
								if (normalizeNoise < min_render_noise_threshold) break;
								//else std::cout << std::format("Noise is not enough: {:.10f}", normalizeNoise) << std::endl;
							}
						}
					}
			
					finalImageData_[index] = averageColors.finalColor.GetColorData();
					normalImageData_[index] = averageColors.normalColor.GetColorData();
					albedoImageData_[index] = averageColors.albedoColor.GetColorData();
				}
			}
		);
	}
	// NO MT
	else
	{
		for (uint32_t x = 0; x < width; x++)		
		{
			for (uint32_t y = 0; y < height; y++)
			{
					TracingColors averageColors;
					uint32_t index = y * width + x;
					// 
					double accumulateVariance = 0;
					// Samples
					for (int sample_count = 1; sample_count < max_render_samples_per_pixel_ + 1; sample_count++)
					{
						// Base Context
							// Random Sample
						float u = ((float)x + Walnut::Random::Float()) / (float)width;
						float v = ((float)y + Walnut::Random::Float()) / (float)height;
						// Ray
						Ray ray = camera.GetNormalizedRay(u, v);

						// Shader
						TracingColors finalCols = Ray_Colors(ray, max_bounce_count_, scene, true);

						// Accumulate
						averageColors = finalCols / (float)sample_count + averageColors * (1.0f - 1.0f / (float)sample_count);
						// Adaptive
						if (isAdaptiveNoise)
						{
							double diffSqr = (finalCols.finalColor - averageColors.finalColor).NormSqr();
							accumulateVariance += diffSqr;
							// Debug
							//if (x == width / 2 && y == height / 2) std::cout << std::format("r:{:.4f} g:{:.4f} b:{:.4f} ", finalCols.finalColor.r,finalCols.finalColor.g, finalCols.finalColor.b)
							//	<< std::format("average--r:{:.4f} g:{:.4f} b:{:.4f} ", averageColors.finalColor.r,averageColors.finalColor.g, averageColors.finalColor.b)
							//		<< std::format(" noise:{:.8f}", diffSqr) << std::endl;

							// Noise
							if (sample_count >= min_render_samples_per_pixel_ && sample_count > 1)
							{
								double noise = accumulateVariance / ((float)(sample_count - 1) * std::sqrt(sample_count));
								double norm = averageColors.finalColor.Norm();
								double normalizeNoise = norm < 1e-8 ? 0 : noise / norm;
								// Debug
								//if (x == width / 2 && y == height / 2) std::cout << std::format("NoiseValue: {:.6f}", normalizeNoise) << std::endl;
								if (normalizeNoise < min_render_noise_threshold) break;
								//else std::cout << std::format("Noise is not enough: {:.10f}", normalizeNoise) << std::endl;
							}
						}
					}
				finalImageData_[index] = averageColors.finalColor.GetColorData();
				normalImageData_[index] = averageColors.normalColor.GetColorData();
				albedoImageData_[index] = averageColors.albedoColor.GetColorData();
			}
		}
	}

	// Denoise
	if (isDenoise)
	{
		denoiser_.reserve(width, height, isDenoseUsePrefilter);
		// Set
		auto albedoPtr = denoiser_.GetInputPointer(Denoise_Image_Type::albedo);
		auto normalPtr = denoiser_.GetInputPointer(Denoise_Image_Type::normal);
		auto colorPtr = denoiser_.GetInputPointer(Denoise_Image_Type::color);
		for (size_t i = 0; i < width * height; i++)
		{
			size_t offset = i * 3;
			for (int j = 0; j < 3; j++)
			{
				int bit = j * 8;
				*(albedoPtr + offset + j) = static_cast<float>((albedoImageData_[i] & (0x000000ff << bit)) >> bit) / 255.0f;
				*(normalPtr + offset + j) = static_cast<float>((normalImageData_[i] & (0x000000ff << bit)) >> bit) / 255.0f;
				*(colorPtr + offset + j) = static_cast<float>((finalImageData_[i] & (0x000000ff << bit)) >> bit) / 255.0f;
			}
		}

		// denoise
		auto finalPtr = denoiser_.execute();

		// Get Result to my Buffer
		for (size_t i = 0; i < width * height; i++)
		{
			size_t offset = i * 3;
			Color col(0, 0, 0, 1);

			col.r = *(colorPtr + offset + 0);
			col.g = *(colorPtr + offset + 1);
			col.b = *(colorPtr + offset + 2);

			finalImageData_[i] = col.GetColorData();
			// Get Prefilter Result
			if constexpr (isDenoseUsePrefilter)
			{
				Color albedoCol(0, 0, 0, 1);
				Color normalCol(0, 0, 0, 1);

				albedoCol.r = *(albedoPtr + offset + 0);
				albedoCol.g = *(albedoPtr + offset + 1);
				albedoCol.b = *(albedoPtr + offset + 2);

				normalCol.r = *(normalPtr + offset + 0);
				normalCol.g = *(normalPtr + offset + 1);
				normalCol.b = *(normalPtr + offset + 2);

				albedoImageData_[i] = albedoCol.GetColorData();
				normalImageData_[i] = normalCol.GetColorData();
			}
		}

	}
	// Gamma
	for (size_t i = 0; i < width * height; i++)
	{
		finalImageData_[i] = uint_Power(finalImageData_[i], 0.45f);
		albedoImageData_[i] = uint_Power(albedoImageData_[i], 0.45f);
	}


	// Set Image
	finalImage_->SetData(finalImageData_);
	normalImage_->SetData(normalImageData_);
	albedoImage_->SetData(albedoImageData_);
}

// Main Shader
TracingColors Renderer::Ray_Colors(const Ray& ray, int depth, RenderScene& scene, bool isFirstHit)
{
	if (depth < 0)
	{
		if (Walnut::Random::Float() < 0.5f)
		{
			TracingColors colos;
			colos.finalColor = Color(0, 0, 0, 1);
			return colos;
		}
	}

	HitResult res;

	if (scene.hit(ray, 0.001, infinity, res))
	{
		Ray scatter;
		TracingColors colors;
		if (!res.material)
		{
			std::cout << "[Error Renderer] material is Null" << std::endl;
		}
		Color emitColor = res.material->emitted(res.u, res.v, res.hitPosition);
		Color inderictAttenuation(0);
		if (res.material->scatter(ray, res, inderictAttenuation, scatter))
		{
			if (isFirstHit)
			{
				colors.normalColor = res.hitNormal;
				colors.albedoColor = inderictAttenuation;
			}
			// Indirected Light
			auto scatterCols = Ray_Colors(scatter, depth - 1, scene, false);
			scatterCols.finalColor = scatterCols.finalColor.MulWithoutAlpha(inderictAttenuation);
			// Directed Light
			float prob;
			// ��Դ��Ҫ�Բ������������ȸߵĹ�Դ
			if (auto light = scene.pickLightWeighted(prob))
			{
				auto sample = light->sample();
				Color directCol;
				// ���� ֱ�ӹ�Դ��˥�� �� ����ʱ��˥����������Ҫ�Բ���ʱѡȡ��Դ�ĸ��ʣ�����
				if (CalculateDirectLightAttenuation(sample, ray, res, scene, directCol))
				{
					directCol = directCol / prob;
					scatterCols.finalColor = (scatterCols.finalColor + directCol) / 2.0f;				
				}
			}

			colors.finalColor = emitColor + scatterCols.finalColor;
			return colors;
		}
		if (isFirstHit)
		{
			colors.normalColor = res.hitNormal;
			colors.albedoColor = emitColor;
		}
		colors.finalColor = emitColor;
		return colors;
	}

	glm::vec3 unit_direction = glm::normalize(ray.direction());
	float a = 0.5f * (unit_direction.y + 1);

	TracingColors colors;
	colors.finalColor = Color::Lerp(g_Background_Color_Down, g_Background_Color_Up, a);
	if (isFirstHit)
	{
		colors.normalColor = unit_direction * -1.0f;
		colors.albedoColor = colors.finalColor;
	}
	return colors;
}

bool Renderer::CalculateDirectLightAttenuation(const LightSample& lightSample, const Ray& ray_in, const HitResult& hitres, RenderScene& scene, Color& resColor)
{
	glm::vec3 dir = lightSample.samplePosition - hitres.hitPosition;
	Ray ray_out(hitres.hitPosition, dir);
	HitResult res;
	float epison = 0.0001f;
	if (scene.hit(ray_out, epison, 1.0f - epison, res))
	{
		return false;
	}
	float theta1 = glm::dot(-dir, lightSample.rayDirection);
	if (theta1 <= 0) return false;
	float dis = glm::length(dir);

	resColor = hitres.material->getAttenuation(ray_in, hitres, ray_out);
	resColor = resColor.MulWithoutAlpha(lightSample.sampleColor) * theta1 / (dis * dis);
	return true;
}

#else

	Renderer::Renderer()
	{
		InitRayTracing();
	}

	Renderer::~Renderer()
	{
		CleanUpRayTracing();
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

		// 
		updateDescriptorSets();

		isNeedTransition = false;

	}

	void Renderer::Render(Camera& camera, RenderScene& scene, bool isAdaptiveNoise, bool isDenoise)
	{
		// Swap
		outputFinalImage_.swap(lastFrameFinalImage_);
		nowFrameRadianceImage_.swap(lastFrameVarianceImage_);
		nowFrameAlbedoImage_.swap(lastFrameAlbedoImage_);
		nowFrameNormalImage_.swap(lastFrameNormalImage_);
		nowFrameWorldPositionImage_.swap(lastFrameWorldPositionImage_);

		// Add callBack
		ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
		auto renderFunc = std::bind(&Renderer::buildCommandBuffers, this, wd, &camera);
		s_VulkanRenderFuncQueue.push_back(renderFunc);

	}


	void Renderer::InitRayTracing()
	{
		// ��ȡ�豸֧�ֵĹ�׷����
			// Get ray tracing pipeline properties, which will be used later on in the sample
		rayTracingPipelineProperties_.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
		VkPhysicalDeviceProperties2 deviceProperties2{};
		deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		deviceProperties2.pNext = &rayTracingPipelineProperties_;
		vkGetPhysicalDeviceProperties2(g_PhysicalDevice, &deviceProperties2);

			// Get acceleration structure properties, which will be used later on in the sample
		accelerationStructureFeatures_.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
		VkPhysicalDeviceFeatures2 deviceFeatures2{};
		deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		deviceFeatures2.pNext = &accelerationStructureFeatures_;
		vkGetPhysicalDeviceFeatures2(g_PhysicalDevice, &deviceFeatures2);

		frameDatas_.resize(g_MinImageCount);

		// ������׷��Դ
		createBottomLevelAccelerationStructure();
		createTopLevelAccelerationStructure();
		createUniformBuffer();
		createRayTracingPipeline();
		createDenoisePipeline();
		createShaderBindingTable();
		createDescriptorSets();
	}

	void Renderer::createBottomLevelAccelerationStructure()
	{
		//Setup identity transform matrix
		VkTransformMatrixKHR transformMatrix = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f
		};

		//glm::mat4 rot = glm::rotate(glm::mat4(1), glm::radians(180.0f), glm::vec3(0.0, 0.0, 1.0));
		//glm::mat3x4 transformMatrix = glm::mat3x4(glm::transpose(rot));
		// Transform buffer
		transformBuffer_ = g_pVkMemoryAllocator->createBuffer(sizeof(VkTransformMatrixKHR),
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
			VMA_MEMORY_USAGE_CPU_ONLY).release();
		transformBuffer_->uploadData(&transformMatrix, VK_WHOLE_SIZE);

		// Load
		model_ = std::make_shared<RTModel>(g_pVkMemoryAllocator, g_pCommandPool, g_Queue);
		model_->LoadModel("assets/model/cornel_box.fbx");

		VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress{};
		VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress{};
		VkDeviceOrHostAddressConstKHR transformBufferDeviceAddress{};

		vertexBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(model_->vertices.buffer->buffer());
		indexBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(model_->indices.buffer->buffer());
		transformBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(transformBuffer_->buffer());

		// ============ Build==============
		std::vector<uint32_t> maxPrimitiveCounts;
		std::vector<VkAccelerationStructureGeometryKHR> geometries;
		std::vector<VkAccelerationStructureBuildRangeInfoKHR> accelerationBuildStructureRangeInfos;
		std::vector<VkAccelerationStructureBuildRangeInfoKHR*> pAccelerationBuildStructureRangeInfos;
		std::vector<GeometryNode> geometriesNode;

		for(auto& mesh : model_->linerMeshes)
		{
			if(mesh->geometry.indexCount <= 0) continue;

			VkDeviceOrHostAddressConstKHR indexBufferDeviceAddressOffseted {};
			indexBufferDeviceAddressOffseted.deviceAddress = indexBufferDeviceAddress.deviceAddress + mesh->geometry.firstIndex * sizeof(uint32_t);

			// 
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

			//
			uint32_t numTriangle = mesh->geometry.indexCount / 3;
			VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
			buildRangeInfo.primitiveCount = numTriangle;

			accelerationBuildStructureRangeInfos.push_back(buildRangeInfo);

			// 
			maxPrimitiveCounts.push_back(numTriangle);

			// 
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
		uint32_t geometriesCount = static_cast<uint32_t>(geometries.size());
		pAccelerationBuildStructureRangeInfos.resize(geometriesCount);
		for (size_t i = 0; i < geometriesCount; i++)
		{
			pAccelerationBuildStructureRangeInfos[i] = &accelerationBuildStructureRangeInfos[i];
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

		// Get size info
		VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
		accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		accelerationStructureBuildGeometryInfo.geometryCount = geometries.size();
		accelerationStructureBuildGeometryInfo.pGeometries = geometries.data();
		
		// ��ȡ���ٽṹ��Ҫ��size
		VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
		accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		//
		auto vkGetAccelerationStructureBuildSizesKHR_Func = (PFN_vkGetAccelerationStructureBuildSizesKHR)g_dynamicLoader.GetDeviceProc("vkGetAccelerationStructureBuildSizesKHR");
		vkGetAccelerationStructureBuildSizesKHR_Func(
			g_Device,
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&accelerationStructureBuildGeometryInfo,
			maxPrimitiveCounts.data(),
			&accelerationStructureBuildSizesInfo);

		// �������ٽṹ��buffer��scratch Buffer
		createAccelerationStructureBuffer(bottomLevelAS_, accelerationStructureBuildSizesInfo);

		VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
		accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		accelerationStructureCreateInfo.buffer = bottomLevelAS_.buffer->buffer();
		accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
		accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

		// Create ���ٽṹ
		auto vkCreateAccelerationStructureKHR_Func = (PFN_vkCreateAccelerationStructureKHR)g_dynamicLoader.GetDeviceProc("vkCreateAccelerationStructureKHR");
		vkCreateAccelerationStructureKHR_Func(g_Device, &accelerationStructureCreateInfo, nullptr, &bottomLevelAS_.handle);

		// Create a small scratch buffer used during build of the bottom level acceleration structure
		RayTracingScratchBuffer scratchBuffer = createScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

		VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
		accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		accelerationBuildGeometryInfo.dstAccelerationStructure = bottomLevelAS_.handle;
		accelerationBuildGeometryInfo.geometryCount = geometries.size();
		accelerationBuildGeometryInfo.pGeometries = geometries.data();
		accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;


		// Build
		{
			vulkan::SingleTimeCommands cmd(g_pCommandPool);
			// 
			auto vkCmdBuildAccelerationStructuresKHR_Func = (PFN_vkCmdBuildAccelerationStructuresKHR)g_dynamicLoader.GetDeviceProc("vkCmdBuildAccelerationStructuresKHR");
			vkCmdBuildAccelerationStructuresKHR_Func(
				cmd.getBuffer(),
				1,
				&accelerationBuildGeometryInfo,
				pAccelerationBuildStructureRangeInfos.data());
			cmd.Submit(g_Queue);
		}

		bottomLevelAS_.deviceAddress = getAccelerationStructureDeviceAddress(bottomLevelAS_.handle);

		deleteScratchBuffer(scratchBuffer);
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

		// Buffer for instance data
		vulkan::VulkanMemoryResource* instancesBuffer = g_pVkMemoryAllocator->createBuffer(sizeof(VkAccelerationStructureInstanceKHR),
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
			VMA_MEMORY_USAGE_CPU_ONLY).release();
		instancesBuffer->uploadData(&instance, VK_WHOLE_SIZE);

		VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress{};
		instanceDataDeviceAddress.deviceAddress = getBufferDeviceAddress(instancesBuffer->buffer());

		VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
		accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		accelerationStructureGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
		accelerationStructureGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
		accelerationStructureGeometry.geometry.instances.data = instanceDataDeviceAddress;

		// Get size info
		/*
		The pSrcAccelerationStructure, dstAccelerationStructure, and mode members of pBuildInfo are ignored. Any VkDeviceOrHostAddressKHR members of pBuildInfo are ignored by this command, except that the hostAddress member of VkAccelerationStructureGeometryTrianglesDataKHR::transformData will be examined to check if it is NULL.*
		*/
		VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
		accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		accelerationStructureBuildGeometryInfo.geometryCount = 1;
		accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

		uint32_t primitive_count = 1;

		VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
		accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		auto vkGetAccelerationStructureBuildSizesKHR_Func = (PFN_vkGetAccelerationStructureBuildSizesKHR)g_dynamicLoader.GetDeviceProc("vkGetAccelerationStructureBuildSizesKHR");
		vkGetAccelerationStructureBuildSizesKHR_Func(
			g_Device,
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&accelerationStructureBuildGeometryInfo,
			&primitive_count,
			&accelerationStructureBuildSizesInfo);

		// �������ٽṹ��buffer��scratch Buffer
		createAccelerationStructureBuffer(topLevelAS_, accelerationStructureBuildSizesInfo);

		VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
		accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		accelerationStructureCreateInfo.buffer = topLevelAS_.buffer->buffer();
		accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
		accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		// Create ���ٽṹ
		auto vkCreateAccelerationStructureKHR_Func = (PFN_vkCreateAccelerationStructureKHR)g_dynamicLoader.GetDeviceProc("vkCreateAccelerationStructureKHR");
		vkCreateAccelerationStructureKHR_Func(g_Device, &accelerationStructureCreateInfo, nullptr, &topLevelAS_.handle);

		// Create a small scratch buffer used during build of the top level acceleration structure
		RayTracingScratchBuffer scratchBuffer = createScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

		VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
		accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		accelerationBuildGeometryInfo.dstAccelerationStructure = topLevelAS_.handle;
		accelerationBuildGeometryInfo.geometryCount = 1;
		accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
		accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

		VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
		accelerationStructureBuildRangeInfo.primitiveCount = 1;
		accelerationStructureBuildRangeInfo.primitiveOffset = 0;
		accelerationStructureBuildRangeInfo.firstVertex = 0;
		accelerationStructureBuildRangeInfo.transformOffset = 0;
		std::vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

		// Build the acceleration structure on the device via a one-time command buffer submission
		// Some implementations may support acceleration structure building on the host (VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructureHostCommands), but we prefer device builds
		{
			vulkan::SingleTimeCommands cmd(g_pCommandPool);
			// 
			auto vkCmdBuildAccelerationStructuresKHR_Func = (PFN_vkCmdBuildAccelerationStructuresKHR)g_dynamicLoader.GetDeviceProc("vkCmdBuildAccelerationStructuresKHR");
			vkCmdBuildAccelerationStructuresKHR_Func(
				cmd.getBuffer(),
				1,
				&accelerationBuildGeometryInfo,
				accelerationBuildStructureRangeInfos.data());
			cmd.Submit(g_Queue);
		}

		topLevelAS_.deviceAddress = getAccelerationStructureDeviceAddress(topLevelAS_.handle);

		deleteScratchBuffer(scratchBuffer);
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

		// Lights
		UniformLightsData lightData{};
		// lightData.areaLightCount = 1;
		lightData.radiusLightCount = 1;
		
		auto& light1 = lightData.areaLightsData[0];
		light1.beginPos = glm::vec3(-0.215f, 0.924f, -0.175f);
		light1.u = glm::vec3(0.43f, 0, 0);
		light1.v = glm::vec3(0, 0, 0.35f);
		light1.color = glm::vec3(1.15f, 0.8f, 0.27f) * 80.0f;
		light1.rayDir = glm::vec3(0, -1, 0);

		auto& light2 = lightData.radiusLightsData[0];
		light2.centerPos = glm::vec3(0, 0.8f, 0);
		light2.color = glm::vec3(1.15f, 0.8f, 0.27f) * 1.0f;
		light2.radius = 0.15f;

		vulkan::VulkanLocalBuffer::CopierCreateInfo copierInfo {
				.commandPool = g_pCommandPool,
				.transferQueue = g_Queue
		};
		lightsBuffer_ = new vulkan::VulkanLocalBuffer(g_pVkMemoryAllocator, sizeof(UniformLightsData),
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, copierInfo);
		lightsBuffer_->UploadMemory(&lightData, sizeof(UniformLightsData), 0);

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

		/*
			Setup ray tracing shader groups
		*/
		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

		// Ray generation group
		{
			shaderStages.push_back(loadShader("E:/Git/Walnut-Learning/Walnut-Learning/Walnut/src/Walnut/shaders/rt/raygen.rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR));
			VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
			shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
			shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
			shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
			shaderGroups_.push_back(shaderGroup);
		}

		// Miss group
		{
			shaderStages.push_back(loadShader("E:/Git/Walnut-Learning/Walnut-Learning/Walnut/src/Walnut/shaders/rt/miss.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR));
			VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
			shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
			shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
			shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
			shaderGroups_.push_back(shaderGroup);
					// Second shader for shadows
			shaderStages.push_back(loadShader("E:/Git/Walnut-Learning/Walnut-Learning/Walnut/src/Walnut/shaders/rt/shadow.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR));
			shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
			shaderGroups_.push_back(shaderGroup);
		}

		// Closest hit group
		{
			shaderStages.push_back(loadShader("E:/Git/Walnut-Learning/Walnut-Learning/Walnut/src/Walnut/shaders/rt/closesthit.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR));
			VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
			shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
			shaderGroup.generalShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.closestHitShader = static_cast<uint32_t>(shaderStages.size()) - 1;
			shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
			shaderGroups_.push_back(shaderGroup);
		}

		/*
			Create the ray tracing pipeline
		*/
		VkRayTracingPipelineCreateInfoKHR rayTracingPipelineCI{};
		rayTracingPipelineCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
		rayTracingPipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		rayTracingPipelineCI.pStages = shaderStages.data();
		rayTracingPipelineCI.groupCount = static_cast<uint32_t>(shaderGroups_.size());
		rayTracingPipelineCI.pGroups = shaderGroups_.data();
		rayTracingPipelineCI.maxPipelineRayRecursionDepth = 1;
		rayTracingPipelineCI.layout = rtPipelineLayout_;
		// 
		auto func = (PFN_vkCreateRayTracingPipelinesKHR)g_dynamicLoader.GetDeviceProc("vkCreateRayTracingPipelinesKHR");
		func(g_Device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracingPipelineCI, nullptr, &rtPipeline_);

		// Clear Shader Module
		for (auto& it : shaderModules_)
		{
			vkDestroyShaderModule(g_Device, it, nullptr);
		}
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
		VkPipelineShaderStageCreateInfo shaderStage = loadShader("E:/Git/Walnut-Learning/Walnut-Learning/Walnut/src/Walnut/shaders/denoise/svgf.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);

		// Pipeline
		VkComputePipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipelineInfo.layout = denoisePipelineLayout_;
		pipelineInfo.stage = shaderStage;

		if (vkCreateComputePipelines(g_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &denoisePipeline_) != VK_SUCCESS) {
			throw std::runtime_error("failed to create compute pipeline!");
		}
	}

	void Renderer::createShaderBindingTable() {
		const uint32_t handleSize = rayTracingPipelineProperties_.shaderGroupHandleSize;
		const uint32_t handleSizeAligned = tool::roundUp(rayTracingPipelineProperties_.shaderGroupHandleSize, rayTracingPipelineProperties_.shaderGroupHandleAlignment);
		const uint32_t groupCount = static_cast<uint32_t>(shaderGroups_.size());
		const uint32_t sbtSize = groupCount * handleSizeAligned;

		std::vector<uint8_t> shaderHandleStorage(sbtSize);
		auto func = (PFN_vkGetRayTracingShaderGroupHandlesKHR)g_dynamicLoader.GetDeviceProc("vkGetRayTracingShaderGroupHandlesKHR");
		func(g_Device, rtPipeline_, 0, groupCount, sbtSize, shaderHandleStorage.data());

		// Create STB
		raygenShaderBindingTable_ = g_pVkMemoryAllocator->createBuffer(handleSize,
				VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				VMA_MEMORY_USAGE_CPU_ONLY).release();
		missShaderBindingTable_ = g_pVkMemoryAllocator->createBuffer(handleSize * 2,
				VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				VMA_MEMORY_USAGE_CPU_ONLY).release();
		hitShaderBindingTable_ = g_pVkMemoryAllocator->createBuffer(handleSize,
				VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				VMA_MEMORY_USAGE_CPU_ONLY).release();

		// Copy handles
		raygenShaderBindingTable_->uploadData(shaderHandleStorage.data(), handleSize);
		missShaderBindingTable_->uploadData(shaderHandleStorage.data() + handleSizeAligned, handleSize * 2);
		hitShaderBindingTable_->uploadData(shaderHandleStorage.data() + handleSizeAligned * 3, handleSize);
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
			writer1.write_image(9, g_texturePool->GetImageInfo()->data(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, g_texturePool->GetImageCount());
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
			writer1.write_image(1, nowFrameRadianceImage_->GetImageView(), nowFrameRadianceImage_->GetSampler(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
			writer1.write_image(2, nowFrameAlbedoImage_->GetImageView(), nowFrameAlbedoImage_->GetSampler(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
			writer1.write_image(3, nowFrameNormalImage_->GetImageView(), nowFrameNormalImage_->GetSampler(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
			writer1.write_image(4, nowFrameWorldPositionImage_->GetImageView(), nowFrameWorldPositionImage_->GetSampler(), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
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

				// ��3��4��1��2����ƹ��Blit
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
		delete transformBuffer_;
		for (size_t i = 0; i < frameDatas_.size(); i++)
		{
			delete frameDatas_[i].RTUniformBuffer_;
			delete frameDatas_[i].DenoiseUniformBuffer_;
		}
		delete lightsBuffer_;
		// stb
		delete raygenShaderBindingTable_;
		delete missShaderBindingTable_;
		delete hitShaderBindingTable_;
		delete geometryNodeBuffer_;

		for (size_t i = 0; i < denoiseUniformBuffers_.size(); i++)
		{
			delete denoiseUniformBuffers_[i];
		}

		bottomLevelAS_.buffer.reset();
		topLevelAS_.buffer.reset();
		destoryAccelerationStructure(bottomLevelAS_.handle);
		destoryAccelerationStructure(topLevelAS_.handle);

		vkDestroyDescriptorSetLayout(g_Device, rtDescriptorSetLayout_, nullptr);
		vkDestroyPipelineLayout(g_Device, rtPipelineLayout_, nullptr);
		vkDestroyPipeline(g_Device, rtPipeline_, nullptr);
	
		vkDestroyDescriptorSetLayout(g_Device, denoiseDescriptorSetLayout_, nullptr);
		vkDestroyPipelineLayout(g_Device, denoisePipelineLayout_, nullptr);
		vkDestroyPipeline(g_Device, denoisePipeline_, nullptr);
	}

	void Renderer::buildCommandBuffers(ImGui_ImplVulkanH_Window* wd, Camera* pCamera)
	{
		// Update
		updateDescriptorSets();

		// 
		uint32_t width = outputFinalImage_->GetWidth();
		uint32_t height = outputFinalImage_->GetHeight();
		VkSemaphore denoise_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].denoiseComputeSemaphore;

		const uint32_t handleSizeAligned = tool::roundUp(rayTracingPipelineProperties_.shaderGroupHandleSize, rayTracingPipelineProperties_.shaderGroupHandleAlignment);

		VkStridedDeviceAddressRegionKHR raygenShaderSbtEntry{};
		raygenShaderSbtEntry.deviceAddress = getBufferDeviceAddress(raygenShaderBindingTable_->buffer());
		raygenShaderSbtEntry.stride = handleSizeAligned;
		raygenShaderSbtEntry.size = handleSizeAligned;

		VkStridedDeviceAddressRegionKHR missShaderSbtEntry{};
		missShaderSbtEntry.deviceAddress = getBufferDeviceAddress(missShaderBindingTable_->buffer());
		missShaderSbtEntry.stride = handleSizeAligned;
		missShaderSbtEntry.size = handleSizeAligned;

		VkStridedDeviceAddressRegionKHR hitShaderSbtEntry{};
		hitShaderSbtEntry.deviceAddress = getBufferDeviceAddress(hitShaderBindingTable_->buffer());
		hitShaderSbtEntry.stride = handleSizeAligned;
		hitShaderSbtEntry.size = handleSizeAligned;

		VkStridedDeviceAddressRegionKHR callableShaderSbtEntry{};

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

			// Last Frame Camera For Donoise	
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

		// RT Pass 
		vkCmdBindPipeline(fd->CommandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline_);

		vkCmdBindDescriptorSets(fd->CommandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipelineLayout_, 0, 1, &frameDatas_[wd->FrameIndex].rtDescriptorSet_, 0, 0);

		auto vkCmdTraceRaysKHR_Func = (PFN_vkCmdTraceRaysKHR)g_dynamicLoader.GetDeviceProc("vkCmdTraceRaysKHR");
		vkCmdTraceRaysKHR_Func(
			fd->CommandBuffer,
			&raygenShaderSbtEntry,
			&missShaderSbtEntry,
			&hitShaderSbtEntry,
			&callableShaderSbtEntry,
			width,
			height,
			1);

		// Transform: RayTracing -> Compute
		vulkan::VulkanImage::transitionImageLayout(fd->CommandBuffer, outputFinalImage_->GetImage(),  VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_ASPECT_COLOR_BIT);

		// Denosie Pass 
		vkCmdBindPipeline(fd->CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, denoisePipeline_);

		constexpr uint32_t Group_Size = 16;

		uint32_t groupCountX = (width + Group_Size - 1) / Group_Size;  // ����ȡ��
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

	// ============================== �������� ===============================
		// ��ȡbuffer������?
	uint64_t Renderer::getBufferDeviceAddress(VkBuffer buffer)
	{
		VkBufferDeviceAddressInfoKHR bufferDeviceAI{};
		bufferDeviceAI.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		bufferDeviceAI.buffer = buffer;
		// 
		auto func = (PFN_vkGetBufferDeviceAddressKHR)g_dynamicLoader.GetDeviceProc("vkGetBufferDeviceAddressKHR");
		return func(g_Device, &bufferDeviceAI);
	}
		// ��ȡ���ٽṹ������?
	uint64_t Renderer::getAccelerationStructureDeviceAddress(VkAccelerationStructureKHR accelerationStructure)
	{
		VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
		accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		accelerationDeviceAddressInfo.accelerationStructure = accelerationStructure;
		// ���غ�����ַ
		auto func = (PFN_vkGetAccelerationStructureDeviceAddressKHR)g_dynamicLoader.GetDeviceProc("vkGetAccelerationStructureDeviceAddressKHR");
		return func(g_Device, &accelerationDeviceAddressInfo);
	}

	void Renderer::destoryAccelerationStructure(VkAccelerationStructureKHR handle)
	{
		auto func = (PFN_vkDestroyAccelerationStructureKHR)g_dynamicLoader.GetDeviceProc("vkDestroyAccelerationStructureKHR");
		func(g_Device, handle, 0);
		return;
	}

	RayTracingScratchBuffer Renderer::createScratchBuffer(VkDeviceSize size)
	{
		RayTracingScratchBuffer scratchBuffer{};
		// Buffer, Memory
		scratchBuffer.buffer = std::move(g_pVkMemoryAllocator->createBuffer(size,
						VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
						VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE));
		// DeviceAddress
		scratchBuffer.deviceAddress = getBufferDeviceAddress(scratchBuffer.buffer->buffer());

		return scratchBuffer;
	}

	void Renderer::deleteScratchBuffer(RayTracingScratchBuffer& scratchBuffer) 
	{
		if (!scratchBuffer.buffer)
		{
			delete scratchBuffer.buffer.get();
		}
	}

	void Renderer::createAccelerationStructureBuffer(AccelerationStructure &accelerationStructure, VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo)
	{
		accelerationStructure.buffer = std::move(g_pVkMemoryAllocator->createBuffer(buildSizeInfo.accelerationStructureSize,
										VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
										VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE));
	}

	VkPipelineShaderStageCreateInfo Renderer::loadShader(std::string fileName, VkShaderStageFlagBits stage)
	{
		VkPipelineShaderStageCreateInfo shaderStage = {};
		shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStage.stage = stage;
		shaderStage.module = vulkan::loadShader(fileName.c_str(), g_Device);

		shaderStage.pName = "main";
		assert(shaderStage.module != VK_NULL_HANDLE);
		shaderModules_.push_back(shaderStage.module);
		return shaderStage;
	}

#endif

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
	float focusMagnification = DOF_focus_distance_ / focus_distance_;	// ���ս�ƽ�������FOVƽ��ķŴ�?? ��
		// FOVƽ��
	horizontal_ = normalize(glm::cross(front_, up_));
	vertical_ = normalize(glm::cross(horizontal_, front_));
	horizontal_ *= (float)width / (float)height;
	screen_left_down_corner_ = position_ + front_ * focus_distance_ - (vertical_ + horizontal_) * .5f;
	relative_left_down_corner_ = front_ * focus_distance_ - (vertical_ + horizontal_) * .5f;
		// ��ƽ��
	focus_vertical_ = focusMagnification * vertical_;
	focus_horizontal_ = focusMagnification * horizontal_;

	focus_left_down_corner_ = (screen_left_down_corner_ - position_) * focusMagnification + position_;

	// Matrix
	ViewMatrix_ = glm::lookAt(position_, position_ + front_, up_);
	float sensorWidth = width == 0 ? 1.0f : (float)width / (float)height;
	float dialogue = std::sqrt(sensorWidth * sensorWidth + 1 * 1);
	float FOV = 2 * atan(dialogue / (2 * focus_distance_)) * 180 / M_PI;
	ProjMatrix_ = glm::perspective(glm::radians(FOV), sensorWidth, 0.1f, 100.0f);
	preVPMatrix_ = ProjMatrix_ * ViewMatrix_;
}

Ray Camera::GetRay(float u, float v)
{
	auto src = position_;
	auto tar = screen_left_down_corner_ + u * horizontal_ + (1.0f - v) * vertical_;

	if (useDOF_)
	{
		auto lens = Walnut::Random::InUnitCircle() * lens_radius_;
		glm::vec3 offset = lens.x * horizontal_ + lens.y * vertical_;
		src += offset;
		tar = focus_left_down_corner_ + u * focus_horizontal_ + (1.0f - v) * focus_vertical_;
	}

	return Ray(src, tar - src);
}

Ray Camera::GetNormalizedRay(float u, float v)
{
	auto ray = GetRay(u, v);
	return Ray(ray.origin(), glm::normalize(ray.direction()));
}