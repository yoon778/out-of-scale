#include "HomeRotatingRoom.h"

#include "../shared/PerspectiveCarry.h"

#include <GL/glm/gtc/constants.hpp>
#include <GL/glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace home_rotate {
namespace {
constexpr float playerRadius = .25f;
constexpr float eyeHeight = 1.65f;
constexpr float headClearance = .15f;
constexpr float stepHeight = .23f;
constexpr float fullTurn = glm::two_pi<float>();
constexpr float openAngle = glm::half_pi<float>();
constexpr float maxAngularSpeed = glm::radians(58.0f);
constexpr float startApronZ = 6.64f;
constexpr float endLandingZ = 16.43f;
constexpr float centerZ = (frontZ + farZ) * .5f;
constexpr float lengthHalf = (farZ - frontZ) * .5f;
constexpr float shellCenter = halfExtent + shellThickness * .5f;
constexpr std::array<Box, 4> shell{{
    {{-shellCenter, 0.0f, centerZ}, {shellThickness * .5f, halfExtent, lengthHalf}},
    {{shellCenter, 0.0f, centerZ}, {shellThickness * .5f, halfExtent, lengthHalf}},
    {{0.0f, -shellCenter, centerZ}, {halfExtent + shellThickness, shellThickness * .5f, lengthHalf}},
    {{0.0f, shellCenter, centerZ}, {halfExtent + shellThickness, shellThickness * .5f, lengthHalf}},
}};
constexpr Box boardBox{{boardLength * .5f, 0.0f, 0.0f},
                       {boardLength * .5f, boardHalfWidth, boardHalfThickness}};

glm::vec3 turned(glm::vec3 vector, float angle) {
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    return {cosine * vector.x - sine * vector.y, sine * vector.x + cosine * vector.y, vector.z};
}

struct VerticalSpan {
    float low = -std::numeric_limits<float>::infinity();
    float high = std::numeric_limits<float>::infinity();
};

std::optional<VerticalSpan> chamberSpan(float x, float angle) {
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    const float relativeX = x - center.x;
    VerticalSpan span;
    const auto addWalls = [&](float horizontal, float vertical) {
        const float extent = halfExtent - std::abs(horizontal) * playerRadius;
        const float amount = horizontal * relativeX;
        if (std::abs(vertical) < .0001f) {
            return std::abs(amount) <= extent;
        }
        const float a = (-extent - amount) / vertical + center.y;
        const float b = (extent - amount) / vertical + center.y;
        span.low = std::max(span.low, std::min(a, b));
        span.high = std::min(span.high, std::max(a, b));
        return span.high - span.low >= eyeHeight + headClearance;
    };
    if (!addWalls(cosine, sine) || !addWalls(-sine, cosine)) {
        return std::nullopt;
    }
    return span;
}

std::optional<float> boardSurface(const glm::mat4& model, float x, float z) {
    const glm::vec3 normal{model[2]};
    if (std::abs(normal.y) < .87f) return std::nullopt;
    const glm::vec3 hinge{model[3]};
    const auto top = hinge + normal * std::copysign(boardHalfThickness, normal.y);
    const float y = top.y - (normal.x * (x - top.x) + normal.z * (z - top.z)) / normal.y;
    const glm::vec3 offset = glm::vec3{x, y, z} - top;
    const float length = glm::dot(offset, glm::vec3{model[0]});
    const float width = glm::dot(offset, glm::vec3{model[1]});
    if (length < -.08f || length > boardLength + .01f ||
        std::abs(width) > boardHalfWidth - playerRadius * .7f) {
        return std::nullopt;
    }
    return y;
}

bool inDoorway(glm::vec3 eye, float angle) {
    const auto feet = eye.y - eyeHeight;
    for (const float x : {eye.x - playerRadius, eye.x + playerRadius}) {
        for (const float y : {feet + .08f, eye.y + headClearance}) {
            const auto local = turned(glm::vec3{x, y, 0.0f} - center, -angle);
            if (local.x < -halfExtent - .01f || local.x > 0.0f ||
                std::abs(local.y) > wallBox.half.y) {
                return false;
            }
        }
    }
    return true;
}

std::optional<float> raySphere(glm::vec3 eye, glm::vec3 direction, const Prop& prop) {
    const auto delta = prop.position - eye;
    const float projected = glm::dot(delta, direction);
    const float discriminant = projected * projected - glm::dot(delta, delta) + prop.radius * prop.radius;
    if (discriminant < 0.0f || projected < 0.0f) return std::nullopt;
    return std::max(0.0f, projected - std::sqrt(discriminant));
}

void contain(Prop& prop, float angle, float angularSpeed) {
    auto local = turned(prop.position - center, -angle);
    const float extent = halfExtent - prop.radius;
    const auto bounce = [&](glm::vec3 inward, float surfaceSpeed) {
        const auto normal = turned(inward, angle);
        const float relativeSpeed = glm::dot(prop.velocity, normal) - surfaceSpeed;
        if (relativeSpeed < 0.0f) {
            prop.velocity -= normal * (1.24f * relativeSpeed);
        }
    };
    for (int axis = 0; axis < 2; ++axis) {
        if (std::abs(local[axis]) <= extent) continue;
        const float side = std::copysign(1.0f, local[axis]);
        local[axis] = side * extent;
        glm::vec3 inward{};
        inward[axis] = -side;
        const glm::vec3 wallVelocity{-angularSpeed * local.y, angularSpeed * local.x, 0.0f};
        bounce(inward, glm::dot(wallVelocity, inward));
        const auto tangential = turned(glm::vec3{inward.y, -inward.x, 0.0f}, angle);
        prop.velocity -= tangential * (glm::dot(prop.velocity, tangential) * .065f);
        prop.velocity.z *= .90f;
    }
    const float minZ = frontZ + prop.radius;
    const float maxZ = farZ - prop.radius - .15f;
    if (local.z < minZ) {
        local.z = minZ;
        bounce({0.0f, 0.0f, 1.0f}, 0.0f);
    } else if (local.z > maxZ) {
        local.z = maxZ;
        bounce({0.0f, 0.0f, -1.0f}, 0.0f);
    }
    prop.position = center + turned(local, angle);
}
} // namespace

