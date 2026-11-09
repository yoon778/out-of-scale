#pragma once

#include <GL/glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace perspective {

struct Pose {
    glm::vec3 center;
    float scale;
};

inline std::optional<float> rayBoxDistance(const glm::vec3& eye, const glm::vec3& direction,
                                           const glm::vec3& center, const glm::vec3& half) {
    float enter = 0.0f;
    float leave = std::numeric_limits<float>::infinity();
    for (int axis = 0; axis < 3; ++axis) {
        const float minOffset = center[axis] - half[axis] - eye[axis];
        const float maxOffset = center[axis] + half[axis] - eye[axis];
        if (std::abs(direction[axis]) < 0.0001f) {
            if (minOffset > 0.0f || maxOffset < 0.0f) {
                return std::nullopt;
            }
            continue;
        }

        const float a = minOffset / direction[axis];
        const float b = maxOffset / direction[axis];
        enter = std::max(enter, std::min(a, b));
        leave = std::min(leave, std::max(a, b));
        if (enter > leave) {
            return std::nullopt;
        }
    }
    return enter;
}

// The normalized view direction gives camera depth. Cap the ratio at close range.
inline float grabRatio(const glm::vec3& eye, const glm::vec3& direction, const glm::vec3& center,
                       float scale, float largestDimension) {
    const float depth = glm::dot(center - eye, direction);
    return scale / std::max(depth, scale * largestDimension);
}

inline std::optional<Pose> onFloor(const glm::vec3& eye, const glm::vec3& direction, float ratio,
                                  float unscaledHalfHeight, float floorY = 0.0f) {
    if (direction.y >= -0.001f || eye.y <= floorY || ratio <= 0.0f) {
        return std::nullopt;
    }

    // The center lies on the sight ray and one scaled half-height above the floor.
    const float depth = (eye.y - floorY) / (unscaledHalfHeight * ratio - direction.y);
    const float scale = ratio * depth;
    const glm::vec3 center = eye + direction * depth;
    return Pose{{center.x, floorY + scale * unscaledHalfHeight, center.z}, scale};
}

inline float scaleAtSupport(const glm::vec3& eye, const glm::vec3& direction, const glm::vec3& support,
                            float ratio, float unscaledHalfHeight) {
    const float baseDepth = glm::dot(support - eye, direction);
    return ratio * baseDepth / (1.0f - direction.y * ratio * unscaledHalfHeight);
}

}  // namespace perspective
