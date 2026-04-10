#define VMA_IMPLEMENTATION
#include "VkBufferWrapper.h"

DynamicLoader g_dynamicLoader;

VkShaderModule vulkan::loadShader(const char* fileName, VkDevice device)
{
	std::ifstream is(fileName, std::ios::binary | std::ios::in | std::ios::ate);

	if (is.is_open())
	{
		size_t size = is.tellg();
		is.seekg(0, std::ios::beg);
		char* shaderCode = new char[size];
		is.read(shaderCode, size);
		is.close();

		assert(size > 0);

		VkShaderModule shaderModule;
		VkShaderModuleCreateInfo moduleCreateInfo{};
		moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		moduleCreateInfo.codeSize = size;
		moduleCreateInfo.pCode = (uint32_t*)shaderCode;

		vkCreateShaderModule(device, &moduleCreateInfo, NULL, &shaderModule);

		delete[] shaderCode;

		return shaderModule;
	}
	else
	{
		std::cerr << "Error: Could not open shader file \"" << fileName << "\"" << "\n";
		return VK_NULL_HANDLE;
	}
}

PFN_vkVoidFunction DynamicLoader::GetInstanceProc(const std::string& name)
{
    auto func = vkGetInstanceProcAddr(instance_, name.c_str());
	if (func != nullptr) {
        return func;
	}
	else {
		return 0;
	}
}

PFN_vkVoidFunction DynamicLoader::GetDeviceProc(const std::string& name)
{
    auto func = vkGetDeviceProcAddr(device_, name.c_str());
	if (func != nullptr) {
        return func;
	}
	else {
		return 0;
	}
}


void* vulkan::VulkanMemoryResource::map()
{
    if (!m_isMapped)
    {
        VkResult result = vmaMapMemory(m_allocator, m_allocation, &m_allocationInfo.pMappedData);

        if (result == VK_SUCCESS)
        {
                m_isMapped = true;
        }
    }
    return m_allocationInfo.pMappedData;
}

void vulkan::VulkanMemoryResource::unmap()
{
    if (m_isMapped) {
        vmaUnmapMemory(m_allocator, m_allocation);
        m_allocationInfo.pMappedData = nullptr;
        m_isMapped = false;
    }
}

void vulkan::VulkanMemoryResource::flush(VkDeviceSize offset /*= 0*/, VkDeviceSize size /*= VK_WHOLE_SIZE*/)
{
    vmaFlushAllocation(m_allocator, m_allocation, offset, size);
}

void vulkan::VulkanMemoryResource::invalidate(VkDeviceSize offset /*= 0*/, VkDeviceSize size /*= VK_WHOLE_SIZE*/)
{
    vmaInvalidateAllocation(m_allocator, m_allocation, offset, size);
}

uint64_t vulkan::VulkanMemoryResource::getBufferDeviceAddress()
{
		VkBufferDeviceAddressInfoKHR bufferDeviceAI{};
		bufferDeviceAI.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		bufferDeviceAI.buffer = m_buffer;
        std::string name = "vkGetBufferDeviceAddressKHR";
        auto func = (PFN_vkGetBufferDeviceAddressKHR)(g_dynamicLoader.GetDeviceProc(name));
		return func(m_device, &bufferDeviceAI);
}

void vulkan::VulkanMemoryResource::uploadData(const void* data, VkDeviceSize size, VkDeviceSize offset /*= 0*/)
{
    if (size == VK_WHOLE_SIZE) {
        size = m_createInfo.size - offset;
    }
    
    void* mapped = map();
    memcpy(static_cast<char*>(mapped) + offset, data, size);
    // 
    flush(offset, size);
}

vulkan::VulkanMemoryResource::~VulkanMemoryResource()
{
    if (m_isMapped) unmap();

    if (m_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
    }
}

vulkan::VulkanMemoryResource::VulkanMemoryResource(VulkanAllocator* allocator, CreateInfo& info)
    : m_allocator(allocator->handle()), m_createInfo(info)
{
    m_device = allocator->getDevice();
    VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.size = info.size;
    bufferInfo.usage = info.usage;
    bufferInfo.flags = info.flags;

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = info.memoryUsage;
    allocCreateInfo.flags = info.allocFlags;

    // 自动映射请求
    if (allocCreateInfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) {
        allocCreateInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }
    // 自动获取address请求
    if (bufferInfo.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        allocCreateInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    }

    // 创建缓冲区和分配
    VkResult result = vmaCreateBuffer(
        m_allocator,
        &bufferInfo,
        &allocCreateInfo,
        &m_buffer,
        &m_allocation,
        &m_allocationInfo
    );

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan memory resource");
    }

    // 设置调试名称
    if (info.debugName) {
        vmaSetAllocationName(m_allocator, m_allocation, info.debugName);
    }
}

