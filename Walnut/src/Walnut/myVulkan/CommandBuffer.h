#pragma once
#include "VulkanInclude.h"

namespace vulkan
{
    class CommandBuffer;
    class CommandPool;

    class CommandPool
    {
	    public:
        friend class CommandBuffer;
        friend class SingleTimeCommands;

        // 创建命令池（指定队列族、重置模式
        CommandPool(VkDevice device, uint32_t queueFamilyIndex, bool allowIndividualReset = true);
        ~CommandPool();
    
        // 分配命令缓冲区
        CommandBuffer* allocateBuffer (VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, bool isNeedFree = true);
   
        // 批量分配
        std::vector<CommandBuffer*> allocateBuffers(uint32_t count, VkCommandBufferLevel level);
    
        // 重置整个池（可选标记）
        void reset(bool releaseResources = false);
    
        // 访问底层句柄
        inline VkCommandPool handle() const { return pool_; }

    private:
        VkDevice device_;
        VkCommandPool pool_;
        std::vector<CommandBuffer*> buffers;
        bool individualReset_; // 支持单独重置命令缓冲区

    };

    class CommandBuffer
    {
    public:
        // 包装现有缓冲区（用于外部分配）
        CommandBuffer(VkCommandBuffer buffer, CommandPool* owner);
        ~CommandBuffer();

        // 记录控制
        void begin(VkCommandBufferUsageFlags flags = 0);
        void end();
        void reset(bool releaseResources = false);
    
        // 提交命令（自动处理信号量/围栏）
        void submit(
            VkQueue queue,
            VkFence fence = VK_NULL_HANDLE,
            const std::vector<VkSemaphore>& waitSemaphores = {},
            const std::vector<VkPipelineStageFlags>& waitStages = {},
            const std::vector<VkSemaphore>& signalSemaphores = {}
        );

        void submitAndWait(VkQueue queue);

        // 访问底层句柄
        inline VkCommandBuffer& handle(){ return buffer_; }
        inline const VkCommandBuffer* handlePtr() const { return &buffer_; }

    private:
        VkCommandBuffer buffer_;
        CommandPool* owner_; // 非拥有指针
        bool isRecording_ = false;

    };

    class SingleTimeCommands
    {
    public:
        CommandPool* pool;
        CommandBuffer* buffer;

        SingleTimeCommands() = delete;

        
        SingleTimeCommands(CommandPool* pool);  // 销毁时不会自动提交！！！ 记得手动提交！！！

        ~SingleTimeCommands();

        void Submit(VkQueue queue);

        inline VkCommandBuffer getBuffer() { return buffer->handle(); }
        operator VkCommandBuffer() { return buffer->handle(); }
    };

}