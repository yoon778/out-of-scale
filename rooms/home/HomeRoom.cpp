#include "HomeRoom.h"
#include "HomeFinaleVisual.h"
#include "HomePicture.h"

#include "../shared/PerspectiveCarry.h"

#include <GL/glfw3.h>
#include <GL/glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>
#include <vector>

namespace {
struct Box {
    glm::vec3 center;
    glm::vec3 size;
};

constexpr float playerRadius = 0.25f;
constexpr float sideLimit = 5.55f;
constexpr float secretEntryZ = 6.34f;
constexpr float finaleEntryZ = home_finale::frontZ - .25f;
constexpr float wallZ = -6.50f;
constexpr float backLimit = wallZ + .155f + playerRadius;
constexpr float reach = 2.8f;

constexpr Box desk{{2.90f, 0.50f, 0.80f}, {3.30f, 1.00f, 1.40f}};
constexpr Box shelf{{0.0f, 1.30f, -5.95f}, {2.05f, 2.60f, 0.60f}};
constexpr float eyeHeight = 1.65f;
constexpr float headClearance = 0.15f;
constexpr Box monitor{{5.505f, 1.85f, 3.0f}, {0.05f, 1.80f, 3.20f}};
constexpr Box monitorBody{{5.61f, 1.85f, 3.0f}, {0.20f, 2.0f, 3.40f}};
constexpr Box cabinetBody{{3.75f, 1.35f, 5.65f}, {3.60f, 2.70f, 1.28f}};
constexpr Box cabinetFront{{3.75f, 1.35f, 4.99f}, {3.60f, 2.50f, 0.08f}};
constexpr glm::vec3 leftHinge{1.95f, 1.35f, 4.99f};
constexpr glm::vec3 rightHinge{5.55f, 1.35f, 4.99f};
constexpr int socketObject = 0;
constexpr int completeKeyObject = 1;
constexpr std::array keyParts{
    Box{{0.0f, .10f, 0.0f}, {1.30f, .20f, .12f}},
    Box{{.25f, -.10f, 0.0f}, {.13f, .20f, .12f}},
    Box{{.55f, -.10f, 0.0f}, {.13f, .20f, .12f}},
};
constexpr std::array completeKeyParts{
    Box{{-.168f,0,0},{.144f,.144f,.0576f}},
    Box{{.060f,.024f,0},{.312f,.048f,.0288f}},
    Box{{.120f,-.024f,0},{.0312f,.048f,.0288f}},
    Box{{.192f,-.024f,0},{.0312f,.048f,.0288f}},
};
constexpr std::array socketFlatParts{Box{{0,0,0},{.24f,.025f,.32f}}};
constexpr std::array socketUprightParts{Box{{0,0,0},{.24f,.32f,.025f}}};
std::span<const Box> portableParts(int index, bool flatSocket) {
    if (index == completeKeyObject) return completeKeyParts;
    return flatSocket ? std::span<const Box>{socketFlatParts} : std::span<const Box>{socketUprightParts};
}

constexpr std::array cabinetFrame{
    Box{{1.99f,1.35f,5.685f},{.08f,2.70f,1.35f}},
    Box{{5.51f,1.35f,5.685f},{.08f,2.70f,1.35f}},
    Box{{3.75f,2.66f,5.685f},{3.60f,.08f,1.35f}},
};

constexpr std::array staticInterior{
    desk,
    Box{{-3.75f, 0.38f, 1.80f}, {2.35f, 0.76f, 3.65f}},
    Box{{-4.85f, 0.37f, -0.55f}, {0.85f, 0.74f, 0.78f}},
    Box{{4.90f, 0.71f, -3.25f}, {1.20f, 1.42f, 1.50f}},
    Box{{-3.65f, 0.48f, 4.65f}, {1.25f, 0.96f, 1.05f}},
    Box{{-1.95f,1.60f,6.51f},{7.80f,3.20f,.34f}},
    Box{{6.695f,1.60f,6.51f},{2.29f,3.20f,.34f}},
    Box{{3.75f,2.95f,6.51f},{3.60f,.50f,.34f}},
};

bool withinRoomXZ(const glm::vec3& center, float extentX, float extentZ) {
    const float minZ = center.z - extentZ;
    const float maxZ = center.z + extentZ;
    if (minZ < wallZ + .16f || maxZ > home_finale::frontZ - .05f) return false;
    if (minZ < 6.68f && std::abs(center.x) + extentX > 5.69f) return false;
    return maxZ <= 6.34f || std::abs(center.x - home_finale::hallCenterX) + extentX <= home_finale::entranceHalfWidth;
}

constexpr float roomCeiling(float, float = 0.0f) { return 3.22f; }

// These match the artist's shelf platform, front lip, side rails and rear rail.
constexpr std::array doorShelf{
    Box{{5.28f, 2.1675f, -3.25f}, {0.80f, 0.065f, 1.50f}},
    Box{{4.905f, 2.245f, -3.25f}, {0.05f, 0.09f, 1.50f}},
    Box{{5.28f, 2.255f, -3.9775f}, {0.80f, 0.11f, 0.045f}},
    Box{{5.28f, 2.255f, -2.5225f}, {0.80f, 0.11f, 0.045f}},
    Box{{5.665f, 2.26f, -3.25f}, {0.03f, 0.12f, 1.50f}},
};

struct HomeMeshRange {
    GLint first;
    GLsizei count;
};

enum class HomeMesh {
    Interior, Shell, IntactWall, BrokenWall, MiniWalls, Shelf, Door, Rpg, Rack,
    MiniBrokenWall, LatchMounts, LatchBolt, CabinetLeftDoor, CabinetRightDoor, CabinetMiniatures,
    LockSocket, SchoolDiorama, MilitaryDiorama, OfficeDiorama, HomeDioramaBase, PictureDoor, Crate
};

#include "HomeGeometry.inc"

static_assert(std::size(homeMeshes) == 22);

struct BedroomMesh {
    std::vector<MeshVertex> vertices;
    std::array<HomeMeshRange, std::size(homeMeshes)> ranges{};
};

const BedroomMesh& bedroomMesh() {
    static const auto mesh = [] {
        BedroomMesh result;
        constexpr std::array pieces{
            HomeMesh::Interior, HomeMesh::Shell, HomeMesh::IntactWall, HomeMesh::Shelf,
            HomeMesh::CabinetLeftDoor, HomeMesh::CabinetRightDoor,
            HomeMesh::LockSocket, HomeMesh::PictureDoor, HomeMesh::Crate,
        };
        result.vertices.reserve(150000);
        for (const auto piece : pieces) {
            const auto slot = static_cast<std::size_t>(piece);
            const auto original = homeMeshes[slot];
            const auto first = static_cast<GLint>(result.vertices.size());
            for (GLint triangle = original.first; triangle < original.first + original.count; triangle += 3) {
                const auto* vertices = homeGeometry + triangle;
                // The bedroom mesh shares two source groups with the old gallery.
                if ((piece == HomeMesh::Interior || piece == HomeMesh::Shell) &&
                    std::any_of(vertices, vertices + 3, [](const MeshVertex& vertex) {
                        return vertex.position.z > 6.80f;
                    })) continue;
                result.vertices.insert(result.vertices.end(), vertices, vertices + 3);
            }
            result.ranges[slot] = {first, static_cast<GLsizei>(result.vertices.size()) - first};
        }
        return result;
    }();
    return mesh;
}

float swingAngle(float progress) {
    return glm::radians(90.0f) * progress * progress * (3.0f - 2.0f * progress);
}

glm::mat4 hingeModel(const glm::vec3& hinge, float angle) {
    auto model = glm::rotate(glm::translate(glm::mat4{1.0f}, hinge), angle, {0.0f,1.0f,0.0f});
    return glm::translate(model, -hinge);
}

Box swungBox(const Box& box, const glm::vec3& hinge, float angle) {
    const auto center = glm::vec3{hingeModel(hinge,angle) * glm::vec4{box.center,1.0f}};
    const float c = std::abs(std::cos(angle)), s = std::abs(std::sin(angle));
    return {center,{c*box.size.x+s*box.size.z,box.size.y,s*box.size.x+c*box.size.z}};
}

std::array<Box, 2> cabinetDoors(float progress) {
    const float angle = swingAngle(progress);
    return {swungBox({{2.85f,1.35f,4.99f},{1.80f,2.50f,.08f}},leftHinge,angle),
            swungBox({{4.65f,1.35f,4.99f},{1.80f,2.50f,.08f}},rightHinge,-angle)};
}

bool overlapsPlayer(const glm::vec3& position, const Box& box) {
    return std::abs(position.x - box.center.x) < box.size.x * 0.5f + playerRadius &&
           std::abs(position.z - box.center.z) < box.size.z * 0.5f + playerRadius;
}

bool aimsAt(const glm::vec3& eye, const glm::vec3& direction, const Box& box, float maxDistance) {
    const auto distance = perspective::rayBoxDistance(eye, direction, box.center, box.size * 0.5f);
    return distance && *distance <= maxDistance;
}

glm::vec3 cubeLocal(const glm::vec3& delta, float yaw) {
    const float c = std::cos(yaw);
    const float s = std::sin(yaw);
    return {c * delta.x - s * delta.z, delta.y, s * delta.x + c * delta.z};
}

bool cubeFootprint(const glm::vec3& point, const glm::vec3& center, float edge, float yaw) {
    const auto local = cubeLocal(point - center, yaw);
    const float half = edge * 0.5f;
    const float x = local.x - std::clamp(local.x, -half, half);
    const float z = local.z - std::clamp(local.z, -half, half);
    return x * x + z * z < playerRadius * playerRadius;
}

bool cubeOverlapsBox(const glm::vec3& center, float edge, float yaw, const Box& box) {
    const float half = edge * 0.5f;
    if (std::abs(center.y - box.center.y) >= half + box.size.y * 0.5f - 0.001f) return false;
    const float c = std::abs(std::cos(yaw));
    const float s = std::abs(std::sin(yaw));
    const auto delta = center - box.center;
    const float projected = half * (c + s);
    if (std::abs(delta.x) >= projected + box.size.x * 0.5f - 0.001f ||
        std::abs(delta.z) >= projected + box.size.z * 0.5f - 0.001f) return false;
    const auto local = cubeLocal(delta, yaw);
    return std::abs(local.x) < half + (box.size.x * c + box.size.z * s) * 0.5f - 0.001f &&
           std::abs(local.z) < half + (box.size.x * s + box.size.z * c) * 0.5f - 0.001f;
}

bool cubeOverlapsPlayer(const glm::vec3& center, float edge, float yaw, const glm::vec3& eye) {
    return eye.y - eyeHeight < center.y + edge * 0.5f - 0.01f &&
           eye.y + headClearance > center.y - edge * 0.5f + 0.01f &&
           cubeFootprint(eye, center, edge, yaw);
}

std::optional<float> cubeRay(const glm::vec3& eye, const glm::vec3& direction,
                             const glm::vec3& center, float edge, float yaw) {
    return perspective::rayBoxDistance(cubeLocal(eye - center, yaw), cubeLocal(direction, yaw),
                                       glm::vec3{0.0f}, glm::vec3{edge * 0.5f});
}

bool rotatedBoxesOverlap(const Box& a, float yawA, const Box& b, float yawB = 0.0f) {
    if (std::abs(a.center.y - b.center.y) >= (a.size.y + b.size.y) * .5f - .001f) return false;
    const glm::vec2 xA{std::cos(yawA), -std::sin(yawA)};
    const glm::vec2 zA{std::sin(yawA), std::cos(yawA)};
    const glm::vec2 xB{std::cos(yawB), -std::sin(yawB)};
    const glm::vec2 zB{std::sin(yawB), std::cos(yawB)};
    const glm::vec2 delta{a.center.x - b.center.x, a.center.z - b.center.z};
    for (const auto axis : {xA, zA, xB, zB}) {
        const float radiusA = (a.size.x * std::abs(glm::dot(axis, xA)) + a.size.z * std::abs(glm::dot(axis, zA))) * .5f;
        const float radiusB = (b.size.x * std::abs(glm::dot(axis, xB)) + b.size.z * std::abs(glm::dot(axis, zB))) * .5f;
        if (std::abs(glm::dot(delta, axis)) >= radiusA + radiusB - .001f) return false;
    }
    return true;
}

Box worldKeyPart(const glm::vec3& center, float yaw, const Box& part) {
    return {center + cubeLocal(part.center, -yaw), part.size};
}

bool keyOverlapsPlayer(const glm::vec3& center, float yaw, const glm::vec3& eye) {
    const auto local = cubeLocal(eye - center, yaw);
    for (const auto& part : keyParts) {
        if (local.y - eyeHeight >= part.center.y + part.size.y * .5f - .01f ||
            local.y + headClearance <= part.center.y - part.size.y * .5f + .01f) continue;
        const float x = local.x - std::clamp(local.x, part.center.x - part.size.x * .5f, part.center.x + part.size.x * .5f);
        const float z = local.z - std::clamp(local.z, part.center.z - part.size.z * .5f, part.center.z + part.size.z * .5f);
        if (x * x + z * z < playerRadius * playerRadius) return true;
    }
    return false;
}

std::optional<float> keyRay(const glm::vec3& eye, const glm::vec3& direction,
                            const glm::vec3& center, float yaw) {
    std::optional<float> nearest;
    for (const auto& part : keyParts) {
        const auto hit = perspective::rayBoxDistance(cubeLocal(eye - center, yaw), cubeLocal(direction, yaw),
                                                      part.center, part.size * .5f);
        if (hit && (!nearest || *hit < *nearest)) nearest = hit;
    }
    return nearest;
}

std::optional<float> partsRay(std::span<const Box> parts, const glm::vec3& eye,
                              const glm::vec3& direction, const glm::vec3& center, float yaw) {
    std::optional<float> nearest;
    for (const auto& part : parts) {
        const auto hit = perspective::rayBoxDistance(cubeLocal(eye-center,yaw),cubeLocal(direction,yaw),
                                                       part.center,part.size*.5f);
        if (hit && (!nearest || *hit < *nearest)) nearest = hit;
    }
    return nearest;
}

bool partsOverlapPlayer(std::span<const Box> parts, const glm::vec3& center, float yaw, const glm::vec3& eye) {
    const auto local = cubeLocal(eye-center,yaw);
    for (const auto& part : parts) {
        if (local.y-eyeHeight >= part.center.y+part.size.y*.5f-.01f ||
            local.y+headClearance <= part.center.y-part.size.y*.5f+.01f) continue;
        const float x = local.x-std::clamp(local.x,part.center.x-part.size.x*.5f,part.center.x+part.size.x*.5f);
        const float z = local.z-std::clamp(local.z,part.center.z-part.size.z*.5f,part.center.z+part.size.z*.5f);
        if (x*x+z*z < playerRadius*playerRadius) return true;
    }
    return false;
}

std::array<glm::vec3, 8> cubePoints(const glm::vec3& center, float edge, float yaw) {
    std::array<glm::vec3, 8> result{};
    int index = 0;
    for (const float x : {-edge * .5f, edge * .5f}) {
        for (const float y : {-edge * .5f, edge * .5f}) {
            for (const float z : {-edge * .5f, edge * .5f}) result[index++] = center + cubeLocal({x,y,z}, -yaw);
        }
    }
    return result;
}

std::array<glm::vec3, 12> keyPoints(const glm::vec3& center, float yaw) {
    constexpr std::array<glm::vec3, 12> outline{{
        {-.65f,0,-.06f},{-.65f,.20f,-.06f},{.65f,.20f,-.06f},{.65f,0,-.06f},
        {.615f,0,-.06f},{.615f,-.20f,-.06f},{.485f,-.20f,-.06f},{.485f,0,-.06f},
        {.315f,0,-.06f},{.315f,-.20f,-.06f},{.185f,-.20f,-.06f},{.185f,0,-.06f}}};
    std::array<glm::vec3, 12> result{};
    for (std::size_t index = 0; index < result.size(); ++index) result[index] = center + cubeLocal(outline[index], -yaw);
    return result;
}

void drawHomeMesh(const BoxRenderer& renderer, HomeMesh mesh, const glm::mat4& model,
                  const glm::vec3& tint = glm::vec3{1.0f}) {
    const auto& range = bedroomMesh().ranges[static_cast<std::size_t>(mesh)];
    renderer.drawMovableMesh(range.first, range.count, model, tint);
}

}  // namespace