void vulkan::VulkanAllocator::init(const CreateInfo& info, VmaAllocatorCreateFlags flags)
{
    m_device = info.device;

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    
    allocatorInfo.physicalDevice = info.physicalDevice;
    allocatorInfo.device = info.device;
    allocatorInfo.instance = info.instance;
    allocatorInfo.vulkanApiVersion = info.vulkanApiVersion;
    allocatorInfo.flags = flags;

    vmaCreateAllocator(&allocatorInfo, &m_allocator);
}

void vulkan::VulkanAllocator::shutdown()
{
    if (m_allocator)
    {
        vmaDestroyAllocator(m_allocator);
    }
}

std::unique_ptr<vulkan::VulkanMemoryResource> vulkan::VulkanAllocator::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags flags /*= 0*/, const char* debugName /*= nullptr */)
{
    if (!m_allocator)
    {
        throw std::runtime_error("create buffer while not init");
    }

    vulkan::VulkanMemoryResource::CreateInfo info{};
    info.size = size;
    info.usage = usage;
    info.memoryUsage = memoryUsage;
    info.allocFlags = flags;
    info.debugName = debugName;

    std::unique_ptr<vulkan::VulkanMemoryResource> buffer = std::make_unique<vulkan::VulkanMemoryResource>(this, info);
    return buffer;
}

vulkan::VulkanAllocator vulkan::VulkanAllocator::s_instance;



vulkan::VulkanLocalBuffer::VulkanLocalBuffer(VulkanAllocator* allocator, VkDeviceSize size, VkBufferUsageFlags usage, CopierCreateInfo& info)
    :m_allocator(allocator), m_size(size)
{
    m_buffer = m_allocator->createBuffer(size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

    m_memoryCopier = new vulkan::VulkanMemoryCopier(allocator->getDevice(), info.commandPool, info.transferQueue);

}

vulkan::VulkanLocalBuffer::~VulkanLocalBuffer()
{
    delete m_memoryCopier;
}

void vulkan::VulkanLocalBuffer::UploadMemory(const void* data, VkDeviceSize size, VkDeviceSize offset /*= 0*/)
{
    if (!m_allocator)
    {
        throw std::runtime_error("LocalBuffer is not init");
    }

    if (size == VK_WHOLE_SIZE) {
        size = m_size - offset;
    }

    // staging Buffer
    auto ptr = m_allocator->createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_HOST, VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT );
    auto& stagingBuffer = *ptr;
    stagingBuffer.uploadData(data, size, offset);
    // Copy
    m_memoryCopier->CopyBuffer(stagingBuffer.buffer(), m_buffer->buffer(), size);
    // Wait
    m_memoryCopier->waitForCompletion();

    delete ptr.release();
}

vulkan::VulkanMemoryCopier::VulkanMemoryCopier(VkDevice device, CommandPool* commandPool, VkQueue transferQueue)
    :device_(device), commandPool_(commandPool), queue_(transferQueue)
{
    createFence();
}

vulkan::VulkanMemoryCopier::~VulkanMemoryCopier()
{
    vkDestroyFence(device_, fence_, nullptr);
    delete commandBuffer_;
}

void vulkan::VulkanMemoryCopier::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkDeviceSize srcOffest, VkDeviceSize dstOffest)
{
    // 等待上一次完成
    vkWaitForFences(device_, 1, &fence_, VK_TRUE, UINT64_MAX);
    vkResetFences(device_, 1, &fence_);

    // 重用cmd
    if (!commandBuffer_) createCommandBuffer();
    else commandBuffer_->reset(false);
    // 开始记录
    commandBuffer_->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        // copy Buffer
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = srcOffest; // Optional
    copyRegion.dstOffset = dstOffest; // Optional
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer_->handle(), srcBuffer, dstBuffer, 1, &copyRegion);

    commandBuffer_->end();
    commandBuffer_->submit(queue_, fence_);
}

