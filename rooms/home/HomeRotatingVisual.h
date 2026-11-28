#pragma once

#include "HomeRotatingRoom.h"
#include "../shared/BoxRenderer.h"

#include <GL/glm/gtc/matrix_transform.hpp>

#include <array>
#include <cmath>
#include <numbers>
#include <span>
#include <utility>
#include <vector>

namespace home_rotate {

inline constexpr glm::vec3 handwheelCenter{2.18f, 1.18f, 8.15f};

namespace visual_detail {

struct Range {
    GLint first;
    GLsizei count;
};

struct VisualMesh {
    std::vector<MeshVertex> vertices;
    Range fixed, chamber, wall, chalkboard, wheel, lamp, weightLens, prop;
};

inline constexpr float pi = std::numbers::pi_v<float>;
inline constexpr glm::vec3 iron{.25f, .30f, .29f};
inline constexpr glm::vec3 ironEdge{.36f, .42f, .39f};
inline constexpr glm::vec3 recess{.096f, .12f, .125f};
inline constexpr glm::vec3 brass{.78f, .58f, .30f};
inline constexpr glm::vec3 timber{.55f, .39f, .24f};
inline constexpr std::array<glm::vec3, 2> weightTints{{{.95f,.57f,.19f},{.20f,.79f,.73f}}};

inline constexpr glm::vec3 lensCenter(std::size_t index) {
    return handwheelCenter + glm::vec3{index == 0 ? .185f : -.185f, .377f, .003f};
}

inline void box(std::vector<MeshVertex>& values, glm::vec3 position, glm::vec3 size, glm::vec3 color,
                float roll = 0.0f) {
    constexpr std::array<glm::vec3, 8> corners{{
        {-.5f,-.5f,-.5f}, {.5f,-.5f,-.5f}, {.5f,.5f,-.5f}, {-.5f,.5f,-.5f},
        {-.5f,-.5f,.5f}, {.5f,-.5f,.5f}, {.5f,.5f,.5f}, {-.5f,.5f,.5f}}};
    constexpr int faces[6][6]{{0,2,1,0,3,2}, {4,5,6,4,6,7}, {0,4,7,0,7,3},
        {1,2,6,1,6,5}, {3,7,6,3,6,2}, {0,1,5,0,5,4}};
    constexpr std::array brightness{.93f, .81f, .82f, .89f, 1.0f, .74f};
    const float c = std::cos(roll), s = std::sin(roll);
    std::array<glm::vec3, 8> points;
    for (std::size_t i = 0; i < corners.size(); ++i) {
        const auto p = corners[i] * size;
        points[i] = position + glm::vec3{c * p.x - s * p.y, s * p.x + c * p.y, p.z};
    }
    for (std::size_t face = 0; face < brightness.size(); ++face) {
        for (const int index : faces[face]) values.push_back({points[index], color * brightness[face]});
    }
}

inline void stroke(std::vector<MeshVertex>& values, const glm::vec3& a, const glm::vec3& b,
                   float width, float depth, glm::vec3 tint) {
    const glm::vec2 span{b - a};
    box(values, (a + b) * .5f, {glm::length(span) + width * .18f, width, depth}, tint,
        std::atan2(span.y, span.x));
}

inline void ring(std::vector<MeshVertex>& values, glm::vec3 axis, float radius,
                 float width, float depth, int segments, glm::vec3 tint) {
    const auto point = [&](int i) {
        const float a = 2.0f * pi * static_cast<float>(i) / static_cast<float>(segments);
        return axis + radius * glm::vec3{std::cos(a), std::sin(a), 0.0f};
    };
    for (int i = 0; i < segments; ++i) stroke(values, point(i), point(i + 1), width, depth, tint);
}

inline void addFixed(std::vector<MeshVertex>& v) {
    constexpr float apronStart = 6.64f;
    constexpr float landingEnd = 16.50f;
    const auto apron = (apronStart + frontZ) * .5f;
    const auto landing = (landingZ + landingEnd) * .5f;
    const auto roomCenter = (apronStart + landingEnd) * .5f;
    box(v, {center.x, -.14f, apron}, {2 * apronHalfWidth, .28f, frontZ - apronStart}, ironEdge);
    box(v, {center.x, -.18f, landing}, {2 * landingHalfWidth, .36f, landingEnd - landingZ}, ironEdge);
    box(v, {center.x, -1.61f, roomCenter}, {8.98f, .16f, 10.20f}, recess);
    box(v, {center.x, 6.37f, roomCenter}, {8.98f, .20f, 10.20f}, {.26f,.30f,.28f});
    for (const float side : {-1.0f, 1.0f}) {
        const float boundary = center.x + side * (apronHalfWidth + .055f);
        const float baySide = center.x + side * 4.36f;
        box(v, {baySide, 2.42f, roomCenter}, {.25f, 8.00f, 10.20f}, {.32f,.36f,.34f});
        for (const float z : {7.02f, 8.56f}) {
            box(v, {boundary,.47f,z}, {.085f,.94f,.085f}, iron);
            box(v, {boundary,.92f,z}, {.104f,.09f,.104f}, brass);
        }
        box(v, {boundary,.86f,7.79f}, {.063f,.057f,1.60f}, ironEdge);
        box(v, {boundary,.39f,7.79f}, {.039f,.044f,1.60f}, ironEdge);
        box(v, {center.x + side * (landingHalfWidth - .025f),.055f,landing},
            {.050f,.11f,landingEnd - landingZ}, {.70f,.59f,.38f});
        box(v, {baySide - side * .18f,4.13f,roomCenter}, {.09f,.075f,7.40f}, {.54f,.57f,.39f});
    }

    // The stationary trunnion hoops frame the chamber's full rotation envelope.
    for (const float z : {frontZ - .05f, farZ + .20f}) {
        ring(v, {center.x,center.y,z}, 3.83f, .15f, .32f, 20, iron);
        ring(v, {center.x,center.y,z - .16f}, 3.83f, .062f, .024f, 20, ironEdge);
        for (int side = 0; side < 4; ++side) {
            const float angle = pi * .5f * static_cast<float>(side);
            const glm::vec3 radial{std::cos(angle),std::sin(angle),0};
            const glm::vec3 base{center.x,center.y,z};
            const auto bearing = base + radial * 3.95f;
            box(v, bearing, {.54f,1.04f,.64f}, iron, angle);
            box(v, bearing - glm::vec3{0,0,.342f}, {.40f,.78f,.057f}, ironEdge, angle);
            box(v, bearing - glm::vec3{0,0,.377f}, {.23f,.39f,.052f}, brass, angle);
            const glm::vec3 tangent{-radial.y,radial.x,0};
            for (const float sign : {-1.0f,1.0f}) {
                box(v, bearing + tangent * (sign * .316f) - glm::vec3{0,0,.385f},
                    {.105f,.108f,.060f}, recess, angle);
            }
        }
    }
    for (const float z : {frontZ - .15f, landingZ + .13f}) {
        for (int i = -3; i <= 3; ++i) {
            box(v, {center.x + i * .19f,.004f,z}, {.095f,.008f,.10f}, brass);
        }
    }

    // A warm, fixed doorway gives a distant reference as the room turns.
    const float portalZ = landingEnd + .01f;
    for (const float side : {-1.0f, 1.0f}) {
        box(v, {center.x+side*2.83f,2.36f,portalZ}, {3.22f,7.82f,.26f}, {.24f,.29f,.26f});
        box(v, {center.x+side*1.14f,1.35f,portalZ-.14f}, {.13f,2.70f,.28f}, timber);
        box(v, {center.x+side*1.06f,.35f,portalZ+.27f}, {.11f,.70f,.88f}, {.81f,.70f,.50f});
    }
    box(v, {center.x,4.43f,portalZ}, {2.44f,3.68f,.27f}, {.23f,.28f,.25f});
    box(v, {center.x,2.66f,portalZ-.14f}, {2.33f,.14f,.28f}, timber);
    box(v, {center.x,1.37f,portalZ+.89f}, {2.25f,2.73f,.02f}, {.84f,.91f,.95f});
    box(v, {center.x,.69f,portalZ+.85f}, {2.25f,1.37f,.02f}, {.98f,.87f,.66f});
    box(v, {center.x,-.024f,portalZ+.45f}, {2.25f,.048f,.98f}, {.91f,.80f,.56f});
    box(v, {center.x+.50f,1.84f,portalZ+.81f}, {.24f,.24f,.015f}, {1.0f,.96f,.80f});

    const auto p = handwheelCenter;
    box(v, {p.x,.075f,p.z}, {.61f,.15f,.42f}, iron);
    box(v, {p.x,.58f,p.z+.09f}, {.15f,1.0f,.19f}, ironEdge);
    box(v, {p.x,p.y,p.z+.10f}, {.68f,.69f,.15f}, iron);
    box(v, {p.x,p.y,p.z+.017f}, {.60f,.60f,.023f}, recess);
    ring(v, {p.x,p.y,p.z-.005f}, .279f, .012f, .020f, 16, ironEdge);
    box(v, p + glm::vec3{0,.409f,.088f}, {.58f,.212f,.13f}, iron);
    box(v, p + glm::vec3{0,.432f,.017f}, {.52f,.143f,.022f}, recess);
    for (std::size_t i = 0; i < weightTints.size(); ++i) {
        box(v, lensCenter(i) + glm::vec3{0,0,-.006f}, {.132f,.094f,.037f}, weightTints[i] * .57f);
    }
    // Short, engraved arrows match the two direction keys.
    for (const float sign : {-1.0f, 1.0f}) {
        const glm::vec3 tip{p.x+sign*.26f,p.y-.37f,p.z-.058f};
        const glm::vec3 tail{p.x+sign*.07f,p.y-.37f,p.z-.058f};
        stroke(v, tail, tip, .018f, .016f, brass);
        for (const float slope : {-1.0f, 1.0f}) {
            stroke(v, tip, tip + glm::vec3{-sign*.061f,slope*.044f,0}, .018f,.016f,brass);
        }
    }
}

inline void addChamber(std::vector<MeshVertex>& v) {
    const float length = farZ - frontZ;
    const float midpoint = (frontZ + farZ) * .5f;
    const float outer = halfExtent + shellThickness * .5f;
    constexpr glm::vec3 roof{.67f,.64f,.54f};
    constexpr glm::vec3 floor{.39f,.34f,.24f};
    constexpr glm::vec3 green{.40f,.50f,.42f};
    constexpr glm::vec3 plaster{.62f,.54f,.47f};
    const float wide = 2 * (halfExtent + shellThickness);
    box(v, {0,-outer,midpoint}, {wide,shellThickness,length}, floor);
    box(v, {0, outer,midpoint}, {wide,shellThickness,length}, roof);
    box(v, {-outer,0,midpoint}, {shellThickness,2*halfExtent,length}, green);
    box(v, { outer,0,midpoint}, {shellThickness,2*halfExtent,length}, plaster);
    for (const auto& rim : rearRim) box(v, rim.center, rim.half * 2.0f, {.44f,.47f,.42f});

    for (const float z : {frontZ+.12f, farZ-.35f}) {
        const float bound = halfExtent + shellThickness * .86f;
        for (const float side : {-1.0f, 1.0f}) {
            box(v, {side*bound,0,z}, {.15f,2*bound,.21f}, iron);
            box(v, {0,side*bound,z}, {2*bound,.15f,.21f}, iron);
        }
    }
    for (std::size_t i = 0; i < weightRails.size(); ++i) {
        const auto& rail = weightRails[i];
        const auto tint = weightTints[i];
        const float sign = rail.center.x > 0.0f ? 1.0f : -1.0f;
        const float inside = rail.center.x - sign * rail.half.x;
        box(v, rail.center, rail.half * 2.0f, tint * .45f);
        for (const float strip : {-1.0f,1.0f}) {
            box(v, {inside - sign * .008f, rail.center.y,
                    rail.center.z + strip * (rail.half.z - .03f)},
                {.02f,2.0f*rail.half.y,.055f}, tint * .87f);
        }
        for (int rib = -4; rib <= 4; ++rib) {
            box(v, {inside - sign * .010f, rail.center.y + .47f*static_cast<float>(rib), rail.center.z},
                {.024f,.064f,2.0f*rail.half.z - .16f}, tint * .67f);
        }
        // Matching plates on the rim remain readable while the lane itself is nearly edge-on.
        box(v, {sign*(outer + .075f),0,frontZ+.034f}, {.062f,.85f,.075f}, tint * .82f);
        for (const float end : {-1.0f,1.0f}) {
            box(v, {inside - sign * .018f, rail.center.y + end * (rail.half.y - .055f), rail.center.z},
                {.037f,.112f,rail.half.z*2.0f - .08f}, ironEdge);
        }
    }
    // Each surface has a different finish so a half-turn remains legible at a glance.
    for (int i = -2; i <= 2; ++i) {
        const float a = i * .84f;
        box(v, {a,-halfExtent+.004f,midpoint}, {.014f,.008f,length-.22f}, {.24f,.22f,.17f});
        box(v, {-halfExtent+.004f,a,midpoint}, {.008f,.014f,length-.22f}, {.29f,.37f,.30f});
        box(v, {halfExtent-.004f,a,midpoint}, {.008f,.014f,length-.22f}, {.48f,.43f,.37f});
    }
    for (const float side : {-1.0f, 1.0f}) {
        box(v, {side*1.41f,halfExtent-.012f,midpoint}, {.091f,.024f,length-.84f}, {.92f,.86f,.66f});
        box(v, {side*1.89f,-halfExtent+.006f,midpoint}, {.034f,.012f,length-.28f}, {.59f,.49f,.30f});
    }
}

inline void addWall(std::vector<MeshVertex>& v) {
    box(v, wallBox.center, wallBox.half * 2.0f, {.66f,.61f,.52f});
    const float face = wallBox.center.z - wallBox.half.z - .006f;
    constexpr glm::vec3 crack{.29f,.28f,.24f};
    const std::array<glm::vec3, 7> split{{
        {-2.20f,.89f,face}, {-1.80f,.51f,face}, {-1.64f,.55f,face},
        {-1.24f,.03f,face}, {-1.29f,-.25f,face}, {-.92f,-.52f,face}, {-.83f,-1.03f,face}}};
    for (std::size_t i = 1; i < split.size(); ++i) stroke(v, split[i-1],split[i],.022f,.012f,crack);
    stroke(v, split[3], {-.66f,.17f,face}, .017f,.012f,crack);
    stroke(v, {-.66f,.17f,face}, {-.34f,.05f,face}, .013f,.012f,crack);
    stroke(v, split[5], {-1.39f,-.73f,face}, .014f,.012f,crack);
}

inline void addChalkboard(std::vector<MeshVertex>& v) {
    const float face = boardHalfThickness;
    const float width = boardHalfWidth * 2.0f;
    box(v, {boardLength*.5f,0,0}, {boardLength,width,2*face}, timber);
    for (const float side : {-1.0f, 1.0f}) {
        const float darkFace = side * (face + .003f);
        box(v, {boardLength*.5f,0,darkFace}, {boardLength-.12f,width-.12f,.006f}, {.23f,.38f,.30f});
        // The paint stays inside the collision slab's width and length.
        for (int i = 0; i < 4; ++i) {
            box(v, {.54f+i*.51f, -.28f+i*.10f, side*(face+.0065f)},
                {.26f,.008f,.001f}, {.33f,.45f,.37f});
        }
    }
    for (const float side : {-1.0f, 1.0f}) {
        box(v, {.088f,side*(boardHalfWidth-.12f),0}, {.176f,.18f,2*face+.014f}, iron);
    }
}

inline void addWheel(std::vector<MeshVertex>& v) {
    ring(v, {0,0,0}, .209f,.036f,.039f, 12, brass);
    for (int index = 0; index < 3; ++index) {
        const float a = index * 2.0f * pi / 3.0f;
        const glm::vec3 radial{std::cos(a),std::sin(a),0};
        stroke(v, radial*.038f,radial*.207f,.030f,.033f,ironEdge);
    }
    box(v, {0,0,-.014f}, {.100f,.100f,.062f}, brass);
    box(v, {.202f,0,-.057f}, {.085f,.063f,.081f}, ironEdge);
}

inline void addProp(std::vector<MeshVertex>& v) {
    // One subdivided octahedron per ball, sampled from the exact collider radius.
    constexpr std::array<glm::vec3,6> axis{{
        {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}}};
    constexpr int faces[8][3]{{2,4,0},{2,1,4},{2,5,1},{2,0,5},
        {3,0,4},{3,4,1},{3,1,5},{3,5,0}};
    const auto triangle = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c) {
        const auto normal = glm::normalize(a + b + c);
        const glm::vec3 light = glm::normalize(glm::vec3{-.30f,.80f,-.50f});
        const float shading = .67f + .33f * (glm::dot(normal, light)*.5f+.5f);
        for (const auto point : {a,b,c}) v.push_back({point*.5f,glm::vec3{shading}});
    };
    for (const auto& face : faces) {
        const auto a=axis[face[0]], b=axis[face[1]], c=axis[face[2]];
        const auto ab=glm::normalize(a+b), bc=glm::normalize(b+c), ca=glm::normalize(c+a);
        triangle(a,ab,ca);
        triangle(ab,b,bc);
        triangle(ca,bc,c);
        triangle(ab,bc,ca);
    }
}

inline const VisualMesh& mesh() {
    static const VisualMesh data = [] {
        VisualMesh result{};
        const auto add = [&](auto populate) {
            const auto start = result.vertices.size();
            populate(result.vertices);
            return Range{static_cast<GLint>(start),static_cast<GLsizei>(result.vertices.size()-start)};
        };
        result.fixed = add(addFixed);
        result.chamber = add(addChamber);
        result.wall = add(addWall);
        result.chalkboard = add(addChalkboard);
        result.wheel = add(addWheel);
        result.lamp = add([](auto& vertices) {
            box(vertices, handwheelCenter + glm::vec3{0,.449f,.006f}, {.065f,.092f,.039f}, {1,1,1});
        });
        result.weightLens = add([](auto& vertices) {
            box(vertices, glm::vec3{0.0f}, {.100f,.064f,.034f}, {1,1,1});
        });
        result.prop = add(addProp);
        return result;
    }();
    return data;
}

} // namespace visual_detail