bool HomeRoom::initialize() {
    auto movable = bedroomMesh().vertices;
    const auto& painting = home_picture::art().vertices;
    movable.insert(movable.end(), painting.begin(), painting.end());
    finaleVisualFirst_ = static_cast<GLint>(movable.size());
    const auto finaleVisual = home_finale::visualVertices();
    movable.insert(movable.end(), finaleVisual.begin(), finaleVisual.end());
    return renderer_.initialize({}, movable) && computer_.initialize();
}

void HomeRoom::closeComputer() { computerFocused_ = false; computer_.clearInput(); }

void HomeRoom::computerKey(int key, int action, int mods) {
    if (computerFocused_ || action == GLFW_RELEASE) computer_.keyInput(key, action, mods);
    if (action == GLFW_PRESS && key == GLFW_KEY_R && finale_.isCarrying()) finale_.rotate();
}

void HomeRoom::computerClearInput() { computer_.clearInput(); }

void HomeRoom::scroll(double yOffset) { finale_.scroll(yOffset); }

glm::mat4 HomeRoom::portableModel(int index) const {
    auto model = glm::translate(glm::mat4{1.0f},portable_[index].center);
    model = glm::rotate(model,portable_[index].yaw,{0.0f,1.0f,0.0f});
    if (index == completeKeyObject && completeKeyInserted_)
        model = glm::rotate(model,swingAngle(keyTurn_),{1.0f,0.0f,0.0f});
    if (index == socketObject && socketFlat_) model = glm::rotate(model,glm::radians(90.0f),{1.0f,0.0f,0.0f});
    if (index == completeKeyObject) model = glm::scale(model,glm::vec3{.24f});
    return model;
}

