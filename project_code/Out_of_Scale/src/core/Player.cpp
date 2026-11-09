#include "Player.h"

#include <GL/glfw3.h>
#include <GL/glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace {
constexpr float walkSpeed = 4.0f;
constexpr float eyeHeight = 1.65f;
constexpr float jumpSpeed = 4.8f;
constexpr float gravity = 9.8f;
constexpr float mouseSensitivity = 0.0025f;
constexpr glm::vec3 worldUp{0.0f, 1.0f, 0.0f};
}

Player::Player() : eye{0.0f, eyeHeight, 0.0f}, pitch{glm::radians(-12.0f)} {}

void Player::look(double x, double y) {
    if (hasMouseSample) {
        yaw += static_cast<float>(x - lastMouseX) * mouseSensitivity;
        const float limit = glm::radians(89.0f);
        const float dy = static_cast<float>(y - lastMouseY);
        pitch = std::clamp(pitch - dy * mouseSensitivity, -limit, limit);
    }
    lastMouseX = x;
    lastMouseY = y;
    hasMouseSample = true;
}

void Player::resetMouseSample() {
    hasMouseSample = false;
}

void Player::jump() {
    if (grounded) {
        verticalSpeed = jumpSpeed;
        grounded = false;
    }
}

void Player::update(GLFWwindow* window, float elapsed, float floorHalfSize) {
    if (glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE) {
        move(window, elapsed, floorHalfSize);
    }
    updateJump(elapsed);
}

void Player::setHorizontalPosition(const glm::vec3& position) {
    eye.x = position.x;
    eye.z = position.z;
}

glm::vec3 Player::eyePosition() const {
    return eye;
}

glm::vec3 Player::viewDirection() const {
    return forwardOnFloor() * std::cos(pitch) + worldUp * std::sin(pitch);
}

glm::mat4 Player::view() const {
    return glm::lookAt(eye, eye + viewDirection(), worldUp);
}

glm::vec3 Player::forwardOnFloor() const {
    return {std::sin(yaw), 0.0f, -std::cos(yaw)};
}

void Player::move(GLFWwindow* window, float elapsed, float floorHalfSize) {
    const int forward = glfwGetKey(window, GLFW_KEY_W) - glfwGetKey(window, GLFW_KEY_S);
    const int right = glfwGetKey(window, GLFW_KEY_D) - glfwGetKey(window, GLFW_KEY_A);
    const glm::vec3 facing = forwardOnFloor();
    const glm::vec3 sideways = glm::cross(facing, worldUp);
    glm::vec3 movement = facing * static_cast<float>(forward) + sideways * static_cast<float>(right);
    movement /= std::max(1.0f, glm::length(movement));
    eye += movement * (walkSpeed * elapsed);

    const float edge = floorHalfSize - 0.25f;
    eye.x = std::clamp(eye.x, -edge, edge);
    eye.z = std::clamp(eye.z, -edge, edge);
}

void Player::updateJump(float elapsed) {
    if (grounded) {
        return;
    }

    eye.y += (verticalSpeed - 0.5f * gravity * elapsed) * elapsed;
    verticalSpeed -= gravity * elapsed;
    if (verticalSpeed <= 0.0f && eye.y <= eyeHeight) {
        eye.y = eyeHeight;
        verticalSpeed = 0.0f;
        grounded = true;
    }
}