inline std::span<const MeshVertex> visualVertices() {
    return visual_detail::mesh().vertices;
}

inline void draw(const BoxRenderer& renderer, GLint visualFirst, const Room& room) {
    const auto& art = visual_detail::mesh();
    const glm::mat4 identity{1.0f};
    const auto part = [&](visual_detail::Range range, const glm::mat4& model,
                          glm::vec3 tint = glm::vec3{1.0f}) {
        renderer.drawMovableMesh(visualFirst + range.first, range.count, model, tint);
    };
    const auto chamber = room.model();
    part(art.fixed, identity);
    part(art.chamber, chamber);
    if (!room.wallBroken()) part(art.wall, chamber);
    part(art.chalkboard, room.boardModel());
    const auto wheel = glm::translate(identity, handwheelCenter - glm::vec3{0,0,.064f});
    part(art.wheel, glm::rotate(wheel, room.angle(), {0,0,1}));
    part(art.lamp, identity, room.bridgeReady() ? glm::vec3{.36f,.86f,.55f} : glm::vec3{.92f,.59f,.25f});
    const auto& captured = room.capturedWeights();
    for (std::size_t i = 0; i < captured.size(); ++i) {
        part(art.weightLens, glm::translate(identity, visual_detail::lensCenter(i)),
             visual_detail::weightTints[i] * (captured[i] ? 1.0f : .22f));
    }
    constexpr std::array<glm::vec3, 4> ballTints{{visual_detail::weightTints[0], visual_detail::weightTints[1],
        {.64f,.31f,.27f},{.77f,.74f,.57f}}};
    std::size_t color = 0;
    for (const auto& prop : room.props()) {
        const auto transform = glm::scale(glm::translate(identity,prop.position), glm::vec3{2*prop.radius});
        part(art.prop, transform, ballTints[color++ % ballTints.size()]);
    }
}

} // namespace home_rotate