std::optional<glm::vec2> HomeRoom::computerUv(const glm::vec3& eye, const glm::vec3& direction) const {
    if (!ComputerScreen::rayToUv(eye, direction, reach)) return std::nullopt;
    return ComputerScreen::rayToUv(eye, direction, visibleRange(eye, direction, reach));
}

bool HomeRoom::portableFits(int index, const PortableObject& object, const glm::vec3& eye) const {
    const auto parts = portableParts(index,socketFlat_);
    if (partsOverlapPlayer(parts,object.center,object.yaw,eye)) return false;
    for (const auto& part : parts) {
        const auto world = worldKeyPart(object.center,object.yaw,part);
        const float c = std::abs(std::cos(object.yaw)), s = std::abs(std::sin(object.yaw));
        const float extentX = (part.size.x*c+part.size.z*s)*.5f;
        const float extentZ = (part.size.x*s+part.size.z*c)*.5f;
        if (!withinRoomXZ(world.center,extentX,extentZ) || world.center.y-part.size.y*.5f < -.001f ||
            world.center.y+part.size.y*.5f > roomCeiling(world.center.z,extentZ)) return false;
        const auto blocked = [&](const Box& obstacle,float yaw = 0.0f) {
            return rotatedBoxesOverlap(world,object.yaw,obstacle,yaw);
        };
        for (const auto& obstacle : staticInterior) if (blocked(obstacle)) return false;
        for (const auto& obstacle : doorShelf) if (blocked(obstacle)) return false;
        for (const auto& obstacle : cabinetFrame) if (blocked(obstacle)) return false;
        for (const auto& obstacle : cabinetDoors(cabinetSwing_)) if (blocked(obstacle)) return false;
        if (blocked(monitorBody) || blocked(shelf)) return false;
        if (computerCube_ && held_ != HeldItem::Cube && blocked({computerCube_->center,glm::vec3{computerCube_->edge}},computerCube_->yaw)) return false;
        if (keyPiece_ && held_ != HeldItem::Key) {
            for (const auto& piece : keyParts) if (blocked(worldKeyPart(keyPiece_->center,keyPiece_->yaw,piece),keyPiece_->yaw)) return false;
        }
        for (int other = 0; other < static_cast<int>(portable_.size()); ++other) {
            if (other == index || !portable_[other].active || heldPortable_ == other) continue;
            for (const auto& piece : portableParts(other,socketFlat_)) {
                if (blocked(worldKeyPart(portable_[other].center,portable_[other].yaw,piece),portable_[other].yaw)) return false;
            }
        }
    }
    return true;
}

bool HomeRoom::overlapsPortable(const glm::vec3& center, const glm::vec3& size, float yaw, int ignore) const {
    for (int index = 0; index < static_cast<int>(portable_.size()); ++index) {
        if (index == ignore || !portable_[index].active || heldPortable_ == index) continue;
        for (const auto& part : portableParts(index,socketFlat_))
            if (rotatedBoxesOverlap({center,size},yaw,worldKeyPart(portable_[index].center,portable_[index].yaw,part),portable_[index].yaw)) return true;
    }
    return false;
}

std::optional<HomeRoom::PortableObject> HomeRoom::carriedPortable(
    int index, const glm::vec3& eye, const glm::vec3& direction) const {
    auto object = portable_[index];
    object.verticalSpeed = 0.0f;
    object.loose = true;
    float bottom = 0.0f, top = 0.0f, radius = .20f;
    for (const auto& part : portableParts(index,socketFlat_)) {
        bottom = std::min(bottom,part.center.y-part.size.y*.5f);
        top = std::max(top,part.center.y+part.size.y*.5f);
        radius = std::max(radius,glm::length(glm::vec2{part.size.x,part.size.z})*.5f);
    }
    const auto reachable = [&] {
        if (!portableFits(index,object,eye)) return false;
        const auto delta = object.center-eye;
        const float distance = glm::length(delta);
        return distance > .01f && visibleRange(eye,delta/distance,distance) >= distance-.002f;
    };
    const float visible = visibleRange(eye,direction,2.0f);
    for (int sample = 0; sample <= 24; ++sample) {
        const float distance = visible-static_cast<float>(sample)*.06f;
        if (distance < .35f) break;
        object.center = eye+direction*distance;
        object.center.y = std::clamp(object.center.y,-bottom,roomCeiling(object.center.z,radius)-top);
        if (reachable()) return object;
    }
    for (const float extra : {0.0f,.45f,.90f}) {
        for (int sample = 0; sample < 24; ++sample) {
            const float angle = static_cast<float>(sample)*.2617994f;
            object.center = eye+glm::vec3{std::cos(angle),0.0f,std::sin(angle)}*(radius+playerRadius+.08f+extra);
            object.center.y = std::clamp(eye.y-.45f,-bottom,roomCeiling(object.center.z,radius)-top);
            if (reachable()) return object;
        }
    }
    return std::nullopt;
}

