#pragma once

#include "../shared/BoxRenderer.h"

#include <array>
#include <optional>

class HomeRoom {
public:
    enum class ExitState { Explore, WallRevealed, WallBroken, DoorPlaced, DoorOpen, Escaped };

    bool initialize();
    void interact(const glm::vec3& eye, const glm::vec3& direction);
    void shoot(const glm::vec3& eye, const glm::vec3& direction);
    void scroll(double yOffset);
    void update(float elapsed, const glm::vec3& eye);
    glm::vec3 resolveMovement(const glm::vec3& previous, const glm::vec3& desired);
    ExitState exitState() const;
    void draw(const glm::mat4& viewProjection, const glm::vec3& eye, const glm::vec3& direction) const;

    static constexpr float halfSize = 12.0f;
    static constexpr glm::vec3 spawnEye{0.0f, 1.65f, 5.0f};
    static constexpr float spawnYaw = 0.0f;

private:
    enum class HeldItem { None, Rpg, Door };

    struct Fragment {
        glm::vec3 center;
        glm::vec3 velocity;
        glm::vec3 rotationAxis;
        float angle;
        float angularSpeed;
    };

    struct Projectile {
        glm::vec3 center;
        glm::vec3 direction;
        float remaining;
        bool breaksWall;
        float traveled = 0.0f;
    };

    struct Particle {
        glm::vec3 center;
        glm::vec3 velocity;
        float life = 0.0f;
        float size = 0.0f;
        bool spark = false;
    };

    float visibleRange(const glm::vec3& eye, const glm::vec3& direction, float maxDistance) const;
    bool canFitDoor(const glm::vec3& eye, const glm::vec3& direction) const;
    void drawMiniature() const;
    void drawRoom(const glm::mat4& model, bool miniature) const;
    void impact(const Projectile& projectile);

    BoxRenderer renderer_;
    ExitState exitState_ = ExitState::Explore;
    HeldItem held_ = HeldItem::None;
    float shelfOffset_ = 0.0f;
    bool shelfMoving_ = false;
    float doorRatio_ = 0.0f;
    float doorWheelScale_ = 1.0f;
    std::array<Fragment, 18> fragments_{};
    float debrisTime_ = 0.0f;
    float shotFlashTime_ = 0.0f;
    std::optional<Projectile> projectile_;
    std::array<Particle, 36> particles_{};
    glm::vec3 impactCenter_{0.0f};
    float impactTime_ = 0.0f;
};