glm::mat4 Room::model() const {
    return glm::rotate(glm::translate(glm::mat4{1.0f}, center), angle_, {0.0f, 0.0f, 1.0f});
}

glm::mat4 Room::boardModel() const {
    return glm::rotate(glm::translate(model(), boardHinge), -boardAngle_, {0.0f, 1.0f, 0.0f});
}

void Room::advance(float elapsed, float angularSpeed) {
    angle_ = std::remainder(angle_ + angularSpeed * elapsed, fullTurn);
    for (auto& prop : props_) {
        prop.velocity.y -= 9.8f * elapsed;
        prop.position += prop.velocity * elapsed;
        contain(prop, angle_, angularSpeed);
    }
    for (std::size_t first = 0; first < props_.size(); ++first) {
        for (std::size_t second = first + 1; second < props_.size(); ++second) {
            auto& a = props_[first];
            auto& b = props_[second];
            const auto separation = b.position - a.position;
            const float distance = glm::length(separation);
            const float overlap = a.radius + b.radius - distance;
            if (overlap <= 0.0f || distance < .0001f) continue;
            const auto normal = separation / distance;
            a.position -= normal * (.5f * overlap);
            b.position += normal * (.5f * overlap);
            const float approach = glm::dot(b.velocity - a.velocity, normal);
            if (approach < 0.0f) {
                a.velocity += normal * (.60f * approach);
                b.velocity -= normal * (.60f * approach);
            }
            contain(a, angle_, angularSpeed);
            contain(b, angle_, angularSpeed);
        }
    }
    if (wallBroken_) {
        const float torque = 8.0f * std::sin(angle_) * std::sin(boardAngle_);
        boardSpeed_ += (torque - 2.6f * boardSpeed_) * elapsed;
        boardAngle_ = std::clamp(boardAngle_ + boardSpeed_ * elapsed, .07f, openAngle);
        if (boardAngle_ == .07f || boardAngle_ == openAngle) boardSpeed_ = 0.0f;
    }
}

void Room::update(float elapsed, float turn) {
    if (!std::isfinite(elapsed) || elapsed <= 0.0f) return;
    const float time = std::min(elapsed, .1f);
    const int steps = static_cast<int>(std::ceil(time * 180.0f));
    const float angularSpeed = maxAngularSpeed * std::clamp(turn, -1.0f, 1.0f);
    for (int i = 0; i < steps; ++i) {
        advance(time / steps, angularSpeed);
    }
}

void Room::breakWall() {
    if (wallBroken_) return;
    wallBroken_ = true;
    boardAngle_ = .075f;
    boardSpeed_ = 1.35f;
}

