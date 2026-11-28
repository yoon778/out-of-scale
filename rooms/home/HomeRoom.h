#pragma once

#include "../shared/BoxRenderer.h"
#include "ComputerScreen.h"
#include "HomeEnding.h"
#include "HomeFinale.h"

#include <array>
#include <optional>

class HomeRoom {
public:
    enum class ExitState { Explore, ExitOpen, Escaped };

    bool initialize();
    void interact(const glm::vec3& eye, const glm::vec3& direction);
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
    bool isCarrying() const { return held_ != HeldItem::None || finale_.isCarrying(); }
    bool repriseActive() const { return finale_.exitOpen(); }

    static constexpr float halfSize = 40.0f;
    static constexpr glm::vec3 spawnEye{0.0f, 1.65f, 5.0f};
    static constexpr float spawnYaw = 0.0f;

private:
    enum class HeldItem { None, Cube, Key, Portable };

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

    float visibleRange(const glm::vec3& eye, const glm::vec3& direction, float maxDistance) const;
    void drawBedroom() const;
    std::optional<glm::vec2> computerUv(const glm::vec3& eye, const glm::vec3& direction) const;
    bool cubeFits(const ComputerCube& cube, const glm::vec3& eye) const;
    std::optional<ComputerCube> carriedCube(const glm::vec3& eye, const glm::vec3& direction) const;
    void updateCube(float elapsed, const glm::vec3& eye, const glm::vec3& direction);
    bool keyFits(const KeyPiece& key, const glm::vec3& eye) const;
    std::optional<KeyPiece> carriedKey(const glm::vec3& eye, const glm::vec3& direction) const;
    void updateKey(float elapsed, const glm::vec3& eye, const glm::vec3& direction);
    void updatePicture(float elapsed);
    void drawPicture(const glm::mat4& model) const;
    bool portableFits(int index, const PortableObject& object, const glm::vec3& eye) const;
    std::optional<PortableObject> carriedPortable(int index, const glm::vec3& eye, const glm::vec3& direction) const;
    void updatePortables(float elapsed, const glm::vec3& eye, const glm::vec3& direction);
    bool grabPortable(const glm::vec3& eye, const glm::vec3& direction);
    void updateDoors(float elapsed, const glm::vec3& eye);
    bool swingBlocked(const glm::vec3& center, const glm::vec3& size,
                      const glm::vec3& eye) const;
    glm::mat4 portableModel(int index) const;
    bool overlapsPortable(const glm::vec3& center, const glm::vec3& size, float yaw, int ignore = -1) const;

    BoxRenderer renderer_;
    HomeEnding ending_;
    ComputerScreen computer_;
    home_finale::Room finale_;
    GLint finaleVisualFirst_ = 0;
    // A real room prop; coursework scenes never spawn this crate.
    std::optional<ComputerCube> computerCube_{ComputerCube{{2.6f,.5f,-2.0f},1.0f,0.0f}};
    std::optional<KeyPiece> keyPiece_;
    std::array<PortableObject, 2> portable_{{
        {{0.0f,0.0f,0.0f},0.0f,0.0f,false},
        {{0.0f,0.0f,0.0f},0.0f,0.0f,false},
    }};
    int heldPortable_ = -1;
    bool socketFlat_ = false;
    bool pictureOpening_ = false;
    float pictureReveal_ = 0.0f;
    bool socketInstalled_ = false;
    bool completeKeyInserted_ = false;
    glm::vec3 lastEye_{0.0f,1.65f,5.0f};
    bool computerFocused_ = false;
    bool cabinetUnlocked_ = false;
    bool cabinetOpen_ = false;
    float keyTurn_ = 0.0f;
    float cabinetSwing_ = 0.0f;
    HeldItem held_ = HeldItem::None;
};
