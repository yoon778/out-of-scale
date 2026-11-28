#pragma once

#include <GL/glm/glm.hpp>

#include <array>
#include <optional>

namespace home_rotate {
struct Box {
    glm::vec3 center;
    glm::vec3 half;
};

inline constexpr glm::vec3 center{3.75f, 2.40f, 0.0f};
inline constexpr float halfExtent = 2.40f;
inline constexpr float shellThickness = .16f;
inline constexpr float apronHalfWidth = 2.05f;
inline constexpr float landingHalfWidth = 1.19f;
inline constexpr float frontZ = 8.80f;
inline constexpr float farZ = 12.80f;
inline constexpr float landingZ = 15.30f;
inline constexpr float exitZ = 16.25f;
inline constexpr Box wallBox{{-1.19f, 0.0f, 12.76f}, {1.19f, 1.16f, .10f}};
inline constexpr std::array<Box, 3> rearRim{{
    {{1.20f, 0.0f, wallBox.center.z}, {1.20f, halfExtent, wallBox.half.z}},
    {{-1.20f, 1.78f, wallBox.center.z}, {1.20f, .62f, wallBox.half.z}},
    {{-1.20f, -1.78f, wallBox.center.z}, {1.20f, .62f, wallBox.half.z}},
}};
inline constexpr glm::vec3 boardHinge{-2.34f, 0.0f, 12.86f};
inline constexpr float boardLength = 3.00f;
inline constexpr float boardHalfWidth = 1.12f;
inline constexpr float boardHalfThickness = .04f;

struct Prop {
    glm::vec3 position;
    glm::vec3 velocity;
    float radius;
};

struct RayHit {
    float distance;
    bool breakable;
};

class Room {
public:
    void update(float elapsed, float turn);
    glm::vec3 resolveMovement(glm::vec3 previous, glm::vec3 desired, bool* grounded) const;
    std::optional<RayHit> rayHit(glm::vec3 eye, glm::vec3 direction, float maxDistance) const;
    void breakWall();

    float angle() const { return angle_; }
    float boardAngle() const { return boardAngle_; }
    bool wallBroken() const { return wallBroken_; }
    bool bridgeReady() const;
    bool escaped(glm::vec3 eye) const;
    glm::mat4 model() const;
    glm::mat4 boardModel() const;
    const std::array<Prop, 4>& props() const { return props_; }

private:
    void advance(float elapsed, float angularSpeed);

    float angle_ = 0.0f;
    float boardAngle_ = 0.0f;
    float boardSpeed_ = 0.0f;
    bool wallBroken_ = false;
    std::array<Prop, 4> props_{{
        {{4.93f, .28f, 9.74f}, {}, .28f},
        {{3.48f, .21f, 10.43f}, {}, .21f},
        {{2.10f, .31f, 11.55f}, {}, .31f},
        {{2.91f, .24f, 12.11f}, {}, .24f},
    }};
};
} // namespace home_rotate
