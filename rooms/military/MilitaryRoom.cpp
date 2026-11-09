#include "MilitaryRoom.h"

#include <GL/glm/gtc/matrix_transform.hpp>

#include <algorithm>

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
constexpr Footprint approach{{9.45f, 0.0f}, {12.6f, 7.8f}};
constexpr Footprint ledge{{8.4f, 0.0f}, {9.45f, 5.15f}};
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
    return supply.contains(point) || approach.contains(point) || ledge.contains(point);
}
}

bool MilitaryRoom::initialize() {
    return renderer_.initialize(staticGeometry, barricadeGeometry);
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

bool MilitaryRoom::canStand(const glm::vec3& position) const {
    const auto bounds = footprint(position, glm::vec2{playerRadius});
    if (!onRoute(bounds.min) || !onRoute(bounds.max) ||
        !onRoute({bounds.min.x, bounds.max.y}) || !onRoute({bounds.max.x, bounds.min.y}) ||
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
}
