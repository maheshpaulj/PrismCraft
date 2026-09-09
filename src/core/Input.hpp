#pragma once
#include <array>
#include <string>
#include <glm/vec2.hpp>
struct GLFWwindow;

namespace prismcraft {

class Input {
public:
    static void init(GLFWwindow* window);
    static void update(); // Call once per frame BEFORE polling events

    [[nodiscard]] static bool isKeyDown(int key);
    [[nodiscard]] static bool isKeyPressed(int key); // Just pressed this frame
    [[nodiscard]] static bool isMouseButtonDown(int button);
    [[nodiscard]] static bool isMouseButtonPressed(int button);
    [[nodiscard]] static glm::vec2 getMouseDelta();
    [[nodiscard]] static glm::vec2 getMousePosition();
    [[nodiscard]] static float getScrollDelta();
    [[nodiscard]] static const std::string& getTypedChars();

private:
    static void charCallback(GLFWwindow* window, unsigned int codepoint);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    static GLFWwindow* s_window;
    static glm::vec2 s_mouseDelta;
    static glm::vec2 s_mousePos;
    static glm::vec2 s_lastMousePos;
    static float s_scrollDelta;
    static bool s_firstMouse;
    static std::string s_typedChars;
    static std::array<bool, 512> s_currentKeys;
    static std::array<bool, 512> s_previousKeys;
    static std::array<bool, 8> s_currentButtons;
    static std::array<bool, 8> s_previousButtons;
};

} // namespace prismcraft
