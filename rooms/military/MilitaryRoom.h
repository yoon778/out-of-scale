#pragma once

#include "../shared/BoxRenderer.h"
#include "../shared/PerspectiveCarry.h"

#include <array>
#include <optional>
#include <vector>

class MilitaryRoom {
public:
    enum class ExitState { Locked, Open, OutpostReached, BridgeReady, Escaped };

    MilitaryRoom();
    bool initialize();
    void interact(const glm::vec3& eye, const glm::vec3& direction);
    void update(float elapsed, const glm::vec3& playerEye);
    void scroll(double yOffset);
    bool returnToBarracks();
    glm::vec3 resolveMovement(const glm::vec3& previous, const glm::vec3& desired,
                              const glm::vec3& facing = {1.0f, 0.0f, 0.0f}, bool* grounded = nullptr);
    ExitState exitState() const;
    void draw(const glm::mat4& viewProjection, const glm::vec3& eye, const glm::vec3& direction) const;

    static constexpr float halfSize = 36.0f;
    static constexpr glm::vec3 spawnEye{1.30f, 1.65f, -5.20f};
    static constexpr float spawnYaw = 1.5707963268f;

private:
    enum class Support { Free, Shelf, Hook };

    struct Placement {
        perspective::Pose pose;
        Support support = Support::Free;
    };

    static constexpr std::size_t belongingsCount = 3;
    static constexpr std::size_t televisionIndex = belongingsCount;
    static constexpr std::size_t steelPlateIndex = televisionIndex + 1;
    static constexpr std::size_t objectCount = steelPlateIndex + 1;
    bool inBarracks() const;
    bool pressesPad(const glm::vec3& padCenter) const;
    std::optional<Placement> placement(const glm::vec3& eye, const glm::vec3& direction) const;
    bool canEnterTelevision(const glm::vec3& previous, const glm::vec3& desired,
                            const glm::vec3& facing) const;
    void drawBarracks(const glm::vec3& eye, const glm::vec3& direction) const;
    void drawEquipmentPuzzle() const;
    void drawTrainingReflectors(const glm::vec3& moonCenter, float nearbyLightStrength) const;
    void drawObject(std::size_t index, const perspective::Pose& pose, const glm::vec3& tint) const;
    void interactMoon(const glm::vec3& eye, const glm::vec3& direction);
    bool canPlaceMoon(const glm::vec3& center) const;
    ExitState outpostState() const;
    bool supports(const glm::vec2& point) const;
    glm::vec3 resolveOutpostMovement(const glm::vec3& previous, const glm::vec3& desired,
                                    const glm::vec3& facing, bool* grounded);

    BoxRenderer renderer_;
    std::array<perspective::Pose, objectCount> objects_;
    std::array<float, objectCount> verticalSpeeds_{};
    std::array<float, objectCount> fallGrowthRates_{};
    std::array<bool, belongingsCount> returned_{};
    bool equipmentCabinetOpen_ = false;
    std::optional<std::size_t> heldObject_;
    float heldRatio_ = 0.0f;
    ExitState exitState_ = ExitState::Locked;
    std::optional<glm::vec3> placedMoon_;
    float moonScale_ = 0.10f;
    bool moonHeld_ = false;
    std::vector<glm::vec2> bridge_;
    bool climbing_ = false;
};
