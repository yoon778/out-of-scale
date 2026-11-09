#pragma once

#include "../shared/BoxRenderer.h"
#include "../shared/PerspectiveCarry.h"

class MilitaryRoom {
public:
    enum class ExitState { Locked, Open, OutpostReached };

    bool initialize();
    void interact(const glm::vec3& eye, const glm::vec3& direction);
    glm::vec3 resolveMovement(const glm::vec3& previous, const glm::vec3& desired);
    ExitState exitState() const;
    void draw(const glm::mat4& viewProjection, const glm::vec3& eye, const glm::vec3& direction) const;

    static constexpr float halfSize = 20.0f;
    static constexpr glm::vec3 spawnEye{0.95f, 1.65f, -2.2f};
    static constexpr float spawnYaw = 1.5707963268f;

private:
    struct Placement {
        perspective::Pose pose;
        bool returned;
    };

    std::optional<Placement> placement(const glm::vec3& eye, const glm::vec3& direction) const;
    bool canStand(const glm::vec3& position) const;

    BoxRenderer renderer_;
    perspective::Pose barricade_{{6.32f, 0.76f, -2.2f}, 1.0f};
    float heldRatio_ = 0.0f;
    ExitState exitState_ = ExitState::Locked;
};
