#pragma once
#include <string>
#include <cstdint>
struct GLFWwindow;

namespace prismcraft {

class Window {
public:
    Window(const std::string& title, int width, int height);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    [[nodiscard]] bool shouldClose() const;
    void pollEvents();
    [[nodiscard]] GLFWwindow* getHandle() const;
    [[nodiscard]] int getWidth() const;
    [[nodiscard]] int getHeight() const;
    [[nodiscard]] bool wasResized() const;
    void resetResizedFlag();
    void setCursorMode(bool captured); // true = hidden+captured for FPS
    void setWindowMode(int mode, int width, int height, int refreshRate = 0);
    [[nodiscard]] int getRefreshRate() const;

private:
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    GLFWwindow* m_window = nullptr;
    int m_width;
    int m_height;
    bool m_framebufferResized = false;
};

} // namespace prismcraft
