#include "HomeRoom.h"

#include "../shared/PerspectiveCarry.h"

#include <GL/glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <random>
#include <utility>
#include <vector>

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
constexpr float roomMiniatureScale = 0.10f;
constexpr glm::vec3 roomMiniatureOrigin{1.99f, 1.04f, 0.77f};
constexpr float debrisDuration = 1.4f;
constexpr float projectileSpeed = 12.0f;
constexpr float impactDuration = 0.20f;
constexpr float latchOpenTravel = 0.27f;
constexpr glm::vec3 latchCenter{0.70f, 1.45f, -6.58f};
constexpr glm::vec3 plaster{0.856f, 0.837f, 0.777f};

constexpr Box desk{{2.90f, 0.50f, 0.80f}, {3.30f, 1.00f, 1.40f}};
constexpr Box shelf{{0.0f, 1.30f, -5.95f}, {2.05f, 2.60f, 0.60f}};
constexpr Box miniShelf{roomMiniatureOrigin + shelf.center * roomMiniatureScale,
                        shelf.size * roomMiniatureScale + glm::vec3{0.05f, 0.04f, 0.05f}};
constexpr Box miniRpg{{3.16f, 1.21f, 0.59f}, {0.58f, 0.27f, 0.24f}};
constexpr Box miniDoor{{4.05f, 1.23f, 0.56f}, {0.31f, 0.48f, 0.18f}};
constexpr Box wallPanel{{0.0f, 1.30f, wallZ}, {1.80f, 2.60f, 0.31f}};
constexpr Box door{{0.0f, 1.30f, wallZ}, {1.76f, 2.60f, 0.105f}};
constexpr Box openDoor{{-0.85f, 1.30f, wallZ + 0.86f}, {0.105f, 2.60f, 1.76f}};
constexpr glm::vec3 wood{0.53f, 0.32f, 0.20f};

constexpr std::array staticInterior{
    desk,
    Box{{-3.75f, 0.38f, 1.80f}, {2.35f, 0.76f, 3.65f}},
    Box{{-4.85f, 0.37f, -0.55f}, {0.85f, 0.74f, 0.78f}},
    Box{{4.90f, 0.71f, -3.25f}, {1.20f, 1.42f, 1.50f}},
    Box{{-3.65f, 0.48f, 4.65f}, {1.25f, 0.96f, 1.05f}},
};

struct HomeMeshRange {
    GLint first;
    GLsizei count;
};

enum class HomeMesh {
    Interior, Shell, IntactWall, BrokenWall, MiniWalls, Shelf, Door, Rpg, Rack,
    MiniBrokenWall, LatchMounts, LatchBolt
};

#include "HomeGeometry.inc"

static_assert(std::size(homeMeshes) == 12);

struct WallShard {
    glm::vec3 center;
    GLint first;
    GLsizei count;
};

struct FractureMesh {
    std::vector<MeshVertex> vertices;
    std::array<WallShard, 18> shards;
};

