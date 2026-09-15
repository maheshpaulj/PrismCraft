#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>

namespace prismcraft {

enum class CameraMode {
    FirstPerson,
    ThirdPersonBack,
    ThirdPersonFront,
    FreeCam
};

struct Frustum {
    std::array<glm::vec4, 6> planes; // left, right, bottom, top, near, far
};

class Camera {
public:
    Camera();
    
    void processMouseMovement(float xoffset, float yoffset);
    void processKeyboard(int direction, float deltaTime); // 0=fwd,1=back,2=left,3=right,4=up,5=down
    void moveFreeCam(const glm::vec3& delta) { m_position += delta; }
    
    [[nodiscard]] glm::mat4 getViewMatrix() const;
    [[nodiscard]] glm::mat4 getProjectionMatrix(float aspectRatio) const;
    [[nodiscard]] glm::mat4 getJitteredProjectionMatrix(float aspectRatio, const glm::vec2& jitterOffset, const glm::vec2& renderResolution) const;
    static glm::vec2 getHaltonJitter(int frameIndex, int sequenceLength = 16);
    [[nodiscard]] Frustum getFrustum(float aspectRatio) const;
    [[nodiscard]] glm::vec3 getPosition() const { return m_position; }
    [[nodiscard]] glm::vec3 getRenderPosition() const;
    [[nodiscard]] glm::vec3 getForward() const { return m_forward; }
    [[nodiscard]] glm::vec3 getRight() const { return m_right; }
    [[nodiscard]] glm::vec3 getUp() const { return m_up; }
    [[nodiscard]] float getYaw() const { return m_yaw; }
    [[nodiscard]] float getPitch() const { return m_pitch; }
    void setPitch(float pitch);
    void setYaw(float yaw);
    
    void setPosition(const glm::vec3& pos) { m_position = pos; }
    
    [[nodiscard]] CameraMode getMode() const { return m_mode; }
    void setMode(CameraMode mode) { m_mode = mode; }
    void cycleMode();
    void toggleFreeCam();

    float fov = 70.0f;
    float sensitivity = 0.010f; // 5x faster baseline sensitivity
    float speed = 8.0f;
    float nearPlane = 0.1f;
    float farPlane = 500.0f;

private:
    void updateVectors();
    
    CameraMode m_mode = CameraMode::FirstPerson;
    CameraMode m_prevMode = CameraMode::FirstPerson;
    glm::vec3 m_position{0.0f, 80.0f, 0.0f};
    glm::vec3 m_forward{0.0f, 0.0f, -1.0f};
    glm::vec3 m_right{1.0f, 0.0f, 0.0f};
    glm::vec3 m_up{0.0f, 1.0f, 0.0f};
    float m_yaw = -glm::half_pi<float>();
    float m_pitch = 0.0f;
};

} // namespace prismcraft
