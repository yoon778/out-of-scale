#include "MilitaryRoom.h"

#include <GL/glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <span>
#include <vector>

namespace {
#include "MilitaryGeometry.inc"

constexpr float returnClearance = 0.01f;
constexpr float shelfDropHeight = 0.02f;
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
constexpr glm::vec3 outpostEntrance{-8.30f, 1.65f, 9.60f};
constexpr float moonCarryDepth = 3.2f;
constexpr float caveCarryDepth = 1.25f;
constexpr float caveMaximumMoonScale = 0.18f;
constexpr float maximumMoonScale = 0.52f;
constexpr float distantMoonStrength = 0.23f;
constexpr float gapNear = 7.8f;
constexpr float gapFar = 12.5f;
constexpr float trenchY = -4.0f;
constexpr float ladderX = 11.82f;
constexpr float ladderZ = gapNear + playerRadius;
constexpr float ladderReach = 0.46f;
constexpr float climbPerStep = 0.65f;

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

constexpr Solid equipmentBase{{{14.91f, -9.46f}, {16.19f, -8.43f}}, 0.0f, 0.28f};
constexpr Solid equipmentTop{equipmentBase.bounds, 1.36f, 1.48f};
constexpr Solid equipmentSides[]{
    {{{14.91f, -9.46f}, {14.98f, -8.43f}}, equipmentBase.top, equipmentTop.bottom},
    {{{16.12f, -9.46f}, {16.19f, -8.43f}}, equipmentBase.top, equipmentTop.bottom},
    {{{14.98f, -9.46f}, {16.12f, -9.40f}}, equipmentBase.top, equipmentTop.bottom},
};
constexpr Solid equipmentDoor{{{14.98f, -8.49f}, {16.12f, -8.43f}}, equipmentBase.top, equipmentTop.bottom};
constexpr std::array pressurePads{glm::vec3{10.25f, 0.0f, -6.55f}, glm::vec3{11.95f, 0.0f, -6.55f}};
constexpr glm::vec2 padHalfSize{0.27f, 0.27f};
constexpr float padHeight = 0.075f;
constexpr std::size_t gasMaskIndex = 2;
constexpr float gasMaskScale = 0.48f;

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
    float frontOverhang = 0.0f;
    float initialScale = 1.0f;
};

constexpr ObjectSpec objectSpecs[]{
    {{4.80f, 0.725f, -5.95f}, {1.20f, 1.45f, 1.00f}},
    {{7.30f, 0.28f, -4.45f}, {1.54f, 0.56f, 1.05f}},
    {{15.55f, equipmentBase.top + 0.44f * gasMaskScale, -8.94f}, {1.06f, 0.88f, 0.78f}, 0.0f, gasMaskScale},
    {{14.70f, 0.54f, -5.20f}, {0.50f, 1.08f, 1.58f}, 0.03f},
    {{8.90f, 0.04f, -4.05f}, {0.94f, 0.08f, 0.48f}},
};

struct ReturnSlot {
    glm::vec3 anchor;
    glm::vec2 halfSize;
    float floor;
    float ceiling;
    bool hangs = false;
};

// The locker sits beside each bed, with its front facing the aisle.
constexpr float emptyLockerBedX = 10.80f;
constexpr float lockerSideOffset = 1.10f;
constexpr float lockerAisleOffset = 0.74f;
constexpr glm::vec3 lockerScale{1.10f, 1.14f, 1.28f};
constexpr float southWallZ = -10.40f;
constexpr float lockerHalfWidth = 0.50f * lockerScale.x;

constexpr float northLockerZ(float modelZ) {
    return modelZ * lockerScale.z - lockerAisleOffset;
}

constexpr float emptyLockerX(float modelOffsetX) {
    return emptyLockerBedX + lockerSideOffset + modelOffsetX * lockerScale.x;
}

