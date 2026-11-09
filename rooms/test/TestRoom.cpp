#include "TestRoom.h"

#include "../shared/PerspectiveCarry.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float playerRadius = 0.25f;
constexpr float minimumCubeSize = 0.15f;
constexpr glm::vec3 returnBoxCenter{1.45f, 0.0f, -2.65f};
constexpr float returnBoxWidth = 0.84f;
constexpr float returnBoxWall = 0.08f;
constexpr float returnBoxOpening = returnBoxWidth - 2.0f * returnBoxWall;
constexpr float returnBoxFloor = 0.10f;
constexpr float returnBoxRim = 0.65f;
constexpr float maxReturnCubeSize = returnBoxOpening - 0.04f;
constexpr float doorZ = -7.0f;
constexpr float doorHalfWidth = 1.0f;
constexpr float doorHeight = 3.1f;
constexpr float wallHeight = 4.0f;
constexpr float wallDepth = 0.30f;
constexpr float doorDepth = 0.16f;
constexpr glm::vec3 openDoorCenter{-doorHalfWidth - doorDepth * 0.5f, doorHeight * 0.5f, doorZ - doorHalfWidth};

bool insideSquare(const glm::vec3& position, const glm::vec3& center, float halfWidth) {
    return std::abs(position.x - center.x) <= halfWidth &&
           std::abs(position.z - center.z) <= halfWidth;
}
}

bool TestRoom::initialize() {
    return renderer_.initialize();
}

std::optional<TestRoom::Placement> TestRoom::placement(const glm::vec3& eye, const glm::vec3& direction) const {
    if (direction.y >= -0.001f) {
        return std::nullopt;
    }

    const float rimDepth = (returnBoxRim - eye.y) / direction.y;
    const glm::vec3 rimPoint = eye + direction * rimDepth;
    if (rimDepth > 0.0f && insideSquare(rimPoint, returnBoxCenter, returnBoxWidth * 0.5f)) {
        const glm::vec3 binFloor = returnBoxCenter + glm::vec3{0.0f, returnBoxFloor, 0.0f};
        const float size = perspective::scaleAtSupport(eye, direction, binFloor, heldSizePerDepth_, 0.5f);
        if (!insideSquare(rimPoint, returnBoxCenter, returnBoxOpening * 0.5f) ||
            size < minimumCubeSize || size > maxReturnCubeSize) {
            return std::nullopt;
        }
        return Placement{{binFloor + glm::vec3{0.0f, size * 0.5f, 0.0f}, size}, true};
    }

    const auto pose = perspective::onFloor(eye, direction, heldSizePerDepth_, 0.5f);
    if (!pose) {
        return std::nullopt;
    }
    const float size = pose->scale;
    const float half = size * 0.5f;
    const auto& center = pose->center;
    if (size < minimumCubeSize || insideSquare(center, eye, half + playerRadius) ||
        insideSquare(center, returnBoxCenter, half + returnBoxWidth * 0.5f) ||
        std::abs(center.x) + half > halfSize || center.z + half > halfSize ||
        center.z - half <= doorZ + wallDepth * 0.5f) {
        return std::nullopt;
    }
    return Placement{{center, size}, false};
}

void TestRoom::interact(const glm::vec3& eye, const glm::vec3& direction) {
    if (exitState_ != ExitState::Locked) {
        return;
    }
    if (heldSizePerDepth_ > 0.0f) {
        if (const auto target = placement(eye, direction)) {
            cube_ = target->cube;
            heldSizePerDepth_ = 0.0f;
            if (target->inReturnBox) {
                exitState_ = ExitState::Open;
            }
        }
        return;
    }

    const auto hit = perspective::rayBoxDistance(eye, direction, cube_.center, glm::vec3(cube_.size * 0.5f));
    const auto binHit = perspective::rayBoxDistance(
        eye, direction, returnBoxCenter + glm::vec3{0.0f, returnBoxRim * 0.5f, 0.0f},
        {returnBoxWidth * 0.5f, returnBoxRim * 0.5f, returnBoxWidth * 0.5f});
    if (hit && *hit > 0.05f && (!binHit || *hit < *binHit)) {
        heldSizePerDepth_ = perspective::grabRatio(eye, direction, cube_.center, cube_.size, 1.0f);
    }
}

