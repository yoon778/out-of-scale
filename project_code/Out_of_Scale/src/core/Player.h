#pragma once

#include <GL/glm/glm.hpp>

struct GLFWwindow;

class Player {
public:
    Player();

    void look(double x, double y);
    void resetMouseSample();
    void jump();
    void update(GLFWwindow* window, float elapsed, float floorHalfSize);
    void setHorizontalPosition(const glm::vec3& position);
    glm::vec3 eyePosition() const;
    glm::vec3 viewDirection() const;
    glm::mat4 view() const;

private:
    glm::vec3 forwardOnFloor() const;
    void move(GLFWwindow* window, float elapsed, float floorHalfSize);
    void updateJump(float elapsed);

    glm::vec3 eye;
    float yaw = 0.0f;
    float pitch;
    float verticalSpeed = 0.0f;
    bool grounded = true;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    bool hasMouseSample = false;
};