void HomeRoom::updatePortables(float elapsed, const glm::vec3& eye, const glm::vec3& direction) {
    for (int index = 0; index < static_cast<int>(portable_.size()); ++index) {
        auto& object = portable_[index];
        if (!object.active) continue;
        if (heldPortable_ == index) {
            if (const auto pose = carriedPortable(index,eye,direction)) object = *pose;
            continue;
        }
        if (!object.loose) continue;
        const auto parts = portableParts(index,socketFlat_);
        const int steps = std::clamp(static_cast<int>(std::ceil(elapsed*90.0f)),1,18);
        const float step = elapsed/static_cast<float>(steps);
        for (int frame = 0; frame < steps; ++frame) {
            float support = 0.0f;
            for (const auto& part : parts) support = std::max(support,-part.center.y+part.size.y*.5f);
            const auto supportedBy = [&](const Box& obstacle,float yaw = 0.0f) {
                const float obstacleTop = obstacle.center.y+obstacle.size.y*.5f;
                for (const auto& part : parts) {
                    auto world = worldKeyPart(object.center,object.yaw,part);
                    const float bottom = world.center.y-part.size.y*.5f;
                    world.center.y = obstacle.center.y;
                    world.size.y = obstacle.size.y+1.0f;
                    if (bottom >= obstacleTop-.005f && rotatedBoxesOverlap(world,object.yaw,obstacle,yaw))
                        support = std::max(support,obstacleTop-part.center.y+part.size.y*.5f);
                }
            };
            for (const auto& obstacle : staticInterior) supportedBy(obstacle);
            for (const auto& obstacle : doorShelf) supportedBy(obstacle);
            for (const auto& obstacle : cabinetFrame) supportedBy(obstacle);
            for (const auto& obstacle : cabinetDoors(cabinetSwing_)) supportedBy(obstacle);
            supportedBy(monitorBody);
            supportedBy(shelf);
            supportedBy({{eye.x,eye.y+headClearance-.005f,eye.z},{playerRadius*2.0f,.01f,playerRadius*2.0f}});
            if (computerCube_ && held_ != HeldItem::Cube) supportedBy({computerCube_->center,glm::vec3{computerCube_->edge}},computerCube_->yaw);
            if (keyPiece_ && held_ != HeldItem::Key) {
                for (const auto& part : keyParts) supportedBy(worldKeyPart(keyPiece_->center,keyPiece_->yaw,part),keyPiece_->yaw);
            }
            for (int other = 0; other < static_cast<int>(portable_.size()); ++other) {
                if (other == index || !portable_[other].active || heldPortable_ == other) continue;
                for (const auto& part : portableParts(other,socketFlat_))
                    supportedBy(worldKeyPart(portable_[other].center,portable_[other].yaw,part),portable_[other].yaw);
            }
            object.verticalSpeed -= 9.8f*step;
            object.center.y += object.verticalSpeed*step;
            if (object.center.y <= support) {
                object.center.y = support;
                object.verticalSpeed = 0.0f;
            }
        }
    }
}

bool HomeRoom::grabPortable(const glm::vec3& eye, const glm::vec3& direction) {
    float nearest = visibleRange(eye, direction, reach) + .001f;
    int found = -1;
    for (int index = 0; index < static_cast<int>(portable_.size()); ++index) {
        const auto& object = portable_[index];
        if (!object.active || (index == socketObject && socketInstalled_) ||
            (index == completeKeyObject && completeKeyInserted_)) continue;
        const auto hit = partsRay(portableParts(index, socketFlat_), eye, direction, object.center, object.yaw);
        if (hit && *hit < nearest) {
            found = index;
            nearest = *hit;
        }
    }
    if (found < 0) return false;
    heldPortable_ = found;
    held_ = HeldItem::Portable;
    portable_[found].loose = true;
    if (found == socketObject) socketFlat_ = false;
    return true;
}

bool HomeRoom::swingBlocked(const glm::vec3& center, const glm::vec3& size,
                           const glm::vec3& eye) const {
    const Box leaf{center,size};
    if (eye.y+headClearance > center.y-size.y*.5f &&
        eye.y-eyeHeight < center.y+size.y*.5f && overlapsPlayer(eye,leaf)) return true;
    if (computerCube_ && cubeOverlapsBox(computerCube_->center,computerCube_->edge,computerCube_->yaw,leaf)) return true;
    if (keyPiece_) {
        for (const auto& part : keyParts)
            if (rotatedBoxesOverlap(worldKeyPart(keyPiece_->center,keyPiece_->yaw,part),keyPiece_->yaw,leaf)) return true;
    }
    return false;
}

void HomeRoom::updateDoors(float elapsed, const glm::vec3& eye) {
    if (completeKeyInserted_ && keyTurn_ < 1.0f) {
        const float remaining = (1.0f-keyTurn_) * .65f;
        const float used = std::min(elapsed,remaining);
        keyTurn_ = std::min(1.0f,keyTurn_+used/.65f);
        elapsed -= used;
    }
    if (cabinetUnlocked_ && keyTurn_ >= 1.0f && !cabinetOpen_) {
        const float target = std::min(1.0f,cabinetSwing_+elapsed/1.10f);
        while (cabinetSwing_ < target) {
            const float next = std::min(target,cabinetSwing_+.01f);
            const auto leaves = cabinetDoors(next);
            if (std::any_of(leaves.begin(),leaves.end(),[&](const Box& leaf) {
                    return swingBlocked(leaf.center,leaf.size,eye);
                })) break;
            const float delta = swingAngle(next)-swingAngle(cabinetSwing_);
            const auto motion = hingeModel(leftHinge,delta);
            for (const int index : {socketObject,completeKeyObject}) {
                auto& item = portable_[index];
                if (!item.active) continue;
                item.center = glm::vec3{motion*glm::vec4{item.center,1.0f}};
                item.yaw += delta;
            }
            cabinetSwing_ = next;
        }
        cabinetOpen_ = cabinetSwing_ >= 1.0f;
    }
}

bool HomeRoom::cubeFits(const ComputerCube& cube, const glm::vec3& eye) const {
    const float half = cube.edge * 0.5f;
    const float extent = half * (std::abs(std::cos(cube.yaw)) + std::abs(std::sin(cube.yaw)));
    if (!withinRoomXZ(cube.center,extent,extent) || cube.center.y < half - 0.001f ||
        cube.center.y + half > roomCeiling(cube.center.z,extent) || cubeOverlapsPlayer(cube.center, cube.edge, cube.yaw, eye) ||
        overlapsPortable(cube.center,glm::vec3{cube.edge},cube.yaw)) return false;
    for (const auto& obstacle : staticInterior) {
        if (cubeOverlapsBox(cube.center, cube.edge, cube.yaw, obstacle)) return false;
    }
    for (const auto& obstacle : doorShelf) {
        if (cubeOverlapsBox(cube.center, cube.edge, cube.yaw, obstacle)) return false;
    }
    for (const auto& obstacle : cabinetFrame) {
        if (cubeOverlapsBox(cube.center, cube.edge, cube.yaw, obstacle)) return false;
    }
    for (const auto& obstacle : cabinetDoors(cabinetSwing_)) {
        if (cubeOverlapsBox(cube.center, cube.edge, cube.yaw, obstacle)) return false;
    }
    if (keyPiece_ && held_ != HeldItem::Key) {
        for (const auto& part : keyParts) {
            if (rotatedBoxesOverlap({cube.center,glm::vec3{cube.edge}},cube.yaw,
                                     worldKeyPart(keyPiece_->center,keyPiece_->yaw,part),keyPiece_->yaw)) return false;
        }
    }
    return !cubeOverlapsBox(cube.center, cube.edge, cube.yaw, shelf) &&
           !cubeOverlapsBox(cube.center, cube.edge, cube.yaw, monitorBody);
}

