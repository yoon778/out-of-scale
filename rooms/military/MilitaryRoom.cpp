#include "MilitaryRoom.h"

#include <GL/glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
#include "MilitaryGeometry.inc"

constexpr glm::vec3 barricadeSize{0.66f, 1.52f, 3.61f};
constexpr glm::vec3 barricadeHalf = barricadeSize * 0.5f;
constexpr glm::vec3 binCenter{4.55f, 0.0f, -3.67f};
constexpr glm::vec2 binInterior{0.475f, 0.395f};
constexpr float binFloor = 0.22f;
constexpr float binRim = 0.73f;
constexpr float gateFront = -0.12f;
constexpr float playerRadius = 0.25f;
constexpr float minimumScale = 0.05f;
constexpr float moonCarryDepth = 3.2f;
constexpr float moonLightScale = 0.25f;
constexpr float gapNear = 7.8f;
constexpr float gapFar = 12.5f;
constexpr float trenchY = -1.33f;
constexpr float minimumBridgeWidth = 0.80f;
constexpr float overlapOnBank = 0.05f;

using Polygon = std::vector<glm::vec2>;

struct Footprint {
    glm::vec2 min;
    glm::vec2 max;

    bool contains(const glm::vec2& point) const {
        return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y;
    }

    bool overlaps(const Footprint& other) const {
        return min.x < other.max.x && max.x > other.min.x && min.y < other.max.y && max.y > other.min.y;
    }
};

constexpr Footprint supply{{0.0f, -4.4f}, {12.6f, 0.0f}};
constexpr Footprint approach{{9.45f, 0.0f}, {12.6f, gapNear}};
constexpr Footprint ledge{{8.4f, 0.0f}, {9.45f, 5.15f}};
constexpr Footprint gap{{8.4f, gapNear}, {12.6f, gapFar}};
constexpr Footprint bridgeArea{{approach.min.x, gapNear}, {approach.max.x, gapFar}};
constexpr Footprint farBank{{8.4f, gapFar}, {12.6f, 15.4f}};
constexpr Footprint exitWalkway{{9.80f, 15.4f}, {11.20f, 16.18f}};
constexpr Footprint bin{{3.985f, -4.155f}, {5.115f, -3.185f}};
constexpr Footprint obstacles[]{
    bin,
    {{1.95f, -4.12f}, {2.65f, -3.44f}},
    {{2.75f, -4.17f}, {3.45f, -3.49f}},
    {{10.46f, -4.12f}, {11.16f, -3.44f}},
    {{9.865f, 5.745f}, {11.135f, 6.555f}},
};

glm::vec2 horizontal(const glm::vec3& position) {
    return {position.x, position.z};
}

Footprint footprint(const glm::vec3& position, const glm::vec2& half) {
    const auto center = horizontal(position);
    return {center - half, center + half};
}

bool overlapsObstacle(const Footprint& bounds) {
    for (const auto& obstacle : obstacles) {
        if (bounds.overlaps(obstacle)) {
            return true;
        }
    }
    return false;
}

bool onRoute(const glm::vec2& point) {
    return supply.contains(point) || approach.contains(point) || ledge.contains(point) ||
           farBank.contains(point) || exitWalkway.contains(point);
}

float cross(const glm::vec2& a, const glm::vec2& b) {
    return a.x * b.y - a.y * b.x;
}

Polygon convexHull(Polygon points) {
    std::sort(points.begin(), points.end(), [](const glm::vec2& a, const glm::vec2& b) {
        return a.x == b.x ? a.y < b.y : a.x < b.x;
    });
    points.erase(std::unique(points.begin(), points.end()), points.end());
    if (points.size() < 3) {
        return {};
    }
    const auto buildHalf = [](auto begin, auto end) {
        Polygon half;
        for (auto point = begin; point != end; ++point) {
            while (half.size() >= 2 &&
                   cross(half.back() - half[half.size() - 2], *point - half.back()) <= 0.0f) {
                half.pop_back();
            }
            half.push_back(*point);
        }
        half.pop_back();
        return half;
    };
    auto lower = buildHalf(points.begin(), points.end());
    const auto upper = buildHalf(points.rbegin(), points.rend());
    lower.insert(lower.end(), upper.begin(), upper.end());
    return lower;
}

