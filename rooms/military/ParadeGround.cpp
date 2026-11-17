#include "ParadeGround.h"

#include <GL/glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <span>

namespace {
#include "ParadeGeometry.inc"

constexpr std::array sizes{
    glm::vec3{2.20f, 2.00f, 4.90f}, glm::vec3{3.60f, 2.40f, 10.80f}, glm::vec3{0.90f, 0.75f, 0.92f}
};
constexpr glm::vec3 yardMin{-27.0f, 0.0f, -24.0f};
constexpr glm::vec3 yardMax{27.0f, 16.0f, 34.0f};
constexpr float doorHalfWidth = 0.90f;
constexpr float facadeZ = 20.0f;
constexpr float alcoveBackZ = 20.90f;
constexpr float alcoveTop = 5.40f;
constexpr float exitZ = 20.15f;
constexpr float eyeHeight = 1.65f;
constexpr float playerRadius = 0.25f;
constexpr float clearance = 0.025f;
constexpr float contact = 0.002f;
constexpr float gravity = 9.8f;

struct Box {
    glm::vec3 min;
    glm::vec3 max;

    bool overlapsXZ(const Box& other) const {
        return min.x < other.max.x && max.x > other.min.x &&
               min.z < other.max.z && max.z > other.min.z;
    }

    bool overlaps(const Box& other) const {
        return overlapsXZ(other) && min.y < other.max.y - contact && max.y > other.min.y + contact;
    }
};

constexpr Box deck{{-2.3f, 0.0f, 16.75f}, {2.3f, 3.20f, alcoveBackZ}};
// The courtyard-facing wall leaves a recessed opening above the landing.
constexpr std::array structures{
    deck,
    Box{{-10.0f, 0.0f, facadeZ}, {-doorHalfWidth, 13.20f, 32.0f}},
    Box{{doorHalfWidth, 0.0f, facadeZ}, {10.0f, 13.20f, 32.0f}},
    Box{{-doorHalfWidth, alcoveTop, facadeZ}, {doorHalfWidth, 13.20f, 32.0f}},
    Box{{-doorHalfWidth, 0.0f, alcoveBackZ}, {doorHalfWidth, alcoveTop, 32.0f}},
};

Box propBox(std::size_t index, const perspective::Pose& pose) {
    const auto half = sizes[index] * (pose.scale * 0.5f);
    return {pose.center - half, pose.center + half};
}

Box playerBox(const glm::vec3& eye) {
    return {eye - glm::vec3{playerRadius, eyeHeight, playerRadius},
            eye + glm::vec3{playerRadius, 0.12f, playerRadius}};
}

bool withinGround(const Box& box) {
    return box.min.x >= yardMin.x && box.max.x <= yardMax.x &&
           box.min.z >= yardMin.z && box.max.z <= yardMax.z;
}

float landingOn(const Box& body, const Box& support, float height) {
    return support.max.y <= body.min.y + contact && body.overlapsXZ(support)
        ? std::max(height, support.max.y) : height;
}

struct DepthRange {
    float near = 0.0f;
    float far;

    bool bound(float origin, float slope, float limit) {
        if (slope > 0.0f) {
            far = std::min(far, (limit - origin) / slope);
        } else if (slope < 0.0f) {
            near = std::max(near, (limit - origin) / slope);
        } else if (origin > limit) {
            return false;
        }
        return near <= far;
    }
};
}

bool ParadeGround::initialize() {
    return renderer_.initialize(paradeStaticGeometry, paradeMovableGeometry);
}