const FractureMesh& fractureMesh() {
    static const FractureMesh mesh = [] {
        FractureMesh result;
        std::mt19937 random{0xF4u};
        std::uniform_real_distribution<float> horizontal{-0.84f, 0.84f};
        std::uniform_real_distribution<float> vertical{-1.24f, 1.24f};
        std::array<glm::vec2, 18> seeds;
        for (auto& seed : seeds) {
            seed = {horizontal(random), vertical(random)};
        }
        for (std::size_t index = 0; index < seeds.size(); ++index) {
            std::vector<glm::vec2> polygon{{-0.90f, -1.30f}, {0.90f, -1.30f},
                                         {0.90f, 1.30f}, {-0.90f, 1.30f}};
            // Voronoi cuts share edges and fill the wall with unequal polygons.
            for (std::size_t other = 0; other < seeds.size(); ++other) {
                if (other == index) {
                    continue;
                }
                const glm::vec2 normal = seeds[other] - seeds[index];
                const float offset = (glm::dot(seeds[other], seeds[other]) -
                                      glm::dot(seeds[index], seeds[index])) * 0.5f;
                std::vector<glm::vec2> clipped;
                for (std::size_t edge = 0; edge < polygon.size(); ++edge) {
                    const auto a = polygon[edge];
                    const auto b = polygon[(edge + 1) % polygon.size()];
                    const float da = glm::dot(a, normal) - offset;
                    const float db = glm::dot(b, normal) - offset;
                    if (da <= 0.0f) {
                        clipped.push_back(a);
                    }
                    if ((da <= 0.0f) != (db <= 0.0f)) {
                        clipped.push_back(a + (b - a) * (da / (da - db)));
                    }
                }
                polygon = std::move(clipped);
            }
            glm::vec2 center{0.0f};
            for (const auto& point : polygon) {
                center += point;
            }
            center /= static_cast<float>(polygon.size());
            const auto first = static_cast<GLint>(result.vertices.size());
            const auto triangle = [&](const glm::vec3& a, const glm::vec3& b,
                                      const glm::vec3& c, const glm::vec3& color) {
                result.vertices.insert(result.vertices.end(), {{a, color}, {b, color}, {c, color}});
            };
            const auto vertex = [&](std::size_t corner, float depth) {
                return glm::vec3{polygon[corner] - center, depth};
            };
            constexpr float halfDepth = 0.155f;
            for (std::size_t corner = 1; corner + 1 < polygon.size(); ++corner) {
                triangle(vertex(0, halfDepth), vertex(corner, halfDepth), vertex(corner + 1, halfDepth),
                         plaster * 0.64f);
                triangle(vertex(0, -halfDepth), vertex(corner + 1, -halfDepth), vertex(corner, -halfDepth),
                         plaster * 0.50f);
            }
            for (std::size_t corner = 0; corner < polygon.size(); ++corner) {
                const std::size_t next = (corner + 1) % polygon.size();
                const auto a = vertex(corner, halfDepth);
                const auto b = vertex(next, halfDepth);
                const auto c = vertex(next, -halfDepth);
                const auto d = vertex(corner, -halfDepth);
                constexpr glm::vec3 innerPlaster{0.64f, 0.62f, 0.56f};
                triangle(a, d, c, innerPlaster);
                triangle(a, c, b, innerPlaster);
            }
            result.shards[index] = {{center, 0.0f}, first,
                                   static_cast<GLsizei>(result.vertices.size()) - first};
        }
        return result;
    }();
    return mesh;
}

Box shelfAt(float offset) {
    return {shelf.center + glm::vec3{offset, 0.0f, 0.0f}, shelf.size};
}

Box miniatureBox(const Box& box) {
    return {roomMiniatureOrigin + box.center * roomMiniatureScale, box.size * roomMiniatureScale};
}

bool overlapsPlayer(const glm::vec3& position, const Box& box) {
    return std::abs(position.x - box.center.x) < box.size.x * 0.5f + playerRadius &&
           std::abs(position.z - box.center.z) < box.size.z * 0.5f + playerRadius;
}

bool aimsAt(const glm::vec3& eye, const glm::vec3& direction, const Box& box, float maxDistance) {
    const auto distance = perspective::rayBoxDistance(eye, direction, box.center, box.size * 0.5f);
    return distance && *distance <= maxDistance;
}

void drawHomeMesh(const BoxRenderer& renderer, HomeMesh mesh, const glm::mat4& model,
                  const glm::vec3& tint = glm::vec3{1.0f}) {
    const auto& range = homeMeshes[static_cast<std::size_t>(mesh)];
    const auto first = static_cast<GLint>(fractureMesh().vertices.size()) + range.first;
    renderer.drawMovableMesh(first, range.count, model, tint);
}

void drawRpg(const BoxRenderer& renderer, const glm::vec3& center, float scale) {
    const auto model = glm::scale(glm::translate(glm::mat4{1.0f}, center), glm::vec3{scale});
    drawHomeMesh(renderer, HomeMesh::Rpg, model);
}

