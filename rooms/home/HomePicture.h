#pragma once

#include "../shared/BoxRenderer.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <vector>

namespace home_picture {
inline constexpr glm::vec3 framePosition{-5.13f, 2.00f, -6.26f};
inline constexpr glm::vec3 canvasCenter{-4.48f, 2.00f, -6.25f};
inline constexpr glm::vec2 canvasSize{1.068f, 1.148f};

inline glm::vec3 paintPoint(glm::vec2 point, float layer) {
    return canvasCenter + glm::vec3{(point - glm::vec2{.5f}) * canvasSize, layer * .0012f};
}

inline constexpr glm::vec2 keyholeCap{.237f, .231f};
inline constexpr glm::vec2 keyholeRadius{.026f, .024f};
inline constexpr float keyholeLayer = 8.0f;
inline constexpr float keyholeStemTop = .228f;
inline constexpr float keyholeStemBottom = .164f;
inline constexpr float keyholeStemTopHalf = .010f;
inline constexpr float keyholeStemBottomHalf = .019f;
inline constexpr glm::vec3 hiddenTarget{
    canvasCenter.x + (keyholeCap.x - .5f) * canvasSize.x,
    canvasCenter.y + (keyholeCap.y - .5f) * canvasSize.y,
    canvasCenter.z + keyholeLayer * .0012f,
};

inline std::optional<float> hiddenKeyholeHit(glm::vec3 eye, glm::vec3 direction, float reach) {
    if (eye.z <= hiddenTarget.z || direction.z >= -.0001f) return std::nullopt;
    const float distance = (hiddenTarget.z - eye.z) / direction.z;
    if (distance > reach) return std::nullopt;

    const glm::vec2 uv = (glm::vec2{eye + direction * distance} - glm::vec2{canvasCenter}) /
        canvasSize + glm::vec2{.5f};
    // A small aiming margin follows the painted silhouette, even from an oblique view.
    constexpr glm::vec2 aimMargin{.012f, .011f};
    const auto cap = (uv - keyholeCap) / (keyholeRadius + aimMargin);
    if (glm::dot(cap, cap) <= 1.0f) return distance;

    if (uv.y < keyholeStemBottom - aimMargin.y || uv.y > keyholeStemTop + aimMargin.y) return std::nullopt;
    const float stemFraction = std::clamp((uv.y - keyholeStemBottom) /
        (keyholeStemTop - keyholeStemBottom), 0.0f, 1.0f);
    const float halfWidth = glm::mix(keyholeStemBottomHalf, keyholeStemTopHalf, stemFraction);
    return std::abs(uv.x - keyholeCap.x) <= halfWidth + aimMargin.x ? std::optional{distance} : std::nullopt;
}

inline glm::vec3 gazeOffset(const glm::vec3& eye) {
    const auto offset = eye - canvasCenter;
    const float depth = std::max(offset.z, .55f);
    return {std::clamp(.032f * offset.x / depth, -.028f, .028f),
            std::clamp(.028f * (offset.y - .10f) / depth, -.016f, .016f), 0.0f};
}

struct Art {
    std::vector<MeshVertex> vertices;
    GLsizei backdropCount = 0;
    GLsizei pupilCount = 0;
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

        // Bark ridges and ordinary knots give the hidden silhouette a place in the painting.
        quad({0,.08f}, {1,.25f}, {1,.34f}, {0,.32f}, 3, {.33f,.24f,.17f});
        quad({0,.26f}, {1,.31f}, {1,.34f}, {0,.32f}, 4, {.54f,.40f,.27f});
        quad({.09f,.22f}, {.91f,.28f}, {.77f,.27f}, {.44f,.23f}, 5, {.41f,.30f,.21f});
        const glm::vec3 barkHighlight{.47f,.33f,.22f};
        const glm::vec3 barkShadow{.26f,.18f,.13f};
        quad({.01f,.14f}, {.33f,.183f}, {.51f,.229f}, {.28f,.192f}, 6, barkHighlight);
        quad({.00f,.20f}, {.11f,.213f}, {.17f,.254f}, {.10f,.224f}, 6, barkShadow);
        quad({.33f,.272f}, {.66f,.258f}, {.90f,.296f}, {.61f,.272f}, 6, barkHighlight);
        for (const auto& knot : {glm::vec2{.111f,.185f}, glm::vec2{.431f,.213f}, glm::vec2{.800f,.287f}}) {
            ellipse(knot, {.055f,.018f}, 7, barkHighlight);
            ellipse(knot + glm::vec2{.009f,.002f}, {.036f,.011f}, 7.5f, barkShadow);
            ellipse(knot + glm::vec2{.016f,.004f}, {.018f,.004f}, 8, {.34f,.24f,.16f});
        }
        ellipse(keyholeCap - glm::vec2{0.0f,.014f}, {.043f,.046f}, 7, {.38f,.27f,.18f});

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
        result.pupilCount = static_cast<GLsizei>(result.vertices.size()) - result.backdropCount;
        constexpr glm::vec3 keyholeShadow{.22f,.145f,.10f};
        ellipse(keyholeCap, keyholeRadius, keyholeLayer, keyholeShadow);
        quad({keyholeCap.x - keyholeStemTopHalf,keyholeStemTop},
             {keyholeCap.x + keyholeStemTopHalf,keyholeStemTop},
             {keyholeCap.x + keyholeStemBottomHalf,keyholeStemBottom},
             {keyholeCap.x - keyholeStemBottomHalf,keyholeStemBottom}, keyholeLayer, keyholeShadow);
        return result;
    }();
    return painting;
}
} // namespace home_picture
