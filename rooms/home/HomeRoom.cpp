#include "HomeRoom.h"

#include "../shared/PerspectiveCarry.h"

#include <algorithm>
#include <cmath>

namespace {
struct Box {
    glm::vec3 center;
    glm::vec3 size;
};

constexpr float playerRadius = 0.25f;
constexpr float sideLimit = 5.55f;
constexpr float frontLimit = 6.10f;
constexpr float backLimit = -8.25f;
constexpr float doorwayHalfWidth = 0.90f;
constexpr float wallZ = -6.50f;
constexpr float shelfParkedOffset = -2.50f;
constexpr float reach = 2.8f;
constexpr float fittingReach = 10.0f;
constexpr float miniatureScale = 0.16f;
constexpr float debrisDuration = 1.4f;

constexpr Box desk{{2.90f, 0.50f, 0.80f}, {3.30f, 1.00f, 1.40f}};
constexpr Box shelf{{0.0f, 1.30f, -5.95f}, {2.05f, 2.60f, 0.60f}};
constexpr Box miniShelf{{2.04f, 1.22f, 0.56f}, {0.42f, 0.44f, 0.24f}};
constexpr Box miniRpg{{3.16f, 1.21f, 0.59f}, {0.58f, 0.27f, 0.24f}};
constexpr Box miniDoor{{4.05f, 1.23f, 0.56f}, {0.31f, 0.48f, 0.18f}};
constexpr Box wallPanel{{0.0f, 1.30f, wallZ}, {1.80f, 2.60f, 0.29f}};
constexpr Box door{{0.0f, 1.30f, wallZ}, {1.76f, 2.60f, 0.105f}};
constexpr Box openDoor{{-0.85f, 1.30f, wallZ + 0.86f}, {0.105f, 2.60f, 1.76f}};
constexpr glm::vec3 wood{0.53f, 0.32f, 0.20f};
constexpr glm::vec3 accent{0.78f, 0.60f, 0.30f};

Box shelfAt(float offset) {
    return {shelf.center + glm::vec3{offset, 0.0f, 0.0f}, shelf.size};
}

bool overlapsPlayer(const glm::vec3& position, const Box& box) {
    return std::abs(position.x - box.center.x) < box.size.x * 0.5f + playerRadius &&
           std::abs(position.z - box.center.z) < box.size.z * 0.5f + playerRadius;
}

bool aimsAt(const glm::vec3& eye, const glm::vec3& direction, const Box& box, float maxDistance) {
    const auto distance = perspective::rayBoxDistance(eye, direction, box.center, box.size * 0.5f);
    return distance && *distance <= maxDistance;
}

void drawShelf(const BoxRenderer& renderer, const glm::vec3& center, float scale) {
    renderer.drawBox(center - glm::vec3{0.0f, 0.0f, 0.25f} * scale,
                     glm::vec3{2.05f, 2.60f, 0.10f} * scale, wood * 0.67f);
    for (const float side : {-1.0f, 1.0f}) {
        renderer.drawBox(center + glm::vec3{side * 0.96f, 0.0f, 0.0f} * scale,
                         glm::vec3{0.13f, 2.60f, 0.60f} * scale, wood);
    }
    for (const float y : {-1.25f, -0.63f, 0.0f, 0.63f, 1.25f}) {
        renderer.drawBox(center + glm::vec3{0.0f, y, 0.0f} * scale,
                         glm::vec3{1.83f, 0.10f, 0.60f} * scale, wood);
    }
    renderer.drawBox(center + glm::vec3{-0.37f, 0.86f, 0.08f} * scale,
                     glm::vec3{0.35f, 0.33f, 0.30f} * scale, {0.34f, 0.54f, 0.60f});
    renderer.drawBox(center + glm::vec3{0.38f, 0.19f, 0.02f} * scale,
                     glm::vec3{0.46f, 0.31f, 0.38f} * scale, accent);
}

void drawRpg(const BoxRenderer& renderer, const glm::vec3& center, float scale) {
    renderer.drawBox(center, glm::vec3{0.47f, 0.085f, 0.11f} * scale, {0.29f, 0.39f, 0.23f});
    renderer.drawBox(center + glm::vec3{-0.21f, 0.0f, 0.0f} * scale,
                     glm::vec3{0.15f, 0.13f, 0.15f} * scale, {0.19f, 0.26f, 0.14f});
    renderer.drawBox(center + glm::vec3{0.08f, -0.064f, 0.0f} * scale,
                     glm::vec3{0.075f, 0.10f, 0.08f} * scale, wood);
}

void drawDoor(const BoxRenderer& renderer, const Box& box, const glm::vec3& tint, bool opened = false) {
    renderer.drawBox(box.center, box.size, tint);
    const glm::vec3 knob = opened ? glm::vec3{0.08f, -0.12f, 0.6f}
                                 : glm::vec3{0.61f, -0.12f, 0.075f};
    const float scale = box.size.y / door.size.y;
    renderer.drawBox(box.center + knob * scale, glm::vec3{0.11f} * scale, accent);
}

glm::vec3 carrySide(const glm::vec3& direction) {
    const glm::vec3 side = glm::cross(direction, glm::vec3{0.0f, 1.0f, 0.0f});
    return glm::length(side) > 0.001f ? glm::normalize(side) : glm::vec3{1.0f, 0.0f, 0.0f};
}
}  // namespace

