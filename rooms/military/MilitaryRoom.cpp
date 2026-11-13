#include "MilitaryRoom.h"

#include <GL/glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <span>

namespace {
#include "MilitaryGeometry.inc"

constexpr float slotTop = 0.56f;
constexpr float slotZ = -9.85f;
constexpr glm::vec2 rackHalf{0.43f, 0.40f};
constexpr float returnClearance = 0.02f;
constexpr float rackDropHeight = 0.12f;
constexpr float placementClearance = 0.025f;
constexpr float gravity = 9.8f;
constexpr float playerRadius = 0.25f;
constexpr float playerEyeHeight = MilitaryRoom::spawnEye.y;
constexpr float headClearance = 0.12f;
constexpr float barracksCeiling = 4.60f;
constexpr float contactTolerance = 0.001f;
constexpr float minimumScale = 0.05f;
constexpr float televisionEntryScale = 1.9f;
constexpr glm::vec3 screenSize{0.025f, 0.88f, 1.30f};
constexpr glm::vec3 screenOffset{-0.255f, 0.03f, 0.0f};
constexpr glm::vec3 outpostEntrance{9.75f, 1.65f, 1.60f};
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

struct Solid {
    Footprint bounds;
    float bottom;
    float top;
};

struct DepthRange {
    float first = 0.0f;
    float last;

