#include "Player.h"

#include <GL/glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace {
constexpr float eyeHeight = 1.65f;
constexpr float walkSpeed = 4.0f;
constexpr float jumpSpeed = 4.8f;
constexpr float gravity = 9.8f;
constexpr float mouseSensitivity = 0.0025f;
constexpr glm::vec3 up{0.0f, 1.0f, 0.0f};
}

Player::Player(float floorHalfExtent) : floorHalfExtent_(floorHalfExtent) {}

glm::vec3 Player::horizontalForward() const {
    return {std::sin(yaw_), 0.0f, -std::cos(yaw_)};
}

void Player::look(float mouseDeltaX, float mouseDeltaY) {
    yaw_ += mouseDeltaX * mouseSensitivity;
    const float pitchLimit = glm::radians(89.0f);
    pitch_ = std::clamp(pitch_ - mouseDeltaY * mouseSensitivity, -pitchLimit, pitchLimit);
}

void Player::jump() {
    if (grounded_) {
        verticalSpeed_ = jumpSpeed;
        grounded_ = false;
    }
}

void Player::update(float forwardInput, float rightInput, float seconds) {
    const glm::vec3 forward = horizontalForward();
    const glm::vec3 right = glm::cross(forward, up);
    glm::vec3 movement = forward * forwardInput + right * rightInput;
    const float magnitude = glm::length(movement);
    movement /= std::max(1.0f, magnitude);
    feet_ += movement * walkSpeed * seconds;

    const float limit = floorHalfExtent_ - 0.25f;
    feet_.x = std::clamp(feet_.x, -limit, limit);
    feet_.z = std::clamp(feet_.z, -limit, limit);

    if (!grounded_) {
        feet_.y += (verticalSpeed_ - 0.5f * gravity * seconds) * seconds;
        verticalSpeed_ -= gravity * seconds;
        if (feet_.y <= 0.0f && verticalSpeed_ <= 0.0f) {
            feet_.y = 0.0f;
            verticalSpeed_ = 0.0f;
            grounded_ = true;
        }
    }
}

glm::mat4 Player::viewMatrix() const {
    const glm::vec3 eye = feet_ + up * eyeHeight;
    const glm::vec3 direction = horizontalForward() * std::cos(pitch_) + up * std::sin(pitch_);
    return glm::lookAt(eye, eye + direction, up);
}
