#pragma once
#include "VulkanInclude.h"

namespace vulkan
{
	class DescriptorLayoutBuilder {
	public:
		std::vector<VkDescriptorSetLayoutBinding> bindings;

		void AddBinding(uint32_t binding,VkDescriptorType descriptorType, VkShaderStageFlags shaderStages, VkSampler* ImmutableSamplers = nullptr, uint32_t descriptorCount = 1);
		void clear();
		void build(VkDevice device, VkDescriptorSetLayout& descriptorSetLayout, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
	};


	class DescriptorAllocator {
	public:
		struct PoolSizeRatio {
			VkDescriptorType type;
			float ratio;
		};

		void init(VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios);
		void clear_pools(VkDevice device);
		void destroy_pools(VkDevice device);

		void allocate(VkDevice device, VkDescriptorSetLayout layout, VkDescriptorSet& descriptorSet, void* pNext = nullptr);

		inline VkDescriptorPool GetHandle() const { return readyPools[0]; }

	private:
		VkDescriptorPool get_pool(VkDevice device);
		VkDescriptorPool create_pool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios);

		std::vector<PoolSizeRatio> ratios;
		std::vector<VkDescriptorPool> fullPools;
		std::vector<VkDescriptorPool> readyPools;
		uint32_t setsPerPool;

	};


	class DescriptorWriter {
	public:
		std::deque<VkDescriptorImageInfo> imageInfos;
		std::deque<VkDescriptorBufferInfo> bufferInfos;
		std::deque<VkWriteDescriptorSetAccelerationStructureKHR> structureInfos;
		std::vector<VkWriteDescriptorSet> writes;

		DescriptorWriter() = default;
		// 写入多个image
		void write_image(int binding, VkDescriptorImageInfo* descriptorImageInfoes, VkDescriptorType type, int descriptorCount);
		// 写入单个image
		void write_image(int binding,VkImageView image,VkSampler sampler , VkImageLayout layout, VkDescriptorType type);
		void write_buffer(int binding,VkBuffer buffer,size_t size, size_t offset,VkDescriptorType type); 
		void write_structure(int binding, uint32_t structureCount, const VkAccelerationStructureKHR* structure, VkDescriptorType type); 

		void clear();
		void update_set(VkDevice device, VkDescriptorSet set);
	};
}