#pragma once
#include "VulkanInclude.h"
#include "CommandBuffer.h"

class DynamicLoader
{
	public:
	DynamicLoader() = default;

	DynamicLoader(VkInstance instance, VkDevice device)
		:instance_(instance), device_(device) {}

	PFN_vkVoidFunction GetInstanceProc(const std::string&);
	PFN_vkVoidFunction GetDeviceProc(const std::string&);

	private:
		VkInstance instance_{0};
		VkDevice device_{0};
};

extern DynamicLoader g_dynamicLoader;

namespace vulkan
{
	class VulkanAllocator;
	class VulkanMemoryResource {
	public:
		friend class VulkanLocalBuffer;
		friend class VulkanStorageBuffer;

		struct CreateInfo 
		{
			VkDeviceSize size = 0;
			VkBufferCreateFlags flags = 0;
			VkBufferUsageFlags usage = 0;
			VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO;
			VmaAllocationCreateFlags allocFlags = 0;
			const char* debugName = nullptr;
		};

		//
		VulkanMemoryResource(VulkanAllocator* allocator, CreateInfo& info);
		~VulkanMemoryResource();

		// 
		VulkanMemoryResource(VulkanMemoryResource&) = delete;
		VulkanMemoryResource& operator=(const VulkanMemoryResource&) = delete;

		//
		inline VkBuffer buffer() const { return m_buffer; }
		operator VkBuffer() const { return m_buffer; }
		inline VmaAllocation allocation() const { return m_allocation; }
		void* mappedData() const { return m_allocationInfo.pMappedData; }
		VkDeviceSize size() const { return m_createInfo.size; }

		// 内存操作
		void* map();
		void unmap();
		void flush(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
		void invalidate(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
		uint64_t getBufferDeviceAddress();

		// 数据上传
		void uploadData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);


	private:
		VmaAllocator m_allocator = VK_NULL_HANDLE;
		VkDevice m_device;
		VkBuffer m_buffer;
		VmaAllocation m_allocation;
		VmaAllocationInfo m_allocationInfo;
		CreateInfo m_createInfo;
		bool m_isMapped = false;
	};


	// 单例类，vmaAllocator的封装
	class VulkanAllocator
	{
	public:
		struct CreateInfo {
			VkInstance instance = VK_NULL_HANDLE;
			VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
			VkDevice device = VK_NULL_HANDLE; 
			uint32_t vulkanApiVersion = VK_API_VERSION_1_0;
		};
    
		inline static VulkanAllocator& get() { return s_instance; }
		inline VkDevice getDevice() const { return m_device; }

		void init(const CreateInfo& infom, VmaAllocatorCreateFlags flags = 0);
		void shutdown();
    
		inline VmaAllocator handle() const { return m_allocator; }
		operator VmaAllocator() const { return m_allocator; }
    
		// 创建各种类型的资源
		std::unique_ptr<VulkanMemoryResource> createBuffer(
			VkDeviceSize size,
			VkBufferUsageFlags usage,
			VmaMemoryUsage memoryUsage,
			VmaAllocationCreateFlags flags = 0,
			const char* debugName = nullptr
		);
    
		// 统计信息
		// void logMemoryStats() const;

	private:
		VulkanAllocator() = default;
		~VulkanAllocator() = default;
		VulkanAllocator(VulkanAllocator&) = delete;
		
		VmaAllocator m_allocator = VK_NULL_HANDLE;
		VkDevice m_device;
		static VulkanAllocator s_instance;
	};

	class VulkanMemoryCopier
	{
	public:

		VulkanMemoryCopier(VkDevice device, CommandPool* commandPool, VkQueue transferQueue);
		~VulkanMemoryCopier();

		// 
		VulkanMemoryCopier(VulkanMemoryCopier&) = delete;
		VulkanMemoryCopier& operator=(const VulkanMemoryCopier&) = delete;

		void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkDeviceSize srcOffest = 0, VkDeviceSize dstOffest = 0);
		void CopyBufferToImage(VkBuffer srcBuffer, VkImage image, uint32_t width, uint32_t height);

		// 等待复制完成
		inline void waitForCompletion() {
			vkWaitForFences(device_, 1, &fence_, VK_TRUE, UINT64_MAX);
		}

		// 检查是否完成
		inline bool isComplete() {
			return vkGetFenceStatus(device_, fence_) == VK_SUCCESS;
		}

	private:
		void createFence();
		void createCommandBuffer();

		VkDevice device_;
		CommandPool* commandPool_;
		VkQueue queue_;
		CommandBuffer* commandBuffer_ = nullptr;
		VkFence fence_ = VK_NULL_HANDLE;
	};

	class VulkanLocalBuffer
	{
	public:
		struct CopierCreateInfo
		{
			CommandPool* commandPool;
			VkQueue transferQueue;
		};

		VulkanLocalBuffer(VulkanAllocator* allocator, VkDeviceSize size, VkBufferUsageFlags usage, CopierCreateInfo& info);
		~VulkanLocalBuffer();

		//
		VulkanLocalBuffer(VulkanLocalBuffer&) = delete;
		VulkanLocalBuffer& operator=(const VulkanLocalBuffer&) = delete;

		inline VkBuffer buffer() const { return m_buffer->buffer(); }
		operator VkBuffer() const { return m_buffer->buffer(); }
		inline VkDeviceSize getSize() const { return m_size; }

		void UploadMemory(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

	private:
		VulkanAllocator* m_allocator = nullptr;
		std::unique_ptr<VulkanMemoryResource> m_buffer = {};
		VulkanMemoryCopier* m_memoryCopier = nullptr;
		VkDeviceSize m_size = 0;
	};

#ifdef UseStorgeBuffer
	class VulkanStorageBuffer
	{
	public:
		VulkanStorageBuffer(VulkanAllocator* allocator);
		~VulkanStorageBuffer();

		//
		VulkanStorageBuffer(VulkanStorageBuffer&) = delete;
		VulkanStorageBuffer& operator=(const VulkanStorageBuffer&) = delete;
		// Getter
		inline VkBuffer buffer() const { return m_buffer->buffer(); }
		operator VkBuffer() const { return m_buffer->buffer(); }

		void ResetStorageBufferPointer(int currentFrame);
		// 上传数据并更新end
		uint32_t DynamicUpdateData(const void* data, uint32_t size, int currentFrame);
		// 获取mapped指针
		void* BeginUpdateData(uint32_t size, int currentFrame, uint32_t& dynamicOffset);


	private:
		VulkanAllocator* m_allocator = nullptr;
		Global_Vulkan_Context* global_vulkan_context;
		std::shared_ptr<VulkanMemoryResource> m_buffer = {};
		int frames_in_flight_ = 3;
		// Buffer
		uint32_t bufferSize_ = 1024 * 1024 * 128;
		std::vector<uint32_t> _global_upload_ringbuffers_begin;
        std::vector<uint32_t> _global_upload_ringbuffers_end;
        std::vector<uint32_t> _global_upload_ringbuffers_size;

		// limits
        uint32_t _min_uniform_buffer_offset_alignment{ 256 };
        uint32_t _min_storage_buffer_offset_alignment{ 256 };
        uint32_t _max_storage_buffer_range{ 1 << 27 };
        uint32_t _non_coherent_atom_size{ 256 };
	};

#endif

VkShaderModule loadShader(const char* fileName, VkDevice device);

}

