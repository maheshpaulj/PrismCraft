#include "Window.hpp"
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <iostream>
#include <cmath>

namespace prismcraft {

Window::Window(const std::string& title, int width, int height)
    : m_width(width), m_height(height) {
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    
    m_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!m_window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
    glfwSetWindowIconifyCallback(m_window, windowIconifyCallback);
    glfwSetWindowFocusCallback(m_window, windowFocusCallback);

    setCursorMode(true);
}

Window::~Window() {
    if (m_window) {
        glfwDestroyWindow(m_window);
    }
    glfwTerminate();
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(m_window);
}

void Window::pollEvents() {
    glfwPollEvents();
}

GLFWwindow* Window::getHandle() const {
    return m_window;
}

int Window::getWidth() const {
    return m_width;
}

int Window::getHeight() const {
    return m_height;
}

bool Window::wasResized() const {
    return m_framebufferResized;
}

void Window::resetResizedFlag() {
    m_framebufferResized = false;
}

void Window::setCursorMode(bool captured) {
    if (captured) {
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        if (glfwRawMouseMotionSupported()) {
            glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }
    } else {
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        if (glfwRawMouseMotionSupported()) {
            glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
        }
    }
}
 
void Window::setWindowMode(int mode, int width, int height, int refreshRate) {
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* vidmode = monitor ? glfwGetVideoMode(monitor) : nullptr;
    if (!vidmode) return;

    if (mode == 1) {
        // Borderless Fullscreen (Native desktop dimensions, borderless window)
        int monX = 0, monY = 0;
        glfwGetMonitorPos(monitor, &monX, &monY);
        glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_FALSE);
        glfwSetWindowMonitor(m_window, nullptr, monX, monY, vidmode->width, vidmode->height, GLFW_DONT_CARE);
        m_width = vidmode->width;
        m_height = vidmode->height;
    } else if (mode == 2) {
        // Exclusive Fullscreen (Changes GPU display mode to target resolution & best refresh rate)
        int count = 0;
        const GLFWvidmode* modes = glfwGetVideoModes(monitor, &count);
        const GLFWvidmode* bestMode = vidmode;

        int targetW = (width > 0) ? width : vidmode->width;
        int targetH = (height > 0) ? height : vidmode->height;
        int targetHz = (refreshRate > 0) ? refreshRate : vidmode->refreshRate;

        if (modes && count > 0) {
            int bestScore = -1000000;
            for (int i = 0; i < count; ++i) {
                int score = 0;
                if (modes[i].width == targetW && modes[i].height == targetH) {
                    score += 10000;
                } else {
                    score -= (std::abs(modes[i].width - targetW) + std::abs(modes[i].height - targetH)) * 10;
                }

                if (modes[i].refreshRate == targetHz) {
                    score += 1000;
                } else {
                    score -= std::abs(modes[i].refreshRate - targetHz) * 10;
                }

                if (score > bestScore) {
                    bestScore = score;
                    bestMode = &modes[i];
                }
            }
        }

        glfwSetWindowMonitor(m_window, monitor, 0, 0, bestMode->width, bestMode->height, bestMode->refreshRate);
        m_width = bestMode->width;
        m_height = bestMode->height;
    } else {
        // Windowed Mode (Resizes window client area and centers on desktop)
        int targetW = (width > 0) ? width : 1280;
        int targetH = (height > 0) ? height : 720;
        glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_TRUE);
        glfwSetWindowMonitor(m_window, nullptr, 0, 0, targetW, targetH, GLFW_DONT_CARE);
        glfwSetWindowSize(m_window, targetW, targetH);

        int posX = (vidmode->width - targetW) / 2;
        int posY = (vidmode->height - targetH) / 2;
        if (posX < 0) posX = 40;
        if (posY < 0) posY = 40;
        glfwSetWindowPos(m_window, posX, posY);

        m_width = targetW;
        m_height = targetH;
    }

    // Query physical swapchain framebuffer size in pixels
    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(m_window, &fbW, &fbH);
    if (fbW > 0 && fbH > 0) {
        m_width = fbW;
        m_height = fbH;
    }
    m_framebufferResized = true;
}

int Window::getRefreshRate() const {
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (monitor) {
        const GLFWvidmode* vidmode = glfwGetVideoMode(monitor);
        if (vidmode) return vidmode->refreshRate;
    }
    return 60;
}

void Window::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    if (width <= 0 || height <= 0) return;
    auto app = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!app) return;
    if (app->m_width == width && app->m_height == height) return;
    app->m_width = width;
    app->m_height = height;
    app->m_framebufferResized = true;
}

void Window::windowIconifyCallback(GLFWwindow* window, int iconified) {
    auto app = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!app) return;
    app->m_iconified = (iconified != 0);
    if (!app->m_iconified) {
        // Restored from minimized state: query current framebuffer size
        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        if (fbW > 0 && fbH > 0) {
            app->m_width = fbW;
            app->m_height = fbH;
            app->m_framebufferResized = true;
        }
    }
}

void Window::windowFocusCallback(GLFWwindow* window, int focused) {
    auto app = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!app) return;
    app->m_focused = (focused != 0);
}

} // namespace prismcraft