constexpr float lockerLeft = emptyLockerX(-0.475f);
constexpr float lockerRight = emptyLockerX(0.475f);
constexpr float safeRight = emptyLockerX(-0.025f);
constexpr float cubbyLeft = emptyLockerX(0.005f);
constexpr float lockerFront = northLockerZ(-0.428f);
constexpr float lockerBack = northLockerZ(-0.095f);
constexpr float lockerShellFront = northLockerZ(-0.445f);
constexpr float lockerShellBack = northLockerZ(-0.050f);
constexpr float lockerDoorFront = northLockerZ(-0.454f);
constexpr float shelfZ = (lockerFront + lockerBack) * 0.5f;
constexpr float shelfHalfDepth = (lockerBack - lockerFront) * 0.5f;
constexpr ReturnSlot returnSlots[]{
    {{emptyLockerX(-0.25f), 1.83f * lockerScale.y, shelfZ},
     {0.215f * lockerScale.x, shelfHalfDepth}, 0.835f * lockerScale.y, 2.110f * lockerScale.y, true},
    {{emptyLockerX(0.0f), 2.15f * lockerScale.y, shelfZ},
     {(lockerRight - lockerLeft) * 0.5f, shelfHalfDepth}, 2.15f * lockerScale.y, 2.55f * lockerScale.y},
    {{(cubbyLeft + lockerRight) * 0.5f, 0.40f * lockerScale.y, shelfZ},
     {(lockerRight - cubbyLeft) * 0.5f, shelfHalfDepth}, 0.40f * lockerScale.y, 0.805f * lockerScale.y},
};
constexpr Footprint lockerInterior{{lockerLeft, lockerFront}, {lockerRight, lockerBack}};
constexpr Solid lockerPanels[]{
    {{{emptyLockerX(-0.50f), lockerShellFront}, {lockerLeft, lockerShellBack}}, 0.0f, returnSlots[1].floor},
    {{{lockerRight, lockerShellFront}, {emptyLockerX(0.50f), lockerShellBack}}, 0.0f, returnSlots[1].floor},
    {{{lockerLeft, lockerBack}, {lockerRight, lockerShellBack}}, 0.0f, returnSlots[1].floor},
    {{{lockerLeft, lockerDoorFront}, {lockerRight, lockerBack}}, 0.0f, returnSlots[2].floor},
    {lockerInterior, returnSlots[2].ceiling, returnSlots[0].floor},
    {{{lockerLeft, lockerShellFront}, {lockerRight, lockerBack}}, returnSlots[0].ceiling, returnSlots[1].floor},
    {{{safeRight, lockerFront}, {cubbyLeft, lockerBack}}, returnSlots[2].floor, returnSlots[2].ceiling},
    {{{lockerLeft, lockerDoorFront}, {safeRight, lockerBack}}, returnSlots[2].floor, returnSlots[2].ceiling},
};
constexpr Solid gunCabinet{{{12.95f, -10.36f}, {14.85f, -9.75f}}, 0.0f, 1.95f};
constexpr Solid clothingBags{{{11.72f, -10.30f}, {12.53f, -9.69f}}, 0.0f, 0.83f};

constexpr Footprint supply{{0.0f, southWallZ}, {16.60f, 0.0f}};
constexpr Footprint approach{{8.25f, 0.0f}, {12.6f, gapNear}};
constexpr Footprint ledge{{7.20f, 0.0f}, {8.25f, 5.15f}};
constexpr Footprint gap{{8.4f, gapNear}, {12.6f, gapFar}};
constexpr Footprint farBank{{8.4f, gapFar}, {12.6f, 15.4f}};
constexpr Footprint exitWalkway{{9.80f, 15.4f}, {11.20f, 16.18f}};
constexpr Footprint banks[]{approach, ledge, farBank, exitWalkway};
constexpr auto furniture = [] {
    constexpr std::array bedCenters{2.0f, 4.2f, 6.4f, 8.6f, 10.8f};
    constexpr float halfWidth = 0.54f;
    constexpr float bedHeight = 0.85f;
    constexpr float bedAisleOffset = 0.55f;
    constexpr float lockerHeight = returnSlots[1].floor;
    std::array<Solid, bedCenters.size() * 4 + 3 + std::size(lockerPanels) +
                      std::size(equipmentSides) + pressurePads.size()> layout{};
    std::size_t index = 0;
    for (float x : bedCenters) {
        const float left = x - halfWidth;
        const float right = x + halfWidth;
        const float lockerX = x + lockerSideOffset;
        const float lockerLeftEdge = lockerX - lockerHalfWidth;
        const float lockerRightEdge = lockerX + lockerHalfWidth;
        layout[index++] = {{{left, -2.48f - bedAisleOffset}, {right, -0.48f - bedAisleOffset}}, 0.0f, bedHeight};
        layout[index++] = {{{left, -9.92f + bedAisleOffset}, {right, -7.92f + bedAisleOffset}}, 0.0f, bedHeight};
        layout[index++] = {{{lockerLeftEdge, southWallZ - lockerShellBack},
                            {lockerRightEdge, southWallZ - lockerDoorFront}}, 0.0f, lockerHeight};
        if (x != emptyLockerBedX) {
            layout[index++] = {{{lockerLeftEdge, lockerDoorFront},
                                {lockerRightEdge, lockerShellBack}}, 0.0f, lockerHeight};
        }
    }
    for (const auto& panel : lockerPanels) {
        layout[index++] = panel;
    }
    layout[index++] = gunCabinet;
    layout[index++] = clothingBags;
    layout[index++] = equipmentBase;
    layout[index++] = equipmentTop;
    for (const auto& side : equipmentSides) {
        layout[index++] = side;
    }
    for (const auto& pad : pressurePads) {
        layout[index++] = {{{pad.x - padHalfSize.x, pad.z - padHalfSize.y},
                            {pad.x + padHalfSize.x, pad.z + padHalfSize.y}}, 0.0f, padHeight};
    }
    return layout;
}();
constexpr Footprint pillarFoot{{9.865f, 5.745f}, {11.135f, 6.555f}};

