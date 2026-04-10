#include "DescriptorSet.h"

namespace vulkan
{ 
    void DescriptorLayoutBuilder::AddBinding(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags shaderStages, VkSampler* ImmutableSamplers, uint32_t descriptorCount)
    {
	    bindings.push_back({});
        auto& uboLayoutBinding = bindings.back();
        uboLayoutBinding.binding = binding;
        uboLayoutBinding.descriptorType = descriptorType;
        uboLayoutBinding.stageFlags = shaderStages;
        uboLayoutBinding.descriptorCount = descriptorCount;
        uboLayoutBinding.pImmutableSamplers = ImmutableSamplers; 
        //
    }

    void DescriptorLayoutBuilder::clear()
    {
        bindings.clear();
    }

    void DescriptorLayoutBuilder::build(VkDevice device, VkDescriptorSetLayout& descriptorSetLayout, void* pNext, VkDescriptorSetLayoutCreateFlags flags)
    {
        VkDescriptorSetLayoutCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.pNext = pNext;

        info.pBindings = bindings.data();
        info.bindingCount = (uint32_t)bindings.size();
        info.flags = flags;

           if (vkCreateDescriptorSetLayout(device, &info, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }


    //===============
    void DescriptorAllocator::init(VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios)
    {
        ratios.clear();
    
        for (auto& r : poolRatios) {
            ratios.push_back(r);
        }
	
        VkDescriptorPool newPool = create_pool(device, initialSets, poolRatios);

        setsPerPool = initialSets * 1.5; //grow it next allocation

        readyPools.push_back(newPool);
    }

    void DescriptorAllocator::clear_pools(VkDevice device)
    { 
        for (auto p : readyPools) {
            vkResetDescriptorPool(device, p, 0);
        }
        for (auto p : fullPools) {
            vkResetDescriptorPool(device, p, 0);
            readyPools.push_back(p);
        }
        fullPools.clear();
    }

    void DescriptorAllocator::destroy_pools(VkDevice device)
    {
	    for (auto p : readyPools) {
		    vkDestroyDescriptorPool(device, p, nullptr);
	    }
        readyPools.clear();
	    for (auto p : fullPools) {
		    vkDestroyDescriptorPool(device,p,nullptr);
        }
        fullPools.clear();
    }

    // ��readyPool�з��䣬ʧ��ʱget�µ�pool���䣬��ʧ�����쳣
    void DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout, VkDescriptorSet& descriptorSet, void* pNext /*= nullptr*/)
    {
        //get or create a pool to allocate from
        VkDescriptorPool poolToUse = get_pool(device);

	    VkDescriptorSetAllocateInfo allocInfo = {};
	    allocInfo.pNext = pNext;
	    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	    allocInfo.descriptorPool = poolToUse;
	    allocInfo.descriptorSetCount = 1;
	    allocInfo.pSetLayouts = &layout;

	    VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);

        //allocation failed. Try again
        if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {

            fullPools.push_back(poolToUse);
    
            poolToUse = get_pool(device);
            allocInfo.descriptorPool = poolToUse;

           if (!vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet))
           {
               throw std::runtime_error("can't Allocate DescriptorSets");
           }
        }
  
        readyPools.push_back(poolToUse);
    }

    // readyPool����ʱ�����л�ȡ�����򴴽���Pool
    VkDescriptorPool DescriptorAllocator::get_pool(VkDevice device)
    {       
        VkDescriptorPool newPool;
        if (readyPools.size() != 0) {
            newPool = readyPools.back();
            readyPools.pop_back();
        }
        else {
	        //need to create a new pool
	        newPool = create_pool(device, setsPerPool, ratios);

	        setsPerPool = setsPerPool * 1.5;
	        if (setsPerPool > 4092) {
		        setsPerPool = 4092;
	        }
        }   

        return newPool;
    }

    // ÿ�δ��� setsPerPoold������
    VkDescriptorPool DescriptorAllocator::create_pool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios)
    {
	    std::vector<VkDescriptorPoolSize> poolSizes;
	    for (PoolSizeRatio ratio : poolRatios) {
		    poolSizes.push_back(VkDescriptorPoolSize{
			    .type = ratio.type,
			    .descriptorCount = uint32_t(ratio.ratio * setCount)
		    });
	    }

	    VkDescriptorPoolCreateInfo pool_info = {};
	    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	    pool_info.flags = 0;
	    pool_info.maxSets = setCount;
	    pool_info.poolSizeCount = (uint32_t)poolSizes.size();
	    pool_info.pPoolSizes = poolSizes.data();

	    VkDescriptorPool newPool;
	    vkCreateDescriptorPool(device, &pool_info, nullptr, &newPool);
        return newPool;
    }

    void DescriptorWriter::write_image(int binding, VkDescriptorImageInfo* descriptorImageInfoes, VkDescriptorType type, int descriptorCount)
    {
	    VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

	    write.dstBinding = binding;
	    write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
	    write.descriptorCount = descriptorCount;
	    write.descriptorType = type;
	    write.pImageInfo = descriptorImageInfoes;

	    writes.push_back(write);
    }

    void DescriptorWriter::write_image(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type)
    {
        VkDescriptorImageInfo& info = imageInfos.emplace_back(VkDescriptorImageInfo{
		    .sampler = sampler,
		    .imageView = image,
		    .imageLayout = layout
	    });

	    VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

	    write.dstBinding = binding;
	    write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
	    write.descriptorCount = 1;
	    write.descriptorType = type;
	    write.pImageInfo = &info;

	    writes.push_back(write);
    }

    void DescriptorWriter::write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type)
    {
	    VkDescriptorBufferInfo& info = bufferInfos.emplace_back(VkDescriptorBufferInfo{
		    .buffer = buffer,
		    .offset = offset,
		    .range = size
		    });

	    VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};

	    write.dstBinding = binding;
	    write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
	    write.descriptorCount = 1;
	    write.descriptorType = type;
	    write.pBufferInfo = &info;

	    writes.push_back(write);
    }

	void DescriptorWriter::write_structure(int binding, uint32_t structureCount, const VkAccelerationStructureKHR* structure, VkDescriptorType type)
	{
        VkWriteDescriptorSetAccelerationStructureKHR& descriptorAccelerationStructureInfo = structureInfos.emplace_back(VkWriteDescriptorSetAccelerationStructureKHR{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .accelerationStructureCount = structureCount,
            .pAccelerationStructures = structure
            });

		VkWriteDescriptorSet accelerationStructureWrite{};
		accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		// The specialized acceleration structure descriptor has to be chained
		accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
		accelerationStructureWrite.dstSet = VK_NULL_HANDLE;
		accelerationStructureWrite.dstBinding = binding;
		accelerationStructureWrite.descriptorCount = 1;
		accelerationStructureWrite.descriptorType = type;

        writes.push_back(accelerationStructureWrite);
	}

    void DescriptorWriter::clear()
    {
        imageInfos.clear();
        writes.clear();
        bufferInfos.clear();
    }

    void DescriptorWriter::update_set(VkDevice device, VkDescriptorSet set)
    {
        for (VkWriteDescriptorSet& write : writes) {
            write.dstSet = set;
        }

        vkUpdateDescriptorSets(device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
    }
}
