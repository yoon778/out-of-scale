#pragma once

#include "../shared/BoxRenderer.h"
#include "../shared/PerspectiveCarry.h"

#include <array>
#include <vector>

class MilitaryRoom {
public:
    enum class ExitState { Locked, Open, OutpostReached, BridgeReady, Escaped };

    MilitaryRoom();
    bool initialize();
    void interact(const glm::vec3& eye, const glm::vec3& direction);
    void update(float elapsed, const glm::vec3& playerEye);
    void scroll(double yOffset);
    glm::vec3 resolveMovement(const glm::vec3& previous, const glm::vec3& desired,
                              const glm::vec3& facing = {1.0f, 0.0f, 0.0f}, bool* grounded = nullptr);
    ExitState exitState() const;
    void draw(const glm::mat4& viewProjection, const glm::vec3& eye, const glm::vec3& direction) const;

    static constexpr float halfSize = 20.0f;
    static constexpr glm::vec3 spawnEye{0.95f, 1.65f, -3.60f};
    static constexpr float spawnYaw = 1.5707963268f;

private:
    struct Placement {
        perspective::Pose pose;
        bool fits = true;
        bool returning = false;
    };

    static constexpr std::size_t belongingsCount = 3;
    static constexpr std::size_t televisionIndex = belongingsCount;
    bool inBarracks() const;
    std::optional<Placement> placement(const glm::vec3& eye, const glm::vec3& direction) const;
    bool canEnterTelevision(const glm::vec3& previous, const glm::vec3& desired,
                            const glm::vec3& facing) const;
    void drawBarracks(const glm::vec3& eye, const glm::vec3& direction) const;
    void drawObject(std::size_t index, const perspective::Pose& pose, const glm::vec3& tint) const;
    void interactMoon(const glm::vec3& eye, const glm::vec3& direction);
    bool canPlaceMoon(const glm::vec3& center) const;
    void updateShadow(const glm::vec3& light);
    bool supports(const glm::vec2& point) const;
    bool canStandAtOutpost(const glm::vec3& position) const;

    BoxRenderer renderer_;
    std::array<perspective::Pose, belongingsCount + 1> objects_;
    std::array<float, belongingsCount + 1> verticalSpeeds_{};
    std::array<float, belongingsCount + 1> fallGrowthRates_{};
    std::array<bool, belongingsCount> returned_{};
    std::optional<std::size_t> heldObject_;
    float heldRatio_ = 0.0f;
    ExitState exitState_ = ExitState::Locked;
    std::optional<glm::vec3> placedMoon_;
    float moonScale_ = 0.10f;
    bool moonHeld_ = false;
    std::vector<glm::vec2> shadow_;
    std::vector<glm::vec2> trenchShadow_;
    std::vector<glm::vec2> bridge_;
};
