#pragma once

#include "../shared/BoxRenderer.h"

#include <array>
#include <cmath>
#include <optional>
#include <vector>

namespace home_picture {
inline constexpr glm::vec3 framePosition{-5.13f, 2.00f, -6.26f};
inline constexpr glm::vec3 alignmentEye{-3.78f, 1.65f, -4.20f};
inline constexpr glm::vec3 focus{-4.48f, 2.02f, -6.25f};
inline constexpr float paperDepth = focus.z;
inline constexpr float trailDepth = paperDepth + .003f;
inline constexpr float moonDepth = -5.91f;
inline constexpr float holdSeconds = .42f;
inline constexpr float revealSeconds = .72f;
inline constexpr float seamTolerance = .048f;
inline constexpr glm::vec3 plateStart{focus.x, focus.y, -5.88f};
inline constexpr glm::vec3 plateEnd{focus.x, focus.y, -5.64f};
inline constexpr glm::vec2 moonCenter{focus.x, 2.15f};
inline constexpr float radius = .151f;
inline constexpr float neckHalfWidth = .058f;
inline constexpr float neckHeight = 2.01059f;
inline constexpr std::array<glm::vec2, 2> seam{{
    {moonCenter.x + neckHalfWidth, neckHeight},
    {moonCenter.x - neckHalfWidth, neckHeight},
}};

inline glm::vec3 pointOnMoon(const glm::vec2& paperPoint) {
    // The two layers line up from the authored viewpoint by construction.
    const float ratio = (moonDepth - alignmentEye.z) / (trailDepth - alignmentEye.z);
    return glm::mix(alignmentEye, glm::vec3{paperPoint, trailDepth}, ratio);
}

inline std::optional<float> alignmentError(const glm::vec3& eye, const glm::vec3& direction) {
    if (eye.z <= moonDepth + .45f) return std::nullopt;
    const glm::vec3 delta = focus - eye;
    const float distance = glm::length(delta);
    if (distance > 3.25f || glm::dot(direction, delta / distance) < .97f) return std::nullopt;

    const float projection = (trailDepth - eye.z) / (moonDepth - eye.z);
    float maximumError = 0.0f;
    for (const auto& joint : seam) {
        const glm::vec3 projected = glm::mix(eye, pointOnMoon(joint), projection);
        maximumError = glm::max(maximumError, glm::length(glm::vec2{projected} - joint));
    }
    return maximumError;
}

struct Art {
    std::vector<MeshVertex> vertices;
    GLint outlineFirst = 0;
};

inline const Art& art() {
    static const Art object = [] {
        Art result;
        const auto triangle = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 color) {
            result.vertices.insert(result.vertices.end(), {{a, color}, {b, color}, {c, color}});
        };
        const auto quad = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 color) {
            triangle(a, b, c, color);
            triangle(a, c, d, color);
        };
        constexpr float left = -5.014f, right = -3.946f, bottom = 1.426f, top = 2.574f;
        quad({left, bottom, paperDepth}, {right, bottom, paperDepth},
             {right, top, paperDepth}, {left, top, paperDepth}, {.22f, .28f, .36f});

        const auto ridge = [&](const std::array<float, 7>& heights, float depth, glm::vec3 color) {
            constexpr std::array<float, 7> x{left, -4.84f, -4.67f, -4.48f, -4.25f, -4.11f, right};
            for (std::size_t i = 1; i < x.size(); ++i) {
                quad({x[i - 1], bottom, depth}, {x[i], bottom, depth},
                     {x[i], heights[i], depth}, {x[i - 1], heights[i - 1], depth}, color);
            }
        };
        ridge({1.88f, 2.06f, 1.91f, 1.83f, 2.02f, 1.95f, 1.82f}, paperDepth + .001f,
              {.20f, .34f, .37f});
        ridge({1.70f, 1.88f, 1.73f, 1.71f, 1.83f, 1.92f, 1.75f}, paperDepth + .002f,
              {.15f, .25f, .27f});

        constexpr std::size_t segments = 32;
        const float startAngle = -std::acos(neckHalfWidth / radius);
        const float endAngle = 3.14159265f - startAngle;
        std::array<glm::vec3, segments + 1> moon;
        for (std::size_t i = 0; i <= segments; ++i) {
            const float angle = glm::mix(startAngle, endAngle, static_cast<float>(i) / static_cast<float>(segments));
            moon[i] = pointOnMoon(moonCenter + radius * glm::vec2{std::cos(angle), std::sin(angle)});
        }
        moon.front() = pointOnMoon(seam[0]);
        moon.back() = pointOnMoon(seam[1]);
        constexpr glm::vec3 moonInk{.77f, .66f, .48f};
        const glm::vec3 moonOrigin = pointOnMoon(moonCenter);
        for (std::size_t i = 1; i < moon.size(); ++i) triangle(moonOrigin, moon[i - 1], moon[i], moonInk);
        triangle(moonOrigin, moon.back(), moon.front(), moonInk);

        const std::array<glm::vec3, 4> trail{{
            {seam[0], trailDepth}, {seam[1], trailDepth},
            {focus.x - .083f, 1.655f, trailDepth}, {focus.x + .083f, 1.655f, trailDepth},
        }};
        quad(trail[0], trail[1], trail[2], trail[3], {.68f, .56f, .40f});

        result.outlineFirst = static_cast<GLint>(result.vertices.size());
        const auto stroke = [&](glm::vec3 a, glm::vec3 b) {
            const auto delta = glm::vec2{b - a};
            // Lift both sides equally, so the outline cannot fight with the paint for depth.
            const glm::vec3 width{glm::normalize(glm::vec2{-delta.y, delta.x}) * .0035f, 0.0f};
            a.z += .0006f;
            b.z += .0006f;
            quad(a - width, b - width, b + width, a + width, glm::vec3{1.0f});
        };
        for (std::size_t i = 1; i < moon.size(); ++i) stroke(moon[i - 1], moon[i]);
        for (std::size_t i = 1; i < trail.size(); ++i) stroke(trail[i], trail[(i + 1) % trail.size()]);
        return result;
    }();
    return object;
}
}  // namespace home_picture