Polygon projectPillar(const glm::vec3& light, float surfaceY) {
    struct Bounds {
        glm::vec3 min;
        glm::vec3 max;
    };
    // Body, foot and cap bounds from the Blender Entry geometry.
    constexpr Bounds parts[]{
        {{9.97f, 0.0f, 5.84f}, {11.03f, 2.28f, 6.46f}},
        {{9.865f, 0.0f, 5.745f}, {11.135f, 0.16f, 6.555f}},
        {{9.945f, 2.255f, 5.815f}, {11.055f, 2.345f, 6.485f}},
    };
    Polygon points;
    for (const auto& part : parts) {
        for (float x : {part.min.x, part.max.x}) {
            for (float y : {part.min.y, part.max.y}) {
                for (float z : {part.min.z, part.max.z}) {
                    const float distance = (light.y - surfaceY) / (light.y - y);
                    points.push_back(horizontal(light + (glm::vec3{x, y, z} - light) * distance));
                }
            }
        }
    }
    return convexHull(std::move(points));
}

Polygon clipTo(const Polygon& outline, const Footprint& area) {
    auto clipped = outline;
    for (int axis = 0; axis < 2; ++axis) {
        for (bool lower : {true, false}) {
            if (clipped.empty()) {
                return {};
            }
            const float edge = lower ? area.min[axis] : area.max[axis];
            const auto inside = [=](const glm::vec2& point) {
                return lower ? point[axis] >= edge : point[axis] <= edge;
            };
            Polygon next;
            auto previous = clipped.back();
            for (const auto& point : clipped) {
                if (inside(previous) != inside(point)) {
                    const float ratio = (edge - previous[axis]) / (point[axis] - previous[axis]);
                    auto intersection = glm::mix(previous, point, ratio);
                    intersection[axis] = edge;
                    next.push_back(intersection);
                }
                if (inside(point)) {
                    next.push_back(point);
                }
                previous = point;
            }
            clipped = std::move(next);
        }
    }
    return clipped;
}

glm::vec2 spanAt(const Polygon& outline, float z) {
    const float infinity = std::numeric_limits<float>::infinity();
    glm::vec2 span{infinity, -infinity};
    for (std::size_t i = 0; i < outline.size(); ++i) {
        const auto& a = outline[i];
        const auto& b = outline[(i + 1) % outline.size()];
        if (z < std::min(a.y, b.y) || z > std::max(a.y, b.y)) {
            continue;
        }
        if (a.y == b.y) {
            span.x = std::min({span.x, a.x, b.x});
            span.y = std::max({span.y, a.x, b.x});
        } else {
            const float x = glm::mix(a.x, b.x, (z - a.y) / (b.y - a.y));
            span.x = std::min(span.x, x);
            span.y = std::max(span.y, x);
        }
    }
    return span;
}

bool contains(const Polygon& outline, const glm::vec2& point) {
    if (outline.size() < 3) {
        return false;
    }
    for (std::size_t i = 0; i < outline.size(); ++i) {
        const auto& a = outline[i];
        const auto& b = outline[(i + 1) % outline.size()];
        if (cross(b - a, point - a) < -0.00001f) {
            return false;
        }
    }
    return true;
}

bool seenThroughWindow(const glm::vec3& eye, const glm::vec3& direction) {
    if (direction.x <= 0.0f) {
        return false;
    }
    const auto atWindow = eye + direction * ((12.60f - eye.x) / direction.x);
    return atWindow.y > 1.18f && atWindow.y < 2.73f && atWindow.z > 2.36f && atWindow.z < 4.76f;
}
}

bool MilitaryRoom::initialize() {
    return renderer_.initialize(staticGeometry, barricadeGeometry, moonGeometry);
}

std::optional<MilitaryRoom::Placement> MilitaryRoom::placement(
    const glm::vec3& eye, const glm::vec3& direction) const {
    if (direction.y >= -0.001f) {
        return std::nullopt;
    }

    const float rimDepth = (binRim - eye.y) / direction.y;
    const glm::vec3 rimPoint = eye + direction * rimDepth;
    if (rimDepth > 0.0f && bin.contains(horizontal(rimPoint))) {
        const auto opening = footprint(binCenter, binInterior);
        const glm::vec3 binTop{binCenter.x, binRim, binCenter.z};
        // Size is chosen at the opening, then the returned prop settles onto the tray.
        const float scale = perspective::scaleAtSupport(eye, direction, binTop, heldRatio_, barricadeHalf.y);
        const glm::vec3 half = barricadeHalf * scale;
        if (!opening.contains(horizontal(rimPoint)) || scale < minimumScale ||
            half.x + 0.02f > binInterior.x || half.z + 0.02f > binInterior.y) {
            return std::nullopt;
        }
        return Placement{{{binCenter.x, binFloor + half.y, binCenter.z}, scale}, true};
    }

    const auto target = perspective::onFloor(eye, direction, heldRatio_, barricadeHalf.y);
    if (!target || target->scale < minimumScale) {
        return std::nullopt;
    }
    const auto half = horizontal(barricadeHalf) * target->scale;
    const auto bounds = footprint(target->center, half);
    if (!supply.contains(bounds.min) || !supply.contains(bounds.max) || bounds.max.y >= gateFront ||
        bounds.overlaps(footprint(eye, glm::vec2{playerRadius})) || overlapsObstacle(bounds)) {
        return std::nullopt;
    }
    return Placement{*target, false};
}

