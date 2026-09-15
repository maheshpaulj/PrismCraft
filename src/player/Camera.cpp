#include "Camera.hpp"
#include <algorithm>
#include <cmath>

namespace prismcraft {

Camera::Camera() {
    updateVectors();
}

void Camera::processMouseMovement(float xoffset, float yoffset) {
    m_yaw += xoffset * sensitivity;
    m_pitch -= yoffset * sensitivity;
    
    float maxPitch = 1.55f;
    m_pitch = std::clamp(m_pitch, -maxPitch, maxPitch);
    
    updateVectors();
}

void Camera::setPitch(float pitch) {
    float maxPitch = 1.55f;
    m_pitch = std::clamp(pitch, -maxPitch, maxPitch);
    updateVectors();
}

void Camera::setYaw(float yaw) {
    m_yaw = yaw;
    updateVectors();
}

void Camera::processKeyboard(int direction, float deltaTime) {
    float velocity = speed * deltaTime;
    glm::vec3 horizForward = glm::normalize(glm::vec3(m_forward.x, 0.0f, m_forward.z));
    
    switch(direction) {
        case 0: // fwd
            m_position += horizForward * velocity;
            break;
        case 1: // back
            m_position -= horizForward * velocity;
            break;
        case 2: // left
            m_position -= m_right * velocity;
            break;
        case 3: // right
            m_position += m_right * velocity;
            break;
        case 4: // up
            m_position += glm::vec3(0.0f, 1.0f, 0.0f) * velocity;
            break;
        case 5: // down
            m_position -= glm::vec3(0.0f, 1.0f, 0.0f) * velocity;
            break;
    }
}

void Camera::cycleMode() {
    if (m_mode == CameraMode::FirstPerson) m_mode = CameraMode::ThirdPersonBack;
    else if (m_mode == CameraMode::ThirdPersonBack) m_mode = CameraMode::ThirdPersonFront;
    else m_mode = CameraMode::FirstPerson;
}

void Camera::toggleFreeCam() {
    if (m_mode == CameraMode::FreeCam) {
        m_mode = m_prevMode;
    } else {
        m_prevMode = m_mode;
        m_mode = CameraMode::FreeCam;
    }
}

glm::vec3 Camera::getRenderPosition() const {
    float dist = 3.5f;
    if (m_mode == CameraMode::ThirdPersonBack) {
        return m_position - m_forward * dist + glm::vec3(0.0f, 0.4f, 0.0f);
    } else if (m_mode == CameraMode::ThirdPersonFront) {
        return m_position + m_forward * dist + glm::vec3(0.0f, 0.4f, 0.0f);
    }
    return m_position;
}

glm::mat4 Camera::getViewMatrix() const {
    glm::vec3 eye = getRenderPosition();
    if (m_mode == CameraMode::ThirdPersonBack) {
        return glm::lookAt(eye, m_position + glm::vec3(0.0f, 0.2f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    } else if (m_mode == CameraMode::ThirdPersonFront) {
        return glm::lookAt(eye, m_position + glm::vec3(0.0f, 0.2f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    }
    return glm::lookAt(m_position, m_position + m_forward, glm::vec3(0.0f, 1.0f, 0.0f));
}

static float haltonRadicalInverse(int index, int base) {
    float result = 0.0f;
    float f = 1.0f / static_cast<float>(base);
    int i = index;
    while (i > 0) {
        result += f * static_cast<float>(i % base);
        i /= base;
        f /= static_cast<float>(base);
    }
    return result;
}

glm::vec2 Camera::getHaltonJitter(int frameIndex, int sequenceLength) {
    int idx = (std::abs(frameIndex) % sequenceLength) + 1;
    float hX = haltonRadicalInverse(idx, 2) - 0.5f;
    float hY = haltonRadicalInverse(idx, 3) - 0.5f;
    return glm::vec2(hX, hY);
}

glm::mat4 Camera::getProjectionMatrix(float aspectRatio) const {
    return glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
}

glm::mat4 Camera::getJitteredProjectionMatrix(float aspectRatio, const glm::vec2& jitterOffset, const glm::vec2& renderResolution) const {
    glm::mat4 proj = getProjectionMatrix(aspectRatio);
    if (renderResolution.x > 0.0f && renderResolution.y > 0.0f) {
        proj[2][0] += (2.0f * jitterOffset.x) / renderResolution.x;
        proj[2][1] += (2.0f * jitterOffset.y) / renderResolution.y;
    }
    return proj;
}

Frustum Camera::getFrustum(float aspectRatio) const {
    Frustum frustum;
    glm::mat4 vp = getProjectionMatrix(aspectRatio) * getViewMatrix();
    
    // Gribb-Hartmann method for row extraction from column-major GLM matrix
    glm::mat4 m = glm::transpose(vp);
    frustum.planes[0] = m[3] + m[0]; // Left:   w + x >= 0
    frustum.planes[1] = m[3] - m[0]; // Right:  w - x >= 0
    frustum.planes[2] = m[3] + m[1]; // Bottom: w + y >= 0
    frustum.planes[3] = m[3] - m[1]; // Top:    w - y >= 0
    frustum.planes[4] = m[2];        // Near:   z >= 0 (Vulkan [0, 1] depth range)
    frustum.planes[5] = m[3] - m[2]; // Far:    w - z >= 0

    for (auto& plane : frustum.planes) {
        float length = glm::length(glm::vec3(plane));
        if (length > 0.0001f) {
            plane /= length;
        }
    }
    
    return frustum;
}

void Camera::updateVectors() {
    glm::vec3 front;
    front.x = std::cos(m_yaw) * std::cos(m_pitch);
    front.y = std::sin(m_pitch);
    front.z = std::sin(m_yaw) * std::cos(m_pitch);
    m_forward = glm::normalize(front);
    
    m_right = glm::normalize(glm::cross(m_forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    m_up = glm::normalize(glm::cross(m_right, m_forward));
}

} // namespace prismcraft