std::optional<perspective::Pose> ParadeGround::placement(const glm::vec3& eye, const glm::vec3& direction) const {
    const std::size_t index = *held_;
    const auto growth = sizes[index] * (0.5f * heldRatio_);
    const auto minimumSlope = direction - growth;
    const auto maximumSlope = direction + growth;
    DepthRange visible{0.0f, reach_};
    // Clip all edges of the growing prop, including height, along the view ray.
    for (int axis = 0; axis < 3; ++axis) {
        if (!visible.bound(-eye[axis], -minimumSlope[axis], -yardMin[axis] - clearance) ||
            !visible.bound(eye[axis], maximumSlope[axis], yardMax[axis] - clearance)) {
            return std::nullopt;
        }
    }
    const auto clipObstacle = [&](const Box& box) {
        DepthRange hit{0.0f, visible.far};
        for (int axis = 0; axis < 3; ++axis) {
            if (!hit.bound(eye[axis], minimumSlope[axis], box.max[axis] + clearance) ||
                !hit.bound(-eye[axis], -maximumSlope[axis], -box.min[axis] + clearance)) {
                return;
            }
        }
        if (hit.far >= visible.near) {
            visible.far = hit.near;
        }
    };
    for (const auto& structure : structures) {
        clipObstacle(structure);
    }
    for (std::size_t other = 0; other < props_.size(); ++other) {
        if (other != index) {
            clipObstacle(propBox(other, props_[other].pose));
        }
    }
    const perspective::Pose pose{eye + direction * visible.far, heldRatio_ * visible.far};
    if (visible.far < visible.near || pose.scale < 0.08f || propBox(index, pose).overlaps(playerBox(eye))) {
        return std::nullopt;
    }
    return pose;
}

void ParadeGround::interact(const glm::vec3& eye, const glm::vec3& direction) {
    if (held_) {
        if (const auto target = placement(eye, direction)) {
            props_[*held_] = {*target};
            held_.reset();
        }
        return;
    }

    float closest = 10.0f;
    for (const auto& structure : structures) {
        const auto hit = perspective::rayBoxDistance(eye, direction,
                                                     (structure.min + structure.max) * 0.5f,
                                                     (structure.max - structure.min) * 0.5f);
        if (hit && *hit > contact) {
            closest = std::min(closest, *hit);
        }
    }
    for (std::size_t index = 0; index < props_.size(); ++index) {
        const auto& pose = props_[index].pose;
        const auto hit = perspective::rayBoxDistance(eye, direction, pose.center, sizes[index] * (0.5f * pose.scale));
        if (hit && *hit > contact && *hit < closest) {
            closest = *hit;
            held_ = index;
        }
    }
    if (held_) {
        const auto& pose = props_[*held_].pose;
        const auto& size = sizes[*held_];
        heldRatio_ = perspective::grabRatio(eye, direction, pose.center, pose.scale,
                                           std::max({size.x, size.y, size.z}));
        reach_ = maximumReach;
    }
}

void ParadeGround::scroll(double yOffset) {
    if (held_) {
        const float steps = static_cast<float>(std::clamp(yOffset, -8.0, 8.0));
        reach_ = std::clamp(reach_ * std::pow(1.18f, -steps), 0.8f, maximumReach);
    }
}

void ParadeGround::cancelCarry() {
    if (!held_) {
        return;
    }
    const std::size_t index = *held_;
    const auto stored = propBox(index, props_[index].pose);
    float top = stored.min.y;
    bool blocked = false;
    const auto check = [&](const Box& box) {
        if (stored.overlapsXZ(box)) {
            top = std::max(top, box.max.y);
            blocked = blocked || stored.overlaps(box);
        }
    };
    for (const auto& structure : structures) {
        check(structure);
    }
    check(playerBox(lastPlayerEye_));
    for (std::size_t other = 0; other < props_.size(); ++other) {
        if (other != index) {
            check(propBox(other, props_[other].pose));
        }
    }
    if (blocked) {
        props_[index].pose.center.y += top - stored.min.y + clearance;
    }
    props_[index].verticalSpeed = 0.0f;
    held_.reset();
}

