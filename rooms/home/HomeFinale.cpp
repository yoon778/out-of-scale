#include "HomeFinale.h"

#include "../shared/PerspectiveCarry.h"

#include <algorithm>
#include <cmath>

namespace home_finale {
namespace {
constexpr float eyeHeight = 1.65f;
constexpr float playerRadius = .25f;
constexpr float headClearance = .14f;
constexpr float stepHeight = .26f;
constexpr float portalHeight = 2.70f;
constexpr float reach = 28.0f;
constexpr float minScale = .32f;
constexpr float maxScale = 5.7f;
constexpr float clearance = .014f;
constexpr float depthInset = .025f;
constexpr Box floor{{hallCenterX, -.15f, (frontZ + backZ) * .5f}, {right - left, .30f, backZ - frontZ}};
constexpr Box exitDoor{{hallCenterX, exitHeight + exitDoorHeight * .5f, backZ},
                      {2 * exitHalfWidth, exitDoorHeight, .16f}};
constexpr std::array levelSurfaces{rests[0], rests[1], rests[2], exitLanding, exitSill};

float top(Box box) { return box.center.y + box.size.y * .5f; }
float bottom(Box box) { return box.center.y - box.size.y * .5f; }
Box bounds(const Prop& prop) { return {prop.center, prop.size()}; }

bool overlap(Box a, Box b) {
    const auto delta = glm::abs(a.center - b.center);
    const auto extent = (a.size + b.size) * .5f - glm::vec3{.003f};
    return glm::all(glm::lessThan(delta, extent));
}

bool overFootprint(glm::vec3 point, Box box, float radius = playerRadius) {
    const auto half = box.size * .5f;
    const float dx = point.x - std::clamp(point.x, box.center.x - half.x, box.center.x + half.x);
    const float dz = point.z - std::clamp(point.z, box.center.z - half.z, box.center.z + half.z);
    return dx * dx + dz * dz < radius * radius;
}

bool stableOn(Box item, Box support) {
    return std::abs(item.center.x - support.center.x) <= support.size.x * .5f - .017f &&
           std::abs(item.center.z - support.center.z) <= support.size.z * .5f - .017f;
}

bool horizontallyOverlaps(Box a, Box b) {
    return std::abs(a.center.x - b.center.x) < (a.size.x + b.size.x) * .5f - .003f &&
           std::abs(a.center.z - b.center.z) < (a.size.z + b.size.z) * .5f - .003f;
}

Box playerBody(glm::vec3 eye) {
    const float bodyHeight = eyeHeight + headClearance;
    return {{eye.x, eye.y - eyeHeight + bodyHeight * .5f, eye.z},
            {2 * playerRadius, bodyHeight, 2 * playerRadius}};
}
} // namespace

Room::SightHit Room::sight(glm::vec3 eye, glm::vec3 direction, float limit) const {
    SightHit hit{limit, std::nullopt, std::nullopt};
    const auto check = [&](Box box, std::optional<std::size_t> index = std::nullopt) {
        const auto distance = perspective::rayBoxDistance(eye, direction, box.center, box.size * .5f);
        if (distance && *distance > .025f && *distance < hit.distance) {
            hit = {*distance, index, box};
        }
    };
    check(floor);
    for (const auto& surface : levelSurfaces) check(surface);
    if (doorProgress_ < .99f) check(exitDoor);
    for (std::size_t i = 0; i < props_.size(); ++i) {
        if (held_ != i) check(bounds(props_[i]), i);
    }

    // Solid wall and ceiling planes limit the reach of a held object.
    const auto wall = [&](int axis, float edge) {
        if (std::abs(direction[axis]) < .0001f) return;
        const float distance = (edge - eye[axis]) / direction[axis];
        if (distance > .025f && distance < hit.distance) hit = {distance, std::nullopt, std::nullopt};
    };
    wall(0, direction.x > 0.0f ? right : left);
    wall(2, direction.z > 0.0f ? backZ : frontZ);
    if (direction.y > 0.0f) wall(1, ceiling);
    return hit;
}

bool Room::placeable(const Prop& prop, glm::vec3 eye) const {
    const auto box = bounds(prop);
    const auto half = box.size * .5f;
    if (box.center.x - half.x < left + depthInset || box.center.x + half.x > right - depthInset ||
        box.center.z - half.z < frontZ + depthInset || box.center.z + half.z > backZ - depthInset ||
        bottom(box) < -.001f || top(box) >= ceiling - depthInset || overlap(box, playerBody(eye))) {
        return false;
    }
    if (std::any_of(levelSurfaces.begin(), levelSurfaces.end(), [&](Box surface) { return overlap(box, surface); })) {
        return false;
    }
    for (std::size_t i = 0; i < props_.size(); ++i) {
        if (held_ != i && overlap(box, bounds(props_[i]))) return false;
    }
    return true;
}

std::optional<Prop> Room::placement(glm::vec3 eye, glm::vec3 direction) const {
    if (!held_ || eye.z < frontZ) return std::nullopt;
    const auto hit = sight(eye, direction, reach);
    const auto point = eye + direction * hit.distance;
    Prop prop = props_[*held_];
    const auto scaleAt = [&](float distance) {
        return std::clamp(carryRatio_ * wheelScale_ * distance, minScale, maxScale);
    };
    if (hit.box && direction.y < -.005f && eye.y > top(*hit.box) + .015f &&
        std::abs(point.y - top(*hit.box)) < .035f) {
        prop.scale = scaleAt(hit.distance);
        prop.center = {point.x, top(*hit.box) + prop.size().y * .5f + clearance, point.z};
        if (stableOn(bounds(prop), *hit.box) && placeable(prop, eye)) return prop;
    }

    // Between surfaces, find the furthest clear center the player can release along the ray.
    const float far = std::min(hit.distance, reach);
    const float step = std::max(.14f, far / 48.f);
    for (float distance = far; distance >= .65f; distance -= step) {
        prop.scale = scaleAt(distance);
        prop.center = eye + direction * distance;
        prop.center.y = std::max(prop.center.y, prop.size().y * .5f + clearance);
        if (placeable(prop, eye)) return prop;
    }
    return std::nullopt;
}

bool Room::interact(glm::vec3 eye, glm::vec3 direction) {
    if (escaped_ || eye.z < frontZ) return false;
    if (held_) {
        // Re-evaluate at the exact cursor and player pose of the click.
        if (const auto pose = placement(eye, direction)) {
            props_[*held_] = *pose;
            verticalSpeeds_[*held_] = 0.0f;
            held_.reset();
            placement_.reset();
            return true;
        }
        return false;
    }

    const auto hit = sight(eye, direction, reach);
    if (!exitOpen_ && hit.box && !hit.prop && hit.box->center.z == backZ &&
        hit.box->center.y == exitDoor.center.y &&
        hit.distance < 2.65f && eye.y - eyeHeight >= exitHeight - .05f &&
        eye.z > exitLanding.center.z - exitLanding.size.z * .5f &&
        std::abs(eye.x - hallCenterX) < exitHalfWidth) {
        exitOpen_ = true;
        return true;
    }
    if (!hit.prop) return false;

    held_ = hit.prop;
    const auto& prop = props_[*held_];
    const float largest = std::max({prop.baseSize.x, prop.baseSize.y, prop.baseSize.z});
    carryRatio_ = perspective::grabRatio(eye, direction, prop.center, prop.scale, largest);
    wheelScale_ = 1.0f;
    verticalSpeeds_[*held_] = 0.0f;
    placement_.reset();
    return true;
}

void Room::scroll(double yOffset) {
    if (!held_ || !std::isfinite(yOffset)) return;
    wheelScale_ = std::clamp(wheelScale_ * std::exp(-.17f * static_cast<float>(std::clamp(yOffset, -8.0, 8.0))),
                             .055f, 18.0f);
}

void Room::rotate() {
    if (held_) props_[*held_].turns = (props_[*held_].turns + 1) % 4;
}

void Room::update(float elapsed, glm::vec3 eye, glm::vec3 direction) {
    if (!std::isfinite(elapsed) || elapsed <= 0.0f) return;
    const float dt = std::min(elapsed, .05f);
    if (exitOpen_) doorProgress_ = std::min(1.0f, doorProgress_ + dt / .85f);

    // Resolve lower props first so a newly placed stack can settle consistently.
    std::array<std::size_t, 20> order{};
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return bottom(bounds(props_[a])) < bottom(bounds(props_[b]));
    });
    for (const auto i : order) {
        if (held_ == i) continue;
        auto& prop = props_[i];
        const auto box = bounds(prop);
        const float oldBottom = bottom(box);
        float support = 0.0f;
        const auto consider = [&](Box under) {
            if (top(under) <= oldBottom + .025f && horizontallyOverlaps(box, under)) {
                support = std::max(support, top(under));
            }
        };
        for (const auto& rest : levelSurfaces) consider(rest);
        for (std::size_t j = 0; j < props_.size(); ++j) {
            if (j != i && held_ != j) consider(bounds(props_[j]));
        }
        if (oldBottom > eye.y + headClearance - .01f && overFootprint(eye, box)) {
            support = std::max(support, eye.y + headClearance + .012f);
        }
        auto& speed = verticalSpeeds_[i];
        speed = std::max(speed - 9.8f * dt, -19.0f);
        const float nextBottom = oldBottom + speed * dt;
        if (nextBottom <= support) {
            prop.center.y = support + box.size.y * .5f;
            speed = 0.0f;
        } else {
            prop.center.y += speed * dt;
        }
    }

    if (held_) {
        placement_ = placement(eye, direction);
        if (placement_) {
            props_[*held_] = *placement_;
        } else {
            auto& prop = props_[*held_];
            prop.center = eye + direction * 1.2f;
            prop.scale = std::clamp(carryRatio_ * wheelScale_ * 1.2f, .18f, 1.3f);
        }
    }
}

