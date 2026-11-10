#pragma once

#include "../shared/BoxRenderer.h"
#include "../shared/PerspectiveCarry.h"

#include <vector>

class MilitaryRoom {
public:
    enum class ExitState { Locked, Open, OutpostReached, BridgeReady, Escaped };

    bool initialize();
    void interact(const glm::vec3& eye, const glm::vec3& direction);
    void scroll(double yOffset);
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
    void interactMoon(const glm::vec3& eye, const glm::vec3& direction);
    bool canPlaceMoon(const glm::vec3& center) const;
    void updateShadow(const glm::vec3& light);
    bool supports(const glm::vec2& point) const;
    bool canStand(const glm::vec3& position) const;

    BoxRenderer renderer_;
    perspective::Pose barricade_{{6.32f, 0.76f, -2.2f}, 1.0f};
    float heldRatio_ = 0.0f;
    ExitState exitState_ = ExitState::Locked;
    std::optional<glm::vec3> placedMoon_;
    float moonScale_ = 0.10f;
    bool moonHeld_ = false;
    std::vector<glm::vec2> shadow_;
    std::vector<glm::vec2> trenchShadow_;
    std::vector<glm::vec2> bridge_;
};
