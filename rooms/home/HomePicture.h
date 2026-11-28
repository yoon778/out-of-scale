#pragma once

#include "../shared/BoxRenderer.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace home_picture {
inline constexpr glm::vec3 framePosition{-5.13f, 2.00f, -6.26f};
inline constexpr glm::vec3 canvasCenter{-4.48f, 2.00f, -6.25f};
inline constexpr glm::vec2 canvasSize{1.068f, 1.148f};
inline constexpr float revealSeconds = .57f;

inline glm::vec3 paintPoint(glm::vec2 point, float layer) {
    return canvasCenter + glm::vec3{(point - glm::vec2{.5f}) * canvasSize, layer * .0012f};
}

inline constexpr glm::vec3 plateEnd{-4.48f, 1.83f, -5.64f};
inline constexpr glm::vec3 plateStart{plateEnd.x, plateEnd.y, canvasCenter.z + .016f};

inline glm::vec3 gazeOffset(const glm::vec3& eye) {
    const auto offset = eye - canvasCenter;
    const float depth = std::max(offset.z, .55f);
    return {std::clamp(.032f * offset.x / depth, -.028f, .028f),
            std::clamp(.028f * (offset.y - .10f) / depth, -.016f, .016f), 0.0f};
}

struct Art {
    std::vector<MeshVertex> vertices;
    GLsizei backdropCount = 0;
};

inline const Art& art() {
    static const Art painting = [] {
        Art result;
        const auto triangle = [&](glm::vec2 a, glm::vec2 b, glm::vec2 c, float layer, glm::vec3 color) {
            result.vertices.insert(result.vertices.end(), {
                {paintPoint(a, layer), color}, {paintPoint(b, layer), color}, {paintPoint(c, layer), color}});
        };
        const auto quad = [&](glm::vec2 a, glm::vec2 b, glm::vec2 c, glm::vec2 d,
                              float layer, glm::vec3 color) {
            triangle(a, b, c, layer, color);
            triangle(a, c, d, layer, color);
        };
        const auto ellipse = [&](glm::vec2 center, glm::vec2 radius, float layer, glm::vec3 color) {
            constexpr int sides = 24;
            glm::vec2 previous{center.x + radius.x, center.y};
            for (int side = 1; side <= sides; ++side) {
                const float angle = (2.0f * std::numbers::pi_v<float> / sides) * side;
                const auto next = center + radius * glm::vec2{std::cos(angle), std::sin(angle)};
                triangle(center, previous, next, layer, color);
                previous = next;
            }
        };

        // A muted evening woodland behind the bird.
        constexpr glm::vec3 skyBottom{.67f, .54f, .37f};
        constexpr glm::vec3 skyTop{.19f, .29f, .31f};
        const MeshVertex lowerLeft{paintPoint({0,0}, 0), skyBottom};
        const MeshVertex lowerRight{paintPoint({1,0}, 0), skyBottom};
        const MeshVertex upperRight{paintPoint({1,1}, 0), skyTop};
        const MeshVertex upperLeft{paintPoint({0,1}, 0), skyTop};
        result.vertices.insert(result.vertices.end(),
            {lowerLeft, lowerRight, upperRight, lowerLeft, upperRight, upperLeft});
        ellipse({.80f,.79f}, {.086f,.080f}, 1, {.86f,.74f,.51f});
        for (int tree = 0; tree < 11; ++tree) {
            const float x = .032f + .093f * tree;
            const float height = .32f + .17f * std::sin(1.77f * tree) * std::sin(1.04f * tree);
            const auto green = glm::mix(glm::vec3{.25f,.33f,.32f}, glm::vec3{.37f,.40f,.33f}, height);
            triangle({x-.035f,.025f}, {x+.039f,.025f}, {x,height}, 2, green);
            triangle({x-.030f,height*.35f}, {x+.034f,height*.35f}, {x,height*1.13f}, 2, green);
        }

        // The keyhole silhouette reads as a knot against the branch's grain.
        quad({0,.08f}, {1,.25f}, {1,.34f}, {0,.32f}, 3, {.33f,.24f,.17f});
        quad({0,.26f}, {1,.31f}, {1,.34f}, {0,.32f}, 4, {.54f,.40f,.27f});
        quad({.09f,.22f}, {.91f,.28f}, {.77f,.27f}, {.44f,.23f}, 5, {.41f,.30f,.21f});
        ellipse({.5f,.269f}, {.043f,.023f}, 6, {.29f,.20f,.14f});
        quad({.493f,.270f}, {.507f,.270f}, {.511f,.235f}, {.489f,.235f},
             6, {.29f,.20f,.14f});

        // Ear tufts, layered plumage, and distinct facial disks.
        const glm::vec3 wing{.28f,.21f,.17f};
        ellipse({.49f,.447f}, {.201f,.178f}, 5.5f, wing);
        ellipse({.51f,.432f}, {.130f,.153f}, 6.5f, {.68f,.53f,.35f});
        triangle({.301f,.648f}, {.300f,.831f}, {.445f,.733f}, 4, wing);
        triangle({.558f,.727f}, {.700f,.831f}, {.692f,.647f}, 4, wing);
        triangle({.326f,.702f}, {.325f,.787f}, {.397f,.728f}, 5, {.60f,.43f,.29f});
        triangle({.607f,.722f}, {.678f,.785f}, {.670f,.703f}, 5, {.60f,.43f,.29f});
        ellipse({.50f,.643f}, {.197f,.138f}, 5, {.42f,.30f,.21f});
        for (int side : {-1, 1}) {
            const float x = .5f + side * .092f;
            ellipse({x,.642f}, {.093f,.081f}, 6, {.78f,.66f,.46f});
            ellipse({x,.650f}, {.071f,.057f}, 7, {.36f,.26f,.19f});
            ellipse({x,.648f}, {.057f,.048f}, 8, {.78f,.59f,.32f});
        }
        triangle({.466f,.627f}, {.533f,.627f}, {.503f,.569f}, 8, {.23f,.17f,.14f});
        triangle({.477f,.622f}, {.524f,.622f}, {.502f,.585f}, 9, {.71f,.46f,.23f});
        for (int row = 0; row < 4; ++row) {
            for (int column = -2; column <= 2; ++column) {
                const float x = .51f + .033f * column + (row % 2) * .010f;
                const float y = .504f - .041f * row + .010f * std::abs(column);
                triangle({x-.012f,y+.014f}, {x+.011f,y+.014f}, {x+.002f,y-.016f},
                         7, {.50f,.37f,.25f});
            }
        }

        result.backdropCount = static_cast<GLsizei>(result.vertices.size());
        for (int side : {-1, 1}) {
            const float x = .5f + side * .092f;
            ellipse({x,.648f}, {.026f,.030f}, 9, {.11f,.088f,.068f});
            ellipse({x-.007f,.658f}, {.008f,.008f}, 10, {.94f,.81f,.58f});
        }
        return result;
    }();
    return painting;
}
} // namespace home_picture
