#pragma once

#include "../shared/BoxRenderer.h"
#include "ComputerScreen.h"

#include <array>
#include <optional>

class HomeRoom {
public:
    enum class ExitState { Explore, WallRevealed, WallBroken, DoorPlaced, DoorOpen, Escaped };

    bool initialize();
    void interact(const glm::vec3& eye, const glm::vec3& direction);
    void shoot(const glm::vec3& eye, const glm::vec3& direction);
    void scroll(double yOffset);
    void update(float elapsed, const glm::vec3& eye,
                const glm::vec3& direction = glm::vec3{0.0f, 0.0f, -1.0f});
    glm::vec3 resolveMovement(const glm::vec3& previous, const glm::vec3& desired, bool* grounded = nullptr);
    bool computerActive() const;
    void closeComputer();
    void computerInput(float horizontal, float vertical, float turn, float elapsed);
    ExitState exitState() const;
    void draw(const glm::mat4& viewProjection, const glm::vec3& eye, const glm::vec3& direction) const;

    static constexpr float halfSize = 12.0f;
    static constexpr glm::vec3 spawnEye{0.0f, 1.65f, 5.0f};
    static constexpr float spawnYaw = 0.0f;
    static constexpr glm::vec3 computerEye{3.25f, 1.85f, 3.0f};
    static constexpr glm::vec3 computerDirection{1.0f, 0.0f, 0.0f};

private:
    enum class HeldItem { None, Rpg, Door, Cube };

    struct ComputerCube {
        glm::vec3 center{0.0f};
        float edge = 0.60f;
        float yaw = 0.35f;
        float verticalSpeed = 0.0f;
    };

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
        bool miniature = false;
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
    bool canTakeShelvedDoor(const glm::vec3& eye, const glm::vec3& direction) const;
    bool canReachMiniatureLatch(const glm::vec3& eye, const glm::vec3& direction) const;
    void drawMiniature() const;
    void drawRoom(const glm::mat4& model, bool miniature) const;
    void impact(const Projectile& projectile);
    bool aimsAtComputer(const glm::vec3& eye, const glm::vec3& direction) const;
    bool cubeFits(const ComputerCube& cube, const glm::vec3& eye) const;
    std::optional<ComputerCube> carriedCube(const glm::vec3& eye, const glm::vec3& direction) const;
    void updateCube(float elapsed, const glm::vec3& eye, const glm::vec3& direction);

    BoxRenderer renderer_;
    ComputerScreen computer_;
    std::optional<ComputerCube> computerCube_;
    bool computerActive_ = false;
    bool standingOnCube_ = false;
    ExitState exitState_ = ExitState::Explore;
    HeldItem held_ = HeldItem::None;
    float shelfOffset_ = 0.0f;
    bool shelfMoving_ = false;
    float doorRatio_ = 0.0f;
    float doorWheelScale_ = 1.0f;
    bool latchReleased_ = false;
    float latchTravel_ = 0.0f;
    float doorRattleTime_ = 0.0f;
    std::array<Fragment, 18> fragments_{};
    float debrisTime_ = 0.0f;
    float shotFlashTime_ = 0.0f;
    std::optional<Projectile> projectile_;
    std::array<Particle, 36> particles_{};
    glm::vec3 impactCenter_{0.0f};
    float impactTime_ = 0.0f;
    float impactScale_ = 1.0f;
};
