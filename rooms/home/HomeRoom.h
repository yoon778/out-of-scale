#pragma once

#include "../shared/BoxRenderer.h"
#include "ComputerScreen.h"
#include "HomeEnding.h"
#include "HomeLift.h"

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
    void closeComputer();
    void computerKey(int key, int action, int mods);
    void computerClearInput();
    ExitState exitState() const;
    void draw(const glm::mat4& viewProjection, const glm::vec3& eye, const glm::vec3& direction) const;
    void drawEndingOverlay() const;
    float endingFade() const { return ending_.fade(); }
    bool isCarrying() const { return held_ != HeldItem::None; }
    float bookcaseSoundGain() const { return bookcaseSoundGain_; }
    float liftSoundGain() const { return liftSoundGain_; }

    static constexpr float halfSize = 20.0f;
    static constexpr glm::vec3 spawnEye{0.0f, 1.65f, 5.0f};
    static constexpr float spawnYaw = 0.0f;

private:
    enum class HeldItem { None, Rpg, Door, Cube, Key, Portable };

    struct PortableObject {
        glm::vec3 center{0.0f};
        float yaw = 0.0f;
        float verticalSpeed = 0.0f;
        bool active = true;
        bool loose = false;
    };

    struct KeyPiece {
        glm::vec3 center{0.0f};
        float yaw = 0.0f;
        float verticalSpeed = 0.0f;
    };

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
        glm::vec3 sourceImpact{0.0f};
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
    void notifyDoorFit();
    bool canTakeShelvedDoor(const glm::vec3& eye, const glm::vec3& direction) const;
    bool canReachMiniatureLatch(const glm::vec3& eye, const glm::vec3& direction) const;
    bool canReachMiniatureShelf(const glm::vec3& eye, const glm::vec3& direction) const;
    bool canTakeMiniatureRpg(const glm::vec3& eye, const glm::vec3& direction) const;
    bool canReachLiftLever(const glm::vec3& eye, const glm::vec3& direction) const;
    void updateLift(float elapsed, const glm::vec3& eye);
    void drawMiniature() const;
    void drawRoom(const glm::mat4& model, bool miniature) const;
    void impact(const Projectile& projectile);
    std::optional<glm::vec2> computerUv(const glm::vec3& eye, const glm::vec3& direction) const;
    bool cubeFits(const ComputerCube& cube, const glm::vec3& eye) const;
    std::optional<ComputerCube> carriedCube(const glm::vec3& eye, const glm::vec3& direction) const;
    void updateCube(float elapsed, const glm::vec3& eye, const glm::vec3& direction);
    bool keyFits(const KeyPiece& key, const glm::vec3& eye) const;
    std::optional<KeyPiece> carriedKey(const glm::vec3& eye, const glm::vec3& direction) const;
    void updateKey(float elapsed, const glm::vec3& eye, const glm::vec3& direction);
    void updatePicture(float elapsed, const glm::vec3& eye, const glm::vec3& direction);
    void drawPicture(const glm::mat4& model) const;
    bool portableFits(int index, const PortableObject& object, const glm::vec3& eye) const;
    std::optional<PortableObject> carriedPortable(int index, const glm::vec3& eye, const glm::vec3& direction) const;
    void updatePortables(float elapsed, const glm::vec3& eye, const glm::vec3& direction);
    bool grabPortable(const glm::vec3& eye, const glm::vec3& direction);
    void updateDoors(float elapsed, const glm::vec3& eye);
    bool swingBlocked(const glm::vec3& center, const glm::vec3& size,
                      const glm::vec3& eye, bool cabinet) const;
    glm::mat4 portableModel(int index) const;
    glm::mat4 homeModel() const;
    float homeRay(const glm::vec3& eye, const glm::vec3& direction,
                  const glm::vec3& center, const glm::vec3& size, float limit) const;
    float portableVisibility(const glm::vec3& eye, const glm::vec3& direction, float limit, int ignore = -1) const;
    bool overlapsPortable(const glm::vec3& center, const glm::vec3& size, float yaw, int ignore = -1) const;

    BoxRenderer renderer_;
    HomeEnding ending_;
    ComputerScreen computer_;
    // A real room prop; coursework scenes never spawn this crate.
    std::optional<ComputerCube> computerCube_{ComputerCube{{2.6f,.5f,-2.0f},1.0f,0.0f}};
    std::optional<KeyPiece> keyPiece_;
    std::array<PortableObject, 6> portable_{{
        {{0.0f,0.0f,0.0f},0.0f,0.0f,false},
        {{0.0f,0.0f,0.0f},0.0f,0.0f,false},
        {{0.40f,.95f,9.30f},0.0f},
        {{5.60f,.95f,9.30f},0.0f},
        {{0.40f,.95f,14.20f},0.0f},
        {{5.60f,.95f,14.20f},1.5707963f},
    }};
    int heldPortable_ = -1;
    bool socketFlat_ = false;
    float pictureHold_ = 0.0f;
    float pictureReveal_ = 0.0f;
    float pictureGlow_ = 0.0f;
    bool socketInstalled_ = false;
    bool completeKeyInserted_ = false;
    glm::vec3 lastEye_{0.0f,1.65f,5.0f};
    glm::vec3 lastDirection_{0.0f,0.0f,-1.0f};
    bool computerFocused_ = false;
    bool liftEnabled_ = false;
    bool standingOnLift_ = false;
    float liftExtension_ = 0.0f;
    float liftTop_ = home_lift::loweredTop;
    float previousLiftTop_ = home_lift::loweredTop;
    bool cabinetUnlocked_ = false;
    bool cabinetOpen_ = false;
    float keyTurn_ = 0.0f;
    float cabinetSwing_ = 0.0f;
    bool doorOpening_ = false;
    float doorSwing_ = 0.0f;
    ExitState exitState_ = ExitState::Explore;
    HeldItem held_ = HeldItem::None;
    float shelfOffset_ = 0.0f;
    float bookcaseSoundGain_ = 0.0f;
    float liftSoundGain_ = 0.0f;
    bool shelfMoving_ = false;
    float doorRatio_ = 0.0f;
    float doorWheelScale_ = 1.0f;
    float shelfLatchRelease_ = 0.0f;
    float doorFitCooldown_ = 0.0f;
    bool doorFitNotified_ = false;
    bool doorFitSoundEnabled_ = false;
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