std::optional<HomeRoom::ComputerCube> HomeRoom::carriedCube(
    const glm::vec3& eye, const glm::vec3& direction) const {
    ComputerCube cube = *computerCube_;
    cube.verticalSpeed = 0.0f;
    glm::vec3 forward{direction.x, 0.0f, direction.z};
    forward = glm::length(forward) > 0.01f ? glm::normalize(forward) : glm::vec3{0.0f,0.0f,-1.0f};
    const float half = cube.edge * 0.5f;
    const auto reachable = [&] {
        if (!cubeFits(cube, eye)) return false;
        const auto offset = cube.center - eye;
        const float distance = glm::length(offset);
        return distance > .01f && visibleRange(eye, offset / distance, distance) >= distance - .002f;
    };
    const float visible = visibleRange(eye, direction, 2.0f);
    for (int sample = 0; sample <= 24; ++sample) {
        const float distance = visible - static_cast<float>(sample) * 0.06f;
        if (distance < .35f) break;
        cube.center = eye + direction * distance;
        cube.center.y = std::clamp(cube.center.y, half, roomCeiling(cube.center.z,half*1.415f) - half);
        if (reachable()) return cube;
    }
    const float clearance = half * (std::abs(std::cos(cube.yaw)) + std::abs(std::sin(cube.yaw))) + playerRadius + .08f;
    for (const float extra : {0.0f, .45f, .90f}) {
        const float radius = clearance + extra;
        for (int sample = 0; sample < 16; ++sample) {
            const float angle = static_cast<float>(sample) * 0.39269908f;
            const glm::vec3 offset{forward.x * std::cos(angle) + forward.z * std::sin(angle), 0.0f,
                                   forward.z * std::cos(angle) - forward.x * std::sin(angle)};
            cube.center = eye + offset * radius;
            cube.center.y = std::clamp(eye.y - .45f, half, roomCeiling(cube.center.z,half*1.415f) - half);
            if (reachable()) return cube;
        }
    }
    return std::nullopt;
}

bool HomeRoom::keyFits(const KeyPiece& key, const glm::vec3& eye) const {
    const float c = std::abs(std::cos(key.yaw));
    const float s = std::abs(std::sin(key.yaw));
    if (!withinRoomXZ(key.center,.65f*c+.06f*s,.65f*s+.06f*c) ||
        key.center.y < .199f || key.center.y > roomCeiling(key.center.z,.65f*s+.06f*c)-.20f ||
        keyOverlapsPlayer(key.center,key.yaw,eye)) return false;
    for (const auto& part : keyParts) {
        const auto world = worldKeyPart(key.center,key.yaw,part);
        if (overlapsPortable(world.center,world.size,key.yaw)) return false;
        const auto blockedBy = [&](const Box& obstacle) { return rotatedBoxesOverlap(world,key.yaw,obstacle); };
        for (const auto& obstacle : staticInterior) if (blockedBy(obstacle)) return false;
        for (const auto& obstacle : doorShelf) if (blockedBy(obstacle)) return false;
        for (const auto& obstacle : cabinetFrame) if (blockedBy(obstacle)) return false;
        for (const auto& obstacle : cabinetDoors(cabinetSwing_)) if (blockedBy(obstacle)) return false;
        if (blockedBy(shelf) || blockedBy(monitorBody)) return false;
        if (computerCube_ && held_ != HeldItem::Cube &&
            rotatedBoxesOverlap(world,key.yaw,{computerCube_->center,glm::vec3{computerCube_->edge}},computerCube_->yaw)) return false;
    }
    return true;
}

std::optional<HomeRoom::KeyPiece> HomeRoom::carriedKey(const glm::vec3& eye, const glm::vec3& direction) const {
    KeyPiece key = *keyPiece_;
    key.verticalSpeed = 0.0f;
    const auto reachable = [&] {
        if (!keyFits(key,eye)) return false;
        const auto delta = key.center-eye;
        const float distance = glm::length(delta);
        return distance > .01f && visibleRange(eye,delta/distance,distance) >= distance-.002f;
    };
    const float visible = visibleRange(eye,direction,2.0f);
    for (int sample = 0; sample <= 24; ++sample) {
        const float distance = visible-static_cast<float>(sample)*.06f;
        if (distance < .35f) break;
        key.center = eye+direction*distance;
        key.center.y = std::clamp(key.center.y,.20f,roomCeiling(key.center.z,.66f)-.20f);
        if (reachable()) return key;
    }
    const float clearance = .65f+.25f+.08f;
    for (const float extra : {0.0f,.45f,.90f}) {
        for (int sample = 0; sample < 16; ++sample) {
            const float angle = static_cast<float>(sample)*.39269908f;
            key.center = eye+glm::vec3{std::cos(angle),0.0f,std::sin(angle)}*(clearance+extra);
            key.center.y = std::clamp(eye.y-.45f,.20f,roomCeiling(key.center.z,.66f)-.20f);
            if (reachable()) return key;
        }
    }
    return std::nullopt;
}

void HomeRoom::updateKey(float elapsed, const glm::vec3& eye, const glm::vec3& direction) {
    if (!keyPiece_) return;
    if (held_ == HeldItem::Key) {
        if (const auto pose = carriedKey(eye,direction)) keyPiece_ = *pose;
        return;
    }
    auto& key = *keyPiece_;
    const int steps = std::clamp(static_cast<int>(std::ceil(elapsed*90.0f)),1,18);
    const float step = elapsed/static_cast<float>(steps);
    for (int index = 0; index < steps; ++index) {
        key.verticalSpeed -= 9.8f*step;
        const float next = key.center.y+key.verticalSpeed*step;
        float supportCenter = .20f;
        const auto supportedBy = [&](const Box& obstacle, float yaw = 0.0f) {
            const float top = obstacle.center.y+obstacle.size.y*.5f;
            for (const auto& part : keyParts) {
                auto world = worldKeyPart(key.center,key.yaw,part);
                const float bottom = world.center.y-world.size.y*.5f;
                world.center.y = obstacle.center.y;
                world.size.y = obstacle.size.y+1.0f;
                if (bottom >= top-.005f && rotatedBoxesOverlap(world,key.yaw,obstacle,yaw))
                    supportCenter = std::max(supportCenter,top-part.center.y+part.size.y*.5f);
            }
        };
        for (const auto& obstacle : staticInterior) supportedBy(obstacle);
        for (const auto& obstacle : doorShelf) supportedBy(obstacle);
        for (const auto& obstacle : cabinetFrame) supportedBy(obstacle);
        for (const auto& obstacle : cabinetDoors(cabinetSwing_)) supportedBy(obstacle);
        supportedBy(shelf);
        supportedBy(monitorBody);
        for (int index = 0; index < static_cast<int>(portable_.size()); ++index) {
            if (!portable_[index].active || heldPortable_ == index) continue;
            for (const auto& part : portableParts(index,socketFlat_))
                supportedBy(worldKeyPart(portable_[index].center,portable_[index].yaw,part),portable_[index].yaw);
        }
        supportedBy({{eye.x,eye.y+headClearance-.005f,eye.z},{playerRadius*2.0f,.01f,playerRadius*2.0f}});
        if (computerCube_ && held_ != HeldItem::Cube)
            supportedBy({computerCube_->center,glm::vec3{computerCube_->edge}},computerCube_->yaw);
        if (next <= supportCenter) {
            key.center.y = supportCenter;
            key.verticalSpeed = 0.0f;
        } else key.center.y = next;
    }
}

