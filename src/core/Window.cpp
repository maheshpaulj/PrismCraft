#include "Window.hpp"
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <iostream>

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
        // Borderless Fullscreen
        int monX = 0, monY = 0;
        glfwGetMonitorPos(monitor, &monX, &monY);
        glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_FALSE);
        glfwSetWindowMonitor(m_window, nullptr, monX, monY, vidmode->width, vidmode->height, GLFW_DONT_CARE);
        m_width = vidmode->width;
        m_height = vidmode->height;
    } else if (mode == 2) {
        // Exclusive Fullscreen
        int hz = refreshRate > 0 ? refreshRate : vidmode->refreshRate;
        glfwSetWindowMonitor(m_window, monitor, 0, 0, width, height, hz);
        m_width = width;
        m_height = height;
    } else {
        // Windowed
        glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_TRUE);
        int posX = (vidmode->width - width) / 2;
        int posY = (vidmode->height - height) / 2;
        if (posX < 0) posX = 50;
        if (posY < 0) posY = 50;
        glfwSetWindowMonitor(m_window, nullptr, posX, posY, width, height, GLFW_DONT_CARE);
        m_width = width;
        m_height = height;
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
    auto app = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    app->m_width = width;
    app->m_height = height;
    app->m_framebufferResized = true;
}

} // namespace prismcraft
