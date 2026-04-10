#include "VulkanImage.h"
#include "stb_image.h"

vulkan::VulkanImage::VulkanImage(VulkanAllocator* allocator, const CreateInfo& createInfo, VmaMemoryUsage memoryUsage /*= VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE*/)
    :allocator_(allocator), info_(createInfo)
{
    createImage(allocator, createInfo, memoryUsage);
    createImageView(allocator, createInfo);
}

void vulkan::VulkanImage::createImage(VulkanAllocator* allocator, const CreateInfo& createInfo, VmaMemoryUsage memoryUsage)
{
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = createInfo.imageType;
        imageInfo.extent.width = createInfo.width;
        imageInfo.extent.height = createInfo.height;
        imageInfo.extent.depth = createInfo.depth;
        imageInfo.mipLevels = createInfo.mipLevels;
        imageInfo.arrayLayers = createInfo.arrayLayers;
        imageInfo.format = createInfo.format;
        imageInfo.tiling = createInfo.tiling;
        imageInfo.initialLayout = createInfo.initialLayout;
        imageInfo.usage = createInfo.usage;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = createInfo.samples;
        imageInfo.flags = createInfo.flags;
        
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = memoryUsage;
        
        if (vmaCreateImage(allocator->handle(), &imageInfo, &allocInfo, &image_, &allocation_, &allocationInfo_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image!");
        }
}

void vulkan::VulkanImage::createImageView(VulkanAllocator* allocator, const CreateInfo& createInfo)
{
    VkDevice device = allocator->getDevice();
        
    if (device == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to get device from allocator!");
    }
        
    createImageView(device, createInfo);
}

void vulkan::VulkanImage::destroyImageView()
{
    if (imageView_ != VK_NULL_HANDLE && allocator_ != nullptr) {
        auto device = allocator_->getDevice();
        vkDestroyImageView(device, imageView_, nullptr);
        imageView_ = VK_NULL_HANDLE;
    }
}


void vulkan::VulkanImage::transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspectMask, uint32_t mipLevels, uint32_t layerCount)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspectMask;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layerCount;
    
    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;
    
    //
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
    }
    // Compute -> Blit
    else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    // Blit -> RT
    else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        destinationStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
    }
    // RT -> Compute
    else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
        destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    // Load Texture
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
    }

    else {
        throw std::invalid_argument("Unsupported layout transition!");
    }

    vkCmdPipelineBarrier(cmd, sourceStage, destinationStage,0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

}

void vulkan::VulkanImage::waitComputeShaderComplete(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspectMask, uint32_t mipLevels /*= 1*/, uint32_t layerCount /*= 1*/)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspectMask;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layerCount;
    
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}

void vulkan::VulkanImage::createImageView(VkDevice device, const CreateInfo& viewInfo)
{
    if (imageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device, imageView_, nullptr);
    }
        
    VkImageViewCreateInfo viewCreateInfo{};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.image = image_;
    viewCreateInfo.viewType = viewInfo.viewType;
    viewCreateInfo.format = info_.format;
    viewCreateInfo.components = viewInfo.components;
    viewCreateInfo.subresourceRange.aspectMask = viewInfo.aspectMask;
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = info_.mipLevels;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount = info_.arrayLayers;
        
    if (vkCreateImageView(device, &viewCreateInfo, nullptr, &imageView_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image view!");
    }
}

void vulkan::VulkanImage::destroy()
{
    if (allocator_ != VK_NULL_HANDLE) {
        VkDevice device = allocator_->getDevice();
        
        if (imageView_ != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
            vkDestroyImageView(device, imageView_, nullptr);
        }
            
        if (image_ != VK_NULL_HANDLE) {
            vmaDestroyImage(allocator_->handle(), image_, allocation_);
        }
    }
        
    image_ = VK_NULL_HANDLE;
    allocation_ = nullptr;
    imageView_ = VK_NULL_HANDLE;
}