struct Bounds {
    glm::vec3 min;
    glm::vec3 max;
};

// Body, foot and cap bounds from the Blender Entry geometry.
constexpr Bounds pillarParts[]{
    {{9.97f, 0.0f, 5.84f}, {11.03f, 2.28f, 6.46f}},
    {{9.865f, 0.0f, 5.745f}, {11.135f, 0.16f, 6.555f}},
    {{9.945f, 2.255f, 5.815f}, {11.055f, 2.345f, 6.485f}},
};

#include "CaveScene.inc"

bool moonLightsPillar(const glm::vec3& moonCenter) {
    const auto& pillar = pillarParts[0];
    const glm::vec3 toPillar = (pillar.min + pillar.max) * 0.5f - moonCenter;
    return caveWallDepth(moonCenter, toPillar, 1.0f) >= 1.0f;
}

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

using RoomSolids = std::array<Solid, furniture.size() + 1 + std::size(objectSpecs)>;

std::span<const Solid> fixedBarracksSolids(RoomSolids& storage, bool cabinetOpen) {
    auto end = std::copy(furniture.begin(), furniture.end(), storage.begin());
    if (!cabinetOpen) {
        *end++ = equipmentDoor;
    }
    return {storage.begin(), end};
}

std::span<const Solid> barracksSolids(
    RoomSolids& storage, const std::array<perspective::Pose, std::size(objectSpecs)>& objects,
    std::optional<std::size_t> held, bool cabinetOpen, bool includeScreen = true) {
    std::size_t count = fixedBarracksSolids(storage, cabinetOpen).size();
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

bool fitsReturnSlot(const Solid& object, const ReturnSlot& slot) {
    const auto opening = footprint(slot.anchor, slot.halfSize - glm::vec2{returnClearance});
    return object.bottom >= slot.floor - contactTolerance && object.top <= slot.ceiling - returnClearance &&
           opening.contains(object.bounds.min) && opening.contains(object.bounds.max);
}

float nearestSolidDepth(const glm::vec3& eye, const glm::vec3& direction, std::span<const Solid> solids) {
    float closest = std::numeric_limits<float>::infinity();
    for (const auto& obstacle : solids) {
        const glm::vec2 middle = (obstacle.bounds.min + obstacle.bounds.max) * 0.5f;
        const glm::vec2 half = (obstacle.bounds.max - obstacle.bounds.min) * 0.5f;
        const auto hit = perspective::rayBoxDistance(eye, direction,
            {middle.x, (obstacle.bottom + obstacle.top) * 0.5f, middle.y},
            {half.x, (obstacle.top - obstacle.bottom) * 0.5f, half.y});
        if (hit) {
            closest = std::min(closest, *hit);
        }
    }
    return closest;
}

bool onOutpostRoute(const glm::vec2& point) {
    const auto contains = [&](const Footprint& area) { return area.contains(point); };
    return clearing.contains(point) || std::any_of(cavePassages.begin(), cavePassages.end(), contains) ||
           std::any_of(std::begin(banks), std::end(banks), contains);
}

bool inCavePassage(const glm::vec2& point) {
    return std::any_of(cavePassages.begin(), cavePassages.end(),
                       [&](const Footprint& area) { return area.contains(point); });
}

glm::vec3 groundAt(glm::vec3 position, float height, bool* grounded) {
    const float restingEye = height + playerEyeHeight;
    if (grounded) {
        *grounded = position.y <= restingEye;
    }
    position.y = std::max(position.y, restingEye);
    return position;
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
    constexpr float projectionClearance = 0.002f;
    if (light.y <= surfaceY + projectionClearance) {
        return {};
    }
    Polygon points;
    for (const auto& part : pillarParts) {
        const float bottom = std::max(part.min.y, surfaceY);
        // The part above a low moon cannot block rays to the ground below it.
        const float top = std::min(part.max.y, light.y - projectionClearance);
        if (top < bottom) {
            continue;
        }
        for (float x : {part.min.x, part.max.x}) {
            for (float y : {bottom, top}) {
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
    float twiceArea = 0.0f;
    for (std::size_t i = 2; i < clipped.size(); ++i) {
        twiceArea += cross(clipped[i - 1] - clipped.front(), clipped[i] - clipped.front());
    }
    if (twiceArea <= 0.00001f) {
        return {};
    }
    return clipped;
}

bool containsPoint(const Polygon& outline, const glm::vec2& point) {
    if (outline.empty()) {
        return false;
    }
    for (std::size_t i = 0; i < outline.size(); ++i) {
        const auto& start = outline[i];
        const auto edge = outline[(i + 1) % outline.size()] - start;
        if (cross(edge, point - start) < -0.00001f) {
            return false;
        }
    }
    return true;
}
}

MilitaryRoom::MilitaryRoom() {
    static_assert(std::size(objectSpecs) == objectCount);
    static_assert(std::size(returnSlots) == belongingsCount);
    static_assert(std::size(gearOffsets) == belongingsCount + 1);
    for (std::size_t index = 0; index < objects_.size(); ++index) {
        objects_[index] = {objectSpecs[index].initialCenter, objectSpecs[index].initialScale};
    }
}

bool MilitaryRoom::initialize() {
    std::vector<MeshVertex> movable(std::begin(barracksGear), std::end(barracksGear));
    const auto& caveVertices = caveScene().vertices;
    movable.insert(movable.end(), caveVertices.begin(), caveVertices.end());
    return renderer_.initialize(staticGeometry, movable, moonGeometry);
}

bool MilitaryRoom::inBarracks() const {
    return exitState_ == ExitState::Locked || exitState_ == ExitState::Open;
}

bool MilitaryRoom::pressesPad(const glm::vec3& padCenter) const {
    const auto plate = objectSolid(steelPlateIndex, objects_[steelPlateIndex], false);
    return heldObject_ != steelPlateIndex && verticalSpeeds_[steelPlateIndex] == 0.0f &&
           std::abs(plate.bottom - padHeight) <= contactTolerance &&
           plate.bounds.contains(horizontal(padCenter));
}

bool MilitaryRoom::returnToBarracks() {
    if (inBarracks()) {
        return false;
    }
    exitState_ = ExitState::Open;
    climbing_ = false;
    return true;
}

std::optional<MilitaryRoom::Placement> MilitaryRoom::placement(
    const glm::vec3& eye, const glm::vec3& direction) const {
    if (!heldObject_ || eye.y <= 0.0f || heldRatio_ <= 0.0f) {
        return std::nullopt;
    }

    const std::size_t index = *heldObject_;
    const glm::vec3 half = objectSpecs[index].size * 0.5f;
    RoomSolids storage;
    const auto solids = barracksSolids(storage, objects_, heldObject_, equipmentCabinetOpen_);
    if (index < belongingsCount) {
        const float obstacleDepth = nearestSolidDepth(eye, direction, solids);
        for (const auto& slot : returnSlots) {
            if (slot.hangs && !returnSlots[index].hangs) {
                continue;
            }
            const float halfHeight = (slot.ceiling - slot.floor) * 0.5f;
            const auto hit = perspective::rayBoxDistance(eye, direction,
                {slot.anchor.x, slot.floor + halfHeight, slot.anchor.z}, {slot.halfSize.x, halfHeight, slot.halfSize.y});
            if (!hit || *hit >= obstacleDepth) {
                continue;
            }

            const float halfFromAnchor = slot.hangs ? -half.y : half.y;
            const float scale = perspective::scaleAtSupport(eye, direction, slot.anchor, heldRatio_, halfFromAnchor);
            const float releaseOffset = halfFromAnchor * scale + (slot.hangs ? 0.0f : shelfDropHeight);
            const perspective::Pose pose{slot.anchor + glm::vec3{0.0f, releaseOffset, 0.0f}, scale};
            const auto release = objectSolid(index, pose, true);
            if (scale >= minimumScale && fitsReturnSlot(release, slot) && !overlapsPlayer(release, eye) &&
                std::none_of(solids.begin(), solids.end(), [&](const Solid& solid) {
                    return release.bottom < solid.top && release.top > solid.bottom && release.bounds.overlaps(solid.bounds);
                })) {
                return Placement{pose, slot.hangs ? Support::Hook : Support::Shelf};
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
    const auto fixed = fixedBarracksSolids(storage, equipmentCabinetOpen_);
    const Solid player{footprint(playerEye, glm::vec2{playerRadius}), playerEye.y - playerEyeHeight,
                       playerEye.y + headClearance};
    for (const std::size_t index : order) {
        if (heldObject_ == index || (index < belongingsCount && returned_[index])) {
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
        if (landed && index < belongingsCount && speed == 0.0f) {
            const auto& slot = returnSlots[index];
            returned_[index] = !slot.hangs && std::abs(support - slot.floor) <= contactTolerance &&
                               fitsReturnSlot(updated[index], slot);
        }
    }
    if (!equipmentCabinetOpen_ && std::all_of(pressurePads.begin(), pressurePads.end(),
                                              [this](const auto& pad) { return pressesPad(pad); })) {
        equipmentCabinetOpen_ = true;
    }
    if (exitState_ == ExitState::Locked &&
        std::all_of(returned_.begin(), returned_.end(), [](bool returned) { return returned; })) {
        exitState_ = ExitState::Open;
    }
}

void MilitaryRoom::interact(const glm::vec3& eye, const glm::vec3& direction) {
    if (exitState_ == ExitState::Escaped) {
        return;
    }
    if (!inBarracks()) {
        interactMoon(eye, direction);
        return;
    }
    if (heldObject_) {
        if (const auto target = placement(eye, direction)) {
            const std::size_t index = *heldObject_;
            objects_[index] = target->pose;
            verticalSpeeds_[index] = 0.0f;
            fallGrowthRates_[index] = target->support == Support::Free ? heldRatio_ : 0.0f;
            if (target->support == Support::Hook) {
                returned_[index] = true;
            }
            heldObject_.reset();
        }
        return;
    }

    RoomSolids storage;
    float closest = nearestSolidDepth(eye, direction, fixedBarracksSolids(storage, equipmentCabinetOpen_));
    for (std::size_t index = 0; index < objects_.size(); ++index) {
        if ((index == televisionIndex && exitState_ == ExitState::Locked) ||
            (index < belongingsCount && returned_[index])) {
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

perspective::Pose MilitaryRoom::carriedMoonPose(const glm::vec3& eye, const glm::vec3& direction) const {
    const bool inside = inCavePassage(horizontal(eye));
    float depth = inside ? caveCarryDepth : moonCarryDepth;
    float scale = inside ? std::min(moonScale_, caveMaximumMoonScale) : moonScale_;
    const float largestHalf = 0.5f * std::max({moonWorldSize.x, moonWorldSize.y, moonWorldSize.z});
    const float probeDepth = depth + largestHalf * scale + placementClearance;
    const float wallDepth = caveWallDepth(eye, direction, probeDepth);
    if (wallDepth < probeDepth) {
        depth = std::min(depth, wallDepth * 0.58f);
        scale = std::min(scale, std::max(0.01f, (wallDepth - depth - placementClearance) / largestHalf));
    }
    const glm::vec3 center = eye + direction * depth;
    const auto touchesRock = [&](float candidateScale) {
        const glm::vec3 half = moonWorldSize * (candidateScale * 0.5f);
        return caveRockIntersects({center - half, center + half});
    };
    if (touchesRock(scale)) {
        // The moon can graze a side wall even if the sight ray is clear.
        float limit = scale;
        scale = 0.0f;
        for (int iteration = 0; iteration < 8; ++iteration) {
            const float candidate = (scale + limit) * 0.5f;
            if (touchesRock(candidate)) {
                limit = candidate;
            } else {
                scale = candidate;
            }
        }
    }
    return {center, scale};
}

bool MilitaryRoom::canPlaceMoon(const perspective::Pose& pose) const {
    if (pose.scale < 0.01f) {
        return false;
    }
    const glm::vec3 half = moonWorldSize * (pose.scale * 0.5f);
    const auto bounds = footprint(pose.center, horizontal(half));
    const auto min = pose.center - half;
    const auto max = pose.center + half;
    if (min.y < trenchY || (min.y < barracksCeiling && bounds.overlaps(supply))) {
        return false;
    }
    const auto overlaps = [&](const Footprint& floor) { return bounds.overlaps(floor); };
    if (min.y < 0.0f && (overlaps(clearing) || std::any_of(cavePassages.begin(), cavePassages.end(), overlaps) ||
                          std::any_of(std::begin(banks), std::end(banks), overlaps))) {
        return false;
    }
    return !caveRockIntersects({min, max}) &&
           std::none_of(std::begin(pillarParts), std::end(pillarParts), [&](const Bounds& part) {
               return glm::all(glm::lessThan(min, part.max)) && glm::all(glm::greaterThan(max, part.min));
           });
}

void MilitaryRoom::interactMoon(const glm::vec3& eye, const glm::vec3& direction) {
    if (moonHeld_) {
        const auto pose = carriedMoonPose(eye, direction);
        if (canPlaceMoon(pose)) {
            placedMoon_ = pose.center;
            moonScale_ = pose.scale;
            moonHeld_ = false;
            bridge_ = moonLightsPillar(pose.center) ? clipTo(projectPillar(pose.center, 0.0f), gap) : Polygon{};
            exitState_ = outpostState();
        }
        return;
    }

    const float scale = placedMoon_ ? moonScale_ : 1.40f;
    const auto hit = perspective::rayBoxDistance(
        eye, direction, placedMoon_.value_or(moonWorldCenter), moonWorldSize * (scale * 0.5f));
    if (!hit || *hit <= 0.05f) {
        return;
    }
    const bool behindPillar = std::any_of(std::begin(pillarParts), std::end(pillarParts), [&](const Bounds& part) {
        const auto obstacle = perspective::rayBoxDistance(eye, direction, (part.min + part.max) * 0.5f,
                                                          (part.max - part.min) * 0.5f);
        return obstacle && *obstacle < *hit;
    });
    if (!behindPillar && caveWallDepth(eye, direction, *hit) >= *hit) {
        moonHeld_ = true;
        bridge_.clear();
        exitState_ = outpostState();
    }
}

void MilitaryRoom::scroll(double yOffset) {
    if (!inBarracks() && moonHeld_) {
        const auto step = static_cast<float>(std::clamp(yOffset, -8.0, 8.0));
        moonScale_ = std::clamp(moonScale_ * std::pow(1.28f, -step), 0.08f, maximumMoonScale);
    }
}

MilitaryRoom::ExitState MilitaryRoom::outpostState() const {
    return bridge_.empty() ? ExitState::OutpostReached : ExitState::BridgeReady;
}

bool MilitaryRoom::supports(const glm::vec2& point) const {
    return onOutpostRoute(point) || containsPoint(bridge_, point);
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
    if (canEnterTelevision(previous, desired, facing)) {
        placedMoon_.reset();
        moonHeld_ = false;
        moonScale_ = initialMoonScale;
        bridge_.clear();
        exitState_ = ExitState::OutpostReached;
        return groundAt(outpostEntrance, 0.0f, grounded);
    }
    if (!inBarracks()) {
        return resolveOutpostMovement(previous, desired, facing, grounded);
    }
    RoomSolids storage;
    // The screen protrudes from the TV body. Keep the portal reachable before touching its body.
    const auto solids = barracksSolids(storage, objects_, heldObject_, equipmentCabinetOpen_, false);
    const auto canMoveTo = [&](const glm::vec3& position) {
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
    return groundAt(position, landingHeight(bounds, previous.y - playerEyeHeight, solids), grounded);
}

glm::vec3 MilitaryRoom::resolveOutpostMovement(const glm::vec3& previous, const glm::vec3& desired,
                                               const glm::vec3& facing, bool* grounded) {
    const bool belowLip = previous.y < playerEyeHeight;
    const float towardLadder = previous.z - desired.z;
    if (climbing_ && towardLadder < 0.0f) {
        climbing_ = false;
    }
    constexpr float ladderEntry = ladderZ + 0.12f;
    const bool facesNearBank = facing.z < -0.30f * glm::length(horizontal(facing));
    if (belowLip && facesNearBank && towardLadder > 0.0f && desired.z <= ladderEntry &&
        std::abs(desired.x - ladderX) <= ladderReach) {
        climbing_ = true;
    }
    if (climbing_) {
        const float y = previous.y + towardLadder * climbPerStep;
        if (grounded) {
            *grounded = true;
        }
        if (y >= playerEyeHeight) {
            climbing_ = false;
            return {ladderX, playerEyeHeight, gapNear - playerRadius - 0.03f};
        }
        return {ladderX, y, ladderZ};
    }

    const auto canMoveTo = [&](const glm::vec3& position) {
        const auto bounds = footprint(position, glm::vec2{playerRadius});
        if (belowLip) {
            return gap.contains(bounds.min) && gap.contains(bounds.max);
        }
        const auto withinOutpost = [](const glm::vec2& point) {
            return onOutpostRoute(point) || gap.contains(point);
        };
        return withinOutpost(bounds.min) && withinOutpost(bounds.max) &&
               withinOutpost({bounds.min.x, bounds.max.y}) && withinOutpost({bounds.max.x, bounds.min.y}) &&
               !bounds.overlaps(pillarFoot);
    };
    glm::vec3 position{desired.x, desired.y, previous.z};
    if (!canMoveTo(position)) {
        position.x = previous.x;
    }
    position.z = desired.z;
    if (!canMoveTo(position)) {
        position.z = previous.z;
    }

    if (inCavePassage(horizontal(position))) {
        position.y = std::min(position.y, caveCeiling - headClearance);
    }
    const auto bounds = footprint(position, glm::vec2{playerRadius});
    const bool inTrench = gap.contains(bounds.min) && gap.contains(bounds.max);
    const bool supported = supports(bounds.min) && supports(bounds.max) &&
                           supports({bounds.min.x, bounds.max.y}) && supports({bounds.max.x, bounds.min.y});
    const float floor = inTrench && (belowLip || !supported) ? trenchY : 0.0f;
    position = groundAt(position, floor, grounded);
    if (exitState_ == ExitState::BridgeReady && position.y == playerEyeHeight &&
        farBank.contains(bounds.min) && farBank.contains(bounds.max)) {
        exitState_ = ExitState::Escaped;
    }
    return position;
}

MilitaryRoom::ExitState MilitaryRoom::exitState() const {
    return exitState_;
}

void MilitaryRoom::drawObject(std::size_t index, const perspective::Pose& pose, const glm::vec3& tint) const {
    const glm::vec3 size = objectSpecs[index].size * pose.scale;
    if (index < belongingsCount) {
        auto model = glm::translate(glm::mat4{1.0f}, pose.center);
        if (index == gasMaskIndex && pose.center.y < equipmentTop.bottom &&
            equipmentBase.bounds.contains(horizontal(pose.center))) {
            model = glm::rotate(model, glm::radians(180.0f), {0.0f, 1.0f, 0.0f});
        }
        model = glm::scale(model, size);
        const GLint first = gearOffsets[index];
        renderer_.drawMovableMesh(first, gearOffsets[index + 1] - first, model, tint);
        return;
    }

    const auto part = [&](const glm::vec3& offset, const glm::vec3& dimensions, const glm::vec3& color) {
        renderer_.drawBox(pose.center + offset * size, dimensions * size, color * tint);
    };
    if (index == steelPlateIndex) {
        part({}, {1.0f, 1.0f, 1.0f}, {0.35f, 0.40f, 0.41f});
        for (const float x : {-0.43f, 0.43f}) {
            part({x, 0.51f, 0.0f}, {0.09f, 0.02f, 0.93f}, {0.73f, 0.49f, 0.12f});
        }
        return;
    }
    part({}, {1.0f, 1.0f, 1.0f}, {0.27f, 0.27f, 0.25f});
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

void MilitaryRoom::drawEquipmentPuzzle() const {
    constexpr glm::vec3 cabinetColor{0.30f, 0.35f, 0.28f};
    constexpr glm::vec3 frameColor{0.35f, 0.40f, 0.33f};
    const auto drawSolid = [&](const Solid& solid, const glm::vec3& color) {
        const auto center = (solid.bounds.min + solid.bounds.max) * 0.5f;
        const auto size = solid.bounds.max - solid.bounds.min;
        renderer_.drawBox({center.x, (solid.bottom + solid.top) * 0.5f, center.y},
                          {size.x, solid.top - solid.bottom, size.y}, color);
    };
    drawSolid(equipmentBase, cabinetColor);
    drawSolid(equipmentTop, frameColor);
    for (const auto& side : equipmentSides) {
        drawSolid(side, cabinetColor);
    }

    constexpr float cabinetZ = equipmentDoor.bounds.max.y;
    if (!equipmentCabinetOpen_) {
        constexpr float doorZ = (equipmentDoor.bounds.min.y + cabinetZ) * 0.5f;
        for (int column = 0; column < 10; ++column) {
            renderer_.drawBox({15.02f + 0.118f * column, 0.82f, doorZ},
                              {0.026f, 1.08f, 0.042f}, frameColor);
        }
        for (const float y : {0.31f, 0.81f, 1.33f}) {
            renderer_.drawBox({15.55f, y, cabinetZ - 0.015f}, {1.13f, 0.044f, 0.028f}, frameColor);
        }
    }

    constexpr glm::vec3 cableColor{0.27f, 0.16f, 0.075f};
    const auto drawCable = [&](const glm::vec2& from, const glm::vec2& to) {
        constexpr float width = 0.036f;
        const auto center = (from + to) * 0.5f;
        const auto length = glm::abs(to - from);
        renderer_.drawBox({center.x, 0.014f, center.y},
                          {length.x + width, 0.028f, length.y + width}, cableColor);
    };
    for (std::size_t index = 0; index < pressurePads.size(); ++index) {
        const auto& pad = pressurePads[index];
        const float laneZ = index == 0 ? -7.22f : -7.03f;
        const float terminalX = index == 0 ? 15.32f : 15.78f;
        drawCable({pad.x, pad.z}, {pad.x, laneZ});
        drawCable({pad.x, laneZ}, {terminalX, laneZ});
        drawCable({terminalX, laneZ}, {terminalX, cabinetZ});
        renderer_.drawBox({terminalX, 0.15f, cabinetZ + 0.013f},
                          {0.036f, 0.30f, 0.028f}, cableColor);
        const bool pressed = pressesPad(pad);
        renderer_.drawBox({pad.x, 0.5f * padHeight, pad.z},
                          {2.0f * padHalfSize.x, padHeight, 2.0f * padHalfSize.y}, {0.32f, 0.25f, 0.13f});
        renderer_.drawBox({pad.x, padHeight - 0.006f, pad.z}, {0.46f, 0.012f, 0.46f},
                          pressed ? glm::vec3{0.44f, 0.62f, 0.24f} : glm::vec3{0.70f, 0.52f, 0.19f});
        renderer_.drawBox({terminalX, 0.20f, cabinetZ + 0.025f}, {0.084f, 0.076f, 0.039f},
                          pressed || equipmentCabinetOpen_ ? glm::vec3{0.35f, 0.72f, 0.29f}
                                                         : glm::vec3{0.58f, 0.30f, 0.12f});
    }
}

void MilitaryRoom::drawBarracks(const glm::vec3& eye, const glm::vec3& direction) const {
    drawEquipmentPuzzle();
    for (std::size_t index = 0; index < objects_.size(); ++index) {
        perspective::Pose displayed = objects_[index];
        glm::vec3 tint{1.0f};
        if (heldObject_ == index) {
            constexpr glm::vec3 blockedTint{1.35f, 0.36f, 0.36f};
            if (const auto target = placement(eye, direction)) {
                displayed = target->pose;
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
    if (inBarracks()) {
        renderer_.drawStaticMesh();
        drawBarracks(eye, direction);
        return;
    }

    const bool distantMoon = !moonHeld_ && !placedMoon_;
    const perspective::Pose moon = moonHeld_ ? carriedMoonPose(eye, direction)
        : perspective::Pose{placedMoon_.value_or(moonWorldCenter), distantMoon ? 1.0f : moonScale_};
    const glm::vec3& moonCenter = moon.center;
    const glm::vec3 moonLight = distantMoon ? glm::normalize(moonCenter - outpostEntrance) : moonCenter;
    const float strength = distantMoon ? distantMoonStrength : std::max(0.70f, 2.7f * moon.scale / maximumMoonScale);
    renderer_.setNightLighting(eye, moonLight, strength, distantMoon);
    renderer_.drawStaticMesh();
    renderer_.clearNightLighting();
    drawCaveScene(renderer_, static_cast<GLint>(std::size(barracksGear)), eye,
                  distantMoon ? std::nullopt : std::optional{moonCenter}, strength);

    if (moonLightsPillar(moonCenter)) {
        const auto shadow = projectPillar(moonCenter, 0.0f);
        const glm::vec3 shadowColor{0.034f, 0.064f, 0.107f};
        constexpr float shadowOffset = 0.020f;
        for (const auto& floor : banks) {
            renderer_.drawFlatPolygon(clipTo(shadow, floor), shadowOffset, shadowColor);
        }
        renderer_.drawFlatPolygon(clipTo(projectPillar(moonCenter, trenchY), gap), trenchY + shadowOffset, shadowColor);
        renderer_.drawFlatPolygon(bridge_, shadowOffset, shadowColor);
    }

    const auto moonModel = glm::scale(glm::translate(glm::mat4{1.0f}, moonCenter), moonWorldSize * moon.scale);
    renderer_.drawMoonMesh(moonModel, glm::vec3{1.0f});
}