float HomeRoom::visibleRange(const glm::vec3& eye, const glm::vec3& direction, float maxDistance) const {
    for (const Box& obstacle : staticInterior) {
        const auto distance = perspective::rayBoxDistance(eye, direction, obstacle.center, obstacle.size * 0.5f);
        if (distance) {
            maxDistance = std::min(maxDistance, *distance);
        }
    }
    for (const Box& obstacle : doorShelf) {
        const auto distance = perspective::rayBoxDistance(eye, direction, obstacle.center, obstacle.size * 0.5f);
        if (distance) maxDistance = std::min(maxDistance, *distance);
    }
    for (const auto& obstacle : cabinetFrame) {
        const auto hit = perspective::rayBoxDistance(eye, direction, obstacle.center, obstacle.size * .5f);
        if (hit) maxDistance = std::min(maxDistance, *hit);
    }
    for (const auto& obstacle : cabinetDoors(cabinetSwing_)) {
        const auto hit = perspective::rayBoxDistance(eye, direction, obstacle.center, obstacle.size * .5f);
        if (hit) maxDistance = std::min(maxDistance, *hit);
    }
    const auto shelfHit = perspective::rayBoxDistance(
        eye, direction, shelf.center, shelf.size * 0.5f);
    if (shelfHit) {
        maxDistance = std::min(maxDistance, *shelfHit);
    }
    if (computerCube_ && held_ != HeldItem::Cube) {
        const auto hit = cubeRay(eye, direction, computerCube_->center, computerCube_->edge, computerCube_->yaw);
        if (hit) maxDistance = std::min(maxDistance, *hit);
    }
    if (keyPiece_ && held_ != HeldItem::Key) {
        const auto hit = keyRay(eye,direction,keyPiece_->center,keyPiece_->yaw);
        if (hit) maxDistance = std::min(maxDistance,*hit);
    }
    for (int index : {socketObject,completeKeyObject}) {
        if (!portable_[index].active || heldPortable_ == index) continue;
        const auto hit = partsRay(portableParts(index,socketFlat_),eye,direction,portable_[index].center,portable_[index].yaw);
        if (hit) maxDistance = std::min(maxDistance,*hit);
    }
    return maxDistance;
}

void HomeRoom::interact(const glm::vec3& eye, const glm::vec3& direction) {
    if (finale_.isCarrying() || (cabinetOpen_ && eye.z >= finaleEntryZ && held_ == HeldItem::None)) {
        finale_.interact(eye, direction);
        return;
    }
    if (held_ == HeldItem::Portable && heldPortable_ >= 0) {
        const int index = heldPortable_;
        if (index == socketObject && !socketInstalled_ &&
            aimsAt(eye,direction,cabinetFront,visibleRange(eye,direction,reach))) {
            portable_[index].center = {3.75f,1.35f,4.94f};
            portable_[index].yaw = 0.0f;
            portable_[index].loose = false;
            socketFlat_ = false;
            socketInstalled_ = true;
            heldPortable_ = -1;
            held_ = HeldItem::None;
            return;
        }
        const Box socketTarget{portable_[socketObject].center,{.28f,.36f,.10f}};
        if (index == completeKeyObject && socketInstalled_ &&
            aimsAt(eye,direction,socketTarget,visibleRange(eye,direction,reach))) {
            portable_[index].center = portable_[socketObject].center+glm::vec3{0.0f,.011f,-.15f};
            portable_[index].yaw = glm::radians(-90.0f);
            portable_[index].loose = false;
            completeKeyInserted_ = true;
            cabinetUnlocked_ = true;
            keyTurn_ = 0.0f;
            heldPortable_ = -1;
            held_ = HeldItem::None;
            return;
        }
        const bool oldFlat = socketFlat_;
        if (index == socketObject) socketFlat_ = true;
        if (const auto pose = carriedPortable(index,eye,direction)) {
            portable_[index] = *pose;
            heldPortable_ = -1;
            held_ = HeldItem::None;
        } else if (portableFits(index,portable_[index],eye)) {
            portable_[index].loose = true;
            heldPortable_ = -1;
            held_ = HeldItem::None;
        } else socketFlat_ = oldFlat;
        return;
    }
    if (held_ == HeldItem::Cube) {
        if (const auto pose = carriedCube(eye, direction)) {
            computerCube_ = *pose;
            held_ = HeldItem::None;
        } else if (cubeFits(*computerCube_, eye)) {
            held_ = HeldItem::None;
        }
        return;
    }
    if (held_ == HeldItem::Key) {
        if (const auto pose = carriedKey(eye,direction)) {
            keyPiece_ = *pose;
            held_ = HeldItem::None;
        } else if (keyFits(*keyPiece_,eye)) held_ = HeldItem::None;
        return;
    }
    if (held_ == HeldItem::None && computerCube_) {
        const auto hit = cubeRay(eye, direction, computerCube_->center, computerCube_->edge, computerCube_->yaw);
        if (hit && *hit <= visibleRange(eye, direction, reach)) {
            held_ = HeldItem::Cube;
            return;
        }
    }
    if (held_ == HeldItem::None && keyPiece_) {
        const auto hit = keyRay(eye,direction,keyPiece_->center,keyPiece_->yaw);
        if (hit && *hit <= visibleRange(eye,direction,reach)) {
            held_ = HeldItem::Key;
            return;
        }
    }
    if (held_ == HeldItem::None) {
        if (!portable_[socketObject].active) {
            const auto hit = home_picture::hiddenKeyholeHit(eye, direction, reach);
            if (hit && *hit <= visibleRange(eye, direction, reach)) {
                if (auto pose = carriedPortable(socketObject, eye, direction)) {
                    pose->active = true;
                    portable_[socketObject] = *pose;
                    heldPortable_ = socketObject;
                    held_ = HeldItem::Portable;
                }
                return;
            }
        }
        if (const auto uv = computerUv(eye,direction)) {
            if (computer_.click(uv->x,uv->y) == ComputerScreen::Action::TakeCompleteKey &&
                !portable_[completeKeyObject].active && !completeKeyInserted_) {
                portable_[completeKeyObject].active = true;
                portable_[completeKeyObject].loose = true;
                if (const auto pose = carriedPortable(completeKeyObject,eye,direction)) {
                    portable_[completeKeyObject] = *pose;
                    heldPortable_ = completeKeyObject;
                    held_ = HeldItem::Portable;
                    computer_.consumeKey();
                    closeComputer();
                } else portable_[completeKeyObject].active = false;
            }
            return;
        }
    }
    if (held_ == HeldItem::None) grabPortable(eye, direction);
}

