#pragma once

#include "../shared/BoxRenderer.h"
#include "../shared/PerspectiveCarry.h"

#include <array>
#include <cstddef>
#include <optional>

class ParadeGround {
public:
    bool initialize();
    void interact(const glm::vec3& eye, const glm::vec3& direction);
    void update(float elapsed, const glm::vec3& playerEye);
    void scroll(double yOffset);
    void cancelCarry();
    glm::vec3 resolveMovement(const glm::vec3& previous, const glm::vec3& desired, bool* grounded);
    bool reachedExit() const;
    void draw(const glm::mat4& viewProjection, const glm::vec3& eye, const glm::vec3& direction) const;

private:
    struct Prop {
        perspective::Pose pose;
        float verticalSpeed = 0.0f;
    };

    std::optional<perspective::Pose> placement(const glm::vec3& eye, const glm::vec3& direction) const;

    BoxRenderer renderer_;
    std::array<Prop, 3> props_{{
        {{glm::vec3{7.8f, 0.20f, 18.5f}, 1.0f}},
        {{glm::vec3{8.9f, 0.275f, 18.5f}, 1.0f}},
        {{glm::vec3{10.0f, 0.375f, 18.5f}, 1.0f}},
    }};
    std::optional<std::size_t> held_;
    float heldRatio_ = 0.0f;
    float reach_ = 10.0f;
    glm::vec3 lastPlayerEye_{};
    bool reachedExit_ = false;
};