bool Room::bridgeReady() const {
    if (!wallBroken_) return false;
    const auto board = boardModel();
    const glm::vec3 hinge{board[3]};
    const auto tip = hinge + glm::vec3{board[0]} * boardLength;
    return std::abs(board[2].y) >= .98f && hinge.y < .17f && tip.y < .23f &&
           tip.z >= landingZ + .22f && std::abs(tip.x - center.x) < landingHalfWidth - .20f;
}

bool Room::escaped(glm::vec3 eye) const {
    return bridgeReady() && eye.z >= exitZ &&
           std::abs(eye.x - center.x) <= landingHalfWidth - playerRadius &&
           std::abs(eye.y - eyeHeight) < .1f;
}

std::optional<RayHit> Room::rayHit(glm::vec3 eye, glm::vec3 direction, float maxDistance) const {
    const auto origin = turned(eye - center, -angle_);
    const auto ray = turned(direction, -angle_);
    std::optional<RayHit> result;
    const auto test = [&](std::optional<float> distance, bool breakable = false) {
        if (distance && *distance < (result ? result->distance : maxDistance)) {
            result = RayHit{*distance, breakable};
        }
    };
    for (const auto& part : shell) {
        test(perspective::rayBoxDistance(origin, ray, part.center, part.half));
    }
    for (const auto& part : rearRim) {
        test(perspective::rayBoxDistance(origin, ray, part.center, part.half));
    }
    if (!wallBroken_) {
        test(perspective::rayBoxDistance(origin, ray, wallBox.center, wallBox.half), true);
    } else {
        const auto inverseBoard = glm::inverse(boardModel());
        const auto boardOrigin = glm::vec3{inverseBoard * glm::vec4{eye, 1.0f}};
        const auto boardRay = glm::vec3{inverseBoard * glm::vec4{direction, 0.0f}};
        test(perspective::rayBoxDistance(boardOrigin, boardRay, boardBox.center, boardBox.half));
    }
    for (const auto& prop : props_) test(raySphere(eye, direction, prop));
    return result;
}

glm::vec3 Room::resolveMovement(glm::vec3 previous, glm::vec3 desired, bool* grounded) const {
    const auto bridge = boardModel();
    const bool bridgeUsable = bridgeReady();
    const float priorFeet = previous.y - eyeHeight;
    const bool descending = desired.y <= previous.y;
    const auto resolve = [&](glm::vec3 position) -> std::optional<glm::vec3> {
        float support = -std::numeric_limits<float>::infinity();
        if (position.z <= frontZ) {
            if (std::abs(position.x - center.x) > apronHalfWidth - playerRadius) return std::nullopt;
            support = 0.0f;
        } else if (position.z <= farZ) {
            const auto span = chamberSpan(position.x, angle_);
            if (!span) return std::nullopt;
            support = span->low;
            position.y = std::min(position.y, span->high - headClearance);
        }
        if (position.z >= landingZ) {
            if (std::abs(position.x - center.x) > landingHalfWidth - playerRadius) return std::nullopt;
            support = 0.0f;
        }
        if (bridgeUsable && position.z >= farZ - .01f) {
            if (const auto board = boardSurface(bridge, position.x, position.z)) {
                support = std::max(support, *board);
            }
        }
        if (position.y - eyeHeight < support) {
            if (support - priorFeet > stepHeight) return std::nullopt;
            position.y = support + eyeHeight;
        }
        if (position.z > farZ - playerRadius && position.z < farZ + playerRadius &&
            (!bridgeUsable || !inDoorway(position, angle_))) {
            return std::nullopt;
        }
        return position;
    };

    // Resolve the horizontal axes separately so the player may slide along the opening.
    auto position = previous;
    position.y = desired.y;
    const float z = std::clamp(desired.z, startApronZ, endLandingZ);
    const auto movedX = resolve({desired.x, desired.y, previous.z});
    if (movedX) position = *movedX;
    const auto movedZ = resolve({position.x, desired.y, z});
    if (movedZ) {
        position = *movedZ;
    } else if (!movedX) {
        // If a candidate only conflicts in height, keep the last valid horizontal pose.
        position = resolve({previous.x, desired.y, previous.z}).value_or(previous);
    }
    if (position.y - eyeHeight < -3.3f) {
        if (grounded) *grounded = true;
        return {center.x, eyeHeight, 7.65f};
    }
    if (grounded) {
        // Query a tiny amount below the resolved pose. A horizontal snap is also a landing.
        auto probe = position;
        probe.y -= .004f;
        const auto touching = resolve(probe);
        *grounded = descending && touching && touching->y >= position.y - .0001f;
    }
    return position;
}
} // namespace home_rotate