bool HomeRoom::initialize() {
    return renderer_.initialize();
}

float HomeRoom::visibleRange(const glm::vec3& eye, const glm::vec3& direction, float maxDistance) const {
    for (const Box& obstacle : {desk, shelfAt(shelfOffset_)}) {
        const auto distance = perspective::rayBoxDistance(eye, direction, obstacle.center, obstacle.size * 0.5f);
        if (distance) {
            maxDistance = std::min(maxDistance, *distance);
        }
    }
    return maxDistance;
}

bool HomeRoom::canFitDoor(const glm::vec3& eye, const glm::vec3& direction) const {
    if (!aimsAt(eye, direction, door, visibleRange(eye, direction, fittingReach))) {
        return false;
    }
    const float depth = glm::dot(door.center - eye, direction);
    const float scale = doorRatio_ * doorWheelScale_ * depth;
    return scale >= 0.72f && scale <= 1.35f;
}

void HomeRoom::interact(const glm::vec3& eye, const glm::vec3& direction) {
    const float nearby = visibleRange(eye, direction, reach);
    switch (exitState_) {
        case ExitState::Explore:
            if (aimsAt(eye, direction, miniShelf, nearby)) {
                shelfMoving_ = true;
            }
            break;
        case ExitState::WallRevealed:
            if (aimsAt(eye, direction, miniRpg, nearby)) {
                held_ = held_ == HeldItem::Rpg ? HeldItem::None : HeldItem::Rpg;
            }
            break;
        case ExitState::WallBroken:
            if (held_ == HeldItem::Door) {
                if (canFitDoor(eye, direction)) {
                    held_ = HeldItem::None;
                    exitState_ = ExitState::DoorPlaced;
                }
            } else if (aimsAt(eye, direction, miniDoor, nearby)) {
                held_ = HeldItem::Door;
                doorRatio_ = perspective::grabRatio(eye, direction, miniDoor.center, miniatureScale, door.size.y);
                doorWheelScale_ = 1.0f;
            }
            break;
        case ExitState::DoorPlaced:
            if (aimsAt(eye, direction, door, nearby)) {
                exitState_ = ExitState::DoorOpen;
            }
            break;
        case ExitState::DoorOpen:
        case ExitState::Escaped:
            break;
    }
}

void HomeRoom::shoot(const glm::vec3& eye, const glm::vec3& direction) {
    if (held_ != HeldItem::Rpg || exitState_ != ExitState::WallRevealed) {
        return;
    }
    shotFlashTime_ = 0.14f;
    if (!aimsAt(eye, direction, wallPanel, visibleRange(eye, direction, 20.0f))) {
        return;
    }

    held_ = HeldItem::None;
    exitState_ = ExitState::WallBroken;
    debrisTime_ = debrisDuration;
    for (unsigned int index = 0; index < fragments_.size(); ++index) {
        const float column = static_cast<float>(index % 3) - 1.0f;
        const float row = static_cast<float>(index / 3) - 1.5f;
        fragments_[index] = {
            wallPanel.center + glm::vec3{0.55f * column, 0.59f * row, 0.10f},
            {column * 1.25f, 2.6f + row * 0.35f, 2.1f + static_cast<float>(index % 4) * 0.42f},
        };
    }
}

void HomeRoom::scroll(double yOffset) {
    if (held_ == HeldItem::Door) {
        const float steps = static_cast<float>(std::clamp(yOffset, -10.0, 10.0));
        doorWheelScale_ = std::clamp(doorWheelScale_ * std::exp(-steps * 0.18f), 0.25f, 6.0f);
    }
}

