#include "Buffer.hpp"
#include "VulkanContext.hpp"
#include "CommandQueue.hpp"
#include <cstring>

namespace prismcraft {

Buffer::Buffer(VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage,
               VmaMemoryUsage memoryUsage)
    : m_context(&context), m_size(size) {

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memoryUsage;
    // For host-visible buffers (staging), enable sequential write access
    if (memoryUsage == VMA_MEMORY_USAGE_CPU_ONLY || memoryUsage == VMA_MEMORY_USAGE_CPU_TO_GPU) {
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }

    VK_CHECK(vmaCreateBuffer(m_context->getAllocator(), &bufferInfo, &allocInfo, &m_buffer, &m_allocation, nullptr), "Failed to create buffer!");
}

Buffer::~Buffer() {
    cleanup();
}

Buffer::Buffer(Buffer&& other) noexcept
    : m_context(other.m_context), m_buffer(other.m_buffer),
      m_allocation(other.m_allocation), m_size(other.m_size) {
    other.m_buffer = VK_NULL_HANDLE;
    other.m_allocation = VK_NULL_HANDLE;
    other.m_size = 0;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        cleanup();
        m_context = other.m_context;
        m_buffer = other.m_buffer;
        m_allocation = other.m_allocation;
        m_size = other.m_size;
        
        other.m_buffer = VK_NULL_HANDLE;
        other.m_allocation = VK_NULL_HANDLE;
        other.m_size = 0;
    }
    return *this;
}

void Buffer::upload(const void* data, VkDeviceSize size) {
    void* mappedData;
    VK_CHECK(vmaMapMemory(m_context->getAllocator(), m_allocation, &mappedData), "Failed to map buffer memory!");
    std::memcpy(mappedData, data, static_cast<size_t>(size));
    vmaUnmapMemory(m_context->getAllocator(), m_allocation);
}

void Buffer::uploadStaged(VulkanContext& context, CommandQueue& cmdQueue,
                          const void* data, VkDeviceSize size) {
    Buffer stagingBuffer(context, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    stagingBuffer.upload(data, size);

    VkCommandBuffer commandBuffer = cmdQueue.beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, stagingBuffer.getBuffer(), m_buffer, 1, &copyRegion);

    cmdQueue.endSingleTimeCommands(commandBuffer);
}

void Buffer::recordCopy(VkCommandBuffer cmd, const Buffer& stagingBuffer, VkDeviceSize size) {
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(cmd, stagingBuffer.getBuffer(), m_buffer, 1, &copyRegion);
}

void Buffer::cleanup() {
    if (m_buffer != VK_NULL_HANDLE && m_context) {
        vmaDestroyBuffer(m_context->getAllocator(), m_buffer, m_allocation);
        m_buffer = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
    }
}

} // namespace prismcraft