void MilitaryRoom::interact(const glm::vec3& eye, const glm::vec3& direction) {
    if (exitState_ != ExitState::Locked) {
        interactMoon(eye, direction);
        return;
    }
    if (heldRatio_ > 0.0f) {
        if (const auto target = placement(eye, direction)) {
            barricade_ = target->pose;
            heldRatio_ = 0.0f;
            if (target->returned) {
                exitState_ = ExitState::Open;
            }
        }
        return;
    }

    const auto hit = perspective::rayBoxDistance(eye, direction, barricade_.center, barricadeHalf * barricade_.scale);
    if (!hit || *hit <= 0.05f) {
        return;
    }
    const auto binHit = perspective::rayBoxDistance(
        eye, direction, binCenter + glm::vec3{0.0f, binRim * 0.5f, 0.0f}, {0.565f, binRim * 0.5f, 0.485f});
    if (!binHit || *hit < *binHit) {
        heldRatio_ = perspective::grabRatio(eye, direction, barricade_.center, barricade_.scale, barricadeSize.z);
    }
}

bool MilitaryRoom::canPlaceMoon(const glm::vec3& center) const {
    const glm::vec3 half = moonWorldSize * (moonScale_ * 0.5f);
    return center.x - half.x > 8.4f && center.x + half.x < 12.6f &&
           center.y >= 3.15f && center.y <= 5.50f &&
           center.z - half.z >= 0.25f && center.z + half.z <= gapNear;
}

void MilitaryRoom::interactMoon(const glm::vec3& eye, const glm::vec3& direction) {
    // Keep the bridge stable for a player already walking across it or beyond it.
    if (eye.z < 1.35f || eye.z + playerRadius >= gapNear) {
        return;
    }
    if (moonHeld_) {
        const glm::vec3 center = eye + direction * moonCarryDepth;
        if (canPlaceMoon(center)) {
            placedMoon_ = center;
            moonHeld_ = false;
            updateShadow(center);
        }
        return;
    }

    if (!placedMoon_ && !seenThroughWindow(eye, direction)) {
        return;
    }
    const float scale = placedMoon_ ? moonScale_ : 1.0f;
    const auto hit = perspective::rayBoxDistance(
        eye, direction, placedMoon_.value_or(moonWorldCenter), moonWorldSize * (scale * 0.5f));
    if (hit && *hit > 0.05f) {
        moonHeld_ = true;
        shadow_.clear();
        trenchShadow_.clear();
        bridge_.clear();
        exitState_ = ExitState::OutpostReached;
    }
}

void MilitaryRoom::scroll(double yOffset) {
    if (moonHeld_) {
        const auto step = static_cast<float>(std::clamp(yOffset, -8.0, 8.0));
        moonScale_ = std::clamp(moonScale_ * std::pow(1.28f, -step), 0.08f, 0.52f);
    }
}

void MilitaryRoom::updateShadow(const glm::vec3& light) {
    shadow_ = projectPillar(light, 0.0f);
    trenchShadow_ = clipTo(projectPillar(light, trenchY), gap);
    bridge_.clear();
    exitState_ = ExitState::OutpostReached;
    const auto near = spanAt(shadow_, gapNear - overlapOnBank);
    const auto far = spanAt(shadow_, gapFar + overlapOnBank);
    const float left = std::max({near.x, far.x, bridgeArea.min.x});
    const float right = std::min({near.y, far.y, bridgeArea.max.x});
    if (moonScale_ >= moonLightScale && right - left >= minimumBridgeWidth) {
        bridge_ = clipTo(shadow_, bridgeArea);
        exitState_ = ExitState::BridgeReady;
    }
}

bool MilitaryRoom::supports(const glm::vec2& point) const {
    return onRoute(point) || contains(bridge_, point);
}

