#pragma once

#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <stdexcept>
#include <iostream>

#define VK_CHECK(result, msg) \
    do { \
        VkResult res = (result); \
        if (res != VK_SUCCESS) { \
            std::cerr << "Vulkan Error [" << res << "]: " << msg << std::endl; \
            throw std::runtime_error(msg); \
        } \
    } while(0)

namespace prismcraft {
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
}
