#pragma once

#include <GL/glm/glm.hpp>

class Player {
public:
    explicit Player(float floorHalfExtent);

    void look(float mouseDeltaX, float mouseDeltaY);
    void jump();
    void update(float forwardInput, float rightInput, float seconds);
    glm::mat4 viewMatrix() const;

private:
    glm::vec3 horizontalForward() const;

    glm::vec3 feet_{0.0f};
    float floorHalfExtent_;
    float yaw_ = 0.0f;
    float pitch_ = glm::radians(-12.0f);
    float verticalSpeed_ = 0.0f;
    bool grounded_ = true;
};