void vulkan::VulkanMemoryCopier::createFence()
{
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // 初始已触发
    vkCreateFence(device_, &fenceInfo, nullptr, &fence_);
}

void vulkan::VulkanMemoryCopier::createCommandBuffer()
{
     commandBuffer_ = commandPool_->allocateBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
}

void vulkan::VulkanMemoryCopier::CopyBufferToImage(VkBuffer srcBuffer, VkImage image, uint32_t width, uint32_t height)
{
        // 等待上一次完成
    vkWaitForFences(device_, 1, &fence_, VK_TRUE, UINT64_MAX);
    vkResetFences(device_, 1, &fence_);

    // Copy
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = {
        width,
        height,
        1
    };
        // 重用cmd
    if (!commandBuffer_) createCommandBuffer();
    else commandBuffer_->reset(false);
    // 开始记录
    commandBuffer_->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    vkCmdCopyBufferToImage(commandBuffer_->handle(), srcBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    commandBuffer_->end();
    commandBuffer_->submit(queue_, fence_);
}

#ifdef UseStorgeBuffer
vulkan::VulkanStorageBuffer::VulkanStorageBuffer(VulkanAllocator* allocator)
    :m_allocator(allocator)
{
    global_vulkan_context = &Global_Vulkan_Context::Get();
    auto& deviceProperties = global_vulkan_context->graphicsbase_->GetpPysicalDeviceProperties();
    frames_in_flight_ = global_vulkan_context->application_->MAX_FRAME_IN_FLIGHT;
    bufferSize_ = 1024 * 1024 * 128;

    // limit
    _min_uniform_buffer_offset_alignment =
        static_cast<uint32_t>(deviceProperties.limits.minUniformBufferOffsetAlignment);
    _min_storage_buffer_offset_alignment =
        static_cast<uint32_t>(deviceProperties.limits.minStorageBufferOffsetAlignment);
    _max_storage_buffer_range = deviceProperties.limits.maxStorageBufferRange;
    _non_coherent_atom_size = deviceProperties.limits.nonCoherentAtomSize;

    // Buffer
    m_buffer = m_allocator->createBuffer(bufferSize_, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);

    // Ptr
    _global_upload_ringbuffers_begin.resize(frames_in_flight_);
    _global_upload_ringbuffers_end.resize(frames_in_flight_);
    _global_upload_ringbuffers_size.resize(frames_in_flight_);
    for (uint32_t i = 0; i < frames_in_flight_; ++i)
    {
        _global_upload_ringbuffers_begin[i] = (bufferSize_ * i) / frames_in_flight_;
        _global_upload_ringbuffers_size[i] =
            (bufferSize_ * (i + 1)) / frames_in_flight_ -
            (bufferSize_ * i) / frames_in_flight_;
    }


}

void vulkan::VulkanStorageBuffer::ResetStorageBufferPointer(int currentFrame)
{
    _global_upload_ringbuffers_end[currentFrame] = _global_upload_ringbuffers_begin[currentFrame];
}

uint32_t vulkan::VulkanStorageBuffer::DynamicUpdateData(const void* data, uint32_t size, int currentFrame)
{
    uint32_t dynamicOffset = roundUp(_global_upload_ringbuffers_end[currentFrame], _min_storage_buffer_offset_alignment);
    _global_upload_ringbuffers_end[currentFrame] = dynamicOffset + size;
    if (_global_upload_ringbuffers_end[currentFrame] > _global_upload_ringbuffers_begin[currentFrame] + _global_upload_ringbuffers_size[currentFrame])
    {
        throw std::runtime_error("Buffer Update overflow");
    }
    m_buffer->uploadData(data, size, dynamicOffset);
    return dynamicOffset;
}

void* vulkan::VulkanStorageBuffer::BeginUpdateData(uint32_t size, int currentFrame, uint32_t& dynamicOffset)
{
    dynamicOffset = roundUp(_global_upload_ringbuffers_end[currentFrame], _min_storage_buffer_offset_alignment);
    _global_upload_ringbuffers_end[currentFrame] = dynamicOffset + size;
    if (_global_upload_ringbuffers_end[currentFrame] > _global_upload_ringbuffers_begin[currentFrame] + _global_upload_ringbuffers_size[currentFrame])
    {
        throw std::runtime_error("Buffer Update overflow");
    }
    return m_buffer->mappedData();
}

vulkan::VulkanStorageBuffer::~VulkanStorageBuffer()
{

}
#endif