void HomeRoom::update(float elapsed, const glm::vec3& eye) {
    if (shelfMoving_) {
        const float nextOffset = std::max(shelfParkedOffset, shelfOffset_ - elapsed * 2.1f);
        if (!overlapsPlayer(eye, shelfAt(nextOffset))) {
            shelfOffset_ = nextOffset;
            if (shelfOffset_ == shelfParkedOffset) {
                shelfMoving_ = false;
                exitState_ = ExitState::WallRevealed;
            }
        }
    }
    shotFlashTime_ = std::max(0.0f, shotFlashTime_ - elapsed);
    if (debrisTime_ > 0.0f) {
        debrisTime_ = std::max(0.0f, debrisTime_ - elapsed);
        for (Fragment& piece : fragments_) {
            piece.velocity.y -= 7.5f * elapsed;
            piece.center += piece.velocity * elapsed;
        }
    }
}

glm::vec3 HomeRoom::resolveMovement(const glm::vec3& previous, const glm::vec3& desired) {
    const bool open = exitState_ == ExitState::DoorOpen || exitState_ == ExitState::Escaped;
    const auto blocked = [&](const glm::vec3& position) {
        if (overlapsPlayer(position, desk) || overlapsPlayer(position, shelfAt(shelfOffset_))) {
            return true;
        }
        if (position.z <= wallZ + wallPanel.size.z * 0.5f + playerRadius) {
            return !open || std::abs(position.x) + playerRadius >= doorwayHalfWidth ||
                   overlapsPlayer(position, openDoor);
        }
        return open && overlapsPlayer(position, openDoor);
    };

    glm::vec3 position{previous.x, desired.y, previous.z};
    const glm::vec3 moveX{std::clamp(desired.x, -sideLimit, sideLimit), desired.y, position.z};
    if (!blocked(moveX)) {
        position.x = moveX.x;
    }
    const glm::vec3 moveZ{position.x, desired.y, std::clamp(desired.z, backLimit, frontLimit)};
    if (!blocked(moveZ)) {
        position.z = moveZ.z;
    }
    if (exitState_ == ExitState::DoorOpen && position.z < wallZ - 0.55f) {
        exitState_ = ExitState::Escaped;
    }
    return position;
}

HomeRoom::ExitState HomeRoom::exitState() const {
    return exitState_;
}

void HomeRoom::drawDesk() const {
    const glm::vec3 deskTop = desk.center + glm::vec3{0.0f, 0.45f, 0.0f};
    renderer_.drawBox(deskTop, {desk.size.x, 0.10f, desk.size.z}, wood);
    for (const float x : {-1.48f, 1.48f}) {
        renderer_.drawBox(desk.center + glm::vec3{x, -0.05f, 0.0f}, {0.16f, 0.90f, 1.15f}, wood * 0.75f);
    }

    renderer_.drawBox({1.99f, 1.035f, 0.68f}, {0.65f, 0.07f, 0.59f}, {0.33f, 0.48f, 0.57f});
    renderer_.drawBox({1.99f, 1.25f, 0.39f}, {0.65f, 0.43f, 0.05f}, {0.33f, 0.48f, 0.57f});
    const float shelfMiniOffset = shelfOffset_ * 0.068f;
    drawShelf(renderer_, miniShelf.center + glm::vec3{shelfMiniOffset, 0.0f, 0.0f}, 0.15f);

    renderer_.drawBox({3.15f, 1.035f, 0.67f}, {0.81f, 0.07f, 0.63f}, {0.38f, 0.46f, 0.29f});
    renderer_.drawBox({3.15f, 1.27f, 0.39f}, {0.81f, 0.40f, 0.055f}, {0.34f, 0.39f, 0.26f});
    renderer_.drawBox({3.16f, 1.10f, 0.57f}, {0.65f, 0.14f, 0.23f}, {0.28f, 0.33f, 0.26f});
    if ((exitState_ == ExitState::Explore || exitState_ == ExitState::WallRevealed) && held_ != HeldItem::Rpg) {
        drawRpg(renderer_, miniRpg.center, 1.0f);
    }

    renderer_.drawBox({4.05f, 1.025f, 0.57f}, {0.52f, 0.05f, 0.45f}, {0.56f, 0.48f, 0.35f});
    if (exitState_ <= ExitState::WallBroken && held_ != HeldItem::Door) {
        drawDoor(renderer_, {miniDoor.center, door.size * miniatureScale}, wood);
    }
}