vulkan::VulkanImage::~VulkanImage()
{
    destroy();
}

vulkan::VulkanImage::VulkanImage(VulkanAllocator* allocator, const CreateInfo& createInfo, bool isCreateView, VmaMemoryUsage memoryUsage /*= VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE*/)
    :allocator_(allocator), info_(createInfo)
{
    createImage(allocator, createInfo, memoryUsage);
    if (isCreateView) createImageView(allocator, createInfo);
}


vulkan::VulkanLoadedTexture::VulkanLoadedTexture(VulkanAllocator* allocator, const std::string& path, CopierCreateInfo createInfo, VkBufferUsageFlags usages)
{
    if (!allocator)
    {
        throw std::runtime_error("allocator is Null");
    }

    // Load
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    width_ = texWidth;
    height_ = texHeight;

    VkDeviceSize imageSize = texWidth * texHeight * 4;
    texSize_ = imageSize;
    if (!pixels || width_ <= 0 || height_ <= 0)
    {
        std::cout << "load texture failed, path is not exsit";
        return;
    }
    
    // Init
        //
    VulkanImage::CreateInfo info;
    info.width = width_;
    info.height = height_;
    info.usage = usages | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    pImage_ = new VulkanImage(allocator, info);

    pMemoryCopier_ = new vulkan::VulkanMemoryCopier(allocator->getDevice(), createInfo.commandPool, createInfo.transferQueue);

    // Translate
    {
        SingleTimeCommands cmd(createInfo.commandPool);
        VulkanImage::transitionImageLayout(cmd.getBuffer(), pImage_->handle(),
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1);
    }
    // Copy
    auto ptr = allocator->createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_HOST, VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT );
    auto& stagingBuffer = *ptr;

    stagingBuffer.uploadData(pixels, imageSize, 0);

    pMemoryCopier_->CopyBufferToImage(stagingBuffer.buffer(), pImage_->handle(), width_, height_);
    // Wait
    pMemoryCopier_->waitForCompletion();
    // Release
    delete ptr.release();
    stbi_image_free(pixels);

}

vulkan::VulkanLoadedTexture::~VulkanLoadedTexture()
{
    delete pImage_;
    delete pMemoryCopier_;

}

vulkan::VulkanSampler::VulkanSampler(VkDevice device)
    :device_(device)
{
    createSampler(createInfo_);
}

void vulkan::VulkanSampler::createSampler(CreateInfo& createInfo)
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = createInfo.magFilter;
    samplerInfo.minFilter = createInfo.minFilter;
    samplerInfo.mipmapMode = createInfo.mipmapMode;
    samplerInfo.addressModeU = createInfo.addressModeU;
    samplerInfo.addressModeV = createInfo.addressModeV;
    samplerInfo.addressModeW = createInfo.addressModeW;
    samplerInfo.mipLodBias = createInfo.mipLodBias;
    samplerInfo.anisotropyEnable = createInfo.anisotropyEnable;
    samplerInfo.maxAnisotropy = createInfo.maxAnisotropy;
    samplerInfo.compareEnable = createInfo.compareEnable;
    samplerInfo.compareOp = createInfo.compareOp;
    samplerInfo.minLod = createInfo.minLod;
    samplerInfo.maxLod = createInfo.maxLod;
    samplerInfo.borderColor = createInfo.borderColor;
    samplerInfo.unnormalizedCoordinates = createInfo.unnormalizedCoordinates;

    if (vkCreateSampler(device_, &samplerInfo, nullptr, &sampler_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture sampler!");
    }
}

vulkan::VulkanSampler::~VulkanSampler()
{
    vkDestroySampler(device_, sampler_, nullptr);
}

vulkan::VulkanSampler::VulkanSampler(VkDevice device, CreateInfo& createInfo)
    :device_(device), createInfo_(createInfo)
{
    createSampler(createInfo);
}
