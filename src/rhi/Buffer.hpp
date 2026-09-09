#pragma once
#include "RHICommon.hpp"

namespace prismcraft {

class VulkanContext;
class CommandQueue;

class Buffer {
public:
    Buffer() = default; // Allows default construction for later initialization
    Buffer(VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage,
           VmaMemoryUsage memoryUsage);
    ~Buffer();

    // Move semantics
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    // Map and copy data directly (for host-visible buffers)
    void upload(const void* data, VkDeviceSize size);

    // Upload via staging buffer (for device-local buffers)
    void uploadStaged(VulkanContext& context, CommandQueue& cmdQueue,
                      const void* data, VkDeviceSize size);

    // Record copy from staging buffer into this buffer using an existing command buffer (for batched uploads)
    void recordCopy(VkCommandBuffer cmd, const Buffer& stagingBuffer, VkDeviceSize size);

    [[nodiscard]] VkBuffer getBuffer() const { return m_buffer; }
    [[nodiscard]] VkDeviceSize getSize() const { return m_size; }
    [[nodiscard]] bool isValid() const { return m_buffer != VK_NULL_HANDLE; }

private:
    void cleanup();

    VulkanContext* m_context = nullptr;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VkDeviceSize m_size = 0;
};

} // namespace prismcraft
