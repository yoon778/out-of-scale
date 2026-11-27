#pragma once

#include "HomeLift.h"
#include "../shared/BoxRenderer.h"

#include <GL/glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace home_lift_visual {

inline constexpr glm::vec3 leverGripSize{.34f, .105f, .16f};
inline constexpr float leverStemWidth = .042f;
inline constexpr float leverStemDepth = .052f;

inline glm::mat4 leverStemModel(const glm::mat4& model, bool enabled) {
    const auto pivot = home_lift::leverPivot;
    const auto handle = home_lift::leverHandle(enabled);
    const auto direction = handle - pivot;
    auto result = glm::translate(model, (pivot + handle) * .5f);
    return glm::rotate(result, std::atan2(-direction.x, direction.y), glm::vec3{0, 0, 1});
}

inline const std::array<MeshVertex, 36>& cubeVertices() {
    static const auto vertices = [] {
        constexpr std::array<glm::vec3, 8> points{{
            {-.5f, -.5f, -.5f}, {.5f, -.5f, -.5f}, {.5f, .5f, -.5f}, {-.5f, .5f, -.5f},
            {-.5f, -.5f, .5f}, {.5f, -.5f, .5f}, {.5f, .5f, .5f}, {-.5f, .5f, .5f}}};
        constexpr int faces[6][6]{{0, 2, 1, 0, 3, 2}, {4, 5, 6, 4, 6, 7},
            {0, 4, 7, 0, 7, 3}, {1, 2, 6, 1, 6, 5}, {3, 7, 6, 3, 6, 2}, {0, 1, 5, 0, 5, 4}};
        std::array<MeshVertex, 36> result{};
        for (int face = 0; face < 6; ++face) {
            for (int vertex = 0; vertex < 6; ++vertex) {
                result[face * 6 + vertex] = {points[faces[face][vertex]], glm::vec3{1.0f}};
            }
        }
        return result;
    }();
    return vertices;
}

inline void draw(const BoxRenderer& renderer, GLint first, const glm::mat4& model,
                 float extension, float top, bool enabled, bool miniature) {
    const glm::vec3 steel{.29f, .33f, .32f};
    const glm::vec3 edge{.45f, .47f, .42f};
    const glm::vec3 rail{.19f, .23f, .23f};
    const glm::vec3 wood{.55f, .40f, .27f};
    const auto part = [&](glm::vec3 position, glm::vec3 size, glm::vec3 color) {
        renderer.drawMovableMesh(first, 36, glm::scale(glm::translate(model, position), size), color);
    };

    const float width = home_lift::platformHalfWidth * 2;
    const float length = home_lift::platformHalfLength * 2;
    const float railCenterX = (home_lift::stowedX + home_lift::deployedX) * .5f;
    const float railLength = home_lift::stowedX - home_lift::deployedX + width;
    const float railOffset = home_lift::platformHalfLength - .10f;
    // The shallow floor runners have the same position in both room scales.
    for (float side : {-1.0f, 1.0f}) {
        const float z = home_lift::platformZ + side * railOffset;
        part({railCenterX, .018f, z}, {railLength, .036f, .06f}, rail);
        for (float end : {-1.0f, 1.0f}) {
            part({railCenterX + end * (railLength * .5f - .035f), .036f, z},
                 {.07f, .014f, .09f}, edge);
        }
    }

    const glm::vec3 platform = home_lift::center(std::clamp(extension, 0.0f, 1.0f), top);
    const float platformTop = platform.y + home_lift::platformThickness * .5f;
    const float underside = platformTop - home_lift::platformThickness;
    // Thin decorative supports stay below the physical walking surface.
    for (float side : {-1.0f, 1.0f}) {
        const float z = home_lift::platformZ + side * railOffset;
        part({platform.x, .037f, z}, {.22f, .026f, .10f}, steel);
        if (underside > .055f) {
            part({platform.x, (.05f + underside) * .5f, z},
                 {.055f, underside - .05f, .055f}, edge);
        }
    }
    constexpr float deckThickness = .022f;
    part({platform.x, platform.y - deckThickness * .5f, platform.z},
         {width, home_lift::platformThickness - deckThickness, length}, steel);
    for (int plank = 0; plank < 4; ++plank) {
        const float cell = (width - .04f) / 4;
        const float x = platform.x - (width - .04f) * .5f + (plank + .5f) * cell;
        part({x, platformTop - deckThickness * .5f, platform.z},
             {cell - .008f, deckThickness, length - .04f}, wood * (.97f + plank * .02f));
    }
    // All upper trim is flush with the collider's top; no false foothold lip.
    for (float side : {-1.0f, 1.0f}) {
        part({platform.x + side * (home_lift::platformHalfWidth - .01f),
              platformTop - .011f, platform.z}, {.02f, .022f, length}, edge);
        part({platform.x, platformTop - .011f,
              platform.z + side * (home_lift::platformHalfLength - .01f)},
             {width - .04f, .022f, .02f}, edge);
    }

    if (!miniature) return;
    const glm::vec3 pivot = home_lift::leverPivot;
    const glm::vec3 handle = home_lift::leverHandle(enabled);
    part({pivot.x, .025f, pivot.z}, {.22f, .05f, .18f}, rail);
    part({pivot.x, pivot.y * .5f, pivot.z}, {.045f, pivot.y, .055f}, steel);
    part(pivot, {.12f, .09f, .09f}, edge);
    renderer.drawMovableMesh(first, 36,
        glm::scale(leverStemModel(model,enabled),
                   {leverStemWidth, glm::length(handle-pivot), leverStemDepth}), {.48f,.52f,.48f});
    part(handle, leverGripSize, {.73f,.55f,.33f});
}

} // namespace home_lift_visual