void HomeRoom::updateCube(float elapsed, const glm::vec3& eye, const glm::vec3& direction) {
    if (!computerCube_) return;
    if (held_ == HeldItem::Cube) {
        if (const auto pose = carriedCube(eye, direction)) computerCube_ = *pose;
        return;
    }
    auto& cube = *computerCube_;
    const int steps = std::clamp(static_cast<int>(std::ceil(elapsed * 90.0f)), 1, 18);
    const float step = elapsed / static_cast<float>(steps);
    for (int index = 0; index < steps; ++index) {
        const float bottom = cube.center.y - cube.edge * .5f;
        cube.verticalSpeed -= 9.8f * step;
        const float nextBottom = bottom + cube.verticalSpeed * step;
        float support = 0.0f;
        const auto supportedBy = [&](const Box& obstacle, float yaw = 0.0f) {
            const float top = obstacle.center.y + obstacle.size.y * .5f;
            Box verticalProbe = obstacle;
            verticalProbe.center.y = cube.center.y;
            verticalProbe.size.y = cube.edge + 2.0f;
            if (bottom >= top - .005f && rotatedBoxesOverlap({cube.center,glm::vec3{cube.edge}},cube.yaw,verticalProbe,yaw)) {
                support = std::max(support, top);
            }
        };
        for (const auto& obstacle : staticInterior) supportedBy(obstacle);
        for (const auto& obstacle : doorShelf) supportedBy(obstacle);
        for (const auto& obstacle : cabinetFrame) supportedBy(obstacle);
        for (const auto& obstacle : cabinetDoors(cabinetSwing_)) supportedBy(obstacle);
        supportedBy(shelf);
        supportedBy(monitorBody);
        for (int index = 0; index < static_cast<int>(portable_.size()); ++index) {
            if (!portable_[index].active || heldPortable_ == index) continue;
            for (const auto& part : portableParts(index,socketFlat_))
                supportedBy(worldKeyPart(portable_[index].center,portable_[index].yaw,part),portable_[index].yaw);
        }
        if (keyPiece_ && held_ != HeldItem::Key) {
            for (const auto& part : keyParts) {
                auto world = worldKeyPart(keyPiece_->center,keyPiece_->yaw,part);
                const float top = world.center.y+world.size.y*.5f;
                world.center.y = cube.center.y;
                world.size.y = cube.edge+1.0f;
                if (bottom >= top-.005f && rotatedBoxesOverlap({cube.center,glm::vec3{cube.edge}},cube.yaw,world,keyPiece_->yaw))
                    support = std::max(support,top);
            }
        }
        const float head = eye.y + headClearance;
        if (bottom >= head - .005f && cubeFootprint(eye, cube.center, cube.edge, cube.yaw)) support = std::max(support, head);
        if (nextBottom <= support) {
            cube.center.y = support + cube.edge * .5f;
            cube.verticalSpeed = 0.0f;
        } else {
            cube.center.y += cube.verticalSpeed * step;
        }
    }
}

void HomeRoom::update(float elapsed, const glm::vec3& eye, const glm::vec3& direction) {
    ending_.update(elapsed, finale_.escaped());
    if (elapsed <= 0.0f) return;
    lastEye_ = eye;
    const bool focused = held_ == HeldItem::None && computerUv(eye,direction).has_value();
    if (computerFocused_ && !focused) computer_.clearInput();
    computerFocused_ = focused;
    if (computerFocused_) computer_.update(elapsed);
    if (heldPortable_ == socketObject && !socketFlat_)
        portable_[socketObject].yaw = std::atan2(direction.x,direction.z);
    updateCube(elapsed, eye, direction);
    updateKey(elapsed, eye, direction);
    updatePortables(elapsed, eye, direction);
    updateDoors(elapsed, eye);
    if (cabinetOpen_) finale_.update(elapsed, eye, direction);
}

glm::vec3 HomeRoom::resolveMovement(const glm::vec3& previous, const glm::vec3& desired, bool* grounded) {
    if (cabinetOpen_ && (previous.z >= finaleEntryZ || desired.z >= finaleEntryZ)) {
        auto target = desired;
        if (finale_.isCarrying()) target.z = std::max(target.z, home_finale::frontZ + .025f);
        return finale_.resolveMovement(previous, target, grounded);
    }
    const auto minX = [](float z) {
        return z > secretEntryZ ? home_finale::hallCenterX - home_finale::entranceHalfWidth + playerRadius : -sideLimit;
    };
    const auto maxX = [](float z) {
        return z > secretEntryZ ? home_finale::hallCenterX + home_finale::entranceHalfWidth - playerRadius : sideLimit;
    };
    const auto blocked = [&](const glm::vec3& position) {
        if (position.x < minX(position.z) || position.x > maxX(position.z)) return true;
        if (overlapsPlayer(position, shelf)) {
            return true;
        }
        for (const auto& obstacle : staticInterior) {
            if (position.y+headClearance > obstacle.center.y-obstacle.size.y*.5f && overlapsPlayer(position, obstacle)) {
                return true;
            }
        }
        if (overlapsPlayer(position, monitorBody)) return true;
        if (!cabinetOpen_ && overlapsPlayer(position,cabinetBody)) return true;
        for (const auto& frame : cabinetFrame)
            if (position.y+headClearance > frame.center.y-frame.size.y*.5f && overlapsPlayer(position,frame)) return true;
        for (const auto& leaf : cabinetDoors(cabinetSwing_)) if (overlapsPlayer(position,leaf)) return true;
        if (keyPiece_ && held_ != HeldItem::Key) {
            const float top = keyPiece_->center.y+.20f;
            if (!(previous.y-eyeHeight >= top-.02f && desired.y <= previous.y) &&
                keyOverlapsPlayer(keyPiece_->center,keyPiece_->yaw,position)) return true;
        }
        for (int index = 0; index < static_cast<int>(portable_.size()); ++index) {
            if (!portable_[index].active || heldPortable_ == index) continue;
            for (const auto& part : portableParts(index,socketFlat_)) {
                const float top = portable_[index].center.y+part.center.y+part.size.y*.5f;
                if (previous.y-eyeHeight >= top-.02f && desired.y <= previous.y) continue;
                const std::array single{part};
                if (partsOverlapPlayer(single,portable_[index].center,portable_[index].yaw,position)) return true;
            }
        }
        if (computerCube_ && held_ != HeldItem::Cube) {
            const auto& cube = *computerCube_;
            const float top = cube.center.y + cube.edge * .5f;
            if (!(previous.y - eyeHeight >= top - .02f && desired.y <= previous.y) &&
                cubeOverlapsPlayer(cube.center, cube.edge, cube.yaw, position)) return true;
        }
        return false;
    };

    glm::vec3 position{previous.x, desired.y, previous.z};
    const glm::vec3 moveX{std::clamp(desired.x, minX(position.z), maxX(position.z)), desired.y, position.z};
    if (!blocked(moveX)) {
        position.x = moveX.x;
    }
    const glm::vec3 moveZ{position.x, desired.y, std::clamp(desired.z, backLimit, home_finale::frontZ)};
    if (!blocked(moveZ)) {
        position.z = moveZ.z;
    }
    float support = 0.0f;
    if (computerCube_ && held_ != HeldItem::Cube) {
        const auto& cube = *computerCube_;
        const float top = cube.center.y + cube.edge * .5f;
        if (cubeFootprint(position, cube.center, cube.edge, cube.yaw)) {
            if (desired.y <= previous.y && previous.y - eyeHeight >= top - .025f) support = top;
            const float bottom = cube.center.y - cube.edge * .5f;
            if (desired.y > previous.y && previous.y + headClearance <= bottom + .01f &&
                position.y + headClearance >= bottom) position.y = bottom - headClearance;
        }
    }
    if (keyPiece_ && held_ != HeldItem::Key) {
        const auto local = cubeLocal(position-keyPiece_->center,keyPiece_->yaw);
        for (const auto& part : keyParts) {
            const float x = local.x-std::clamp(local.x,part.center.x-part.size.x*.5f,part.center.x+part.size.x*.5f);
            const float z = local.z-std::clamp(local.z,part.center.z-part.size.z*.5f,part.center.z+part.size.z*.5f);
            if (x*x+z*z >= playerRadius*playerRadius) continue;
            const float top = keyPiece_->center.y+part.center.y+part.size.y*.5f;
            if (desired.y <= previous.y && previous.y-eyeHeight >= top-.025f) support = std::max(support,top);
            const float bottom = keyPiece_->center.y+part.center.y-part.size.y*.5f;
            if (desired.y > previous.y && previous.y+headClearance <= bottom+.01f &&
                position.y+headClearance >= bottom) position.y = bottom-headClearance;
        }
    }
    for (int index = 0; index < static_cast<int>(portable_.size()); ++index) {
        if (!portable_[index].active || heldPortable_ == index) continue;
        const auto local = cubeLocal(position-portable_[index].center,portable_[index].yaw);
        for (const auto& part : portableParts(index,socketFlat_)) {
            const float x = local.x-std::clamp(local.x,part.center.x-part.size.x*.5f,part.center.x+part.size.x*.5f);
            const float z = local.z-std::clamp(local.z,part.center.z-part.size.z*.5f,part.center.z+part.size.z*.5f);
            if (x*x+z*z >= playerRadius*playerRadius) continue;
            const float top = portable_[index].center.y+part.center.y+part.size.y*.5f;
            if (desired.y <= previous.y && previous.y-eyeHeight >= top-.025f) support = std::max(support,top);
            const float bottom = portable_[index].center.y+part.center.y-part.size.y*.5f;
            if (desired.y > previous.y && previous.y+headClearance <= bottom+.01f &&
                position.y+headClearance >= bottom) position.y = bottom-headClearance;
        }
    }
    const bool landed = position.y - eyeHeight <= support + .001f;
    if (landed) position.y = support + eyeHeight;
    position.y = std::min(position.y,roomCeiling(position.z,playerRadius)-headClearance);
    if (grounded) *grounded = landed;
    return position;
}

