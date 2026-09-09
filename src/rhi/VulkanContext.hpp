#pragma once
#include "RHICommon.hpp"
#include <vector>
#include <string>

struct GLFWwindow;

namespace prismcraft {

class VulkanContext {
public:
    VulkanContext(GLFWwindow* window);
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    void waitIdle();

    [[nodiscard]] VkInstance getInstance() const { return m_instance; }
    [[nodiscard]] VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
    [[nodiscard]] VkDevice getDevice() const { return m_device; }
    [[nodiscard]] VkQueue getGraphicsQueue() const { return m_graphicsQueue; }
    [[nodiscard]] VkQueue getPresentQueue() const { return m_presentQueue; }
    [[nodiscard]] uint32_t getGraphicsQueueFamily() const { return m_graphicsQueueFamily; }
    [[nodiscard]] uint32_t getPresentQueueFamily() const { return m_presentQueueFamily; }
    [[nodiscard]] VkSurfaceKHR getSurface() const { return m_surface; }
    [[nodiscard]] VmaAllocator getAllocator() const { return m_allocator; }

private:
    void createInstance();
    void setupDebugMessenger();
    void createSurface(GLFWwindow* window);
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createAllocator();

    bool checkValidationLayerSupport();
    std::vector<const char*> getRequiredExtensions();

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueFamily = 0;
    uint32_t m_presentQueueFamily = 0;
    VmaAllocator m_allocator = VK_NULL_HANDLE;

    static constexpr bool ENABLE_VALIDATION = 
#ifdef NDEBUG
        false;
#else
        true;
#endif
    static constexpr const char* VALIDATION_LAYER = "VK_LAYER_KHRONOS_validation";
};

} // namespace prismcraft
