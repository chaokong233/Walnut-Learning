#include "CommandBuffer.h"

namespace vulkan
{
    CommandPool::CommandPool(VkDevice device, uint32_t queueFamilyIndex, bool allowIndividualReset /*= true*/)
        : device_(device), individualReset_(allowIndividualReset)
    {
        VkCommandPoolCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        info.queueFamilyIndex = queueFamilyIndex;
        info.flags = allowIndividualReset
            ? VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
            : 0;


        if (vkCreateCommandPool(device_, &info, nullptr, &pool_) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command pool!");
        }
    }

    CommandPool::~CommandPool()
    {
        for (auto buffer : buffers)
        {
            delete buffer;
        }
        buffers.clear();

        vkDestroyCommandPool(device_, pool_, nullptr);
    }

    CommandBuffer* CommandPool::allocateBuffer(VkCommandBufferLevel level /*= VK_COMMAND_BUFFER_LEVEL_PRIMARY*/, bool isNeedFree)
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = pool_;
        allocInfo.level = level;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer buffer;
        if (vkAllocateCommandBuffers(device_, &allocInfo, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
        auto newbuffer = new CommandBuffer(buffer, this);
        if (isNeedFree)
        {
            buffers.push_back(newbuffer);
        }
        return newbuffer;
    }

    std::vector<CommandBuffer*> CommandPool::allocateBuffers(uint32_t count, VkCommandBufferLevel level)
    {
        std::vector<VkCommandBuffer> buffers;
        buffers.resize(count);
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = pool_;
        allocInfo.level = level;
        allocInfo.commandBufferCount = count;

        vkAllocateCommandBuffers(device_, &allocInfo, buffers.data());

        std::vector<CommandBuffer*> _buffers;
        _buffers.resize(count);
        for (size_t i = 0; i < count; i++)
        {
            auto newbuffer = new CommandBuffer(buffers[i], this);
            _buffers.push_back(newbuffer);
        }
        return _buffers;
    }

    void CommandPool::reset(bool releaseResources /*= false*/)
    {
    }

    CommandBuffer::CommandBuffer(VkCommandBuffer buffer, CommandPool* owner)
        :buffer_(buffer), owner_(owner)
    {

    }

    CommandBuffer::~CommandBuffer()
    {
        if (isRecording_) {
            std::cerr << "WARNING: CommandBuffer destroyed while recording!" << std::endl;
            end(); // 安全结束
        }
    }

    void CommandBuffer::begin(VkCommandBufferUsageFlags flags /*= 0*/)
    {
        if (isRecording_) {
            throw std::runtime_error("CommandBuffer is already in recording state");
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = flags;
        beginInfo.pInheritanceInfo = nullptr; // 仅主命令缓冲区需要

        VkResult result = vkBeginCommandBuffer(buffer_, &beginInfo);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin command buffer recording: " + std::to_string(result));
        }

        isRecording_ = true;
    }

    void CommandBuffer::end()
    {
        if (!isRecording_) {
            throw std::runtime_error("CommandBuffer is not in recording state");
        }

        VkResult result = vkEndCommandBuffer(buffer_);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to end command buffer recording: " + std::to_string(result));
        }

        isRecording_ = false;
    }

    void CommandBuffer::reset(bool releaseResources /*= false*/)
    {
        if (isRecording_) {
            throw std::runtime_error("Cannot reset while recording");
        }

        VkCommandBufferResetFlags flags = releaseResources ?
            VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT : 0;

        VkResult result = vkResetCommandBuffer(buffer_, flags);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to reset command buffer: " + std::to_string(result));
        }
    }

    void CommandBuffer::submit(VkQueue queue, VkFence fence /*= VK_NULL_HANDLE*/, const std::vector<VkSemaphore>& waitSemaphores /*= {}*/, const std::vector<VkPipelineStageFlags>& waitStages /*= {}*/, const std::vector<VkSemaphore>& signalSemaphores /*= {} */)
    {
        if (isRecording_) {
            throw std::runtime_error("Cannot submit while recording");
        }

        // 验证等待信号量和阶段数量匹配
        if (waitSemaphores.size() != waitStages.size()) {
            throw std::invalid_argument("waitSemaphores and waitStages must have same size");
        }

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &buffer_;

        if (!waitSemaphores.empty()) {
            submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
            submitInfo.pWaitSemaphores = waitSemaphores.data();
            submitInfo.pWaitDstStageMask = waitStages.data();
        }

        if (!signalSemaphores.empty()) {
            submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
            submitInfo.pSignalSemaphores = signalSemaphores.data();
        }

        VkResult result = vkQueueSubmit(queue, 1, &submitInfo, fence);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Queue submit failed: " + std::to_string(result));
        }
    }

    void CommandBuffer::submitAndWait(VkQueue queue)
    {
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

        VkFence fence;
        VkResult result = vkCreateFence(owner_->device_, &fenceInfo, nullptr, &fence);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Fence creation failed: " + std::to_string(result));
        }

        try {
            submit(queue, fence);
            vkWaitForFences(owner_->device_, 1, &fence, VK_TRUE, UINT64_MAX);

        }
        catch (...) {
            vkDestroyFence(owner_->device_, fence, nullptr);
            throw;
        }

        vkDestroyFence(owner_->device_, fence, nullptr);
    }
    SingleTimeCommands::SingleTimeCommands(CommandPool* pool)
        :pool(pool)
    {
        this->buffer = pool->allocateBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
        buffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    }

    SingleTimeCommands::~SingleTimeCommands()
    {
        vkFreeCommandBuffers(pool->device_, pool->handle(), 1, buffer->handlePtr());
    }

    void SingleTimeCommands::Submit(VkQueue queue)
    {
        buffer->end();
        buffer->submitAndWait(queue);
    }
}