HomeRoom::ExitState HomeRoom::exitState() const {
    if (finale_.escaped()) return ExitState::Escaped;
    return finale_.exitOpen() ? ExitState::ExitOpen : ExitState::Explore;
}

void HomeRoom::drawPicture(const glm::mat4& model) const {
    using namespace home_picture;
    drawHomeMesh(renderer_, HomeMesh::PictureDoor, glm::translate(model, framePosition));
    const auto& painting = art();
    const auto first = static_cast<GLint>(bedroomMesh().vertices.size());
    renderer_.drawMovableMesh(first, painting.backdropCount, model, glm::vec3{1.0f});
    const auto pupils = glm::translate(model, gazeOffset(lastEye_));
    renderer_.drawMovableMesh(first + painting.backdropCount, painting.pupilCount, pupils, glm::vec3{1.0f});
    if (!portable_[socketObject].active) {
        const auto markStart = painting.backdropCount + painting.pupilCount;
        const auto markCount = static_cast<GLsizei>(painting.vertices.size()) - markStart;
        renderer_.drawMovableMesh(first + markStart, markCount, model, glm::vec3{1.0f});
    }
}

void HomeRoom::drawBedroom() const {
    const glm::mat4 model{1.0f};
    drawHomeMesh(renderer_, HomeMesh::Interior, model);
    drawPicture(model);
    drawHomeMesh(renderer_, HomeMesh::Shell, model);
    drawHomeMesh(renderer_, HomeMesh::IntactWall, model);
    const auto left = glm::rotate(glm::translate(model, leftHinge), swingAngle(cabinetSwing_), {0.0f, 1.0f, 0.0f});
    const auto right = glm::rotate(glm::translate(model, rightHinge), -swingAngle(cabinetSwing_), {0.0f, 1.0f, 0.0f});
    drawHomeMesh(renderer_, HomeMesh::CabinetLeftDoor, left);
    drawHomeMesh(renderer_, HomeMesh::CabinetRightDoor, right);
    drawHomeMesh(renderer_, HomeMesh::Shelf, glm::translate(model, shelf.center));
}

void HomeRoom::draw(const glm::mat4& viewProjection, const glm::vec3& eye, const glm::vec3&) const {
    renderer_.begin(viewProjection);
    if (cabinetSwing_ > 0.0f || eye.z > secretEntryZ)
        home_finale::draw(renderer_, finaleVisualFirst_, finale_, eye);
    if (eye.z > home_finale::frontZ + 7.0f) return;
    drawBedroom();
    if (portable_[socketObject].active) {
        drawHomeMesh(renderer_,HomeMesh::LockSocket,portableModel(socketObject));
    }
    if (computerCube_) {
        auto model = glm::translate(glm::mat4{1.0f}, computerCube_->center);
        model = glm::rotate(model, computerCube_->yaw, {0.0f,1.0f,0.0f});
        model = glm::scale(model, glm::vec3{computerCube_->edge});
        drawHomeMesh(renderer_,HomeMesh::Crate,model);
    }
    if (keyPiece_) {
        auto model = glm::translate(glm::mat4{1.0f},keyPiece_->center);
        model = glm::rotate(model,keyPiece_->yaw,{0.0f,1.0f,0.0f});
        computer_.drawKeyPiece(viewProjection,model);
    }
    if (portable_[completeKeyObject].active) {
        computer_.drawCompleteKey(viewProjection,portableModel(completeKeyObject));
    }
    computer_.draw(viewProjection);
}

void HomeRoom::drawEndingOverlay() const {
    const float alpha = ending_.fade();
    if (alpha <= 0.0f) return;

    const GLboolean wasDepth = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean wasBlend = glIsEnabled(GL_BLEND);
    const GLboolean wasScissor = glIsEnabled(GL_SCISSOR_TEST);
    const GLboolean wasCull = glIsEnabled(GL_CULL_FACE);
    GLboolean depthMask;
    GLint program, vertexArray, arrayBuffer, sourceRgb, destinationRgb, sourceAlpha, destinationAlpha;
    GLint equationRgb, equationAlpha;
    GLfloat blendColor[4];
    glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);
    glGetIntegerv(GL_CURRENT_PROGRAM,&program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vertexArray);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&arrayBuffer);
    glGetIntegerv(GL_BLEND_SRC_RGB,&sourceRgb);
    glGetIntegerv(GL_BLEND_DST_RGB,&destinationRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA,&sourceAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA,&destinationAlpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB,&equationRgb);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&equationAlpha);
    glGetFloatv(GL_BLEND_COLOR,blendColor);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_CONSTANT_ALPHA,GL_ONE_MINUS_CONSTANT_ALPHA);
    glBlendColor(0.0f,0.0f,0.0f,alpha);
    // Map the renderer's XZ polygon to clip-space XY for a single blended layer.
    glm::mat4 screen{1.0f};
    screen[1][1] = 0.0f;
    screen[1][2] = 1.0f;
    screen[2][1] = 1.0f;
    screen[2][2] = 0.0f;
    constexpr std::array<glm::vec2,4> fullScreen{{
        {-1.0f,-1.0f},{1.0f,-1.0f},{1.0f,1.0f},{-1.0f,1.0f},
    }};
    renderer_.begin(screen);
    renderer_.drawFlatPolygon(fullScreen,0.0f,{0.0f,0.0f,0.0f});

    glBlendColor(blendColor[0],blendColor[1],blendColor[2],blendColor[3]);
    glBlendFuncSeparate(sourceRgb,destinationRgb,sourceAlpha,destinationAlpha);
    glBlendEquationSeparate(equationRgb,equationAlpha);
    glDepthMask(depthMask);
    if (wasDepth) glEnable(GL_DEPTH_TEST);
    if (!wasBlend) glDisable(GL_BLEND);
    if (wasScissor) glEnable(GL_SCISSOR_TEST);
    if (wasCull) glEnable(GL_CULL_FACE);
    glUseProgram(program);
    glBindVertexArray(vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER,arrayBuffer);
}
