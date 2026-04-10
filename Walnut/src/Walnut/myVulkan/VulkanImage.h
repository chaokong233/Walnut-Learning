#pragma once
#include "VkBufferWrapper.h"

namespace vulkan
{
	class VulkanImage
	{
    public:
        struct CreateInfo {
            uint32_t width = 1;
            uint32_t height = 1;
            uint32_t depth = 1;
            uint32_t mipLevels = 1;
            uint32_t arrayLayers = 1;
            VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
            VkImageType imageType = VK_IMAGE_TYPE_2D;
            VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
            VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT;
            VkImageCreateFlags flags = 0;
            VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
            VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        
            // 视图创建参数
            VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
            VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            VkComponentMapping components = {
                VK_COMPONENT_SWIZZLE_IDENTITY, 
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY
            };
        };


        VulkanImage(VulkanAllocator* allocator,const CreateInfo& createInfo,VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
        VulkanImage(VulkanAllocator* allocator,const CreateInfo& createInfo, bool isCreateView, VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
        ~VulkanImage();

        //
        VulkanImage(const VulkanImage&) = delete;
        VulkanImage& operator=(const VulkanImage&) = delete;

        // Getter
        inline VkImage handle() const { return image_; }
        operator VkImage() { return image_; }
 
        inline VkImageView getView() const { return imageView_; }
        inline VmaAllocationInfo& getAllocationInfo() { return allocationInfo_; }
        inline CreateInfo& getCreateInfo() { return info_; }

        void createImageView(VulkanAllocator* allocator, const CreateInfo& createInfo);
        void destroyImageView();

            // Translate
        static void transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
            VkImageAspectFlags aspectMask, uint32_t mipLevels = 1, uint32_t layerCount = 1);
        static void waitComputeShaderComplete(VkCommandBuffer cmd, VkImage image,
            VkImageAspectFlags aspectMask, uint32_t mipLevels = 1, uint32_t layerCount = 1);

    private:
        VulkanAllocator* allocator_ = nullptr;
        VkImage image_ = VK_NULL_HANDLE;
        VmaAllocation allocation_ = nullptr;
        VkImageView imageView_ = VK_NULL_HANDLE;
        CreateInfo info_;
        VmaAllocationInfo allocationInfo_;

        void createImage(VulkanAllocator* allocator, const CreateInfo& createInfo, VmaMemoryUsage memoryUsage);
        void createImageView(VkDevice device, const CreateInfo& viewInfo);
        void destroy();
	};

    class VulkanLoadedTexture
    {
    public:
        struct CopierCreateInfo
		{
			CommandPool* commandPool;
			VkQueue transferQueue;
		};

        VulkanLoadedTexture(VulkanAllocator* allocator, const std::string& path, CopierCreateInfo info, VkBufferUsageFlags usages);
        ~VulkanLoadedTexture();

        //
        VulkanLoadedTexture(VulkanLoadedTexture&) = delete;
        VulkanLoadedTexture& operator=(VulkanLoadedTexture&) = delete;

        inline VkImage image() { return pImage_->handle(); }
        operator VkImage() { return pImage_->handle(); }
        inline VulkanImage* getVulkanImageHandle() { return pImage_; }
        inline std::string& secPath() { return srcPath; }
        inline VkImageView getView() { return pImage_->getView(); }
        inline uint32_t getTexSize() const { return texSize_; }

    private:
        VulkanAllocator* pAllocator_ = nullptr;
        VulkanImage* pImage_ = nullptr;
		std::unique_ptr<VulkanMemoryResource> buffer_ = {};
        std::string srcPath;
		VulkanMemoryCopier* pMemoryCopier_ = nullptr;
        uint32_t texSize_ = 0;
        uint32_t width_ = 0;
        uint32_t height_ = 0;

    };

    class VulkanSampler
    {
    public:
        struct CreateInfo
        {
            VkFilter magFilter = VK_FILTER_LINEAR;
            VkFilter minFilter = VK_FILTER_LINEAR;
            VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            float mipLodBias = 0.0f;
            bool anisotropyEnable = VK_TRUE;
            float maxAnisotropy = 8.0f;
            bool compareEnable = VK_FALSE;
            VkCompareOp compareOp = VK_COMPARE_OP_ALWAYS;
            float minLod = 0.0f;
            float maxLod = VK_LOD_CLAMP_NONE;
            VkBorderColor borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            bool unnormalizedCoordinates = VK_FALSE;
        };

        // 
        VulkanSampler(VkDevice device);
        VulkanSampler(VkDevice device, CreateInfo& createInfo);
        ~VulkanSampler();

        // 
        VulkanSampler(VulkanSampler&) = delete;
        VulkanSampler& operator=(const VulkanSampler&) = delete; 



        inline VkSampler handle() const { return sampler_; }
        inline VkDevice getDevice() const { return device_; }
        operator VkSampler() const { return sampler_; }

    private:
        VkSampler sampler_;
        CreateInfo createInfo_;
        VkDevice device_;

        void createSampler(CreateInfo& createInfo);

    };

}