void HomeRoom::draw(const glm::mat4& viewProjection, const glm::vec3& eye, const glm::vec3& direction) const {
    renderer_.begin(viewProjection);
    const glm::vec3 plaster{0.52f, 0.59f, 0.62f};
    const glm::vec3 floor{0.41f, 0.37f, 0.35f};
    renderer_.drawBox({0.0f, -0.09f, 0.0f}, {11.7f, 0.18f, 13.1f}, floor);
    renderer_.drawBox({0.0f, 3.32f, 0.0f}, {11.7f, 0.20f, 13.1f}, {0.70f, 0.71f, 0.68f});
    renderer_.drawBox({0.0f, 1.60f, 6.51f}, {11.7f, 3.20f, 0.34f}, plaster);
    for (const float side : {-1.0f, 1.0f}) {
        renderer_.drawBox({side * 5.87f, 1.60f, 0.0f}, {0.34f, 3.20f, 13.1f}, plaster);
        renderer_.drawBox({side * 3.37f, 1.60f, wallZ}, {4.94f, 3.20f, 0.31f}, plaster);
        renderer_.drawBox({side * 1.02f, 1.33f, -7.55f}, {0.23f, 2.66f, 2.06f}, {0.48f, 0.43f, 0.35f});
        renderer_.drawBox({side * 0.88f, 1.30f, wallZ + 0.17f}, {0.08f, 2.60f, 0.13f}, wood);
        if (exitState_ == ExitState::WallBroken) {
            renderer_.drawBox({side * 0.86f, 1.30f, wallZ + 0.245f}, {0.032f, 2.47f, 0.025f}, {0.41f, 0.64f, 0.56f});
        }
    }
    renderer_.drawBox({0.0f, 2.90f, wallZ}, {1.80f, 0.60f, 0.31f}, plaster);
    renderer_.drawBox({0.0f, 2.61f, wallZ + 0.17f}, {1.83f, 0.10f, 0.13f}, wood);
    const bool exitOpen = exitState_ == ExitState::DoorOpen || exitState_ == ExitState::Escaped;
    const glm::vec3 corridorFloor = exitOpen ? accent * 0.75f : floor * 0.62f;
    const glm::vec3 corridorEnd = exitOpen ? glm::vec3{0.95f, 0.80f, 0.48f} : glm::vec3{0.16f, 0.20f, 0.20f};
    renderer_.drawBox({0.0f, -0.09f, -7.52f}, {1.90f, 0.18f, 2.1f}, corridorFloor);
    renderer_.drawBox({0.0f, 2.70f, -7.52f}, {1.90f, 0.18f, 2.1f}, corridorFloor);
    renderer_.drawBox({0.0f, 1.35f, -8.56f}, {1.88f, 2.70f, 0.24f}, corridorEnd);

    drawDesk();
    drawShelf(renderer_, shelfAt(shelfOffset_).center, 1.0f);
    if (exitState_ <= ExitState::WallRevealed) {
        renderer_.drawBox(wallPanel.center, wallPanel.size, {0.68f, 0.57f, 0.43f});
        for (int row = 0; row < 5; ++row) {
            const float x = row % 2 == 0 ? -0.035f : 0.035f;
            const float y = 0.54f + static_cast<float>(row) * 0.32f;
            renderer_.drawBox({x, y, wallZ + 0.15f}, {0.10f, 0.34f, 0.021f}, {0.24f, 0.18f, 0.14f});
        }
    } else if (exitState_ == ExitState::DoorPlaced) {
        drawDoor(renderer_, door, wood);
    } else if (exitOpen) {
        drawDoor(renderer_, openDoor, wood, true);
    }

    if (debrisTime_ > 0.0f) {
        for (const Fragment& piece : fragments_) {
            renderer_.drawBox(piece.center, {0.47f, 0.49f, 0.21f}, {0.68f, 0.57f, 0.43f});
        }
    }
    if (held_ == HeldItem::Rpg) {
        const glm::vec3 center = eye + direction * 0.85f + carrySide(direction) * 0.34f - glm::vec3{0.0f, 0.31f, 0.0f};
        drawRpg(renderer_, center, 1.1f);
        if (shotFlashTime_ > 0.0f) {
            renderer_.drawBox(center + direction * 0.30f, {0.13f, 0.13f, 0.13f}, {1.0f, 0.79f, 0.30f});
        }
    } else if (held_ == HeldItem::Door) {
        if (canFitDoor(eye, direction)) {
            drawDoor(renderer_, {door.center - direction * 0.04f, door.size}, {0.42f, 0.74f, 0.51f});
        } else {
            const glm::vec3 center = eye + direction * 1.15f + carrySide(direction) * 0.32f - glm::vec3{0.0f, 0.23f, 0.0f};
            const float scale = std::clamp(doorRatio_ * doorWheelScale_ * 1.15f, 0.08f, 0.32f);
            drawDoor(renderer_, {center, door.size * scale}, wood);
        }
    }
}