void drawDoor(const BoxRenderer& renderer, const Box& box, const glm::vec3& tint,
              bool opened = false, const glm::mat4& parent = glm::mat4{1.0f}) {
    auto model = glm::translate(parent, box.center);
    if (opened) {
        model = glm::rotate(model, glm::radians(90.0f), {0.0f, 1.0f, 0.0f});
    }
    model = glm::scale(model, glm::vec3{box.size.y / door.size.y});
    drawHomeMesh(renderer, HomeMesh::Door, model, tint);
}

float visibleMeshRange(HomeMesh mesh, const glm::mat4& model, const glm::vec3& eye,
                       const glm::vec3& direction, float limit) {
    const auto& range = homeMeshes[static_cast<std::size_t>(mesh)];
    for (GLint first = range.first; first < range.first + range.count; first += 3) {
        const auto a = glm::vec3{model * glm::vec4{homeGeometry[first].position, 1.0f}};
        const auto b = glm::vec3{model * glm::vec4{homeGeometry[first + 1].position, 1.0f}};
        const auto c = glm::vec3{model * glm::vec4{homeGeometry[first + 2].position, 1.0f}};
        const auto edge = b - a;
        const auto otherEdge = c - a;
        const auto perpendicular = glm::cross(direction, otherEdge);
        const float determinant = glm::dot(edge, perpendicular);
        if (std::abs(determinant) < 1e-9f) {
            continue;
        }
        const auto offset = eye - a;
        const float u = glm::dot(offset, perpendicular) / determinant;
        const auto crossOffset = glm::cross(offset, edge);
        const float v = glm::dot(direction, crossOffset) / determinant;
        if (u < 0.0f || v < 0.0f || u + v > 1.0f) {
            continue;
        }
        const float distance = glm::dot(otherEdge, crossOffset) / determinant;
        if (distance > 0.0f) {
            limit = std::min(limit, distance);
        }
    }
    return limit;
}

glm::vec3 carrySide(const glm::vec3& direction) {
    const glm::vec3 side = glm::cross(direction, glm::vec3{0.0f, 1.0f, 0.0f});
    return glm::length(side) > 0.001f ? glm::normalize(side) : glm::vec3{1.0f, 0.0f, 0.0f};
}
}  // namespace

bool HomeRoom::initialize() {
    std::vector<MeshVertex> movable = fractureMesh().vertices;
    movable.insert(movable.end(), std::begin(homeGeometry), std::end(homeGeometry));
    return renderer_.initialize({}, movable);
}

