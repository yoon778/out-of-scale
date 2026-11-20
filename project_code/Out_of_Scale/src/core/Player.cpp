#include "Player.h"

#include <GL/glfw3.h>
#include <GL/glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace {
constexpr float walkSpeed = 4.0f;
constexpr float jumpSpeed = 4.8f;
constexpr float gravity = 9.8f;
constexpr float mouseSensitivity = 0.0025f;
constexpr glm::vec3 worldUp{0.0f, 1.0f, 0.0f};
}

Player::Player() : Player({0.0f, eyeHeight, 0.0f}, 0.0f) {}

Player::Player(const glm::vec3& spawnEye, float spawnYaw)
    : eye{spawnEye}, yaw{spawnYaw}, pitch{glm::radians(-12.0f)} {}

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
    horizontalVelocity = {};
}

void Player::jump() {
    if (grounded) {
        verticalSpeed = jumpSpeed;
        grounded = false;
    }
}

void Player::update(GLFWwindow* window, float elapsed, float floorHalfSize, bool roomControlsHeight) {
    if (elapsed <= 0.0f)
        return;
    if (glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE) {
        move(window, elapsed, floorHalfSize);
    } else {
        horizontalVelocity = {};
    }
    updateJump(elapsed, roomControlsHeight);
}

void Player::setHorizontalPosition(const glm::vec3& position) {
    // Remove only the velocity rejected by the room, preserving wall sliding.
    for (int axis : {0, 2}) {
        if (std::abs(position[axis] - eye[axis]) > 0.00001f)
            horizontalVelocity[axis] = 0.0f;
    }
    eye.x = position.x;
    eye.z = position.z;
}

void Player::setRoomPosition(const glm::vec3& position, bool roomGrounded) {
    setHorizontalPosition(position);
    if (roomGrounded || (verticalSpeed > 0.0f && position.y < eye.y)) {
        verticalSpeed = 0.0f;
    }
    eye = position;
    grounded = roomGrounded;
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
    const glm::vec3 target = movement * walkSpeed;
    const float response = glm::length(movement) > 0.001f ? 26.0f : 42.0f;
    const float blend = -std::expm1(-response * elapsed);
    // Integrate the exponential velocity, so distance as well as speed is FPS-independent.
    eye += target * elapsed + (horizontalVelocity - target) * (blend / response);
    horizontalVelocity += (target - horizontalVelocity) * blend;
    if (glm::length(target) == 0.0f && glm::length(horizontalVelocity) < 0.001f)
        horizontalVelocity = {};

    const float edge = floorHalfSize - 0.25f;
    setHorizontalPosition({std::clamp(eye.x, -edge, edge), eye.y, std::clamp(eye.z, -edge, edge)});
}

void Player::updateJump(float elapsed, bool roomControlsHeight) {
    if (grounded && !roomControlsHeight) {
        return;
    }

    eye.y += (verticalSpeed - 0.5f * gravity * elapsed) * elapsed;
    verticalSpeed -= gravity * elapsed;
    if (!roomControlsHeight && verticalSpeed <= 0.0f && eye.y <= eyeHeight) {
        eye.y = eyeHeight;
        verticalSpeed = 0.0f;
        grounded = true;
    }
}