    bool clipBelow(float origin, float slope, float limit) {
        if (slope > 0.0f) {
            last = std::min(last, (limit - origin) / slope);
        } else if (slope < 0.0f) {
            first = std::max(first, (limit - origin) / slope);
        } else if (origin > limit) {
            return false;
        }
        return first <= last;
    }
};

struct ObjectSpec {
    glm::vec3 initialCenter;
    glm::vec3 size;
    glm::vec3 color;
    float frontOverhang = 0.0f;
};

constexpr ObjectSpec objectSpecs[]{
    {{4.20f, 0.38f, -5.95f}, {1.62f, 0.76f, 0.98f}, {0.47f, 0.54f, 0.22f}},
    {{7.30f, 0.28f, -4.45f}, {1.54f, 0.56f, 1.05f}, {0.27f, 0.56f, 0.83f}},
    {{10.30f, 0.44f, -5.85f}, {1.25f, 0.88f, 1.10f}, {0.78f, 0.58f, 0.32f}},
    {{14.70f, 0.54f, -5.20f}, {0.50f, 1.08f, 1.58f}, {0.27f, 0.27f, 0.25f}, 0.03f},
};

struct ReturnSlot {
    glm::vec3 center;
    glm::vec2 opening;
};

constexpr ReturnSlot returnSlots[]{
    {{12.80f, slotTop, slotZ}, {0.84f, 0.58f}},
    {{13.90f, slotTop, slotZ}, {0.82f, 0.58f}},
    {{15.00f, slotTop, slotZ}, {0.72f, 0.62f}},
};

constexpr Footprint supply{{0.0f, -10.40f}, {16.60f, 0.0f}};
constexpr Footprint approach{{8.25f, 0.0f}, {12.6f, gapNear}};
constexpr Footprint ledge{{7.20f, 0.0f}, {8.25f, 5.15f}};
constexpr Footprint gap{{8.4f, gapNear}, {12.6f, gapFar}};
constexpr Footprint bridgeArea{{9.45f, gapNear}, {approach.max.x, gapFar}};
constexpr Footprint farBank{{8.4f, gapFar}, {12.6f, 15.4f}};
constexpr Footprint exitWalkway{{9.80f, 15.4f}, {11.20f, 16.18f}};
struct Furniture {
    Footprint bounds;
    float height;
};

constexpr auto furniture = [] {
    constexpr std::array bedCenters{2.0f, 4.2f, 6.4f, 8.6f, 10.8f};
    constexpr float halfWidth = 0.54f;
    constexpr float bedHeight = 0.85f;
    constexpr float lockerHeight = 1.95f;
    std::array<Furniture, bedCenters.size() * 4> layout{};
    for (std::size_t index = 0; index < bedCenters.size(); ++index) {
        const float left = bedCenters[index] - halfWidth;
        const float right = bedCenters[index] + halfWidth;
        layout[index * 4] = {{{left, -2.48f}, {right, -0.48f}}, bedHeight};
        layout[index * 4 + 1] = {{{left, -0.47f}, {right, -0.03f}}, lockerHeight};
        layout[index * 4 + 2] = {{{left, -9.92f}, {right, -7.92f}}, bedHeight};
        layout[index * 4 + 3] = {{{left, -10.37f}, {right, -9.93f}}, lockerHeight};
    }
    return layout;
}();
constexpr Footprint pillarFoot{{9.865f, 5.745f}, {11.135f, 6.555f}};

glm::vec2 horizontal(const glm::vec3& position) {
    return {position.x, position.z};
}

Footprint footprint(const glm::vec3& position, const glm::vec2& half) {
    const auto center = horizontal(position);
    return {center - half, center + half};
}

Footprint objectFootprint(std::size_t index, const perspective::Pose& pose) {
    return footprint(pose.center, horizontal(objectSpecs[index].size) * (0.5f * pose.scale));
}

Solid objectSolid(std::size_t index, const perspective::Pose& pose, bool includeScreen) {
    auto bounds = objectFootprint(index, pose);
    if (includeScreen) {
        bounds.min.x -= objectSpecs[index].frontOverhang * pose.scale;
    }
    const float halfHeight = objectSpecs[index].size.y * (0.5f * pose.scale);
    return {bounds, pose.center.y - halfHeight, pose.center.y + halfHeight};
}

constexpr std::size_t staticSolidCount = furniture.size() + std::size(returnSlots);
using RoomSolids = std::array<Solid, staticSolidCount + std::size(objectSpecs)>;

std::span<const Solid> barracksSolids(
    RoomSolids& storage, const std::array<perspective::Pose, std::size(objectSpecs)>& objects,
    std::optional<std::size_t> held, bool includeScreen = true) {
    std::size_t count = 0;
    for (const auto& obstacle : furniture) {
        storage[count++] = {obstacle.bounds, 0.0f, obstacle.height};
    }
    for (const auto& slot : returnSlots) {
        storage[count++] = {footprint(slot.center, rackHalf), 0.0f, slotTop};
    }
    for (std::size_t index = 0; index < objects.size(); ++index) {
        if (held != index) {
            storage[count++] = objectSolid(index, objects[index], includeScreen);
        }
    }
    return {storage.data(), count};
}

bool overlapsPlayer(const Solid& object, const glm::vec3& eye) {
    return object.top > eye.y - playerEyeHeight + contactTolerance &&
           object.bottom < eye.y + headClearance - contactTolerance &&
           object.bounds.overlaps(footprint(eye, glm::vec2{playerRadius}));
}

bool fitsReturnSlot(const Footprint& bounds, const ReturnSlot& slot) {
    const auto opening = footprint(slot.center, slot.opening * 0.5f - glm::vec2{returnClearance});
    return opening.contains(bounds.min) && opening.contains(bounds.max);
}

float nearestFurnitureDepth(const glm::vec3& eye, const glm::vec3& direction) {
    float closest = std::numeric_limits<float>::infinity();
    for (const auto& obstacle : furniture) {
        const glm::vec2 middle = (obstacle.bounds.min + obstacle.bounds.max) * 0.5f;
        const glm::vec2 half = (obstacle.bounds.max - obstacle.bounds.min) * 0.5f;
        const auto hit = perspective::rayBoxDistance(eye, direction, {middle.x, obstacle.height * 0.5f, middle.y},
                                                     {half.x, obstacle.height * 0.5f, half.y});
        if (hit) {
            closest = std::min(closest, *hit);
        }
    }
    return closest;
}

bool onOutpostRoute(const glm::vec2& point) {
    return approach.contains(point) || ledge.contains(point) ||
           farBank.contains(point) || exitWalkway.contains(point);
}

bool fitsSupply(const Footprint& bounds) {
    return supply.contains(bounds.min) && supply.contains(bounds.max);
}

float wallScaleLimit(const glm::vec2& center, const Footprint& unitBounds) {
    const auto minSide = (center - supply.min - glm::vec2{placementClearance}) / -unitBounds.min;
    const auto maxSide = (supply.max - center - glm::vec2{placementClearance}) / unitBounds.max;
    return std::min({minSide.x, minSide.y, maxSide.x, maxSide.y});
}

float sideContactScale(const glm::vec2& center, const Footprint& unitBounds, const Footprint& obstacle) {
    const auto minSide = (center - obstacle.max - glm::vec2{placementClearance}) / -unitBounds.min;
    const auto maxSide = (obstacle.min - center - glm::vec2{placementClearance}) / unitBounds.max;
    return std::max({0.0f, minSide.x, minSide.y, maxSide.x, maxSide.y});
}

float landingHeight(const Footprint& bounds, float maximumHeight, std::span<const Solid> solids) {
    float height = 0.0f;
    for (const auto& solid : solids) {
        if (solid.top <= maximumHeight + contactTolerance && bounds.overlaps(solid.bounds)) {
            height = std::max(height, solid.top);
        }
    }
    return height;
}

std::optional<perspective::Pose> dropFromAim(const glm::vec3& eye, const glm::vec3& direction, float ratio,
                                            std::size_t index, std::span<const Solid> solids) {
    const auto& object = objectSpecs[index];
    const glm::vec3 half = object.size * 0.5f;
    const glm::vec3 lowerSlope = direction - (half + glm::vec3{object.frontOverhang, 0.0f, 0.0f}) * ratio;
    const glm::vec3 upperSlope = direction + half * ratio;
    const glm::vec3 roomMin{supply.min.x, 0.0f, supply.min.y};
    const glm::vec3 roomMax{supply.max.x, barracksCeiling, supply.max.y};
    DepthRange depth{0.0f, std::numeric_limits<float>::infinity()};
    // The box grows along the aim ray. Stop it short of the first collision at its release height.
    for (int axis = 0; axis < 3; ++axis) {
        if (!depth.clipBelow(-eye[axis], -lowerSlope[axis], -roomMin[axis] - placementClearance) ||
            !depth.clipBelow(eye[axis], upperSlope[axis], roomMax[axis] - placementClearance)) {
            return std::nullopt;
        }
    }
    for (const auto& solid : solids) {
        const glm::vec3 lower{solid.bounds.min.x, solid.bottom, solid.bounds.min.y};
        const glm::vec3 upper{solid.bounds.max.x, solid.top, solid.bounds.max.y};
        DepthRange hit{0.0f, depth.last};
        bool intersects = true;
        for (int axis = 0; axis < 3 && intersects; ++axis) {
            intersects = hit.clipBelow(eye[axis], lowerSlope[axis], upper[axis] + placementClearance) &&
                         hit.clipBelow(-eye[axis], -upperSlope[axis], -lower[axis] + placementClearance);
        }
        if (intersects) {
            depth.last = hit.first;
        }
    }
    const perspective::Pose pose{eye + direction * depth.last, ratio * depth.last};
    if (depth.last < depth.first || pose.scale < minimumScale || overlapsPlayer(objectSolid(index, pose, true), eye)) {
        return std::nullopt;
    }
    return pose;
}

std::optional<perspective::Pose> dropNearFeet(const glm::vec3& eye, const glm::vec3& direction, float ratio,
                                              std::size_t index, std::span<const Solid> solids) {
    const auto origin = horizontal(eye);
    const float feet = std::max(0.0f, eye.y - playerEyeHeight);
    const float halfHeight = objectSpecs[index].size.y * 0.5f;
    const auto unitBounds = objectSolid(index, {{0.0f, 0.0f, 0.0f}, 1.0f}, true).bounds;
    const float preferredScale = std::max(minimumScale, ratio * playerEyeHeight / (1.0f + ratio * halfHeight));

    const auto tryPose = [&](const glm::vec2& heading, bool belowPlayer, float scale)
        -> std::optional<perspective::Pose> {
        glm::vec2 center = origin;
        if (!belowPlayer) {
            const auto distancePastPlayer = [&](int axis) {
                if (std::abs(heading[axis]) < 0.0001f) {
                    return std::numeric_limits<float>::infinity();
                }
                const float halfTowardPlayer = heading[axis] > 0.0f ? -unitBounds.min[axis] : unitBounds.max[axis];
                return (playerRadius + placementClearance + halfTowardPlayer * scale) / std::abs(heading[axis]);
            };
            center += heading * std::min(distancePastPlayer(0), distancePastPlayer(1));
        }

        const Footprint bounds{center + unitBounds.min * scale, center + unitBounds.max * scale};
        if (!fitsSupply(bounds)) {
            return std::nullopt;
        }
        const float support = landingHeight(bounds, feet, solids);
        const float height = 2.0f * halfHeight * scale;
        const float ceiling = belowPlayer ? feet : barracksCeiling;
        const float dropGap = std::min(placementClearance, ceiling - support - height);
        if (dropGap < 0.0f) {
            return std::nullopt;
        }
        const float bottom = support + dropGap;
        const Solid release{bounds, bottom, bottom + height};
        if (overlapsPlayer(release, eye) || std::any_of(solids.begin(), solids.end(), [&](const Solid& solid) {
                return solid.top > support + contactTolerance && solid.bottom < release.top - contactTolerance &&
                       solid.bounds.overlaps(bounds);
            })) {
            return std::nullopt;
        }
        return perspective::Pose{{center.x, bottom + halfHeight * scale, center.y}, scale};
    };

    const auto largestFit = [&](const glm::vec2& heading, bool belowPlayer = false) {
        if (const auto fullSize = tryPose(heading, belowPlayer, preferredScale)) {
            return fullSize;
        }
        auto best = tryPose(heading, belowPlayer, minimumScale);
        if (!best) {
            return best;
        }
        float low = minimumScale;
        float high = preferredScale;
        for (int iteration = 0; iteration < 10; ++iteration) {
            const float scale = (low + high) * 0.5f;
            if (const auto pose = tryPose(heading, belowPlayer, scale)) {
                best = pose;
                low = scale;
            } else {
                high = scale;
            }
        }
        return best;
    };

    if (feet > contactTolerance) {
        if (const auto below = largestFit({}, true)) {
            return below;
        }
    }
    const auto aim = horizontal(direction);
    const glm::vec2 forward = glm::length(aim) > 0.0001f ? glm::normalize(aim) : glm::vec2{1.0f, 0.0f};
    const glm::vec2 right{-forward.y, forward.x};
    const glm::vec2 directions[]{forward, forward + right, forward - right, right, -right,
                                -forward + right, -forward - right, -forward};
    for (const auto& heading : directions) {
        if (const auto beside = largestFit(heading)) {
            return beside;
        }
    }
    return std::nullopt;
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

MilitaryRoom::MilitaryRoom() {
    static_assert(std::size(objectSpecs) == belongingsCount + 1);
    static_assert(std::size(returnSlots) == belongingsCount);
    for (std::size_t index = 0; index < objects_.size(); ++index) {
        objects_[index] = {objectSpecs[index].initialCenter, 1.0f};
    }
}

bool MilitaryRoom::initialize() {
    return renderer_.initialize(staticGeometry, {}, moonGeometry);
}

bool MilitaryRoom::inBarracks() const {
    return exitState_ == ExitState::Locked || exitState_ == ExitState::Open;
}

std::optional<MilitaryRoom::Placement> MilitaryRoom::placement(
    const glm::vec3& eye, const glm::vec3& direction) const {
    if (!heldObject_ || eye.y <= 0.0f || heldRatio_ <= 0.0f) {
        return std::nullopt;
    }

    const std::size_t index = *heldObject_;
    const glm::vec3 half = objectSpecs[index].size * 0.5f;
    RoomSolids storage;
    const auto solids = barracksSolids(storage, objects_, heldObject_);
    if (index != televisionIndex && direction.y < -0.001f && eye.y > slotTop) {
        const float rackDepth = (slotTop - eye.y) / direction.y;
        const glm::vec2 rackAim = horizontal(eye + direction * rackDepth);
        for (std::size_t slotIndex = 0; slotIndex < belongingsCount; ++slotIndex) {
            const auto& slot = returnSlots[slotIndex];
            if (footprint(slot.center, rackHalf).contains(rackAim) &&
                rackDepth <= nearestFurnitureDepth(eye, direction)) {
                const float scale = perspective::scaleAtSupport(eye, direction, slot.center, heldRatio_, half.y);
                const perspective::Pose pose{slot.center + glm::vec3{0.0f, rackDropHeight + half.y * scale, 0.0f}, scale};
                const auto release = objectSolid(index, pose, true);
                const bool fits = index == slotIndex && !returned_[slotIndex] && scale >= minimumScale &&
                                  release.top <= barracksCeiling && fitsReturnSlot(release.bounds, slot) &&
                                  !overlapsPlayer(release, eye) &&
                                  std::none_of(solids.begin(), solids.end(), [&](const Solid& solid) {
                                      return release.bottom < solid.top && release.top > solid.bottom &&
                                             release.bounds.overlaps(solid.bounds);
                                  });
                return Placement{pose, fits, true};
            }
        }
    }

    auto target = dropFromAim(eye, direction, heldRatio_, index, solids);
    if (!target) {
        target = dropNearFeet(eye, direction, heldRatio_, index, solids);
    }
    return target ? std::optional{Placement{*target}} : std::nullopt;
}

void MilitaryRoom::update(float elapsed, const glm::vec3& playerEye) {
    if (!inBarracks() || elapsed <= 0.0f) {
        return;
    }
    std::array<Solid, std::size(objectSpecs)> previous;
    std::array<std::size_t, std::size(objectSpecs)> order;
    for (std::size_t index = 0; index < objects_.size(); ++index) {
        previous[index] = objectSolid(index, objects_[index], true);
        order[index] = index;
    }
    // Move lower bodies first so a falling stack can share the motion of its support.
    std::sort(order.begin(), order.end(), [&previous](std::size_t left, std::size_t right) {
        return previous[left].bottom < previous[right].bottom;
    });
    auto updated = previous;
    RoomSolids storage;
    const auto fixed = barracksSolids(storage, objects_, heldObject_).first(staticSolidCount);
    const Solid player{footprint(playerEye, glm::vec2{playerRadius}), playerEye.y - playerEyeHeight,
                       playerEye.y + headClearance};
    for (const std::size_t index : order) {
        if (heldObject_ == index || (index != televisionIndex && returned_[index])) {
            continue;
        }
        const auto& body = previous[index];
        float support = landingHeight(body.bounds, body.bottom, fixed);
        float supportSpeed = 0.0f;
        if (player.top <= body.bottom + contactTolerance && body.bounds.overlaps(player.bounds)) {
            support = std::max(support, player.top);
        }
        for (std::size_t other = 0; other < objects_.size(); ++other) {
            if (other == index || heldObject_ == other || previous[other].top > body.bottom + contactTolerance ||
                !body.bounds.overlaps(updated[other].bounds)) {
                continue;
            }
            const float top = updated[other].top;
            if (top > support || (top == support && verticalSpeeds_[other] > supportSpeed)) {
                support = top;
                supportSpeed = verticalSpeeds_[other];
            }
        }

        auto& speed = verticalSpeeds_[index];
        const float fallingBottom = body.bottom + (speed - 0.5f * gravity * elapsed) * elapsed;
        const bool landed = fallingBottom <= support + contactTolerance;
        const float bottom = landed ? support : fallingBottom;
        auto& pose = objects_[index];
        const float fallDistance = std::max(0.0f, body.bottom - bottom);
        if (fallDistance > 0.0f && fallGrowthRates_[index] > 0.0f) {
            const auto center = horizontal(pose.center);
            const auto unitBounds = objectSolid(index, {{0.0f, 0.0f, 0.0f}, 1.0f}, true).bounds;
            float scaleLimit = wallScaleLimit(center, unitBounds);
            const auto limitAgainst = [&](const Solid& obstacle) {
                // A top the body is landing on can support a wider footprint.
                if (bottom < obstacle.top - contactTolerance && body.top > obstacle.bottom + contactTolerance) {
                    scaleLimit = std::min(scaleLimit, sideContactScale(center, unitBounds, obstacle.bounds));
                }
            };
            for (const auto& obstacle : fixed) {
                limitAgainst(obstacle);
            }
            for (std::size_t other = 0; other < objects_.size(); ++other) {
                if (other != index && heldObject_ != other) {
                    limitAgainst(updated[other]);
                }
            }
            limitAgainst(player);
            const float desiredScale = pose.scale + fallGrowthRates_[index] * fallDistance;
            pose.scale = std::max(pose.scale, std::min(desiredScale, scaleLimit));
        }
        const float halfHeight = objectSpecs[index].size.y * (0.5f * pose.scale);
        pose.center.y = bottom + halfHeight;
        updated[index] = objectSolid(index, pose, true);
        speed = landed ? supportSpeed : speed - gravity * elapsed;
        if (landed && index < belongingsCount && speed == 0.0f && std::abs(support - slotTop) <= contactTolerance &&
            fitsReturnSlot(updated[index].bounds, returnSlots[index])) {
            returned_[index] = true;
        }
    }
    if (exitState_ == ExitState::Locked &&
        std::all_of(returned_.begin(), returned_.end(), [](bool returned) { return returned; })) {
        exitState_ = ExitState::Open;
    }
}

void MilitaryRoom::interact(const glm::vec3& eye, const glm::vec3& direction) {
    if (!inBarracks()) {
        interactMoon(eye, direction);
        return;
    }
    if (heldObject_) {
        if (const auto target = placement(eye, direction); target && target->fits) {
            const std::size_t index = *heldObject_;
            objects_[index] = target->pose;
            verticalSpeeds_[index] = 0.0f;
            fallGrowthRates_[index] = target->returning ? 0.0f : heldRatio_;
            heldObject_.reset();
        }
        return;
    }

    float closest = nearestFurnitureDepth(eye, direction);
    for (std::size_t index = 0; index < objects_.size(); ++index) {
        if (index == televisionIndex ? exitState_ == ExitState::Locked : returned_[index]) {
            continue;
        }
        const auto& object = objects_[index];
        const auto hit = perspective::rayBoxDistance(eye, direction, object.center,
                                                     objectSpecs[index].size * (object.scale * 0.5f));
        if (hit && *hit > 0.05f && *hit < closest) {
            closest = *hit;
            heldObject_ = index;
        }
    }
    if (heldObject_) {
        const auto& object = objects_[*heldObject_];
        const auto& size = objectSpecs[*heldObject_].size;
        heldRatio_ = perspective::grabRatio(eye, direction, object.center, object.scale,
                                           std::max({size.x, size.y, size.z}));
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
    return onOutpostRoute(point) || contains(bridge_, point);
}

bool MilitaryRoom::canStandAtOutpost(const glm::vec3& position) const {
    const auto bounds = footprint(position, glm::vec2{playerRadius});
    return supports(bounds.min) && supports(bounds.max) &&
           supports({bounds.min.x, bounds.max.y}) && supports({bounds.max.x, bounds.min.y}) &&
           !bounds.overlaps(pillarFoot);
}

bool MilitaryRoom::canEnterTelevision(const glm::vec3& previous, const glm::vec3& desired,
                                      const glm::vec3& facing) const {
    const auto& television = objects_[televisionIndex];
    const float scale = television.scale;
    const glm::vec2 movement = horizontal(desired - previous);
    if (exitState_ != ExitState::Open || heldObject_ || verticalSpeeds_[televisionIndex] != 0.0f ||
        scale < televisionEntryScale ||
        facing.x <= 0.30f || movement.x <= 0.0f || glm::dot(movement, horizontal(facing)) <= 0.0f) {
        return false;
    }
    const glm::vec3 screenCenter = television.center + screenOffset * scale;
    const glm::vec3 screenHalf = screenSize * (0.5f * scale);
    const float edge = screenCenter.x - screenHalf.x - playerRadius;
    if (previous.x > edge || desired.x <= edge) {
        return false;
    }
    const glm::vec3 crossing = glm::mix(previous, desired, (edge - previous.x) / movement.x);
    return std::abs(crossing.z - screenCenter.z) <= screenHalf.z - playerRadius &&
           std::abs(crossing.y - screenCenter.y) < screenHalf.y;
}

glm::vec3 MilitaryRoom::resolveMovement(const glm::vec3& previous, const glm::vec3& desired,
                                        const glm::vec3& facing, bool* grounded) {
    const auto groundAt = [&](glm::vec3 position, float height) {
        const bool landed = position.y <= height + playerEyeHeight;
        if (grounded) {
            *grounded = landed;
        }
        position.y = std::max(position.y, height + playerEyeHeight);
        return position;
    };
    if (canEnterTelevision(previous, desired, facing)) {
        exitState_ = ExitState::OutpostReached;
        return groundAt({outpostEntrance.x, desired.y, outpostEntrance.z}, 0.0f);
    }
    const bool barracks = inBarracks();
    RoomSolids storage;
    // The screen protrudes from the TV body. Keep the portal reachable before touching its body.
    const auto solids = barracks ? barracksSolids(storage, objects_, heldObject_, false) : std::span<const Solid>{};
    const auto canMoveTo = [&](const glm::vec3& position) {
        if (!barracks) {
            return canStandAtOutpost(position);
        }
        return fitsSupply(footprint(position, glm::vec2{playerRadius})) &&
               std::none_of(solids.begin(), solids.end(), [&](const Solid& solid) {
                   return overlapsPlayer(solid, position);
               });
    };
    // Move sideways above a top only after the feet clear it. On descent, land after moving sideways.
    glm::vec3 position{desired.x, std::max(previous.y, desired.y), previous.z};
    if (!canMoveTo(position)) {
        position.x = previous.x;
    }
    position.z = desired.z;
    if (!canMoveTo(position)) {
        position.z = previous.z;
    }
    position.y = desired.y;
    if (barracks) {
        const auto bounds = footprint(position, glm::vec2{playerRadius});
        if (desired.y > previous.y) {
            position.y = std::min(position.y, barracksCeiling - headClearance);
            for (const auto& solid : solids) {
                if (bounds.overlaps(solid.bounds) && previous.y + headClearance <= solid.bottom + contactTolerance) {
                    position.y = std::min(position.y, solid.bottom - headClearance);
                }
            }
            if (grounded) {
                *grounded = false;
            }
            return position;
        }
        return groundAt(position, landingHeight(bounds, previous.y - playerEyeHeight, solids));
    }
    if (exitState_ == ExitState::BridgeReady && position.z >= 15.70f) {
        exitState_ = ExitState::Escaped;
    }
    return groundAt(position, 0.0f);
}

MilitaryRoom::ExitState MilitaryRoom::exitState() const {
    return exitState_;
}

void MilitaryRoom::drawObject(std::size_t index, const perspective::Pose& pose, const glm::vec3& tint) const {
    const auto& spec = objectSpecs[index];
    const glm::vec3 size = spec.size * pose.scale;
    const auto part = [&](const glm::vec3& offset, const glm::vec3& dimensions, const glm::vec3& color) {
        renderer_.drawBox(pose.center + offset * size, dimensions * size, color * tint);
    };

    if (index == 0) {
        // Duffel bag: soft body, shoulder straps and a raised handle.
        part({0.0f, -0.09f, 0.0f}, {1.0f, 0.82f, 0.96f}, spec.color);
        for (float x : {-0.29f, 0.29f}) {
            part({x, 0.32f, 0.0f}, {0.10f, 0.07f, 1.0f}, {0.26f, 0.30f, 0.17f});
        }
        part({0.0f, 0.44f, 0.0f}, {0.37f, 0.12f, 0.12f}, {0.29f, 0.34f, 0.17f});
    } else if (index == 1) {
        // Bedding bundle: folded blanket layers held by light straps.
        part({0.0f, -0.20f, 0.0f}, {1.0f, 0.60f, 1.0f}, spec.color * 0.72f);
        part({0.0f, 0.23f, 0.0f}, {0.95f, 0.27f, 0.92f}, spec.color);
        for (float x : {-0.28f, 0.28f}) {
            part({x, 0.40f, 0.0f}, {0.10f, 0.18f, 1.0f}, {0.68f, 0.76f, 0.81f});
        }
    } else if (index == 2) {
        // Field kit: rigid case, lid and two metal latches.
        part({0.0f, -0.10f, 0.0f}, {0.97f, 0.80f, 1.0f}, spec.color * 0.82f);
        part({0.0f, 0.38f, 0.0f}, {1.0f, 0.24f, 1.0f}, spec.color);
        for (float z : {-0.29f, 0.29f}) {
            part({-0.47f, 0.22f, z}, {0.05f, 0.29f, 0.12f}, {0.37f, 0.39f, 0.38f});
        }
    } else {
        part({}, {1.0f, 1.0f, 1.0f}, spec.color);
        const bool powered = exitState_ == ExitState::Open;
        const bool portal = powered && pose.scale >= televisionEntryScale;
        const glm::vec3 screenColor = portal ? glm::vec3{0.006f, 0.008f, 0.012f}
            : powered ? glm::vec3{0.23f, 0.61f, 0.79f} : glm::vec3{0.07f, 0.08f, 0.09f};
        renderer_.drawBox(pose.center + screenOffset * pose.scale, screenSize * pose.scale, screenColor * tint);
        if (powered && !portal) {
            part({-0.55f, 0.08f, 0.0f}, {0.02f, 0.06f, 0.63f}, {0.47f, 0.81f, 0.90f});
        }
        part({-0.516f, -0.443f, 0.37f}, {0.05f, 0.04f, 0.04f},
             powered ? glm::vec3{0.28f, 0.91f, 0.43f} : glm::vec3{0.64f, 0.16f, 0.12f});
    }
}

void MilitaryRoom::drawBarracks(const glm::vec3& eye, const glm::vec3& direction) const {
    for (std::size_t index = 0; index < belongingsCount; ++index) {
        const auto& slot = returnSlots[index];
        const glm::vec2 half = slot.opening * 0.5f;
        const glm::vec3 mark = objectSpecs[index].color;
        renderer_.drawBox({slot.center.x, slotTop * 0.5f, slot.center.z},
                          {2.0f * rackHalf.x, slotTop, 2.0f * rackHalf.y}, {0.32f, 0.33f, 0.28f});
        for (float side : {-1.0f, 1.0f}) {
            renderer_.drawBox(slot.center + glm::vec3{side * half.x, 0.012f, 0.0f},
                              {0.023f, 0.024f, slot.opening.y}, mark);
            renderer_.drawBox(slot.center + glm::vec3{0.0f, 0.012f, side * half.y},
                              {slot.opening.x, 0.024f, 0.023f}, mark);
        }
        renderer_.drawBox({slot.center.x, slotTop - 0.12f, slot.center.z + rackHalf.y},
                          {0.30f, 0.075f, 0.012f}, mark);
    }

    for (std::size_t index = 0; index < objects_.size(); ++index) {
        perspective::Pose displayed = objects_[index];
        glm::vec3 tint{1.0f};
        if (heldObject_ == index) {
            constexpr glm::vec3 blockedTint{1.35f, 0.36f, 0.36f};
            if (const auto target = placement(eye, direction)) {
                displayed = target->pose;
                tint = target->fits ? glm::vec3{0.82f, 1.27f, 1.05f} : blockedTint;
            } else {
                constexpr float carryDepth = 1.45f;
                displayed = {eye + direction * carryDepth, std::min(heldRatio_ * carryDepth, 0.55f)};
                tint = blockedTint;
            }
        }
        drawObject(index, displayed, tint);
    }
}

void MilitaryRoom::draw(const glm::mat4& viewProjection, const glm::vec3& eye, const glm::vec3& direction) const {
    renderer_.begin(viewProjection);
    renderer_.drawStaticMesh();

    if (inBarracks()) {
        drawBarracks(eye, direction);
    }

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