glm::vec3 TestRoom::resolveMovement(const glm::vec3& previous, const glm::vec3& desired) {
    const auto blocked = [this](const glm::vec3& position) {
        const bool atWall = std::abs(position.z - doorZ) <= wallDepth * 0.5f + playerRadius;
        const bool locked = exitState_ == ExitState::Locked;
        if (atWall && (locked || std::abs(position.x) > doorHalfWidth - playerRadius)) {
            return true;
        }
        const bool atOpenDoor = std::abs(position.x - openDoorCenter.x) < doorDepth * 0.5f + playerRadius &&
                                std::abs(position.z - openDoorCenter.z) < doorHalfWidth + playerRadius;
        return (!locked && atOpenDoor) ||
               insideSquare(position, returnBoxCenter, returnBoxWidth * 0.5f + playerRadius);
    };

    glm::vec3 position{desired.x, desired.y, previous.z};
    if (blocked(position)) {
        position.x = previous.x;
    }
    position.z = desired.z;
    if (blocked(position)) {
        position.z = previous.z;
    }
    if (exitState_ == ExitState::Open && position.z < doorZ - wallDepth * 0.5f - playerRadius) {
        exitState_ = ExitState::Escaped;
    }
    return position;
}

TestRoom::ExitState TestRoom::exitState() const {
    return exitState_;
}

void TestRoom::draw(const glm::mat4& viewProjection, const glm::vec3& eye, const glm::vec3& direction) const {
    renderer_.begin(viewProjection);
    renderer_.drawGrid(halfSize);

    const glm::vec3 wallTint{0.42f, 0.49f, 0.56f};
    const float sideCenter = (halfSize + doorHalfWidth) * 0.5f;
    const glm::vec3 sideSize{halfSize - doorHalfWidth, wallHeight, wallDepth};
    renderer_.drawBox({-sideCenter, wallHeight * 0.5f, doorZ}, sideSize, wallTint);
    renderer_.drawBox({sideCenter, wallHeight * 0.5f, doorZ}, sideSize, wallTint);
    renderer_.drawBox({0.0f, (wallHeight + doorHeight) * 0.5f, doorZ},
                      {doorHalfWidth * 2.0f, wallHeight - doorHeight, wallDepth}, wallTint);

    const bool locked = exitState_ == ExitState::Locked;
    const glm::vec3 doorTint{0.23f, 0.54f, 0.74f};
    if (locked) {
        renderer_.drawBox({0.0f, doorHeight * 0.5f, doorZ}, {doorHalfWidth * 2.0f, doorHeight, doorDepth}, doorTint);
    } else {
        renderer_.drawBox(openDoorCenter, {doorDepth, doorHeight, doorHalfWidth * 2.0f}, doorTint);
    }
    const glm::vec3 indicator = locked ? glm::vec3{0.95f, 0.29f, 0.24f} : glm::vec3{0.30f, 0.95f, 0.46f};
    renderer_.drawBox({0.0f, doorHeight + 0.30f, doorZ + wallDepth * 0.5f + 0.03f},
                      {0.50f, 0.15f, 0.06f}, indicator);

    const glm::vec3 binTint{0.84f, 0.77f, 0.34f};
    renderer_.drawBox(returnBoxCenter + glm::vec3{0.0f, returnBoxFloor * 0.5f, 0.0f},
                      {returnBoxWidth, returnBoxFloor, returnBoxWidth}, {0.47f, 0.43f, 0.22f});
    const float binWallOffset = (returnBoxWidth - returnBoxWall) * 0.5f;
    const float binWallY = (returnBoxRim + returnBoxFloor) * 0.5f;
    const float binWallHeight = returnBoxRim - returnBoxFloor;
    for (float side : {-binWallOffset, binWallOffset}) {
        renderer_.drawBox(returnBoxCenter + glm::vec3{side, binWallY, 0.0f},
                          {returnBoxWall, binWallHeight, returnBoxWidth}, binTint);
        renderer_.drawBox(returnBoxCenter + glm::vec3{0.0f, binWallY, side},
                          {returnBoxOpening, binWallHeight, returnBoxWall}, binTint);
    }

    Cube displayed = cube_;
    glm::vec3 tint{0.98f, 0.61f, 0.24f};
    if (heldSizePerDepth_ > 0.0f) {
        if (const auto target = placement(eye, direction)) {
            displayed = target->cube;
            tint = {0.32f, 0.88f, 0.77f};
        } else {
            constexpr float carryDepth = 2.0f;
            displayed = {eye + direction * carryDepth, std::min(heldSizePerDepth_ * carryDepth, 1.5f)};
            tint = {0.94f, 0.45f, 0.36f};
        }
    }
    renderer_.drawBox(displayed.center, glm::vec3(displayed.size), tint);
}