glm::vec3 Room::resolveMovement(glm::vec3 previous, glm::vec3 desired, bool* grounded) {
    const float previousFeet = previous.y - eyeHeight;
    const float previousHead = previous.y + headClearance;
    const bool descending = desired.y <= previous.y;
    const auto forEachSolid = [&](const auto& handle) {
        for (const auto& surface : levelSurfaces) handle(surface);
        for (std::size_t i = 0; i < props_.size(); ++i) {
            if (held_ != i) handle(bounds(props_[i]));
        }
    };
    const auto inside = [&](glm::vec3 position) {
        if (position.x < left + playerRadius || position.x > right - playerRadius ||
            position.z < entranceZ) return false;
        const float side = std::abs(position.x - hallCenterX) + playerRadius;
        if (position.z < frontZ + playerRadius &&
            (side > entranceHalfWidth || position.y + headClearance > portalHeight)) return false;
        if (position.z > backZ - playerRadius) {
            return doorProgress_ >= .99f && side <= exitHalfWidth &&
                   position.y - eyeHeight >= exitHeight - .09f &&
                   position.y + headClearance <= exitHeight + exitDoorHeight &&
                   position.z <= exitSill.center.z + exitSill.size.z * .5f - .04f;
        }
        return true;
    };
    const auto blocked = [&](glm::vec3 candidate) {
        if (!inside(candidate)) return true;
        bool collides = false;
        const float feet = candidate.y - eyeHeight;
        const float head = candidate.y + headClearance;
        forEachSolid([&](Box obstacle) {
            if (!overFootprint(candidate, obstacle) || head <= bottom(obstacle) + .002f ||
                feet >= top(obstacle) - .012f) return;
            const float heightAllowance = descending ? stepHeight : .016f;
            if (previousFeet >= top(obstacle) - heightAllowance) return;
            collides = true;
        });
        return collides;
    };

    glm::vec3 position{previous.x, desired.y, previous.z};
    if (const glm::vec3 tryX{desired.x, desired.y, position.z}; !blocked(tryX)) position.x = desired.x;
    if (const glm::vec3 tryZ{position.x, desired.y, desired.z}; !blocked(tryZ)) position.z = desired.z;

    float floorHeight = 0.0f;
    float ceilingHeight = position.z < frontZ + playerRadius ? portalHeight : ceiling;
    forEachSolid([&](Box obstacle) {
        if (!overFootprint(position, obstacle)) return;
        if (descending && previousFeet >= top(obstacle) - stepHeight) {
            floorHeight = std::max(floorHeight, top(obstacle));
        }
        if (!descending && previousHead <= bottom(obstacle) + .015f) {
            ceilingHeight = std::min(ceilingHeight, bottom(obstacle));
        }
    });
    const bool landed = descending && position.y - eyeHeight <= floorHeight + .001f;
    if (landed) position.y = floorHeight + eyeHeight;
    position.y = std::min(position.y, ceilingHeight - headClearance);
    if (grounded) *grounded = landed;
    if (doorProgress_ >= .99f && position.z >= exitZ &&
        std::abs(position.x - hallCenterX) + playerRadius <= exitHalfWidth &&
        position.y >= exitHeight + eyeHeight - .08f) {
        escaped_ = true;
    }
    return position;
}
} // namespace home_finale
