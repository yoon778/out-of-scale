#pragma once

#include <GL/glm/glm.hpp>

#include <array>

namespace military {
class Frustum {
public:
    explicit Frustum(const glm::mat4& viewProjection) {
        const auto row = [&](int index) {
            return glm::vec4{viewProjection[0][index], viewProjection[1][index],
                             viewProjection[2][index], viewProjection[3][index]};
        };
        const auto w = row(3);
        const std::array clipPlanes{w + row(0), w - row(0), w + row(1),
                                    w - row(1), w + row(2), w - row(2)};
        for (std::size_t i = 0; i < planes_.size(); ++i) {
            const glm::vec3 normal{clipPlanes[i]};
            // Keep boundary geometry when the exported coordinates are rounded.
            planes_[i] = {normal, clipPlanes[i].w + 0.002f * glm::length(normal)};
        }
    }

    bool intersects(const glm::vec3& min, const glm::vec3& max) const {
        for (const auto& plane : planes_) {
            const auto& n = plane.normal;
            const glm::vec3 nearest{n.x < 0.0f ? min.x : max.x,
                                    n.y < 0.0f ? min.y : max.y,
                                    n.z < 0.0f ? min.z : max.z};
            if (glm::dot(n, nearest) + plane.offset < 0.0f) {
                return false;
            }
        }
        return true;
    }

private:
    struct Plane {
        glm::vec3 normal;
        float offset;
    };
    std::array<Plane, 6> planes_;
};
}
