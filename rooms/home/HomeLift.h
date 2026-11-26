#pragma once

#include <GL/glm/glm.hpp>

#include <cmath>

namespace home_lift {
inline constexpr float platformHalfWidth = .50f;
inline constexpr float platformHalfLength = .53f;
inline constexpr float platformThickness = .10f;
inline constexpr float platformZ = -3.25f;
inline constexpr float stowedX = 4.88f;
inline constexpr float deployedX = 3.67f;
inline constexpr float loweredTop = .12f;
inline constexpr float raisedTop = .94f;
inline constexpr glm::vec3 platformSize{2 * platformHalfWidth, platformThickness, 2 * platformHalfLength};
inline constexpr glm::vec3 leverPivot{3.88f, .49f, -2.30f};

inline glm::vec3 center(float extension, float top) {
    const float slide = glm::smoothstep(0.0f, 1.0f, extension);
    return {glm::mix(stowedX, deployedX, slide), top - platformThickness * .5f, platformZ};
}

inline glm::vec3 leverHandle(bool enabled) {
    return leverPivot + (enabled ? glm::vec3{-.18f, .25f, 0.0f} : glm::vec3{-.13f, .36f, 0.0f});
}

inline bool footOnDeck(const glm::vec3& eye, const glm::vec3& deckCenter) {
    constexpr float edgeAllowance = .06f;
    return std::abs(eye.x - deckCenter.x) < platformHalfWidth + edgeAllowance &&
           std::abs(eye.z - deckCenter.z) < platformHalfLength + edgeAllowance;
}
}  // namespace home_lift
