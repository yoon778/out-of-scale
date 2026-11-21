#include "SchoolCorridor.h"
#include "SchoolRoom.h"
#include "SchoolRenderer.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace school {
namespace {
constexpr float halfPi = 1.570796327f;
constexpr float doorAlignmentThreshold = .80f, bridgeAlignmentThreshold = .75f;
constexpr float cupboardAngleTolerance = .025f;
constexpr float falseHallWallX = 9.65f;
constexpr glm::vec3 classroomDoorSize{1.25f, 2.286f, .072f};
constexpr glm::vec3 brass{.67f, .53f, .30f};
constexpr float gapNear = -12.0f, gapFar = -15.65f, stairShift = -3.65f;
constexpr float bridgeWidth = 1.00f;
constexpr float stairStart = -12.72f + stairShift, stairRun = .6f, stairRise = .2f;
const glm::vec3 plaster{0.86f, 0.84f, 0.75f}, paint{0.45f, 0.57f, 0.49f};
const glm::vec3 wood{0.54f, 0.36f, 0.20f}, metal{0.31f, 0.38f, 0.36f};
Box box(glm::vec3 center, glm::vec3 size) {
    return {center - size * .5f, center + size * .5f};
}
bool touches(const Camera& camera, const Box& b) {
    const auto p = camera.feet;
    const float dx = p.x - std::clamp(p.x, b.min.x, b.max.x);
    const float dz = p.z - std::clamp(p.z, b.min.z, b.max.z);
    return dx * dx + dz * dz < SchoolRoom::radius * SchoolRoom::radius &&
           p.y + SchoolRoom::bodyHeight > b.min.y + .002f && p.y < b.max.y - .002f;
}
void beveledBoard(Renderer& r, glm::vec3 c, glm::vec3 size, glm::vec3 color, float bevel) {
    const auto half = size * .5f;
    r.box(c - glm::vec3(0, bevel * .5f, 0), size - glm::vec3(0, bevel, 0), color);
    r.quad(c + glm::vec3(0, half.y, 0), {half.x - bevel, 0, 0}, {0, 0, -half.z + bevel}, color * 1.025f);
    const glm::vec2 signs[] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    for (int i = 0; i < 4; ++i) {
        const auto a = signs[i], b = signs[(i + 1) % 4];
        const auto lowA = c + glm::vec3(a.x * half.x, half.y - bevel, a.y * half.z);
        const auto lowB = c + glm::vec3(b.x * half.x, half.y - bevel, b.y * half.z);
        const auto topA = c + glm::vec3(a.x * (half.x - bevel), half.y, a.y * (half.z - bevel));
        const auto topB = c + glm::vec3(b.x * (half.x - bevel), half.y, b.y * (half.z - bevel));
        r.triangle(lowA, topA, topB, color);
        r.triangle(lowA, topB, lowB, color);
    }
}
} // namespace
float advanceAlignmentHold(float held, float score, float threshold, float dt) {
    if (score >= threshold)
        return held + dt;
    if (score >= threshold - .035f)
        return std::max(0.f, held - 2.f * dt);
    return 0;
}
SchoolCorridor::SchoolCorridor() {
    build();
    buildDrawing();
    buildBridgeDrawing();
}
void SchoolCorridor::reset() {
    angle_ = 0;
    angularSpeed_ = 0;
    targetOpen_ = false;
    classroomKeyTaken_ = classroomDoorUnlocked_ = false;
    classroomDoorOpening_ = 0;
    recognition_ = formation_ = opening_ = 0;
    forming_ = bridgeForming_ = false;
    bridgeHeld_ = bridgeFormation_ = 0;
}
void SchoolCorridor::build() {
    auto solid = [&](glm::vec3 c, glm::vec3 s, glm::vec3 color, bool visible = true) {
        solids_.push_back({c, s, color, visible});
    };
    auto detail = [&](glm::vec3 c, glm::vec3 s, glm::vec3 color, float bevel = 0) {
        details_.push_back({c, s, color, true, bevel});
    };
    auto floor = [&](float x0, float x1, float z0, float z1, float y = floorHeight) {
        solid({(x0 + x1) * .5f, y - .12f, (z0 + z1) * .5f}, {x1 - x0, .24f, z1 - z0}, {.65f, .66f, .60f});
        for (float x = x0; x < x1 - .01f; x += .8f)
            for (float z = z0; z < z1 - .01f; z += .8f) {
                float dx = std::min(.8f, x1 - x), dz = std::min(.8f, z1 - z);
                float shade = .965f + .006f * (static_cast<int>((x - x0) * 5 + (z - z0) * 3) % 5);
                detail({x + dx * .5f, y + .002f, z + dz * .5f}, {dx - .006f, .004f, dz - .006f},
                       glm::vec3(.73f, .74f, .68f) * shade);
            }
    };
    auto wall = [&](float a, float b, float z, float low = 0, float high = 3.3f) {
        solid({(a + b) * .5f, floorHeight + (low + high) * .5f, z}, {b - a, high - low, .20f}, plaster);
        if (low == 0) {
            detail({(a + b) * .5f, floorHeight + .52f, z + .106f}, {b - a, 1.04f, .014f}, paint);
            detail({(a + b) * .5f, floorHeight + .09f, z + .12f}, {b - a, .18f, .025f}, paint * .77f);
            detail({(a + b) * .5f, floorHeight + 1.06f, z + .12f}, {b - a, .04f, .025f}, paint * .90f);
        }
    };
    auto side = [&](float x, float z0, float z1) {
        solid({x, floorHeight + 1.65f, (z0 + z1) * .5f}, {.2f, 3.3f, z1 - z0}, plaster);
    };
    // The old materials bay opens into this passage, then turns east into the corridor.
    floor(2.37f, 3.73f, -8.4f, -7.0f);
    side(2.27f, -8.3f, -7.0f);
    side(3.83f, -8.3f, -7.0f);
    solid({3.05f, 4.4f, -7.7f}, {1.76f, .15f, 1.4f}, plaster);
    // Carry the same wall finish through the old materials bay and the short connector.
    for (float x : {2.378f, 3.722f}) {
        detail({x, 2.32f, -6.65f}, {.012f, 1.04f, 3.25f}, paint);
        detail({x, 1.90f, -6.65f}, {.022f, .20f, 3.25f}, paint * .77f);
        detail({x, 2.86f, -6.65f}, {.022f, .04f, 3.25f}, paint * .90f);
    }
    detail({3.05f, 1.806f, -7.03f}, {1.34f, .012f, .10f}, wood);
    detail({3.05f, 4.28f, -7.8f}, {.62f, .07f, .26f}, {.95f, .91f, .78f});
    floor(2.37f, 25.1f, -11.5f, -8.3f);
    side(2.27f, -11.6f, -8.3f);
    detail({2.378f, 2.32f, -9.9f}, {.012f, 1.04f, 3.2f}, paint);
    detail({2.378f, 1.90f, -9.9f}, {.022f, .20f, 3.2f}, paint * .77f);
    detail({2.378f, 2.86f, -9.9f}, {.022f, .04f, 3.2f}, paint * .90f);
    side(25.2f, -17, -8.3f);
    solid({13.735f, 5.2f, -9.9f}, {22.93f, .2f, 3.4f}, plaster);
    // Keep the pane collision at the window while the view extends into the courtyard.
    wall(3.73f, 25.1f, -8.2f, 0, 1.0f);
    wall(3.73f, 25.1f, -8.2f, 2.65f, 3.3f);
    detail({14.415f, 2.30f, -8.306f}, {21.37f, 1.0f, .014f}, paint);
    detail({14.415f, 1.90f, -8.32f}, {21.37f, .20f, .025f}, paint * .77f);
    for (float x = 3.73f; x < 25.1f; x += 3.55f) {
        float length = std::min(3.55f, 25.1f - x);
        solid({x + length * .5f, 3.625f, -8.2f}, {length, 1.65f, .06f}, plaster, false);
        detail({x, 3.625f, -8.32f}, {.09f, 1.7f, .12f}, wood);
        if (length > 1.f)
            detail({x + length * .5f, 3.625f, -8.32f}, {.055f, 1.64f, .10f}, wood);
        detail({x + length * .5f, 3.65f, -8.33f}, {length, .06f, .10f}, wood);
        detail({x + length * .5f, 4.445f, -8.31f}, {length, .07f, .15f}, wood);
        detail({x + length * .5f, 2.8f, -8.37f}, {length, .09f, .3f}, wood);
    }
    detail({25.065f, 3.625f, -8.32f}, {.07f, 1.70f, .12f}, wood);
    detail({17.1f, -.12f, -2.0f}, {23.f, .22f, 12.4f}, {.48f, .54f, .39f});
    detail({16.1f, .015f, -6.85f}, {21.f, .05f, 1.8f}, {.65f, .63f, .54f});
    for (const glm::vec4 tree : {glm::vec4{7.8f, 3.30f, -5.02f, 1.10f},
                                 {10.6f, 3.65f, -2.80f, 1.36f},
                                 {13.7f, 2.85f, -4.83f, 1.08f},
                                 {16.7f, 3.92f, -3.22f, 1.40f},
                                 {20.3f, 3.10f, -5.18f, 1.22f},
                                 {24.7f, 4.05f, -3.75f, 1.48f}})
        foliage_.push_back(tree);
    wall(2.37f, 5.6f, -11.6f);
    wall(5.6f, 6.9f, -11.6f, 2.3f);
    wall(6.9f, 7.6f, -11.6f);
    wall(11.05f, 12.30f, -11.6f, 2.3f);
    wall(12.30f, 12.8f, -11.6f);
    wall(12.8f, 14.1f, -11.6f, 2.3f);
    wall(14.1f, 14.7f, -11.6f);
    wall(17.8f, 18.04f, -11.6f);
    wall(18.04f, 19.36f, -11.6f, 2.2f);
    wall(19.36f, 21.9f, -11.6f);
    for (const auto opening : {glm::vec2(5.6f, 6.9f), glm::vec2(11.05f, 12.30f), glm::vec2(12.8f, 14.1f)}) {
        floor(opening.x, opening.y, -11.7f, -11.5f);
        detail({(opening.x + opening.y) * .5f, floorHeight + .011f, -11.60f},
               {opening.y - opening.x, .018f, .065f}, wood * .86f);
    }
    for (const auto window : {Window{7.6f, 11.05f}, Window{14.7f, 17.8f}}) {
        wall(window.start, window.end, -11.6f, 0, 1.05f);
        wall(window.start, window.end, -11.6f, 2.4f);
        // Transparent closed panes stop jumping through the observation window.
        solid({(window.start + window.end) * .5f, 3.525f, -11.6f}, {window.end - window.start, 1.35f, .035f},
              plaster, false);
        for (float x = window.start; x < window.end + .01f; x += 1.025f)
            detail({x, 3.525f, -11.6f}, {.055f, 1.4f, .10f}, wood);
        for (float y : {2.85f, 4.2f})
            detail({(window.start + window.end) * .5f, y, -11.6f}, {window.end - window.start, .07f, .15f},
                   wood);
    }
    // 2-1's rear doorway leads around the painted cross-wall; glass remains closed.
    solid({falseHallWallX, 3.45f, -9.90f}, {.20f, 3.30f, 3.20f}, plaster);
    for (float x : {falseHallWallX, 20.60f})
        detail({x, 3.625f, -8.25f}, {.2f, 1.65f, .25f}, plaster);
    for (const auto range : {glm::vec2(5.2f, 12.3f), glm::vec2(12.7f, 20.5f)}) {
        floor(range.x, range.y, -16.6f, -11.7f);
        side(range.x - .1f, -16.7f, -11.7f);
        side(range.y + .1f, -16.7f, -11.7f);
        wall(range.x, range.y, -16.7f);
        solid({(range.x + range.y) * .5f, 5.2f, -14.2f}, {range.y - range.x, .2f, 5.0f}, plaster);
        detail({(range.x + range.y) * .5f, 3.55f, -16.57f}, {3.5f, 1.2f, .07f}, wood);
        detail({(range.x + range.y) * .5f, 3.55f, -16.52f}, {3.35f, 1.06f, .02f}, {.17f, .31f, .25f});
        for (int i = 0; i < 3; i++) {
            float x = range.x + 1.1f + i * 1.65f;
            solid({x, 2.19f, -15.1f}, {1.0f, .78f, .62f}, wood, false);
            detail({x, 2.54f, -15.1f}, {1.0f, .08f, .62f}, wood, .015f);
            for (float dx : {-.4f, .4f})
                for (float dz : {-.23f, .23f})
                    detail({x + dx, 2.15f, -15.1f + dz}, {.045f, .7f, .045f}, metal);
            detail({x, 2.6f, -15.1f}, {.32f, .04f, .23f}, {.47f, .39f, .26f});
        }
    }
    // Cleaning furniture stays beside the route. The perspective drawing is the actual puzzle.
    auto deskStack = [&](float x, float z) {
        furnitureBases_.push_back({x, floorHeight, z});
        solid({x, floorHeight + 1.02f, z}, {1.06f, 2.04f, .94f}, wood, false);
        for (float y : {.76f, 1.5f}) {
            detail({x, floorHeight + y, z}, {1.06f, .09f, .94f}, wood, .018f);
            detail({x, floorHeight + y - .14f, z}, {.92f, .2f, .72f}, wood * .82f);
            for (float dx : {-.43f, .43f})
                for (float dz : {-.36f, .36f})
                    detail({x + dx, floorHeight + y - .38f, z + dz}, {.045f, .68f, .045f}, metal);
        }
        detail({x, floorHeight + 1.57f, z}, {.44f, .07f, .43f}, wood, .012f);
        detail({x, floorHeight + 1.33f, z + .22f}, {.44f, .4f, .055f}, wood);
        for (float dx : {-.18f, .18f})
            for (float dz : {-.18f, .18f})
                detail({x + dx, floorHeight + 1.81f, z + dz}, {.035f, .46f, .035f}, metal);
    };
    deskStack(10.40f, -10.94f);
    deskStack(12.3f, -8.88f);
    deskStack(15.0f, -8.88f);
    // A normal cupboard and the far wall supply separate everyday edges for the doorway.
    solid({18.70f, 2.85f, -11.94f}, {1.35f, 2.1f, .66f}, paint, false);
    detail({18.70f, 2.85f, -12.25f}, {1.35f, 2.1f, .08f}, paint);
    for (float x : {18.04f, 19.36f})
        detail({x, 2.85f, -11.94f}, {.08f, 2.1f, .66f}, paint);
    for (float y : {1.85f, 3.9f})
        detail({18.70f, y, -11.94f}, {1.35f, .08f, .66f}, paint);
    // A classroom notice ledge belongs to the return route, away from the cupboard handle.
    solid({14.95f, 2.74f, -11.40f}, {1.12f, .055f, .23f}, wood);
    detail({14.83f, 2.795f, -11.40f}, {.33f, .045f, .19f}, plaster);
    detail({15.19f, 2.80f, -11.40f}, {.25f, .055f, .16f}, paint * .8f);
    // A real partition, with a doorway-sized section that only becomes a passage on materialization.
    for (float z : {-11.1f, -8.7f}) {
        solid({20.60f, 3.45f, z}, {.20f, 3.3f, .80f}, plaster);
        detail({20.493f, 2.32f, z}, {.012f, 1.04f, .80f}, paint);
        detail({20.485f, 1.9f, z}, {.02f, .20f, .80f}, paint * .77f);
    }
    solid({20.60f, 4.65f, -9.9f}, {.20f, .9f, 1.6f}, plaster);
    // The service stairwell has a removable crossing across its upper landing.
    floor(21.99f, 25.1f, -12.0f, -11.5f);
    wall(21.99f, 25.1f, -11.6f, 2.25f);
    side(21.89f, -17 + stairShift, -11.6f);
    side(25.2f, -17 + stairShift, -17);
    floor(21.99f, 25.1f, gapFar, gapNear, 0);
    // Close the void under the near landing when viewed back up the stairs.
    solid({23.545f, .78f, gapNear + .10f}, {3.11f, 1.56f, .2f}, plaster);
    // Meet the landing underside so the two front faces do not overlap at the lip.
    solid({23.545f, .78f, gapFar - .10f}, {3.11f, 1.56f, .2f}, plaster);
    floor(21.99f, 25.1f, gapFar - .72f, gapFar);
    // Return stairs serve the inspection level, without providing a route up the far wall.
    for (int i = 0; i < 6; ++i) {
        const float top = floorHeight - (i + 1) * .30f;
        if (top > .01f)
            solid({22.39f, top * .5f, gapNear - .25f - i * .50f}, {.80f, top, .50f}, {.59f, .62f, .58f});
        if (top > .01f)
            detail({22.39f, top + .007f, gapNear - .474f - i * .50f}, {.78f, .014f, .045f}, metal);
    }
    for (float x : {21.89f, 25.2f})
        solid({x, .9f, (gapNear + gapFar) * .5f}, {.2f, 1.8f, gapNear - gapFar}, plaster);
    for (const auto span : {glm::vec2(22.85f, 23.39f), glm::vec2(24.55f, 25.05f)}) {
        const float width = span.y - span.x;
        solid({(span.x + span.y) * .5f, 2.22f, gapNear + .06f}, {width, .84f, .045f}, metal, false);
        for (float x : {span.x, span.y})
            detail({x, 2.22f, gapNear + .06f}, {.04f, .84f, .045f}, metal);
        for (float y : {2.19f, 2.63f})
            detail({(span.x + span.y) * .5f, y, gapNear + .06f}, {width + .05f, .045f, .048f}, metal);
    }
    detail({22.45f, 1.81f, gapNear + .026f}, {.92f, .02f, .046f}, metal);
    detail({bridgeEye.x, 1.81f, gapNear + .026f}, {bridgeWidth, .02f, .046f}, metal);
    for (float z : {-12.6f, -15.03f, -18.45f}) {
        detail({23.55f, 5.033f, z}, {.43f, .13f, .80f}, metal);
        detail({23.55f, 4.962f, z}, {.35f, .02f, .72f}, {.96f, .93f, .80f});
    }
    detail({25.065f, 3.59f, -11.43f}, {.05f, .48f, .93f}, paint);
    for (int i = 0; i < 6; i++) {
        const float top = floorHeight - (i + 1) * stairRise;
        const float nose = stairStart - (i + 1) * stairRun;
        solid({23.545f, top - .12f, nose + stairRun * .5f}, {3.11f, .24f, stairRun}, {.65f, .66f, .60f});
        detail({23.545f, top + .007f, nose + .032f}, {3.06f, .014f, .062f}, metal * 1.12f);
        detail({23.545f, top - .04f, nose - .003f}, {3.06f, .08f, .006f}, {.47f, .49f, .44f});
    }
    floor(21.99f, 25.1f, -17 + stairShift, -16.32f + stairShift, .6f);
    // Stairwell walls extend below the corridor floor to enclose the descending flight.
    for (float x : {21.89f, 25.2f})
        solid({x, 1.2f, -14.3f + stairShift}, {.2f, 1.2f, 5.4f}, plaster);
    solid({22.32f, 2.85f, -17.1f + stairShift}, {.86f, 4.5f, .2f}, plaster);
    solid({24.78f, 2.85f, -17.1f + stairShift}, {.86f, 4.5f, .2f}, plaster);
    solid({23.55f, 4.0f, -17.1f + stairShift}, {1.6f, 2.2f, .2f}, plaster);
    floor(22.75f, 24.35f, -18.8f + stairShift, -17.0f + stairShift, .6f);
    for (float x : {22.65f, 24.45f})
        solid({x, 1.85f, -17.95f + stairShift}, {.2f, 2.5f, 1.9f}, plaster);
    solid({23.55f, 1.85f, -18.9f + stairShift}, {2.0f, 2.5f, .2f}, plaster);
    solid({23.55f, 3.2f, -17.95f + stairShift}, {2.0f, .2f, 1.9f}, plaster);
    solid({23.55f, 5.2f, (-11.6f - 17.0f + stairShift) * .5f}, {3.3f, .2f, 5.4f - stairShift}, plaster);
}
void SchoolCorridor::buildDrawing() {
    // Project our own school-door drawing from one eye onto two actual receiving surfaces.
    // At the reference eye, near and far pieces subtend exactly the same rays as the real door.
    // Technique reference: Phil Fortier (Pillow Castle), PlayStation Blog, 2020-06-30.
    struct Rectangle {
        float y0, y1, z0, z1;
        glm::vec3 color;
        bool frame;
        DoorMaterial material;
    };
    std::vector<Rectangle> rectangles;
    auto rectangle = [&](float y0, float y1, float z0, float z1, glm::vec3 color, bool frame = false,
                         DoorMaterial material = DoorMaterial::infill) {
        // Subtract overpainted regions before triangulation. Coplanar ink must not fight for depth.
        std::vector<Rectangle> uncovered;
        for (const auto& old : rectangles) {
            const float ya = std::max(y0, old.y0), yb = std::min(y1, old.y1);
            const float za = std::max(z0, old.z0), zb = std::min(z1, old.z1);
            if (ya >= yb || za >= zb) {
                uncovered.push_back(old);
                continue;
            }
            auto part = [&](float a, float b, float c, float d) {
                if (b > a && d > c)
                    uncovered.push_back({a, b, c, d, old.color, old.frame, old.material});
            };
            part(old.y0, ya, old.z0, old.z1);
            part(yb, old.y1, old.z0, old.z1);
            part(ya, yb, old.z0, za);
            part(ya, yb, zb, old.z1);
        }
        rectangles = std::move(uncovered);
        rectangles.push_back({y0, y1, z0, z1, color, frame, material});
    };
    const glm::vec3 frame{.43f, .32f, .22f}, timber{.62f, .47f, .30f}, glass{.53f, .66f, .63f};
    const float nearX = hinge.x - .042f, farX = 20.478f, x = illusionFocus.x;
    const float nearScale = (nearX - illusionEye.x) / (x - illusionEye.x);
    const float split = illusionEye.z + (hinge.z + doorWidth - illusionEye.z) / nearScale;
    rectangle(1.82f, 4.2f, -10.75f, -10.65f, frame, true, DoorMaterial::timber);
    rectangle(1.82f, 4.2f, -9.15f, -9.05f, frame, true, DoorMaterial::timber);
    rectangle(4.10f, 4.2f, -10.65f, -9.15f, frame, true, DoorMaterial::timber);
    rectangle(1.82f, 1.87f, -10.65f, -9.15f, frame, true, DoorMaterial::timber);
    rectangle(1.87f, 4.10f, -10.65f, -9.15f, timber);
    rectangle(2.81f, 4.01f, -10.55f, -9.25f, frame, false, DoorMaterial::timber);
    rectangle(2.86f, 3.96f, -10.50f, -9.30f, glass, false, DoorMaterial::paper);
    // Cleaning/check sheets read as ordinary notices nearby. Their rows become wired glass on formation.
    for (float z = -10.30f; z < -9.30f; z += .20f)
        rectangle(2.86f, 3.72f, z, z + .006f, glass * .90f, false, DoorMaterial::ruling);
    for (float y = 3.04f; y < 3.73f; y += .18f)
        rectangle(y, y + .006f, -10.50f, -9.30f, glass * .90f, false, DoorMaterial::ruling);
    // Close both sheets locally: neither should look like half of a missing drawing.
    rectangle(2.81f, 4.01f, split - .025f, split + .025f, frame, false, DoorMaterial::timber);
    rectangle(2.03f, 2.72f, -10.55f, -9.25f, frame * 1.2f);
    rectangle(2.07f, 2.68f, -10.51f, -9.29f, timber * .91f);
    rectangle(2.51f, 2.77f, -9.39f, -9.30f, {.23f, .29f, .28f});
    rectangle(2.55f, 2.73f, -9.37f, -9.32f, {.70f, .74f, .71f});
    for (const auto& rect : rectangles)
        for (bool leaf : {true, false}) {
            const float a = leaf ? rect.z0 : std::max(rect.z0, split);
            const float b = leaf ? std::min(rect.z1, split) : rect.z1;
            if (b <= a)
                continue;
            InkPatch patch{{glm::vec3{x, rect.y0, a}, {x, rect.y0, b}, {x, rect.y1, b}, {x, rect.y1, a}},
                           {},
                           rect.color,
                           leaf,
                           rect.frame,
                           rect.material};
            const float scale = ((leaf ? nearX : farX) - illusionEye.x) / (x - illusionEye.x);
            for (int i = 0; i < 4; ++i)
                patch.receiver[i] = illusionEye + (patch.target[i] - illusionEye) * scale;
            drawing_.push_back(patch);
        }
}
glm::vec3 SchoolCorridor::paintedPoint(const InkPatch& patch, int corner) const {
    const auto p = patch.receiver[corner];
    if (!patch.onLeaf)
        return p;
    const float along = p.z - hinge.z, out = hinge.x - p.x;
    return hinge + glm::vec3(along * std::cos(angle_) - out * std::sin(angle_), p.y - hinge.y,
                             along * std::sin(angle_) + out * std::cos(angle_));
}
std::optional<float> SchoolCorridor::doorViewAlignment(const Camera& camera) const {
    const auto eye = camera.eye();
    if (!camera.grounded || std::abs(camera.feet.y - floorHeight) > .08f ||
        eye.x >= hinge.x - .25f || eye.x < 12.5f)
        return std::nullopt;
    if (glm::dot(camera.direction(), glm::normalize(illusionFocus - eye)) < .96f)
        return std::nullopt;
    float error = 0;
    for (const auto& patch : drawing_) {
        if (!patch.frame)
            continue;
        for (int i = 0; i < 4; ++i) {
            const auto ink = paintedPoint(patch, i) - eye;
            const auto direction = glm::normalize(ink);
            const float distance = glm::length(ink) - .045f;
            error = std::max(error, glm::length(direction - glm::normalize(patch.target[i] - eye)));
            // Test real visibility, so looking through opaque walls cannot recognize the drawing.
            for (const auto& s : solids_) {
                const float hit = SchoolRoom::rayBox(eye, direction, box(s.center, s.size));
                if (hit >= 0 && hit < distance)
                    return std::nullopt;
            }
        }
    }
    return std::clamp(1 - error / .12f, 0.0f, 1.0f);
}
void SchoolCorridor::updateIllusion(const Camera& camera, float dt) {
    if (!forming_) {
        const float score = std::abs(angle_ - halfPi) < cupboardAngleTolerance ? alignment(camera) : 0;
        recognition_ = advanceAlignmentHold(recognition_, score, doorAlignmentThreshold, dt);
        if (score >= doorAlignmentThreshold && recognition_ >= recognitionDuration) {
            forming_ = true;
            targetOpen_ = false;
        }
        return;
    }
    if (formation_ < 1)
        formation_ = std::min(1.0f, formation_ + dt / formationDuration);
    else
        opening_ = std::min(1.0f, opening_ + dt / openingDuration);
}
void SchoolCorridor::drawFalseHall(Renderer& r) const {
    // Paint the view into an imaginary hall onto the solid cross-wall.
    constexpr glm::vec3 eye{4.8f, 3.45f, -9.90f};
    constexpr float a = falseHallWallX - .10f, b = 24.75f;
    constexpr float left = -11.5f, right = -8.3f, low = floorHeight + .004f, high = 5.10f;
    if (!r.visible({a, 3.45f, -9.90f}, {.04f, 1.65f, 1.60f}))
        return;
    auto project = [&](glm::vec3 p, float offset) {
        return eye + (p - eye) * ((a - .002f - offset - eye.x) / (p.x - eye.x));
    };
    auto patch = [&](glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec3 color, float offset = 0.f) {
        p0 = project(p0, offset);
        p1 = project(p1, offset);
        p2 = project(p2, offset);
        p3 = project(p3, offset);
        if (glm::cross(p1 - p0, p2 - p0).x > 0)
            std::swap(p1, p3);
        r.triangle(p0, p1, p2, color);
        r.triangle(p0, p2, p3, color);
    };
    auto sidePatch = [&](float x0, float x1, float y0, float y1, float z, glm::vec3 color, float offset = 0.f) {
        patch({x0, y0, z}, {x1, y0, z}, {x1, y1, z}, {x0, y1, z}, color, offset);
    };
    auto horizontalPatch = [&](float x0, float x1, float z0, float z1, float y, glm::vec3 color,
                               float offset = 0.f) {
        patch({x0, y, z0}, {x1, y, z0}, {x1, y, z1}, {x0, y, z1}, color, offset);
    };
    auto endPatch = [&](float z0, float z1, float y0, float y1, glm::vec3 color, float offset = 0.f) {
        patch({b, y0, z0}, {b, y0, z1}, {b, y1, z1}, {b, y1, z0}, color, offset);
    };

    // Match the nearby wall shading and tile grid as they cross onto the painting.
    const glm::vec3 floorTint{.75f, .76f, .70f};
    const glm::vec3 grout{.68f, .69f, .63f};
    horizontalPatch(a, b, left, right, low, floorTint);
    for (float x = 9.57f; x < b; x += .8f)
        horizontalPatch(x, x + .009f, left, right, low, grout, .001f);
    for (float z = left + .8f; z < right - .01f; z += .8f)
        horizontalPatch(a, b, z, z + .009f, low, grout, .002f);
    horizontalPatch(a, b, left, right, high, plaster * .80f);
    for (float z : {left, right}) {
        const float shade = z == left ? .88f : .80f;
        const float dado = z == left ? 2.84f : 2.80f;
        sidePatch(a, b, low, dado, z, paint * shade);
        sidePatch(a, b, dado, high, z, plaster * shade);
        sidePatch(a, b, low, 1.99f, z, paint * (.77f * shade), .001f);
        sidePatch(a, b, dado, dado + .04f, z, paint * (.90f * shade), .002f);
    }

    for (float x : {12.8f, 17.0f, 21.2f}) {
        sidePatch(x, x + 1.40f, low, 4.10f, left, wood * .71f, .003f);
        sidePatch(x + .07f, x + 1.33f, low, 4.02f, left, wood * .92f, .004f);
        sidePatch(x + .23f, x + 1.17f, 2.94f, 3.82f, left, paint * .86f, .006f);
    }
    for (float x : {10.04f, 13.59f, 17.14f, 20.69f}) {
        sidePatch(x, x + 3.43f, 2.80f, 4.45f, right, wood * .82f, .003f);
        sidePatch(x + .055f, x + 3.375f, 2.88f, 4.38f, right, {.65f, .78f, .81f}, .004f);
        sidePatch(x + .055f, x + 3.375f, 2.88f, 3.24f, right, {.48f, .59f, .48f}, .006f);
        sidePatch(x + 1.69f, x + 1.74f, 2.88f, 4.38f, right, wood * .88f, .007f);
        sidePatch(x, x + 3.43f, 3.62f, 3.68f, right, wood * .88f, .008f);
    }
    for (float x : {12.0f, 16.0f, 20.0f, 23.5f}) {
        horizontalPatch(x - .525f, x + .525f, -10.06f, -9.74f, 4.98f, metal * .93f, .003f);
        horizontalPatch(x - .48f, x + .48f, -10.02f, -9.78f, 4.96f, {.98f, .96f, .85f}, .004f);
    }
    endPatch(left, right, low, high, plaster * .85f);
    endPatch(left, right, low, 2.82f, paint * .85f, .001f);
    endPatch(-10.59f, -9.21f, low, 4.12f, wood * .72f, .003f);
    endPatch(-10.52f, -9.28f, low, 4.04f, wood * .89f, .004f);
    endPatch(-10.35f, -9.45f, 2.90f, 3.82f, paint * .87f, .006f);
    const auto latch = eye + (classroomKeySpot - eye) * ((b - eye.x) / (classroomKeySpot.x - eye.x));
    endPatch(latch.z - .065f, latch.z + .065f, latch.y - .14f, latch.y + .14f, brass * .83f, .007f);
}
Box SchoolCorridor::classroomDoorBox(float opening) const {
    const float slide = 1.32f * glm::smoothstep(0.0f, 1.0f, opening);
    return box(classroomRearDoor - glm::vec3(slide, 0, 0), classroomDoorSize);
}
void SchoolCorridor::drawClassroomLock(Renderer& r) const {
    const auto door = classroomDoorBox(classroomDoorOpening_);
    const auto center = (door.min + door.max) * .5f;
    r.box(center, classroomDoorSize, wood);
    r.box(center + glm::vec3(0, .49f, -.038f), {.93f, .85f, .012f}, wood * .76f);
    r.box(center + glm::vec3(0, .49f, -.047f), {.84f, .76f, .010f}, paint * .88f);
    for (float y : {.30f, .49f, .68f})
        r.box(center + glm::vec3(0, y, -.054f), {.84f, .022f, .012f}, wood * .72f);
    for (float z : {-.049f, .049f})
        r.box(center + glm::vec3(.45f, -.01f, z), {.075f, .22f, .024f}, metal);
    r.box(center + glm::vec3(.45f, -.15f, -.064f), {.042f, .050f, .025f}, brass * .84f);
    const auto notice = center + glm::vec3(-.06f, -.13f, -.041f);
    r.box(notice, {.46f, .25f, .010f}, {.84f, .82f, .71f});
    r.label(L"뒷문 열쇠", notice + glm::vec3(0, .052f, -.006f), {-.16f, 0, 0}, {0, .038f, 0}, metal);
    r.label(L"복도 끝에 반납", notice + glm::vec3(0, -.048f, -.006f), {-.19f, 0, 0}, {0, .035f, 0}, metal);

    const auto key = classroomKeySpot;
    const float hookLength = falseHallWallX - .10f - key.x;
    r.box(key + glm::vec3(hookLength * .5f, .041f, 0), {hookLength, .009f, .010f}, metal);
    if (classroomKeyTaken_ || !r.visible(key, {.05f, .06f, .015f}))
        return;
    // The key reads edge-on from the corridor entrance and face-on from the side.
    constexpr float fullCircle = 6.283185307f;
    for (int i = 0; i < 10; ++i) {
        const float a = i * fullCircle / 10, b = (i + 1) * fullCircle / 10;
        const auto ring = key + glm::vec3(0, .026f, 0);
        r.line(ring + glm::vec3(std::cos(a), std::sin(a), 0) * .018f,
               ring + glm::vec3(std::cos(b), std::sin(b), 0) * .018f, .006f, brass);
    }
    r.box(key + glm::vec3(0, -.015f, 0), {.011f, .052f, .009f}, brass);
    r.box(key + glm::vec3(.009f, -.030f, 0), {.023f, .009f, .009f}, brass);
    r.box(key + glm::vec3(.007f, -.041f, 0), {.019f, .007f, .009f}, brass);
}
SchoolCorridor::ClassroomAction SchoolCorridor::classroomAction(const Camera& camera) const {
    const auto eye = camera.eye(), direction = camera.direction();
    auto reaches = [&](const Box& target, float range) {
        const float distance = SchoolRoom::rayBox(eye, direction, target);
        if (distance < 0 || distance > range)
            return false;
        return std::none_of(solids_.begin(), solids_.end(), [&](const Solid& solid) {
            const float hit = SchoolRoom::rayBox(eye, direction, box(solid.center, solid.size));
            return hit >= 0 && hit < distance;
        });
    };
    if (!classroomKeyTaken_ && reaches(box(classroomKeySpot, {.075f, .11f, .055f}), 1.28f))
        return ClassroomAction::takeKey;
    if (classroomKeyTaken_ && !classroomDoorUnlocked_ && reaches(classroomDoorBox(0), 1.65f))
        return ClassroomAction::unlockDoor;
    return ClassroomAction::none;
}
const wchar_t* SchoolCorridor::classroomInteractionLabel(const Camera& camera) const {
    switch (classroomAction(camera)) {
    case ClassroomAction::takeKey: return L"열쇠 줍기";
    case ClassroomAction::unlockDoor: return L"잠금 풀기";
    default: return nullptr;
    }
}
bool SchoolCorridor::interactClassroom(const Camera& camera) {
    switch (classroomAction(camera)) {
    case ClassroomAction::takeKey:
        classroomKeyTaken_ = true;
        return true;
    case ClassroomAction::unlockDoor:
        classroomDoorUnlocked_ = true;
        return true;
    default:
        return false;
    }
}
void SchoolCorridor::buildBridgeDrawing() {
    // A single grating silhouette is reprojected onto the lower floor and the far riser.
    constexpr float floorY = .012f, wallZ = gapFar + .012f;
    const float scale = (floorY - bridgeEye.y) / (floorHeight - bridgeEye.y);
    const float split = bridgeEye.z + (wallZ - bridgeEye.z) / scale;
    auto strip = [&](float x0, float x1, float z0, float z1, glm::vec3 color, bool frame) {
        // Start the painted tread just past the lip, so edge rays do not graze the real landing.
        z1 = std::min(z1, gapNear - .026f);
        for (bool floor : {true, false}) {
            float za = floor ? std::max(z0, split) : z0;
            float zb = floor ? z1 : std::min(z1, split);
            if (za >= zb)
                continue;
            InkPatch patch{{glm::vec3{x0, floorHeight, za},
                            {x0, floorHeight, zb},
                            {x1, floorHeight, zb},
                            {x1, floorHeight, za}},
                           {},
                           color,
                           floor,
                           frame};
            for (size_t i = 0; i < patch.target.size(); ++i) {
                const auto ray = patch.target[i] - bridgeEye;
                patch.receiver[i] = bridgeEye + ray * (floor ? scale : (wallZ - bridgeEye.z) / ray.z);
            }
            bridgeDrawing_.push_back(patch);
        }
    };
    constexpr float left = bridgeEye.x - bridgeWidth * .5f, right = bridgeEye.x + bridgeWidth * .5f;
    const glm::vec3 steel{.34f, .46f, .48f}, tread{.52f, .61f, .61f};
    strip(left, left + .09f, gapFar, gapNear, steel, true);
    strip(right - .09f, right, gapFar, gapNear, steel, true);
    for (int i = 0; i < 21; ++i) {
        const float z = gapNear - .175f * i;
        strip(left + .09f, right - .09f, z - .145f, z, i % 4 == 0 ? steel : tread, false);
    }
}
std::optional<float> SchoolCorridor::bridgeViewAlignment(const Camera& camera) const {
    if (!doorwayReady() || !camera.grounded || std::abs(camera.feet.y - floorHeight) > .08f ||
        camera.feet.z < gapNear || camera.feet.x < 22.5f || camera.feet.x > 24.7f)
        return std::nullopt;
    const auto eye = camera.eye();
    if (glm::dot(camera.direction(), glm::normalize(bridgeFocus - eye)) < .965f)
        return std::nullopt;
    float error = 0;
    for (const auto& p : bridgeDrawing_) {
        if (!p.frame)
            continue;
        for (size_t i = 0; i < p.target.size(); ++i) {
            const auto ray = p.receiver[i] - eye;
            const auto direction = glm::normalize(ray);
            const float distance = glm::length(ray) - .025f;
            error = std::max(error, glm::length(direction - glm::normalize(p.target[i] - eye)));
            for (const auto& s : solids_) {
                const float hit = SchoolRoom::rayBox(eye, direction, box(s.center, s.size));
                if (hit >= 0 && hit < distance)
                    return std::nullopt;
            }
        }
    }
    return std::clamp(1 - error / .20f, 0.0f, 1.0f);
}
std::optional<AlignmentFeedback> SchoolCorridor::alignmentFeedback(const Camera& camera) const {
    if (!forming_ && std::abs(angle_ - halfPi) < cupboardAngleTolerance) {
        if (const auto score = doorViewAlignment(camera))
            return AlignmentFeedback{*score, doorAlignmentThreshold, *score >= doorAlignmentThreshold,
                                     std::clamp(recognition_ / recognitionDuration, 0.f, 1.f)};
    }
    if (!bridgeForming_) {
        if (const auto score = bridgeViewAlignment(camera))
            return AlignmentFeedback{*score, bridgeAlignmentThreshold, *score >= bridgeAlignmentThreshold,
                                     std::clamp(bridgeHeld_ / .5f, 0.f, 1.f)};
    }
    return std::nullopt;
}
void SchoolCorridor::updateBridge(const Camera& camera, float dt) {
    if (!bridgeForming_) {
        const float score = bridgeAlignment(camera);
        bridgeHeld_ = advanceAlignmentHold(bridgeHeld_, score, bridgeAlignmentThreshold, dt);
        bridgeForming_ = score >= bridgeAlignmentThreshold && bridgeHeld_ >= .5f;
        return;
    }
    bridgeFormation_ = std::min(1.0f, bridgeFormation_ + dt / formationDuration);
}
void SchoolCorridor::drawBridge(Renderer& r) const {
    const float progress = glm::smoothstep(0.f, 1.f, bridgeFormation_);
    const float thickness = .036f * glm::smoothstep(.72f, 1.f, bridgeFormation_);
    for (const auto& patch : bridgeDrawing_) {
        std::array<glm::vec3, 4> p;
        for (size_t i = 0; i < p.size(); ++i)
            p[i] = glm::mix(patch.receiver[i], patch.target[i], progress);
        const auto color = patch.color * (patch.onLeaf ? 1.f : glm::mix(1.22f, 1.f, progress));
        r.triangle(p[0], p[1], p[2], color);
        r.triangle(p[0], p[2], p[3], color);
        if (thickness > .002f) {
            const glm::vec3 drop{0, thickness, 0};
            for (size_t i = 0; i < p.size(); ++i) {
                const auto a = p[i], b = p[(i + 1) % p.size()];
                r.triangle(a, a - drop, b - drop, color * .83f);
                r.triangle(a, b - drop, b, color * .83f);
            }
        }
    }
    const float rail = glm::smoothstep(.80f, 1.f, bridgeFormation_);
    if (rail > .02f) {
        for (float x : {bridgeEye.x - .51f, bridgeEye.x + .51f}) {
            for (float z : {gapNear - .035f, bridgeFocus.z, gapFar + .035f})
                r.box({x, floorHeight + .39f * rail, z}, {.045f, .78f * rail, .045f}, metal);
            r.box({x, floorHeight + .78f * rail, bridgeFocus.z}, {.055f, .048f, gapNear - gapFar}, metal);
        }
    }
}
Box SchoolCorridor::passageBlocker() const {
    if (formation_ < 1)
        return box({20.60f, 3.0f, -9.90f}, {.20f, 2.4f, 1.6f});
    const float slide = 1.70f * glm::smoothstep(0.0f, 1.0f, opening_);
    return box({20.505f, 2.99f, -9.90f - slide}, {.11f, 2.24f, 1.50f});
}
void SchoolCorridor::drawIllusion(Renderer& r) const {
    if (formation_ < 1) {
        r.box({20.60f, 3.0f, -9.90f}, {.20f, 2.4f, 1.6f}, plaster);
        r.box({20.494f, 2.32f, -9.9f}, {.009f, 1.04f, 1.6f}, paint);
    }
    const float formed = glm::smoothstep(0.0f, 1.0f, formation_);
    const float slide = 1.70f * glm::smoothstep(0.0f, 1.0f, opening_);
    if (formation_ >= 1) {
        for (float z : {-10.70f, -9.10f})
            r.box({20.53f, 3.01f, z}, {.14f, 2.38f, .10f}, wood * .8f);
        r.box({20.53f, 4.15f, -9.90f}, {.14f, .10f, 1.6f}, wood * .8f);
        r.box({20.51f, 2.99f, -9.90f - slide}, {.10f, 2.24f, 1.5f}, wood);
        r.box({20.44f, 4.26f, -10.70f}, {.14f, .065f, 3.30f}, metal);
    }
    for (const auto& patch : drawing_) {
        if (patch.material == DoorMaterial::infill && !forming_)
            continue;
        std::array<glm::vec3, 4> p;
        for (int i = 0; i < 4; ++i) {
            // From the aligned view each vertex travels along its original projection ray.
            const auto source = forming_ ? patch.receiver[i] : paintedPoint(patch, i);
            p[i] = glm::mix(source, patch.target[i], formed);
            if (!patch.frame)
                p[i].z -= slide;
        }
        auto color = patch.color;
        if (patch.material != DoorMaterial::infill) {
            const auto ordinary = patch.material == DoorMaterial::timber ? wood * .85f :
                                  patch.material == DoorMaterial::paper ? plaster * 1.04f : paint * .70f;
            color = glm::mix(ordinary, patch.color, formed);
        } else if (formed < 1) {
            // A complete painted door is not exposed before recognition. Its panels fill in as it forms.
            r.opacity(formed);
        }
        r.triangle(p[0], p[1], p[2], color);
        r.triangle(p[0], p[2], p[3], color);
        if (patch.material == DoorMaterial::infill && formed < 1)
            r.opacity(1);
        if (patch.material == DoorMaterial::timber && formed < 1) {
            const auto normal = patch.onLeaf && !forming_ ? glm::vec3(-std::sin(angle_), 0, std::cos(angle_))
                                                          : glm::vec3(-1, 0, 0);
            const auto lip = normal * (.012f * (1 - formed));
            for (int edge = 0; edge < 4; ++edge) {
                const auto a = p[edge], b = p[(edge + 1) % 4];
                r.triangle(a, b, b + lip, color * .82f);
                r.triangle(a, b + lip, a + lip, color * .82f);
            }
            r.triangle(p[0] + lip, p[1] + lip, p[2] + lip, color);
            r.triangle(p[0] + lip, p[2] + lip, p[3] + lip, color);
        }
    }
    drawCupboardNotices(r);
    r.sign(L"계단실", {20.485f, 4.55f, -9.9f}, {0, 0, .55f}, {0, .18f, 0});
}
void SchoolCorridor::drawCupboardNotices(Renderer& r) const {
    const float nearX = hinge.x - .042f, farX = 20.478f;
    const float nearScale = (nearX - illusionEye.x) / (illusionFocus.x - illusionEye.x);
    const float split = illusionEye.z + (hinge.z + doorWidth - illusionEye.z) / nearScale;
    for (bool leaf : {true, false}) {
        const float low = leaf ? -10.50f : split, high = leaf ? split : -9.30f;
        const float scale = ((leaf ? nearX : farX) - illusionEye.x) / (illusionFocus.x - illusionEye.x);
        const glm::vec3 right = leaf ? glm::vec3(std::cos(angle_), 0, std::sin(angle_)) : glm::vec3(0, 0, 1);
        const auto normal = glm::cross(right, glm::vec3(0, 1, 0));
        auto at = [&](float y, float z) {
            InkPatch point{};
            point.onLeaf = leaf;
            point.receiver[0] = illusionEye + (glm::vec3(illusionFocus.x, y, z) - illusionEye) * scale;
            return paintedPoint(point, 0) + normal * .004f;
        };
        if (leaf) {
            // Extend the puzzle edges into complete, practical cupboard bracing.
            const float hingeZ = illusionEye.z + (hinge.z + .075f - illusionEye.z) / scale;
            const float bottomY = illusionEye.y + (floorHeight + .10f - illusionEye.y) / scale;
            auto strip = [&](float y0, float y1, float z0, float z1) {
                r.quad(at((y0 + y1) * .5f, (z0 + z1) * .5f), right * ((z1 - z0) * scale * .5f),
                       {0, (y1 - y0) * scale * .5f, 0}, wood * .85f);
            };
            strip(4.10f, 4.20f, hingeZ, -10.75f);
            strip(1.82f, 1.87f, hingeZ, -10.75f);
            strip(bottomY, 4.20f, hingeZ, hingeZ + .08f);
            strip(bottomY, 1.82f, -10.75f, -10.65f);
            strip(bottomY, bottomY + .08f, hingeZ, -10.65f);
        } else {
            // The distant horizontal edges belong to wall trim, not an isolated half-door.
            for (const auto span : {glm::vec2(-11.46f, -8.35f)}) {
                r.quad(at(4.15f, (span.x + span.y) * .5f), right * ((span.y - span.x) * scale * .5f),
                       {0, .05f * scale, 0}, wood * .85f);
                r.quad(at(1.845f, (span.x + span.y) * .5f), right * ((span.y - span.x) * scale * .5f),
                       {0, .025f * scale, 0}, wood * .85f);
            }
            // Continue the notice into the real wall beside the future opening.
            r.quad(at(3.01f, -8.39f), right * (.04f * scale), {0, 1.19f * scale, 0}, wood * .85f);
            r.quad(at(3.18f, -8.72f), right * (.25f * scale), {0, .48f * scale, 0}, plaster * 1.04f);
            r.label(L"관리 일지", at(3.50f, -8.72f) + normal * .003f,
                    right * (.22f * scale), {0, .045f * scale, 0}, metal);
            for (int row = 0; row < 5; ++row)
                r.line(at(3.35f - row * .13f, -8.91f) + normal * .003f,
                       at(3.35f - row * .13f, -8.53f) + normal * .003f, .008f, paint);
        }
        if (forming_)
            continue;
        r.label(leaf ? L"청소 당번" : L"시설 점검", at(3.83f, (low + high) * .5f),
                right * ((high - low) * scale * .42f), {0, .042f * scale, 0}, metal);
        for (int row = 0; row < 4; ++row) {
            const float y = 3.66f - row * .18f;
            const float z = low + (high - low) * (.24f + .12f * (row % 2));
            r.line(at(y, z), at(y - .035f, z + .028f), .008f * scale, metal);
            r.line(at(y - .035f, z + .028f), at(y + .035f, z + .09f), .008f * scale, metal);
        }
    }
}
std::vector<Box> SchoolCorridor::doorBoxes(float angle) const {
    std::vector<Box> result;
    const float c = std::cos(angle), s = std::sin(angle), length = doorWidth / 12;
    for (int i = 0; i < 12; i++) {
        auto center = hinge + glm::vec3(c * length * (i + .5f), 1.06f, s * length * (i + .5f));
        result.push_back(box(center, {std::abs(c) * length + std::abs(s) * .07f, 2.12f,
                                      std::abs(s) * length + std::abs(c) * .07f}));
    }
    return result;
}
std::vector<Box> SchoolCorridor::collisionBoxes() const {
    std::vector<Box> result;
    result.reserve(solids_.size() + 16);
    for (const auto& solid : solids_)
        result.push_back(box(solid.center, solid.size));
    result.push_back(classroomDoorBox(classroomDoorOpening_));
    if (bridgeReady()) {
        result.push_back(
            box({bridgeEye.x, 1.78f, (gapNear + gapFar) * .5f}, {bridgeWidth, .04f, gapNear - gapFar}));
        for (float x : {bridgeEye.x - .51f, bridgeEye.x + .51f})
            result.push_back(box({x, 2.19f, bridgeFocus.z}, {.055f, .78f, gapNear - gapFar}));
    }
    result.push_back(passageBlocker());
    if (formation_ >= 1) {
        for (float z : {-10.70f, -9.10f})
            result.push_back(box({20.53f, 3.01f, z}, {.14f, 2.38f, .10f}));
        result.push_back(box({20.53f, 4.15f, -9.90f}, {.14f, .10f, 1.6f}));
    }
    auto leaf = doorBoxes(angle_);
    result.insert(result.end(), leaf.begin(), leaf.end());
    return result;
}
bool SchoolCorridor::canInteract(const Camera& camera) const {
    if (forming_ && formation_ < 1)
        return false;
    float nearest = std::numeric_limits<float>::max();
    for (const auto& leaf : doorBoxes(angle_)) {
        float hit = SchoolRoom::rayBox(camera.eye(), camera.direction(), leaf);
        if (hit >= 0)
            nearest = std::min(nearest, hit);
    }
    if (nearest > 2.1f)
        return false;
    const float partitionHit = SchoolRoom::rayBox(camera.eye(), camera.direction(), passageBlocker());
    if (partitionHit >= 0 && partitionHit < nearest - .04f)
        return false;
    for (const auto& solid : solids_) {
        float hit = SchoolRoom::rayBox(camera.eye(), camera.direction(), box(solid.center, solid.size));
        if (hit >= 0 && hit < nearest - .04f)
            return false;
    }
    return true;
}
bool SchoolCorridor::interact(const Camera& camera) {
    if (!canInteract(camera))
        return false;
    targetOpen_ = !targetOpen_;
    return true;
}
void SchoolCorridor::update(const Camera& camera, float dt) {
    if (classroomDoorUnlocked_ && classroomDoorOpening_ < 1) {
        const float next = std::min(1.f, classroomDoorOpening_ + dt / .74f);
        if (!touches(camera, classroomDoorBox(next)))
            classroomDoorOpening_ = next;
    }
    const float target = targetOpen_ ? halfPi : 0;
    const float distance = target - angle_;
    constexpr float acceleration = 12.0f;
    const float desiredSpeed =
        std::copysign(std::min(2.2f, std::sqrt(2 * acceleration * std::abs(distance))), distance);
    const float previousSpeed = angularSpeed_;
    angularSpeed_ += std::clamp(desiredSpeed - angularSpeed_, -acceleration * dt, acceleration * dt);
    float delta = (previousSpeed + angularSpeed_) * .5f * dt;
    if (delta * distance >= 0 && std::abs(delta) >= std::abs(distance)) {
        delta = distance;
        angularSpeed_ = 0;
    }
    delta = std::clamp(angle_ + delta, 0.0f, halfPi) - angle_;
    const int slices = std::max(1, static_cast<int>(std::ceil(std::abs(delta) / .015f)));
    for (int i = 0; i < slices; i++) {
        float next = angle_ + delta / slices;
        auto boxes = doorBoxes(next);
        if (std::any_of(boxes.begin(), boxes.end(), [&](const Box& b) { return touches(camera, b); })) {
            // Back off instead of following the player and pinning them against the adjacent stack.
            targetOpen_ = !targetOpen_;
            angularSpeed_ = 0;
            break;
        }
        angle_ = next;
    }
    updateIllusion(camera, dt);
    if (!bridgeReady())
        updateBridge(camera, dt);
}
bool SchoolCorridor::finished(const Camera& camera) const {
    return camera.feet.x > 22.95f && camera.feet.x < 24.15f && camera.feet.z < -17.6f + stairShift &&
           camera.feet.z > -18.8f + stairShift && std::abs(camera.feet.y - .6f) < .06f;
}
void SchoolCorridor::draw(Renderer& r) const {
    for (const auto& s : solids_)
        if (s.visible)
            r.box(s.center, s.size, s.color);
    for (const auto& s : details_) {
        if (s.bevel > 0) {
            if (r.visible(s.center, s.size * .5f))
                beveledBoard(r, s.center, s.size, s.color, s.bevel);
        } else {
            r.box(s.center, s.size, s.color);
        }
    }
    for (const auto& tree : foliage_) {
        const float radius = std::max(tree.w, .09f);
        if (!r.visible({tree.x, tree.y * .5f, tree.z}, {radius, tree.y * .5f, radius}))
            continue;
        r.box({tree.x, tree.y * .29f, tree.z}, {.12f, tree.y * .58f, .12f}, {.39f, .34f, .25f}, .3f);
        constexpr float fullCircle = 6.283185307f;
        for (int segment = 0; segment < 9; ++segment) {
            const float a = segment * fullCircle / 9 + tree.x, b = a + fullCircle / 9;
            auto ring = [&](float angle, float radius, float y) {
                return glm::vec3(tree.x + std::cos(angle) * tree.w * radius, y,
                                 tree.z + std::sin(angle) * tree.w * radius);
            };
            const auto lowerA = ring(a, .64f, tree.y * .47f), lowerB = ring(b, .64f, tree.y * .47f);
            const auto midA = ring(a, .98f, tree.y * .70f), midB = ring(b, .98f, tree.y * .70f);
            const auto upperA = ring(a, .62f, tree.y * .91f), upperB = ring(b, .62f, tree.y * .91f);
            const glm::vec3 shade = glm::vec3(.40f, .53f, .34f) * (1.f + .04f * (segment % 3));
            r.triangle(lowerA, midB, lowerB, shade * .85f);
            r.triangle(lowerA, midA, midB, shade * .85f);
            r.triangle(midA, upperB, midB, shade);
            r.triangle(midA, upperA, upperB, shade);
            r.triangle(upperA, {tree.x, tree.y, tree.z}, upperB, shade * 1.04f);
            r.triangle(lowerA, lowerB, {tree.x, tree.y * .45f, tree.z}, shade * .82f);
        }
    }
    r.opacity(.012f);
    for (const auto& base : furnitureBases_) {
        if (!r.visible(base + glm::vec3(0, .008f, 0), {.67f, .002f, .58f}))
            continue;
        for (int layer = 0; layer < 20; layer++) {
            const auto center = base + glm::vec3(0, .007f + layer * .00008f, 0);
            const float rx = .67f - layer * .021f, rz = .58f - layer * .018f;
            for (int segment = 0; segment < 32; segment++) {
                const float a = segment * 6.283185307f / 32, b = (segment + 1) * 6.283185307f / 32;
                r.triangle(center, center + glm::vec3(std::cos(b) * rx, 0, std::sin(b) * rz),
                           center + glm::vec3(std::cos(a) * rx, 0, std::sin(a) * rz), {.18f, .24f, .20f});
            }
        }
    }
    r.opacity(1);
    for (float x : {4.5f, 8.0f, 12.0f, 16.0f, 20.0f, 23.5f}) {
        r.box({x, 5.03f, -9.9f}, {1.05f, .10f, .32f}, metal);
        r.box({x, 4.97f, -9.9f}, {.96f, .02f, .24f}, {.98f, .96f, .85f});
    }
    // Ordinary school signs and notices identify spaces without spelling out a solution.
    r.sign(L"2-1", {6.25f, 4.43f, -11.48f}, {.47f, 0, 0}, {0, .18f, 0});
    r.sign(L"2-2", {13.45f, 4.43f, -11.48f}, {.47f, 0, 0}, {0, .18f, 0});
    r.sign(L"2-1  뒷문", {11.675f, 4.43f, -11.48f}, {.55f, 0, 0}, {0, .17f, 0});
    r.sign(L"복도", {11.675f, 4.43f, -11.72f}, {-.47f, 0, 0}, {0, .16f, 0});
    for (float x : {11.045f, 12.305f})
        r.box({x, 2.95f, -11.60f}, {.075f, 2.30f, .21f}, wood);
    r.box({11.675f, 4.104f, -11.6f}, {1.33f, .085f, .22f}, wood);
    r.sign(L"계단실", {23.545f, 4.56f, -11.457f}, {.55f, 0, 0}, {0, .18f, 0});
    r.sign(L"계단 · 현관", {24.88f, 3.6f, -9.9f}, {0, 0, .56f}, {0, .20f, 0});
    r.sign(L"2층 복도", {3.0f, 4.45f, -11.425f}, {.59f, 0, 0}, {0, .18f, 0});
    for (float x : {22.72f, 24.38f})
        r.box({x, 1.77f, -16.97f + stairShift}, {.09f, 2.34f, .12f}, wood);
    r.box({23.55f, 2.94f, -16.97f + stairShift}, {1.75f, .09f, .12f}, wood);
    r.sign(L"1층 현관", {23.55f, 3.25f, -16.98f + stairShift}, {.58f, 0, 0}, {0, .18f, 0});
    r.box({3.95f, 3.45f, -11.47f}, {1.7f, 1.35f, .07f}, wood);
    r.box({3.95f, 3.45f, -11.42f}, {1.58f, 1.23f, .015f}, {.73f, .68f, .54f});
    r.label(L"학급 소식", {3.95f, 3.92f, -11.4f}, {.55f, 0, 0}, {0, .12f, 0}, metal);
    for (int i = 0; i < 3; i++) {
        r.box({3.43f + i * .5f, 3.35f, -11.39f}, {.41f, .58f, .01f}, plaster);
        for (int j = 0; j < 4; j++)
            r.line({3.27f + i * .5f, 3.52f - j * .1f, -11.38f}, {3.57f + i * .5f, 3.52f - j * .1f, -11.38f},
                   .012f, metal * .9f);
    }
    r.label(L"청소 당번  /  책걸상 정리", {10.0f, 4.55f, -16.56f}, {1.25f, 0, 0}, {0, .14f, 0}, metal);
    r.label(L"수업이 끝나면 창문을 닫아 주세요", {16.6f, 4.55f, -16.56f}, {1.45f, 0, 0}, {0, .14f, 0}, metal);
    for (float x : {18.4f, 18.8f, 19.2f}) {
        r.line({x, 1.87f, -11.88f}, {x + .09f, 3.65f, -12.04f}, .035f, wood);
        r.box({x, 1.98f, -11.88f}, {.25f, .17f, .10f}, {.56f, .51f, .35f});
    }
    const glm::vec3 along{std::cos(angle_), 0, std::sin(angle_)};
    const glm::vec3 front{-std::sin(angle_), 0, std::cos(angle_)};
    const auto center = hinge + along * (doorWidth * .5f) + glm::vec3(0, 1.06f, 0);
    r.box(center, {doorWidth, 2.12f, .07f}, paint, -angle_);
    for (float y : {.13f, 1.98f})
        r.box(hinge + glm::vec3(0, y, 0), {.09f, .12f, .09f}, metal);
    const auto handle = hinge + along * (doorWidth * .45f) + glm::vec3(0, 1.05f, 0);
    for (float sign : {-1.f, 1.f}) {
        r.box(handle + front * (sign * .065f), {.06f, .20f, .05f}, metal, -angle_);
        r.label(L"청소도구함", hinge + along * .32f + glm::vec3(0, .52f, 0) + front * (sign * .041f),
                along * (sign * .23f), {0, .045f, 0}, plaster);
    }
    drawIllusion(r);
    drawFalseHall(r);
    drawClassroomLock(r);
    drawBridge(r);
    r.label(L"점검 통로", {25.031f, 3.59f, -11.43f}, {0, 0, .37f}, {0, .13f, 0}, plaster);
    // Closed glass remains visible while preserving the useful side view into each gap.
    r.opacity(.08f);
    for (const auto window : {Window{7.6f, 11.05f}, Window{14.7f, 17.8f}})
        r.quad({(window.start + window.end) * .5f, 3.525f, -11.6f}, {(window.end - window.start) * .5f, 0, 0},
               {0, .675f, 0}, {.65f, .80f, .78f});
    r.opacity(1);
    for (const float wall : {21.99f, 25.1f}) {
        const float inward = wall < 23.f ? 1.f : -1.f;
        const float rail = wall + inward * .16f;
        const float end = stairStart - 6 * stairRun;
        r.quad({wall + inward * .004f, 1.38f, (stairStart + end) * .5f}, glm::vec3(0, -.60f, -1.8f) * inward,
               {0, .47f, 0}, paint);
        r.line({rail, 2.62f, stairStart}, {rail, 1.42f, end}, .053f, wood);
        for (float z : {stairStart + .18f, end - .18f}) {
            const float y = z > stairStart ? 2.62f : 1.42f;
            r.line({rail, y, z}, {rail, y, z > stairStart ? stairStart : end}, .053f, wood);
            r.line({rail, y, z}, {wall, y, z}, .045f, metal);
        }
        for (int i : {1, 3, 5}) {
            const float y = 2.62f - i * stairRise, z = stairStart - i * stairRun;
            r.box({wall + inward * .017f, y - .07f, z}, {.035f, .13f, .095f}, metal);
            r.line({wall, y - .095f, z}, {rail, y, z}, .032f, metal);
        }
    }
    r.opacity(.18f);
    r.quad({14.415f, 3.625f, -8.23f}, {10.685f, 0, 0}, {0, .825f, 0}, {.66f, .81f, .83f});
    r.opacity(1);
}
} // namespace school