float HomeRoom::visibleRange(const glm::vec3& eye, const glm::vec3& direction, float maxDistance) const {
    for (const Box& obstacle : staticInterior) {
        const auto distance = perspective::rayBoxDistance(eye, direction, obstacle.center, obstacle.size * 0.5f);
        if (distance) {
            maxDistance = std::min(maxDistance, *distance);
        }
    }
    const auto shelfHit = perspective::rayBoxDistance(
        eye, direction, shelfAt(shelfOffset_).center, shelf.size * 0.5f);
    if (shelfHit) {
        maxDistance = std::min(maxDistance, *shelfHit);
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

bool HomeRoom::canReachMiniatureLatch(const glm::vec3& eye, const glm::vec3& direction) const {
    const Box latch = miniatureBox({latchCenter, {0.50f, 0.35f, 0.28f}});
    float visible = visibleRange(eye, direction, 2.3f);
    if (eye.z >= latch.center.z - 0.02f || !aimsAt(eye, direction, latch, visible)) {
        return false;
    }
    const auto model = glm::scale(glm::translate(glm::mat4{1.0f}, roomMiniatureOrigin),
                                  glm::vec3{roomMiniatureScale});
    visible = visibleMeshRange(HomeMesh::MiniWalls, model, eye, direction, visible);
    visible = visibleMeshRange(HomeMesh::MiniBrokenWall, model, eye, direction, visible);
    visible = visibleMeshRange(HomeMesh::Door, glm::translate(model, door.center), eye, direction, visible);
    visible = visibleMeshRange(HomeMesh::Shelf, glm::translate(model, shelfAt(shelfOffset_).center),
                               eye, direction, visible);
    return aimsAt(eye, direction, latch, visible);
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
            if (!latchReleased_ && canReachMiniatureLatch(eye, direction)) {
                latchReleased_ = true;
            } else if (aimsAt(eye, direction, door, nearby)) {
                if (latchTravel_ >= latchOpenTravel) {
                    exitState_ = ExitState::DoorOpen;
                } else {
                    doorRattleTime_ = 0.30f;
                }
            }
            break;
        case ExitState::DoorOpen:
        case ExitState::Escaped:
            break;
    }
}

void HomeRoom::shoot(const glm::vec3& eye, const glm::vec3& direction) {
    if (held_ != HeldItem::Rpg || exitState_ != ExitState::WallRevealed || projectile_) {
        return;
    }
    shotFlashTime_ = 0.14f;
    float distance = 24.0f;
    bool breaksWall = false;
    bool miniature = false;
    // Test the complete flight segment so fast shots cannot skip thin furniture.
    const auto hit = [&](const Box& obstacle, bool breakable = false, bool small = false) {
        const Box target = small ? miniatureBox(obstacle) : obstacle;
        const auto depth = perspective::rayBoxDistance(eye, direction, target.center, target.size * 0.5f);
        if (depth && *depth < distance) {
            distance = *depth;
            breaksWall = breakable;
            miniature = small;
        }
    };
    hit(wallPanel, true);
    for (const auto& obstacle : staticInterior) {
        hit(obstacle);
    }
    hit(shelfAt(shelfOffset_));
    hit({{0.0f, -0.09f, 0.0f}, {11.7f, 0.18f, 13.1f}});
    hit({{0.0f, 3.32f, 0.0f}, {11.7f, 0.20f, 13.1f}});
    hit({{0.0f, 1.60f, 6.51f}, {11.7f, 3.20f, 0.34f}});
    for (const float side : {-1.0f, 1.0f}) {
        hit({{side * 5.87f, 1.60f, 0.0f}, {0.34f, 3.20f, 13.1f}});
        hit({{side * 3.37f, 1.60f, wallZ}, {4.94f, 3.20f, 0.31f}});
    }
    hit({{0.0f, 2.90f, wallZ}, {1.80f, 0.60f, 0.31f}});

    hit(wallPanel, true, true);
    for (const auto& obstacle : staticInterior) {
        hit(obstacle, false, true);
    }
    hit(shelfAt(shelfOffset_), false, true);
    hit({{0.0f, -0.09f, 0.0f}, {11.7f, 0.18f, 13.1f}}, false, true);
    hit({{0.0f, 0.30f, 6.51f}, {11.7f, 0.60f, 0.34f}}, false, true);
    for (const float side : {-1.0f, 1.0f}) {
        hit({{side * 5.87f, 0.30f, 0.0f}, {0.34f, 0.60f, 13.1f}}, false, true);
        hit({{side * 3.37f, 1.60f, wallZ}, {4.94f, 3.20f, 0.31f}}, false, true);
    }
    hit({{0.0f, 2.90f, wallZ}, {1.80f, 0.60f, 0.31f}}, false, true);
    const float launchDepth = std::min(distance, 0.45f);
    projectile_ = Projectile{eye + direction * launchDepth, direction, distance - launchDepth, breaksWall,
                             0.0f, miniature};
}

void HomeRoom::impact(const Projectile& projectile) {
    impactScale_ = projectile.miniature ? roomMiniatureScale : 1.0f;
    impactCenter_ = projectile.center - projectile.direction * (0.08f * impactScale_);
    impactTime_ = impactDuration;
    const glm::vec3 side = carrySide(projectile.direction);
    const glm::vec3 up = glm::normalize(glm::cross(side, projectile.direction));
    for (unsigned int index = 0; index < particles_.size(); ++index) {
        const float angle = static_cast<float>(index) * 2.39996f;
        const bool spark = index % 3 == 0;
        const float spread = 0.65f + static_cast<float>(index % 5) * 0.28f;
        const glm::vec3 radial = side * std::cos(angle) + up * std::sin(angle);
        particles_[index] = {
            impactCenter_,
            (radial * spread - projectile.direction * 1.1f) * (spark ? 2.0f : 0.7f) * impactScale_,
            spark ? 0.40f : 0.95f,
            (spark ? 0.045f : 0.18f + static_cast<float>(index % 4) * 0.025f) * impactScale_,
            spark,
        };
    }
    if (projectile.breaksWall) {
        held_ = HeldItem::None;
        exitState_ = ExitState::WallBroken;
        debrisTime_ = debrisDuration;
        std::mt19937 random{0xB019u};
        std::uniform_real_distribution<float> jitter{-1.0f, 1.0f};
        const auto& shapes = fractureMesh().shards;
        const glm::vec3 realImpact = projectile.miniature
            ? (projectile.center - roomMiniatureOrigin) / roomMiniatureScale : projectile.center;
        for (std::size_t index = 0; index < fragments_.size(); ++index) {
            const glm::vec3 center = wallPanel.center + shapes[index].center;
            const glm::vec3 outward = center - realImpact;
            const glm::vec3 axis = glm::normalize(glm::vec3{jitter(random), jitter(random), jitter(random)});
            fragments_[index] = {
                center,
                {outward.x * 2.0f + jitter(random), 2.0f + outward.y * 0.7f + jitter(random),
                 2.6f + jitter(random) * 1.5f},
                axis,
                jitter(random) * 0.12f,
                jitter(random) * 7.0f,
            };
        }
    }
}

void HomeRoom::scroll(double yOffset) {
    if (held_ == HeldItem::Door) {
        const float steps = static_cast<float>(std::clamp(yOffset, -10.0, 10.0));
        doorWheelScale_ = std::clamp(doorWheelScale_ * std::exp(-steps * 0.18f), 0.25f, 6.0f);
    }
}

void HomeRoom::update(float elapsed, const glm::vec3& eye) {
    if (elapsed <= 0.0f) {
        return;
    }
    if (latchReleased_) {
        latchTravel_ = std::min(latchOpenTravel, latchTravel_ + elapsed * 0.80f);
    }
    doorRattleTime_ = std::max(0.0f, doorRattleTime_ - elapsed);
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
    impactTime_ = std::max(0.0f, impactTime_ - elapsed);
    for (Particle& particle : particles_) {
        if (particle.life <= 0.0f) {
            continue;
        }
        particle.life = std::max(0.0f, particle.life - elapsed);
        particle.velocity.y += (particle.spark ? -7.5f : 0.5f) * elapsed * impactScale_;
        particle.center += particle.velocity * elapsed;
    }
    if (debrisTime_ > 0.0f) {
        debrisTime_ = std::max(0.0f, debrisTime_ - elapsed);
        const auto& mesh = fractureMesh();
        for (std::size_t index = 0; index < fragments_.size(); ++index) {
            auto& piece = fragments_[index];
            piece.velocity.y -= 7.5f * elapsed;
            piece.center += piece.velocity * elapsed;
            piece.angle += piece.angularSpeed * elapsed;
            const auto rotation = glm::rotate(glm::mat4{1.0f}, piece.angle, piece.rotationAxis);
            const auto& shape = mesh.shards[index];
            float floorHeight = 0.0f;
            for (GLint vertex = shape.first; vertex < shape.first + shape.count; ++vertex) {
                const auto rotated = rotation * glm::vec4{mesh.vertices[vertex].position, 0.0f};
                floorHeight = std::max(floorHeight, -rotated.y);
            }
            if (piece.center.y < floorHeight) {
                piece.center.y = floorHeight;
                piece.velocity.y = std::abs(piece.velocity.y) * 0.18f;
                piece.velocity.x *= 0.65f;
                piece.velocity.z *= 0.65f;
                piece.angularSpeed *= 0.65f;
            }
        }
    }
    if (projectile_) {
        const float distance = std::min(projectileSpeed * elapsed, projectile_->remaining);
        projectile_->center += projectile_->direction * distance;
        projectile_->remaining -= distance;
        projectile_->traveled += distance;
        if (projectile_->remaining <= 0.0f) {
            impact(*projectile_);
            projectile_.reset();
        }
    }
}

glm::vec3 HomeRoom::resolveMovement(const glm::vec3& previous, const glm::vec3& desired) {
    const bool open = exitState_ == ExitState::DoorOpen || exitState_ == ExitState::Escaped;
    const auto blocked = [&](const glm::vec3& position) {
        if (overlapsPlayer(position, shelfAt(shelfOffset_))) {
            return true;
        }
        for (const auto& obstacle : staticInterior) {
            if (overlapsPlayer(position, obstacle)) {
                return true;
            }
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

void HomeRoom::drawMiniature() const {
    renderer_.drawBox({roomMiniatureOrigin.x, 1.0125f, roomMiniatureOrigin.z},
                      {1.24f, 0.055f, 1.40f}, wood);
    const auto model = glm::scale(glm::translate(glm::mat4{1.0f}, roomMiniatureOrigin),
                                  glm::vec3{roomMiniatureScale});
    drawRoom(model, true);

    drawHomeMesh(renderer_, HomeMesh::Rack,
                 glm::translate(glm::mat4{1.0f}, {3.15f, 1.20f, 0.67f}));
    if ((exitState_ == ExitState::Explore || exitState_ == ExitState::WallRevealed) && held_ != HeldItem::Rpg) {
        drawRpg(renderer_, miniRpg.center, 1.0f);
    }

    renderer_.drawBox({4.05f, 1.025f, 0.57f}, {0.52f, 0.05f, 0.45f}, {0.56f, 0.48f, 0.35f});
    if (exitState_ <= ExitState::WallBroken && held_ != HeldItem::Door) {
        drawDoor(renderer_, {miniDoor.center, door.size * miniatureScale}, glm::vec3{1.0f});
    }
}

void HomeRoom::drawRoom(const glm::mat4& model, bool miniature) const {
    drawHomeMesh(renderer_, HomeMesh::Interior, model);
    drawHomeMesh(renderer_, miniature ? HomeMesh::MiniWalls : HomeMesh::Shell, model);
    const bool wallBroken = exitState_ >= ExitState::WallBroken;
    const HomeMesh rearWall = wallBroken
        ? (miniature ? HomeMesh::MiniBrokenWall : HomeMesh::BrokenWall) : HomeMesh::IntactWall;
    drawHomeMesh(renderer_, rearWall, model);
    const auto shelfModel = glm::translate(model, shelfAt(shelfOffset_).center);
    drawHomeMesh(renderer_, HomeMesh::Shelf, shelfModel);
    const bool exitOpen = exitState_ == ExitState::DoorOpen || exitState_ == ExitState::Escaped;
    auto doorParent = model;
    if (exitState_ == ExitState::DoorPlaced) {
        const glm::vec3 hinge = door.center - glm::vec3{door.size.x * 0.5f, 0.0f, 0.0f};
        doorParent = glm::translate(model, hinge);
        const float rattle = std::sin(doorRattleTime_ * 55.0f) * doorRattleTime_ * 0.04f;
        doorParent = glm::rotate(doorParent, rattle, {0.0f, 1.0f, 0.0f});
        doorParent = glm::translate(doorParent, -hinge);
        drawDoor(renderer_, door, glm::vec3{1.0f}, false, doorParent);
    } else if (exitOpen) {
        drawDoor(renderer_, openDoor, glm::vec3{1.0f}, true, model);
        const auto center = glm::vec3{model * glm::vec4{0.0f, 1.35f, -8.38f, 1.0f}};
        const float scale = miniature ? roomMiniatureScale : 1.0f;
        renderer_.drawBox(center, glm::vec3{1.87f, 2.65f, 0.02f} * scale, {0.95f, 0.80f, 0.48f});
    }
    if (exitState_ >= ExitState::DoorPlaced) {
        auto latchModel = glm::translate(doorParent, exitOpen ? openDoor.center : door.center);
        if (exitOpen) {
            latchModel = glm::rotate(latchModel, glm::radians(90.0f), {0.0f, 1.0f, 0.0f});
        }
        latchModel = glm::translate(latchModel, latchCenter - door.center);
        drawHomeMesh(renderer_, HomeMesh::LatchMounts, latchModel);
        latchModel = glm::translate(latchModel, {-latchTravel_, 0.0f, 0.0f});
        drawHomeMesh(renderer_, HomeMesh::LatchBolt, latchModel);
    }

    if (debrisTime_ > 0.0f) {
        const auto& shapes = fractureMesh().shards;
        for (std::size_t index = 0; index < fragments_.size(); ++index) {
            const auto& piece = fragments_[index];
            auto fragmentModel = glm::translate(model, piece.center);
            fragmentModel = glm::rotate(fragmentModel, piece.angle, piece.rotationAxis);
            renderer_.drawMovableMesh(shapes[index].first, shapes[index].count, fragmentModel, glm::vec3{1.0f});
        }
    }
}

void HomeRoom::draw(const glm::mat4& viewProjection, const glm::vec3& eye, const glm::vec3& direction) const {
    renderer_.begin(viewProjection);
    drawRoom(glm::mat4{1.0f}, false);
    drawMiniature();
    if (projectile_) {
        renderer_.drawBox(projectile_->center, {0.10f, 0.10f, 0.10f}, {1.0f, 0.89f, 0.55f});
        const glm::vec3 side = carrySide(projectile_->direction);
        const glm::vec3 up = glm::normalize(glm::cross(side, projectile_->direction));
        for (int index = 1; index <= 7; ++index) {
            const float trail = static_cast<float>(index) * 0.13f;
            if (trail > projectile_->traveled) {
                break;
            }
            const float angle = static_cast<float>(index) * 2.39996f;
            const float spread = 0.02f + static_cast<float>(index) * 0.025f;
            const glm::vec3 drift = (side * std::cos(angle) + up * std::sin(angle)) * spread;
            const float size = 0.04f + static_cast<float>(index) * 0.008f;
            renderer_.drawBox(projectile_->center - projectile_->direction * trail + drift,
                             glm::vec3{size}, {1.0f - trail * 0.22f, 0.86f - trail * 0.24f, 0.52f});
        }
    }
    if (impactTime_ > 0.0f) {
        const float size = (0.35f + (impactDuration - impactTime_) * 2.0f) * impactScale_;
        renderer_.drawBox(impactCenter_, glm::vec3{size}, {1.0f, 0.82f, 0.43f});
    }
    for (const Particle& particle : particles_) {
        if (particle.life > 0.0f) {
            const float size = particle.size * std::min(1.0f, particle.life / 0.18f);
            const glm::vec3 tint = particle.spark ? glm::vec3{1.0f, 0.65f, 0.23f}
                                                 : plaster * (0.65f + particle.life * 0.25f);
            renderer_.drawBox(particle.center, glm::vec3{size}, tint);
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
            drawDoor(renderer_, {door.center - direction * 0.04f, door.size}, {0.68f, 1.0f, 0.75f});
        } else {
            const glm::vec3 center = eye + direction * 1.15f + carrySide(direction) * 0.32f - glm::vec3{0.0f, 0.23f, 0.0f};
            const float scale = std::clamp(doorRatio_ * doorWheelScale_ * 1.15f, 0.08f, 0.32f);
            drawDoor(renderer_, {center, door.size * scale}, glm::vec3{1.0f});
        }
    }
}
