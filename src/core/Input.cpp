#include "Input.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>

namespace prismcraft {

GLFWwindow* Input::s_window = nullptr;
glm::vec2 Input::s_mouseDelta = {0.0f, 0.0f};
glm::vec2 Input::s_mousePos = {0.0f, 0.0f};
glm::vec2 Input::s_lastMousePos = {0.0f, 0.0f};
float Input::s_scrollDelta = 0.0f;
bool Input::s_firstMouse = true;
std::string Input::s_typedChars = "";
std::array<bool, 512> Input::s_currentKeys = {false};
std::array<bool, 512> Input::s_previousKeys = {false};
std::array<bool, 8> Input::s_currentButtons = {false};
std::array<bool, 8> Input::s_previousButtons = {false};

void Input::init(GLFWwindow* window) {
    s_window = window;
    
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCharCallback(window, charCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetScrollCallback(window, scrollCallback);

    s_currentKeys.fill(false);
    s_previousKeys.fill(false);
    s_currentButtons.fill(false);
    s_previousButtons.fill(false);
    s_firstMouse = true;
    s_mouseDelta = {0.0f, 0.0f};
    s_scrollDelta = 0.0f;
    s_typedChars.clear();
}

void Input::update() {
    s_previousKeys = s_currentKeys;
    s_previousButtons = s_currentButtons;
    s_mouseDelta = {0.0f, 0.0f};
    s_scrollDelta = 0.0f;
    s_typedChars.clear();
}

bool Input::isKeyDown(int key) {
    if (key >= 0 && key < 512) {
        return s_currentKeys[key];
    }
    return false;
}

bool Input::isKeyPressed(int key) {
    if (key >= 0 && key < 512) {
        return s_currentKeys[key] && !s_previousKeys[key];
    }
    return false;
}

bool Input::isMouseButtonDown(int button) {
    if (button >= 0 && button < 8) {
        return s_currentButtons[button];
    }
    return false;
}

bool Input::isMouseButtonPressed(int button) {
    if (button >= 0 && button < 8) {
        return s_currentButtons[button] && !s_previousButtons[button];
    }
    return false;
}

glm::vec2 Input::getMouseDelta() {
    return s_mouseDelta;
}

glm::vec2 Input::getMousePosition() {
    return s_mousePos;
}

float Input::getScrollDelta() {
    return s_scrollDelta;
}

void Input::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)window;
    (void)scancode;
    (void)mods;
    if (key >= 0 && key < 512) {
        if (action == GLFW_PRESS) {
            s_currentKeys[key] = true;
        } else if (action == GLFW_RELEASE) {
            s_currentKeys[key] = false;
        }
    }
}

void Input::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    (void)window;
    (void)mods;
    if (button >= 0 && button < 8) {
        if (action == GLFW_PRESS) {
            s_currentButtons[button] = true;
        } else if (action == GLFW_RELEASE) {
            s_currentButtons[button] = false;
        }
    }
}

void Input::cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    (void)window;
    float x = static_cast<float>(xpos);
    float y = static_cast<float>(ypos);

    if (s_firstMouse) {
        s_lastMousePos = {x, y};
        s_firstMouse = false;
    }

    s_mouseDelta = {x - s_lastMousePos.x, y - s_lastMousePos.y};
    s_lastMousePos = {x, y};
    s_mousePos = {x, y};
}

void Input::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)window;
    (void)xoffset;
    s_scrollDelta = static_cast<float>(yoffset);
}

void Input::charCallback(GLFWwindow* window, unsigned int codepoint) {
    (void)window;
    if (codepoint >= 32 && codepoint <= 126) {
        s_typedChars += static_cast<char>(codepoint);
    }
}

const std::string& Input::getTypedChars() {
    return s_typedChars;
}

} // namespace prismcraft