bool MilitaryRoom::canStand(const glm::vec3& position) const {
    const auto bounds = footprint(position, glm::vec2{playerRadius});
    if (!supports(bounds.min) || !supports(bounds.max) ||
        !supports({bounds.min.x, bounds.max.y}) || !supports({bounds.max.x, bounds.min.y}) ||
        (exitState_ == ExitState::Locked && bounds.max.y >= gateFront) || overlapsObstacle(bounds)) {
        return false;
    }
    const auto barricadeBounds = footprint(barricade_.center, horizontal(barricadeHalf) * barricade_.scale);
    return heldRatio_ > 0.0f || exitState_ != ExitState::Locked || !bounds.overlaps(barricadeBounds);
}

glm::vec3 MilitaryRoom::resolveMovement(const glm::vec3& previous, const glm::vec3& desired) {
    glm::vec3 position{desired.x, desired.y, previous.z};
    if (!canStand(position)) {
        position.x = previous.x;
    }
    position.z = desired.z;
    if (!canStand(position)) {
        position.z = previous.z;
    }
    if (exitState_ == ExitState::Open && position.z >= 1.35f) {
        exitState_ = ExitState::OutpostReached;
    }
    if (exitState_ == ExitState::BridgeReady && position.z >= 15.70f) {
        exitState_ = ExitState::Escaped;
    }
    return position;
}

MilitaryRoom::ExitState MilitaryRoom::exitState() const {
    return exitState_;
}

void MilitaryRoom::draw(const glm::mat4& viewProjection, const glm::vec3& eye, const glm::vec3& direction) const {
    renderer_.begin(viewProjection);
    renderer_.drawStaticMesh();

    const bool locked = exitState_ == ExitState::Locked;
    if (locked) {
        renderer_.drawBox({10.5f, 1.34f, -0.04f}, {4.2f, 2.68f, 0.16f}, {0.27f, 0.33f, 0.30f});
        for (float x : {8.8f, 9.65f, 10.5f, 11.35f, 12.2f}) {
            renderer_.drawBox({x, 1.34f, -0.13f}, {0.055f, 2.54f, 0.025f}, {0.42f, 0.47f, 0.42f});
        }
    }
    renderer_.drawBox({10.5f, 2.82f, -0.04f}, {4.2f, 0.28f, 0.26f}, {0.25f, 0.29f, 0.25f});
    renderer_.drawBox({10.5f, 2.82f, -0.19f}, {0.48f, 0.10f, 0.04f},
                      locked ? glm::vec3{0.95f, 0.25f, 0.16f} : glm::vec3{0.40f, 1.0f, 0.48f});

    perspective::Pose displayed = barricade_;
    glm::vec3 tint{1.0f};
    if (heldRatio_ > 0.0f) {
        if (const auto target = placement(eye, direction)) {
            displayed = target->pose;
            tint = {0.65f, 1.25f, 1.05f};
        } else {
            constexpr float carryDepth = 2.4f;
            displayed = {eye + direction * carryDepth, std::min(heldRatio_ * carryDepth, 0.55f)};
            tint = {1.0f, 0.43f, 0.40f};
        }
    }
    const auto model = glm::scale(glm::translate(glm::mat4{1.0f}, displayed.center), barricadeSize * displayed.scale);
    renderer_.drawMovableMesh(model, tint);

    if (!shadow_.empty()) {
        const glm::vec3 shadowColor = moonScale_ >= moonLightScale
            ? glm::vec3{0.08f, 0.14f, 0.21f} : glm::vec3{0.18f, 0.23f, 0.30f};
        for (const auto& floor : {approach, ledge, farBank}) {
            renderer_.drawFlatPolygon(clipTo(shadow_, floor), 0.012f, shadowColor);
        }
        renderer_.drawFlatPolygon(trenchShadow_, trenchY + 0.012f, shadowColor);
        renderer_.drawFlatPolygon(bridge_, 0.012f, {0.13f, 0.27f, 0.37f});
    }

    glm::vec3 moonCenter = placedMoon_.value_or(moonWorldCenter);
    glm::vec3 moonTint{1.0f};
    if (moonHeld_) {
        moonCenter = eye + direction * moonCarryDepth;
        if (!canPlaceMoon(moonCenter)) {
            moonTint = {1.0f, 0.38f, 0.38f};
            moonCenter = eye + direction * (moonCarryDepth * 0.5f);
        }
    }
    const float drawnScale = (placedMoon_ || moonHeld_) ? moonScale_ : 1.0f;
    const auto moonModel = glm::scale(glm::translate(glm::mat4{1.0f}, moonCenter), moonWorldSize * drawnScale);
    renderer_.drawMoonMesh(moonModel, moonTint);
}