void ParadeGround::update(float elapsed, const glm::vec3& playerEye) {
    lastPlayerEye_ = playerEye;
    if (elapsed <= 0.0f) {
        return;
    }
    std::array<Box, 3> previous;
    for (std::size_t index = 0; index < props_.size(); ++index) {
        previous[index] = propBox(index, props_[index].pose);
    }
    std::array<std::size_t, 3> order{0, 1, 2};
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return previous[a].min.y < previous[b].min.y;
    });
    for (std::size_t index : order) {
        if (held_ == index) {
            continue;
        }
        auto& prop = props_[index];
        const auto& body = previous[index];
        float support = 0.0f;
        for (const auto& structure : structures) {
            support = landingOn(body, structure, support);
        }
        for (std::size_t other = 0; other < props_.size(); ++other) {
            if (index != other && held_ != other && previous[other].max.y <= body.min.y + contact) {
                support = std::max(support, landingOn(body, propBox(other, props_[other].pose), 0.0f));
            }
        }
        support = landingOn(body, playerBox(playerEye), support);
        const float fallingBottom = body.min.y + (prop.verticalSpeed - 0.5f * gravity * elapsed) * elapsed;
        const bool landed = fallingBottom <= support + contact;
        prop.pose.center.y += (landed ? support : fallingBottom) - body.min.y;
        prop.verticalSpeed = landed ? 0.0f : prop.verticalSpeed - gravity * elapsed;
    }
}

glm::vec3 ParadeGround::resolveMovement(const glm::vec3& previous, const glm::vec3& desired, bool* grounded) {
    std::array<Box, structures.size() + sizes.size()> solids;
    std::copy(structures.begin(), structures.end(), solids.begin());
    std::size_t count = structures.size();
    for (std::size_t index = 0; index < props_.size(); ++index) {
        if (held_ != index) {
            solids[count++] = propBox(index, props_[index].pose);
        }
    }
    const std::span active{solids.data(), count};
    const auto canMove = [&](const glm::vec3& eye) {
        const auto body = playerBox(eye);
        return withinGround(body) &&
               std::none_of(active.begin(), active.end(), [&](const Box& box) { return body.overlaps(box); });
    };

    glm::vec3 result{desired.x, std::max(previous.y, desired.y), previous.z};
    if (!canMove(result)) {
        result.x = previous.x;
    }
    result.z = desired.z;
    if (!canMove(result)) {
        result.z = previous.z;
    }

    const auto body = playerBox({result.x, previous.y, result.z});
    float support = 0.0f;
    result.y = desired.y;
    if (desired.y <= previous.y) {
        for (const auto& box : active) {
            support = landingOn(body, box, support);
        }
    } else {
        for (const auto& box : active) {
            if (body.overlapsXZ(box) && box.min.y >= body.max.y - contact) {
                result.y = std::min(result.y, previous.y + box.min.y - body.max.y);
            }
        }
    }
    result.y = std::max(result.y, support + eyeHeight);
    const bool landed = result.y <= support + eyeHeight + contact && desired.y <= previous.y;
    if (grounded) {
        *grounded = landed;
    }
    if (landed && body.min.x >= -doorHalfWidth && body.max.x <= doorHalfWidth &&
        result.z >= exitZ && result.z < alcoveBackZ && std::abs(support - deck.max.y) <= contact) {
        reachedExit_ = true;
    }
    return result;
}

bool ParadeGround::reachedExit() const {
    return reachedExit_;
}

void ParadeGround::draw(const glm::mat4& viewProjection, const glm::vec3& eye, const glm::vec3& direction) const {
    renderer_.begin(viewProjection);
    renderer_.drawStaticMesh();
    for (std::size_t index = 0; index < props_.size(); ++index) {
        auto pose = props_[index].pose;
        glm::vec3 color{1.0f};
        if (index == held_) {
            if (const auto target = placement(eye, direction)) {
                pose = *target;
                color = {0.90f, 1.17f, 1.05f};
            } else {
                pose = {eye + direction * 1.5f, heldRatio_ * 1.5f};
                color = {1.25f, 0.56f, 0.52f};
            }
        }
        const auto model = glm::scale(glm::translate(glm::mat4{1.0f}, pose.center), sizes[index] * pose.scale);
        renderer_.drawMovableMesh(paradePropOffsets[index], paradePropOffsets[index + 1] - paradePropOffsets[index],
                                  model, color);
    }
}
