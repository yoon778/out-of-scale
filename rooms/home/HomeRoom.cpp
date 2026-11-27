#include "HomeRoom.h"
#include "HomeLiftVisual.h"
#include "HomePicture.h"
#include "../../project_code/Out_of_Scale/src/core/Audio.h"

#include "../shared/PerspectiveCarry.h"

#include <GL/glfw3.h>
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
constexpr float frontLimit = 16.25f;
constexpr float secretEntryZ = 6.34f;
constexpr float secretLeftLimit = -1.50f;
constexpr float secretRightLimit = 7.50f;
constexpr float backLimit = -8.25f;
constexpr float doorwayHalfWidth = 0.90f;
constexpr float wallZ = -6.50f;
constexpr float shelfParkedOffset = -2.50f;
constexpr float reach = 2.8f;
constexpr float fittingReach = 10.0f;
constexpr float miniatureScale = 0.16f;
constexpr float roomMiniatureScale = 0.075f;
constexpr float debrisDuration = 1.4f;
constexpr float projectileSpeed = 12.0f;
constexpr float impactDuration = 0.20f;
constexpr float latchOpenTravel = 0.27f;
constexpr float shelfLatchReleaseSeconds = .42f;
constexpr glm::vec3 latchCenter{0.70f, 1.45f, -6.58f};
constexpr glm::vec3 plaster{0.856f, 0.837f, 0.777f};

constexpr Box desk{{2.90f, 0.50f, 0.80f}, {3.30f, 1.00f, 1.40f}};
constexpr Box shelf{{0.0f, 1.30f, -5.95f}, {2.05f, 2.60f, 0.60f}};
constexpr Box shelvedDoor{{5.20f, 2.245f, -3.25f}, {0.31f, 0.09f, 0.48f}};
constexpr Box wallPanel{{0.0f, 1.30f, wallZ}, {1.80f, 2.60f, 0.31f}};
constexpr Box door{{0.0f, 1.30f, wallZ}, {1.76f, 2.60f, 0.105f}};
constexpr glm::vec3 exitHinge{-0.88f, 1.30f, wallZ};
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
constexpr int schoolObject = 2;
constexpr int militaryObject = 3;
constexpr int officeObject = 4;
constexpr int homeObject = 5;
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
constexpr std::array schoolParts{Box{{0,.117f,0},{1.50f,.261f,1.08f}}};
constexpr std::array militaryParts{Box{{0,.10825f,0},{1.50f,.2635f,1.08f}}};
constexpr std::array officeParts{Box{{0,.13675f,0},{1.50f,.3205f,1.08f}}};
constexpr std::array homeParts{Box{{0,.1125f,0},{1.15f,.255f,2.05f}}};

// Drawing, picking and occlusion must use the same miniature prop transforms.
glm::mat4 miniatureRackModel(const glm::mat4& militaryModel) {
    return glm::scale(glm::translate(militaryModel,{.420f,.07425f,-.363375f}),glm::vec3{roomMiniatureScale});
}

glm::mat4 miniatureRpgModel(const glm::mat4& militaryModel) {
    // Bring the grip just beyond the rack front while keeping all ray tests aligned.
    return glm::scale(glm::translate(militaryModel,{.42075f,.057f,-.344125f}),glm::vec3{roomMiniatureScale});
}

std::span<const Box> portableParts(int index, bool flatSocket) {
    switch (index) {
    case socketObject: return flatSocket ? std::span<const Box>{socketFlatParts} : std::span<const Box>{socketUprightParts};
    case completeKeyObject: return completeKeyParts;
    case schoolObject: return schoolParts;
    case militaryObject: return militaryParts;
    case officeObject: return officeParts;
    default: return homeParts;
    }
}

Box miniatureBase(int index) {
    if (index == homeObject) return {{0,.005f,0},{1.15f,.04f,2.05f}};
    if (index == schoolObject) return {{0,-.00175f,0},{1.50f,.0235f,1.08f}};
    return {{0,-.00675f,0},{1.50f,.0335f,1.08f}};
}
constexpr std::array displayTops{
    Box{{.40f,.8965f,9.30f},{2.65f,.08f,2.35f}},
    Box{{5.60f,.8865f,9.30f},{2.65f,.08f,2.35f}},
    Box{{.40f,.8865f,14.20f},{2.65f,.08f,2.35f}},
    Box{{5.60f,.8950f,14.20f},{2.65f,.08f,2.35f}},
};
const std::array<Box,23> cabinetFrame = [] {
    std::array<Box,23> boxes{};
    boxes[0]={{1.99f,1.35f,5.685f},{.08f,2.70f,1.35f}};
    boxes[1]={{5.51f,1.35f,5.685f},{.08f,2.70f,1.35f}};
    boxes[2]={{3.75f,2.66f,5.685f},{3.60f,.08f,1.35f}};
    std::size_t index=3;
    for (const auto& top : displayTops) {
        boxes[index++]=top;
        const float legHeight=top.center.y-top.size.y*.5f;
        for (const float x : {-1.18f,1.18f}) for (const float z : {-1.03f,1.03f})
            boxes[index++]={{top.center.x+x,legHeight*.5f,top.center.z+z},{.12f,legHeight,.12f}};
    }
    return boxes;
}();

constexpr std::array staticInterior{
    desk,
    Box{{-3.75f, 0.38f, 1.80f}, {2.35f, 0.76f, 3.65f}},
    Box{{-4.85f, 0.37f, -0.55f}, {0.85f, 0.74f, 0.78f}},
    Box{{4.90f, 0.71f, -3.25f}, {1.20f, 1.42f, 1.50f}},
    Box{{-3.65f, 0.48f, 4.65f}, {1.25f, 0.96f, 1.05f}},
    Box{{-1.95f,1.60f,6.51f},{7.80f,3.20f,.34f}},
    Box{{6.695f,1.60f,6.51f},{2.29f,3.20f,.34f}},
    Box{{3.75f,2.95f,6.51f},{3.60f,.50f,.34f}},
    Box{{-1.84f,1.60f,11.55f},{.18f,3.20f,10.08f}},
    Box{{7.84f,1.60f,11.55f},{.18f,3.20f,10.08f}},
    Box{{3.00f,1.60f,16.59f},{9.86f,3.20f,.18f}},
};

// Check both room footprints when an object straddles their connecting wall.
bool withinRoomXZ(const glm::vec3& center,float extentX,float extentZ) {
    const float minZ=center.z-extentZ,maxZ=center.z+extentZ;
    if (minZ<wallZ+.16f || maxZ>16.50f) return false;
    if (minZ<6.68f && (center.x-extentX< -5.69f || center.x+extentX>5.69f)) return false;
    if (maxZ>6.34f && (center.x-extentX< -1.75f || center.x+extentX>7.75f)) return false;
    return true;
}

float roomCeiling(float z,float extentZ=0.0f) {
    return z+extentZ>6.34f ? 3.20f : 3.22f;
}

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

HomeMesh portableMesh(int index) {
    switch (index) {
    case socketObject: return HomeMesh::LockSocket;
    case schoolObject: return HomeMesh::SchoolDiorama;
    case militaryObject: return HomeMesh::MilitaryDiorama;
    case officeObject: return HomeMesh::OfficeDiorama;
    default: return HomeMesh::HomeDioramaBase;
    }
}

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

Box liftDeck(float extension, float top) {
    return {home_lift::center(extension,top),home_lift::platformSize};
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
    const auto& range = homeMeshes[static_cast<std::size_t>(mesh)];
    const auto first = static_cast<GLint>(fractureMesh().vertices.size()) + range.first;
    renderer.drawMovableMesh(first, range.count, model, tint);
}

void drawRpg(const BoxRenderer& renderer, const glm::vec3& center, float scale) {
    const auto model = glm::scale(glm::translate(glm::mat4{1.0f}, center), glm::vec3{scale});
    drawHomeMesh(renderer, HomeMesh::Rpg, model);
}

void drawMiniatureShelfCues(const BoxRenderer& renderer, const glm::mat4& model, float shelfOffset) {
    const auto first = static_cast<GLint>(fractureMesh().vertices.size() +
        std::size(homeGeometry) + home_picture::art().vertices.size());
    const auto part = [&](glm::vec3 position, glm::vec3 size, glm::vec3 color) {
        renderer.drawMovableMesh(first,36,glm::scale(glm::translate(model,position),size),color);
    };
    // A worn patch stays on the front upright, backed by the existing pickable shelf.
    part(shelfAt(shelfOffset).center + glm::vec3{.965f,.05f,.306f},
         {.078f,.45f,.018f},{.68f,.50f,.31f});
    // Broken, low-contrast scuffs imply a dragged foot rather than a drawn route.
    part({-1.31f,.009f,-5.56f},{.32f,.006f,.055f},{.31f,.23f,.15f});
    part({-2.02f,.009f,-5.54f},{.46f,.006f,.040f},{.28f,.21f,.14f});
    part({-3.02f,.009f,-5.58f},{.64f,.006f,.060f},{.30f,.22f,.15f});
}

void drawDoor(const BoxRenderer& renderer, const Box& box, const glm::vec3& tint,
              const glm::mat4& parent = glm::mat4{1.0f}) {
    auto model = glm::translate(parent, box.center);
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
    const auto& painting = home_picture::art().vertices;
    movable.insert(movable.end(), painting.begin(), painting.end());
    const auto& liftCube = home_lift_visual::cubeVertices();
    movable.insert(movable.end(), liftCube.begin(), liftCube.end());
    if (!renderer_.initialize({}, movable) || !computer_.initialize()) return false;
    doorFitSoundEnabled_ = true;
    return true;
}

void HomeRoom::closeComputer() { computerFocused_ = false; computer_.clearInput(); }
void HomeRoom::computerKey(int key, int action, int mods) {
    if (computerFocused_ || action == GLFW_RELEASE) computer_.keyInput(key,action,mods);
}
void HomeRoom::computerClearInput() { computer_.clearInput(); }

glm::mat4 HomeRoom::portableModel(int index) const {
    auto model = glm::translate(glm::mat4{1.0f},portable_[index].center);
    model = glm::rotate(model,portable_[index].yaw,{0.0f,1.0f,0.0f});
    if (index == completeKeyObject && completeKeyInserted_)
        model = glm::rotate(model,swingAngle(keyTurn_),{1.0f,0.0f,0.0f});
    if (index == socketObject && socketFlat_) model = glm::rotate(model,glm::radians(90.0f),{1.0f,0.0f,0.0f});
    if (index == completeKeyObject) model = glm::scale(model,glm::vec3{.24f});
    return model;
}

glm::mat4 HomeRoom::homeModel() const {
    return glm::translate(glm::scale(portableModel(homeObject),glm::vec3{roomMiniatureScale}),{-.945f,0.0f,-3.895f});
}

void HomeRoom::updatePicture(float elapsed, const glm::vec3& eye, const glm::vec3& direction) {
    using namespace home_picture;
    if (pictureReveal_ >= 1.0f) return;
    if (pictureHold_ >= holdSeconds) {
        pictureReveal_ = std::min(1.0f, pictureReveal_ + elapsed / revealSeconds);
        pictureGlow_ = 1.0f - pictureReveal_;
        if (pictureReveal_ >= 1.0f) {
            auto& socket = portable_[socketObject];
            socket.center = plateEnd;
            socket.active = true;
        }
        return;
    }

    auto error = alignmentError(eye, direction);
    if (error) {
        const glm::vec3 towardPicture = focus - eye;
        const float distance = glm::length(towardPicture);
        const glm::vec3 ray = towardPicture / distance;
        if (portableVisibility(eye, ray, visibleRange(eye, ray, distance)) < distance - .005f) {
            error.reset();
        }
    }

    if (error && *error <= seamTolerance) {
        pictureHold_ = std::min(holdSeconds, pictureHold_ + elapsed);
    } else {
        pictureHold_ = 0.0f;
    }
    const float alignment = error ? std::clamp(1.0f - *error / .15f, 0.0f, 1.0f) : 0.0f;
    const float glow = .38f * alignment + .62f * (pictureHold_ / holdSeconds);
    pictureGlow_ = glm::mix(pictureGlow_, glow, std::min(1.0f, 9.0f * elapsed));
}

float HomeRoom::homeRay(const glm::vec3& eye, const glm::vec3& direction,
                        const glm::vec3& center, const glm::vec3& size, float limit) const {
    const auto inverse = glm::inverse(homeModel());
    const auto origin = glm::vec3{inverse*glm::vec4{eye,1.0f}};
    const auto ray = glm::vec3{inverse*glm::vec4{direction,0.0f}};
    const auto hit = perspective::rayBoxDistance(origin,ray,center,size*.5f);
    return hit ? std::min(limit,*hit) : limit;
}

float HomeRoom::portableVisibility(const glm::vec3& eye, const glm::vec3& direction, float limit, int ignore) const {
    for (int index = 0; index < static_cast<int>(portable_.size()); ++index) {
        const auto& object = portable_[index];
        if (index == ignore || !object.active || heldPortable_ == index || (index >= schoolObject && !cabinetOpen_)) continue;
        const auto broad = partsRay(portableParts(index,socketFlat_),eye,direction,object.center,object.yaw);
        if (!broad || *broad > limit) continue;
        if (index == completeKeyObject) {
            limit = std::min(limit,*broad);
            continue;
        }
        limit = visibleMeshRange(portableMesh(index),portableModel(index),eye,direction,limit);
        if (index == homeObject) {
            const auto model = homeModel();
            limit = visibleMeshRange(HomeMesh::Interior,model,eye,direction,limit);
            limit = visibleMeshRange(HomeMesh::MiniWalls,model,eye,direction,limit);
            limit = visibleMeshRange(exitState_ >= ExitState::WallBroken ? HomeMesh::MiniBrokenWall : HomeMesh::IntactWall,
                                       model,eye,direction,limit);
            limit = visibleMeshRange(HomeMesh::Shelf,glm::translate(model,shelfAt(shelfOffset_).center),eye,direction,limit);
            if (exitState_ == ExitState::DoorPlaced)
                limit = visibleMeshRange(HomeMesh::Door,glm::translate(model,door.center),eye,direction,limit);
        } else if (index == militaryObject) {
            const auto rack = miniatureRackModel(portableModel(index));
            limit = visibleMeshRange(HomeMesh::Rack,rack,eye,direction,limit);
            if (held_ != HeldItem::Rpg && exitState_ <= ExitState::WallRevealed) {
                const auto rpg = miniatureRpgModel(portableModel(index));
                limit = visibleMeshRange(HomeMesh::Rpg,rpg,eye,direction,limit);
            }
        }
    }
    return limit;
}

std::optional<glm::vec2> HomeRoom::computerUv(const glm::vec3& eye, const glm::vec3& direction) const {
    const float limit = portableVisibility(eye,direction,visibleRange(eye,direction,reach));
    return ComputerScreen::rayToUv(eye,direction,limit);
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
        if (exitState_ >= ExitState::DoorPlaced && blocked(swungBox(door,exitHinge,-swingAngle(doorSwing_)))) return false;
        if (blocked(monitorBody) || blocked(shelfAt(shelfOffset_))) return false;
        if (liftExtension_ > 0.0f && blocked(liftDeck(liftExtension_,liftTop_))) return false;
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
        if (index == ignore || !portable_[index].active || heldPortable_ == index ||
            (index >= schoolObject && !cabinetOpen_)) continue;
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
            supportedBy(shelfAt(shelfOffset_));
            if (liftExtension_ > 0.0f) supportedBy(liftDeck(liftExtension_,liftTop_));
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
    int found = -1;
    float nearest = visibleRange(eye,direction,reach);
    for (int index = 0; index < static_cast<int>(portable_.size()); ++index) {
        const auto& object = portable_[index];
        if (!object.active || (index >= schoolObject && !cabinetOpen_) ||
            (index == socketObject && socketInstalled_) || (index == completeKeyObject && completeKeyInserted_)) continue;
        float hit = reach+.01f;
        if (index == completeKeyObject || index == socketObject) {
            const auto ray = partsRay(portableParts(index,socketFlat_),eye,direction,object.center,object.yaw);
            if (ray) hit = *ray;
        } else {
            hit = visibleMeshRange(portableMesh(index),portableModel(index),eye,direction,reach+.01f);
            if (index >= schoolObject && hit <= reach) {
                const auto local = cubeLocal(eye+direction*hit-object.center,object.yaw);
                const auto base = miniatureBase(index);
                if (local.y < base.center.y-base.size.y*.5f-.004f || local.y > base.center.y+base.size.y*.5f+.004f) continue;
            }
        }
        if (hit > nearest+.001f || hit > portableVisibility(eye,direction,nearest)+.001f) continue;
        found = index;
        nearest = hit;
    }
    if (found < 0) return false;
    heldPortable_ = found;
    held_ = HeldItem::Portable;
    portable_[found].loose = true;
    if (found == socketObject) socketFlat_ = false;
    return true;
}

bool HomeRoom::swingBlocked(const glm::vec3& center, const glm::vec3& size,
                           const glm::vec3& eye, bool cabinet) const {
    const Box leaf{center,size};
    if (eye.y+headClearance > center.y-size.y*.5f &&
        eye.y-eyeHeight < center.y+size.y*.5f && overlapsPlayer(eye,leaf)) return true;
    if (computerCube_ && cubeOverlapsBox(computerCube_->center,computerCube_->edge,computerCube_->yaw,leaf)) return true;
    if (keyPiece_) {
        for (const auto& part : keyParts)
            if (rotatedBoxesOverlap(worldKeyPart(keyPiece_->center,keyPiece_->yaw,part),keyPiece_->yaw,leaf)) return true;
    }
    for (int index = 0; index < static_cast<int>(portable_.size()); ++index) {
        if (!portable_[index].active ||
            (cabinet && (index == socketObject || index == completeKeyObject))) continue;
        for (const auto& part : portableParts(index,socketFlat_))
            if (rotatedBoxesOverlap(worldKeyPart(portable_[index].center,portable_[index].yaw,part),
                                    portable_[index].yaw,leaf)) return true;
    }
    if (!cabinet) {
        for (const auto& obstacle : staticInterior)
            if (rotatedBoxesOverlap(leaf,0.0f,obstacle)) return true;
        for (const auto& obstacle : doorShelf)
            if (rotatedBoxesOverlap(leaf,0.0f,obstacle)) return true;
        if (rotatedBoxesOverlap(leaf,0.0f,shelfAt(shelfOffset_))) return true;
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
                    return swingBlocked(leaf.center,leaf.size,eye,true);
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
    if (doorOpening_ && exitState_ == ExitState::DoorPlaced) {
        const float target = std::min(1.0f,doorSwing_+elapsed);
        while (doorSwing_ < target) {
            const float next = std::min(target,doorSwing_+.01f);
            const auto leaf = swungBox(door,exitHinge,-swingAngle(next));
            if (swingBlocked(leaf.center,leaf.size,eye,false)) break;
            doorSwing_ = next;
        }
        if (doorSwing_ >= 1.0f) {
            doorOpening_ = false;
            exitState_ = ExitState::DoorOpen;
        }
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
    if (exitState_ >= ExitState::DoorPlaced &&
        cubeOverlapsBox(cube.center,cube.edge,cube.yaw,swungBox(door,exitHinge,-swingAngle(doorSwing_)))) return false;
    if (liftExtension_ > 0.0f &&
        cubeOverlapsBox(cube.center,cube.edge,cube.yaw,liftDeck(liftExtension_,liftTop_))) return false;
    if (keyPiece_ && held_ != HeldItem::Key) {
        for (const auto& part : keyParts) {
            if (rotatedBoxesOverlap({cube.center,glm::vec3{cube.edge}},cube.yaw,
                                     worldKeyPart(keyPiece_->center,keyPiece_->yaw,part),keyPiece_->yaw)) return false;
        }
    }
    return !cubeOverlapsBox(cube.center, cube.edge, cube.yaw, shelfAt(shelfOffset_)) &&
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
        if (exitState_ >= ExitState::DoorPlaced && blockedBy(swungBox(door,exitHinge,-swingAngle(doorSwing_)))) return false;
        if (blockedBy(shelfAt(shelfOffset_)) || blockedBy(monitorBody)) return false;
        if (liftExtension_ > 0.0f && blockedBy(liftDeck(liftExtension_,liftTop_))) return false;
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
        supportedBy(shelfAt(shelfOffset_));
        supportedBy(monitorBody);
        if (liftExtension_ > 0.0f) supportedBy(liftDeck(liftExtension_,liftTop_));
        for (int index = 0; index < static_cast<int>(portable_.size()); ++index) {
            if (!portable_[index].active || heldPortable_ == index || (index >= schoolObject && !cabinetOpen_)) continue;
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
        eye, direction, shelfAt(shelfOffset_).center, shelf.size * 0.5f);
    if (shelfHit) {
        maxDistance = std::min(maxDistance, *shelfHit);
    }
    if (liftExtension_ > 0.0f) {
        const auto deck = liftDeck(liftExtension_,liftTop_);
        const auto hit = perspective::rayBoxDistance(eye,direction,deck.center,deck.size * .5f);
        if (hit) maxDistance = std::min(maxDistance,*hit);
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

bool HomeRoom::canFitDoor(const glm::vec3& eye, const glm::vec3& direction) const {
    if (!aimsAt(eye, direction, door, visibleRange(eye, direction, fittingReach))) {
        return false;
    }
    const float depth = glm::dot(door.center - eye, direction);
    const float scale = doorRatio_ * doorWheelScale_ * depth;
    return scale >= 0.72f && scale <= 1.35f;
}

void HomeRoom::notifyDoorFit() {
    if (doorFitNotified_ || doorFitCooldown_ > 0.0f) return;
    doorFitNotified_ = true;
    doorFitCooldown_ = .22f;
    if (doorFitSoundEnabled_) Audio::instance().playDoorFitClick();
}

bool HomeRoom::canTakeShelvedDoor(const glm::vec3& eye, const glm::vec3& direction) const {
    if (!cabinetOpen_ || !standingOnLift_ || liftTop_ < home_lift::raisedTop - .001f ||
        shelfLatchRelease_ < 1.0f || held_ != HeldItem::None) return false;
    const float visible = visibleRange(eye, direction, reach);
    if (std::abs(eye.y - eyeHeight - liftTop_) >= .03f ||
        !home_lift::footOnDeck(eye, home_lift::center(liftExtension_,liftTop_)) ||
        !aimsAt(eye, direction, shelvedDoor, visible)) return false;
    auto model = glm::translate(glm::mat4{1.0f}, shelvedDoor.center);
    model = glm::rotate(model, glm::radians(-90.0f), {1.0f, 0.0f, 0.0f});
    model = glm::scale(model, glm::vec3{miniatureScale});
    return visibleMeshRange(HomeMesh::Door, model, eye, direction, reach + .01f) <= visible;
}

bool HomeRoom::canReachMiniatureLatch(const glm::vec3& eye, const glm::vec3& direction) const {
    if (!cabinetOpen_ || heldPortable_ == homeObject) return false;
    const auto model = homeModel();
    const auto inverse = glm::inverse(model);
    const auto localEye = glm::vec3{inverse*glm::vec4{eye,1.0f}};
    const auto localRay = glm::vec3{inverse*glm::vec4{direction,0.0f}};
    const auto latchHit = perspective::rayBoxDistance(localEye,localRay,latchCenter,{.25f,.175f,.14f});
    float visible = portableVisibility(eye,direction,visibleRange(eye,direction,2.3f),homeObject);
    if (localEye.z >= latchCenter.z-.02f/roomMiniatureScale || !latchHit || *latchHit > visible) return false;
    visible = visibleMeshRange(HomeMesh::Interior,model,eye,direction,visible);
    visible = visibleMeshRange(HomeMesh::MiniWalls, model, eye, direction, visible);
    visible = visibleMeshRange(HomeMesh::MiniBrokenWall, model, eye, direction, visible);
    visible = visibleMeshRange(HomeMesh::Door, glm::translate(model, door.center), eye, direction, visible);
    visible = visibleMeshRange(HomeMesh::Shelf, glm::translate(model, shelfAt(shelfOffset_).center),
                               eye, direction, visible);
    return *latchHit <= visible;
}

bool HomeRoom::canReachMiniatureShelf(const glm::vec3& eye, const glm::vec3& direction) const {
    if (!cabinetOpen_ || heldPortable_ == homeObject) return false;
    const auto model = homeModel();
    float visible = portableVisibility(eye,direction,visibleRange(eye,direction,reach),homeObject);
    visible = visibleMeshRange(HomeMesh::Interior,model,eye,direction,visible);
    visible = visibleMeshRange(HomeMesh::MiniWalls,model,eye,direction,visible);
    visible = visibleMeshRange(HomeMesh::IntactWall,model,eye,direction,visible);
    const auto shelfModel = glm::translate(model,shelfAt(shelfOffset_).center);
    return visibleMeshRange(HomeMesh::Shelf,shelfModel,eye,direction,reach+.01f) <= visible;
}

bool HomeRoom::canTakeMiniatureRpg(const glm::vec3& eye, const glm::vec3& direction) const {
    if (!cabinetOpen_ || heldPortable_ == militaryObject) return false;
    const auto rack = miniatureRackModel(portableModel(militaryObject));
    float visible = portableVisibility(eye,direction,visibleRange(eye,direction,reach),militaryObject);
    visible = visibleMeshRange(HomeMesh::Rack,rack,eye,direction,visible);
    visible = visibleMeshRange(HomeMesh::MilitaryDiorama,portableModel(militaryObject),eye,direction,visible);
    const auto model = miniatureRpgModel(portableModel(militaryObject));
    return visibleMeshRange(HomeMesh::Rpg,model,eye,direction,reach+.01f) <= visible;
}

bool HomeRoom::canReachLiftLever(const glm::vec3& eye, const glm::vec3& direction) const {
    if (!cabinetOpen_ || held_ != HeldItem::None || liftEnabled_) return false;
    const auto model = homeModel();
    const auto inverse = glm::inverse(model);
    const auto localEye = glm::vec3{inverse * glm::vec4{eye,1.0f}};
    const auto localRay = glm::vec3{inverse * glm::vec4{direction,0.0f}};
    const auto handle = home_lift::leverHandle(false);
    auto hit = perspective::rayBoxDistance(localEye,localRay,handle,home_lift_visual::leverGripSize*.5f);
    const auto stemInverse = glm::inverse(home_lift_visual::leverStemModel(glm::mat4{1.0f},false));
    const auto stemEye = glm::vec3{stemInverse*glm::vec4{localEye,1.0f}};
    const auto stemRay = glm::vec3{stemInverse*glm::vec4{localRay,0.0f}};
    const auto stemHit = perspective::rayBoxDistance(stemEye,stemRay,glm::vec3{0.0f},
        {home_lift_visual::leverStemWidth*.5f,glm::length(handle-home_lift::leverPivot)*.5f,
         home_lift_visual::leverStemDepth*.5f});
    if (stemHit && (!hit || *stemHit < *hit)) hit = stemHit;
    if (!hit || *hit > 2.3f) return false;
    float visible = portableVisibility(eye,direction,visibleRange(eye,direction,2.3f),homeObject);
    visible = visibleMeshRange(HomeMesh::Interior,model,eye,direction,visible);
    visible = visibleMeshRange(HomeMesh::MiniWalls,model,eye,direction,visible);
    return *hit <= visible;
}

void HomeRoom::interact(const glm::vec3& eye, const glm::vec3& direction) {
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
    if (canReachLiftLever(eye,direction)) {
        liftEnabled_ = true;
        return;
    }
    const float nearby = visibleRange(eye, direction, reach);
    switch (exitState_) {
        case ExitState::Explore:
            if (canReachMiniatureShelf(eye,direction)) {
                shelfMoving_ = true;
                return;
            }
            break;
        case ExitState::WallRevealed:
            if (canTakeMiniatureRpg(eye,direction)) {
                held_ = held_ == HeldItem::Rpg ? HeldItem::None : HeldItem::Rpg;
                return;
            }
            break;
        case ExitState::WallBroken:
            if (held_ == HeldItem::Door) {
                if (canFitDoor(eye, direction)) {
                    notifyDoorFit();
                    held_ = HeldItem::None;
                    exitState_ = ExitState::DoorPlaced;
                }
                return;
            } else if (canTakeShelvedDoor(eye, direction)) {
                held_ = HeldItem::Door;
                doorRatio_ = perspective::grabRatio(eye, direction, shelvedDoor.center, miniatureScale, door.size.y);
                doorWheelScale_ = 1.0f;
                return;
            }
            break;
        case ExitState::DoorPlaced:
            if (!latchReleased_ && canReachMiniatureLatch(eye, direction)) {
                latchReleased_ = true;
                return;
            } else if (aimsAt(eye, direction, door, nearby)) {
                if (latchTravel_ >= latchOpenTravel) {
                    doorOpening_ = true;
                } else {
                    doorRattleTime_ = 0.30f;
                }
                return;
            }
            break;
        case ExitState::DoorOpen:
        case ExitState::Escaped:
            break;
    }
    if (held_ == HeldItem::None) grabPortable(eye,direction);
}

void HomeRoom::shoot(const glm::vec3& eye, const glm::vec3& direction) {
    if (!cabinetOpen_ || held_ != HeldItem::Rpg || exitState_ != ExitState::WallRevealed || projectile_) {
        return;
    }
    shotFlashTime_ = 0.14f;
    float distance = 24.0f;
    bool breaksWall = false;
    bool miniature = false;
    // Test the complete flight segment so fast shots cannot skip thin furniture.
    const auto hit = [&](const Box& obstacle, bool breakable = false, bool inMiniature = false) {
        const auto depth = inMiniature ? homeRay(eye,direction,obstacle.center,obstacle.size,distance)
                                 : perspective::rayBoxDistance(eye,direction,obstacle.center,obstacle.size*.5f).value_or(distance);
        if (depth < distance && !(inMiniature && heldPortable_ == homeObject)) {
            distance = depth;
            breaksWall = breakable;
            miniature = inMiniature;
        }
    };
    hit(wallPanel, true);
    for (const auto& obstacle : staticInterior) {
        hit(obstacle);
    }
    for (const auto& obstacle : doorShelf) hit(obstacle);
    for (const auto& obstacle : cabinetFrame) hit(obstacle);
    for (const auto& obstacle : cabinetDoors(cabinetSwing_)) hit(obstacle);
    hit(shelfAt(shelfOffset_));
    if (liftExtension_ > 0.0f) hit(liftDeck(liftExtension_,liftTop_));
    hit({{0.0f, -0.09f, 0.0f}, {11.7f, 0.18f, 13.1f}});
    hit({{0.0f, 3.32f, 0.0f}, {11.7f, 0.20f, 13.1f}});
    // The passage is open between the partition pieces in staticInterior.
    hit({{3.00f, -.10f, 11.55f}, {9.50f, .20f, 10.08f}});
    hit({{3.00f, 3.31f, 11.55f}, {9.86f, .22f, 10.08f}});
    for (const float side : {-1.0f, 1.0f}) {
        hit({{side * 5.87f, 1.60f, 0.0f}, {0.34f, 3.20f, 13.1f}});
        hit({{side * 3.37f, 1.60f, wallZ}, {4.94f, 3.20f, 0.31f}});
    }
    hit({{0.0f, 2.90f, wallZ}, {1.80f, 0.60f, 0.31f}});

    hit(wallPanel, true, true);
    for (const auto& obstacle : staticInterior) {
        hit(obstacle, false, true);
    }
    for (const auto& obstacle : doorShelf) hit(obstacle, false, true);
    for (const auto& obstacle : cabinetFrame) hit(obstacle,false,true);
    hit(shelfAt(shelfOffset_), false, true);
    if (liftExtension_ > 0.0f) hit(liftDeck(liftExtension_,liftTop_), false, true);
    hit({{0.0f, -0.09f, 0.0f}, {11.7f, 0.18f, 13.1f}}, false, true);
    hit({{3.00f, -.10f, 11.55f}, {9.50f, .20f, 10.08f}}, false, true);
    hit({{0.0f, 0.30f, 6.51f}, {11.7f, 0.60f, 0.34f}}, false, true);
    for (const float side : {-1.0f, 1.0f}) {
        hit({{side * 5.87f, 0.30f, 0.0f}, {0.34f, 0.60f, 13.1f}}, false, true);
        hit({{side * 3.37f, 1.60f, wallZ}, {4.94f, 3.20f, 0.31f}}, false, true);
    }
    hit({{0.0f, 2.90f, wallZ}, {1.80f, 0.60f, 0.31f}}, false, true);
    if (computerCube_) {
        const auto depth = cubeRay(eye, direction, computerCube_->center, computerCube_->edge, computerCube_->yaw);
        if (depth && *depth < distance) {
            distance = *depth;
            breaksWall = false;
            miniature = false;
        }
    }
    if (keyPiece_) {
        const auto depth = keyRay(eye,direction,keyPiece_->center,keyPiece_->yaw);
        if (depth && *depth < distance) {
            distance = *depth;
            breaksWall = false;
            miniature = false;
        }
    }
    const float otherHit = portableVisibility(eye,direction,distance,homeObject);
    if (otherHit < distance-.0001f) {
        distance = otherHit;
        breaksWall = false;
        miniature = false;
    }
    const float launchDepth = std::min(distance, 0.45f);
    projectile_ = Projectile{eye + direction * launchDepth, direction, distance - launchDepth, breaksWall,
                             0.0f, miniature};
    if (miniature) projectile_->sourceImpact = glm::vec3{glm::inverse(homeModel())*glm::vec4{eye+direction*distance,1.0f}};
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
            ? projectile.sourceImpact : projectile.center;
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
    if (held_ == HeldItem::Portable && heldPortable_ >= schoolObject) {
        const auto previous = portable_[heldPortable_];
        portable_[heldPortable_].yaw = std::remainder(previous.yaw+static_cast<float>(yOffset)*glm::radians(15.0f),6.2831853f);
        if (const auto pose = carriedPortable(heldPortable_,lastEye_,lastDirection_)) portable_[heldPortable_] = *pose;
        else portable_[heldPortable_] = previous;
        return;
    }
    if (held_ == HeldItem::Door) {
        const float steps = static_cast<float>(std::clamp(yOffset, -10.0, 10.0));
        doorWheelScale_ = std::clamp(doorWheelScale_ * std::exp(-steps * 0.18f), 0.25f, 6.0f);
    }
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
        supportedBy(shelfAt(shelfOffset_));
        supportedBy(monitorBody);
        if (liftExtension_ > 0.0f) supportedBy(liftDeck(liftExtension_,liftTop_));
        for (int index = 0; index < static_cast<int>(portable_.size()); ++index) {
            if (!portable_[index].active || heldPortable_ == index || (index >= schoolObject && !cabinetOpen_)) continue;
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

void HomeRoom::updateLift(float elapsed, const glm::vec3& eye) {
    using namespace home_lift;
    previousLiftTop_ = liftTop_;
    if (!liftEnabled_) return;

    float nextExtension = liftExtension_;
    float nextTop = liftTop_;
    if (liftExtension_ < 1.0f) {
        nextExtension = std::min(1.0f, liftExtension_ + elapsed / .9f);
    } else if (standingOnLift_) {
        nextTop = std::min(raisedTop, liftTop_ + elapsed * .64f);
    } else if (!overlapsPlayer(eye,liftDeck(liftExtension_,liftTop_))) {
        nextTop = std::max(loweredTop, liftTop_ - elapsed * .72f);
    }
    if (nextExtension == liftExtension_ && nextTop == liftTop_) return;

    const auto deck = liftDeck(nextExtension,nextTop);
    const float feet = eye.y - eyeHeight;
    if (!standingOnLift_ && feet < nextTop - .01f &&
        eye.y + headClearance > nextTop - platformThickness &&
        overlapsPlayer(eye,deck)) return;
    if (computerCube_ && held_ != HeldItem::Cube &&
        cubeOverlapsBox(computerCube_->center,computerCube_->edge,computerCube_->yaw,deck)) return;
    if (keyPiece_ && held_ != HeldItem::Key)
        for (const auto& part : keyParts)
            if (rotatedBoxesOverlap(worldKeyPart(keyPiece_->center,keyPiece_->yaw,part),
                                    keyPiece_->yaw,deck)) return;
    if (overlapsPortable(deck.center,deck.size,0.0f)) return;

    liftExtension_ = nextExtension;
    liftTop_ = nextTop;
}

void HomeRoom::update(float elapsed, const glm::vec3& eye, const glm::vec3& direction) {
    ending_.update(elapsed, exitState_ == ExitState::Escaped);
    if (elapsed <= 0.0f) {
        return;
    }
    lastEye_ = eye;
    lastDirection_ = direction;
    updatePicture(elapsed, eye, direction);
    const bool focused = held_ == HeldItem::None && computerUv(eye,direction).has_value();
    if (computerFocused_ && !focused) computer_.clearInput();
    computerFocused_ = focused;
    if (computerFocused_) computer_.update(elapsed);
    if (heldPortable_ == socketObject && !socketFlat_)
        portable_[socketObject].yaw = std::atan2(direction.x,direction.z);
    updateLift(elapsed,eye);
    updateCube(elapsed, eye, direction);
    updateKey(elapsed,eye,direction);
    updatePortables(elapsed,eye,direction);
    updateDoors(elapsed,eye);
    if (exitState_ >= ExitState::WallBroken && shelfLatchRelease_ < 1.0f) {
        shelfLatchRelease_ = std::min(1.0f, shelfLatchRelease_ + elapsed / shelfLatchReleaseSeconds);
    }
    doorFitCooldown_ = std::max(0.0f, doorFitCooldown_ - elapsed);
    if (held_ == HeldItem::Door && canFitDoor(eye, direction)) {
        notifyDoorFit();
    } else {
        doorFitNotified_ = false;
    }
    if (latchReleased_) {
        latchTravel_ = std::min(latchOpenTravel, latchTravel_ + elapsed * 0.80f);
    }
    doorRattleTime_ = std::max(0.0f, doorRattleTime_ - elapsed);
    if (shelfMoving_) {
        const float nextOffset = std::max(shelfParkedOffset, shelfOffset_ - elapsed * 2.1f);
        const bool cubeBlocks = computerCube_ && held_ != HeldItem::Cube &&
            cubeOverlapsBox(computerCube_->center, computerCube_->edge, computerCube_->yaw, shelfAt(nextOffset));
        bool keyBlocks = false;
        if (keyPiece_ && held_ != HeldItem::Key) {
            for (const auto& part : keyParts)
                keyBlocks = keyBlocks || rotatedBoxesOverlap(worldKeyPart(keyPiece_->center,keyPiece_->yaw,part),keyPiece_->yaw,shelfAt(nextOffset));
        }
        const auto nextShelf = shelfAt(nextOffset);
        if (!cubeBlocks && !keyBlocks && !overlapsPortable(nextShelf.center,nextShelf.size,0.0f) &&
            !overlapsPlayer(eye, nextShelf)) {
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

glm::vec3 HomeRoom::resolveMovement(const glm::vec3& previous, const glm::vec3& desired, bool* grounded) {
    const bool open = exitState_ == ExitState::DoorOpen || exitState_ == ExitState::Escaped;
    const auto movingDoor = swungBox(door,exitHinge,-swingAngle(doorSwing_));
    const auto deck = liftDeck(liftExtension_,liftTop_);
    const bool liftPresent = liftExtension_ > 0.0f;
    const bool descending = desired.y <= previous.y;
    const float previousFeet = previous.y - eyeHeight;
    // The prior deck height is a support only for a rider who stays on it.
    const bool riding = standingOnLift_ && descending &&
        std::abs(previousFeet - previousLiftTop_) < .03f && home_lift::footOnDeck(previous,deck.center);
    const bool stepOntoDeck = liftExtension_ >= 1.0f && descending &&
        liftTop_ <= home_lift::loweredTop + .001f && previousFeet >= -.025f;
    const bool canLandOnDeck = descending && (previousFeet >= liftTop_ - .025f || riding);
    const auto deckClearance = [&](const glm::vec3& position) {
        return std::max(std::abs(position.x - deck.center.x) - deck.size.x * .5f,
                        std::abs(position.z - deck.center.z) - deck.size.z * .5f) - playerRadius;
    };
    const float previousDeckClearance = deckClearance(previous);
    const auto blocked = [&](const glm::vec3& position) {
        const float left=position.z>secretEntryZ ? secretLeftLimit : -sideLimit;
        const float right=position.z>secretEntryZ ? secretRightLimit : sideLimit;
        if (position.x<left || position.x>right) return true;
        if (overlapsPlayer(position, shelfAt(shelfOffset_))) {
            return true;
        }
        if (liftPresent && !stepOntoDeck && !canLandOnDeck &&
            position.y - eyeHeight < liftTop_ - .001f &&
            position.y + headClearance > liftTop_ - home_lift::platformThickness) {
            const float clearance = deckClearance(position);
            // After falling just past the edge, the body can still brush the deck side.
            if (clearance < 0.0f && !(previousDeckClearance < 0.0f && clearance > previousDeckClearance))
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
            if (!portable_[index].active || heldPortable_ == index || (index >= schoolObject && !cabinetOpen_)) continue;
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
        if (position.z <= wallZ + wallPanel.size.z * 0.5f + playerRadius) {
            return !open || std::abs(position.x) + playerRadius >= doorwayHalfWidth ||
                   overlapsPlayer(position, movingDoor);
        }
        return exitState_ >= ExitState::DoorPlaced && overlapsPlayer(position, movingDoor);
    };

    glm::vec3 position{previous.x, desired.y, previous.z};
    const float left=position.z>secretEntryZ ? secretLeftLimit : -sideLimit;
    const float right=position.z>secretEntryZ ? secretRightLimit : sideLimit;
    const glm::vec3 moveX{std::clamp(desired.x,left,right), desired.y, position.z};
    if (!blocked(moveX)) {
        position.x = moveX.x;
    }
    const glm::vec3 moveZ{position.x, desired.y, std::clamp(desired.z, backLimit, frontLimit)};
    if (!blocked(moveZ)) {
        position.z = moveZ.z;
    }
    float support = 0.0f;
    const bool onDeck = liftPresent && (stepOntoDeck || canLandOnDeck) &&
        home_lift::footOnDeck(position,deck.center);
    if (onDeck) support = liftTop_;
    if (liftPresent && !descending && overlapsPlayer(position,deck)) {
        const float bottom = liftTop_ - home_lift::platformThickness;
        if (previous.y + headClearance <= bottom + .01f && position.y + headClearance >= bottom)
            position.y = bottom - headClearance;
    }
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
        if (!portable_[index].active || heldPortable_ == index || (index >= schoolObject && !cabinetOpen_)) continue;
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
    standingOnLift_ = landed && onDeck && liftExtension_ >= 1.0f &&
        std::abs(support - liftTop_) < .001f;
    if (exitState_ == ExitState::DoorOpen && position.z < wallZ - 0.55f) {
        exitState_ = ExitState::Escaped;
    }
    return position;
}

HomeRoom::ExitState HomeRoom::exitState() const {
    return exitState_;
}

void HomeRoom::drawMiniature() const {
    if (cabinetSwing_ <= 0.0f) return;
    for (int index = schoolObject; index <= homeObject; ++index) {
        if (!portable_[index].active) continue;
        drawHomeMesh(renderer_,portableMesh(index),portableModel(index));
    }
    drawRoom(homeModel(), true);
    drawMiniatureShelfCues(renderer_,homeModel(),shelfOffset_);
    const auto rack = miniatureRackModel(portableModel(militaryObject));
    drawHomeMesh(renderer_,HomeMesh::Rack,rack);
    if ((exitState_ == ExitState::Explore || exitState_ == ExitState::WallRevealed) && held_ != HeldItem::Rpg) {
        const auto rpg = miniatureRpgModel(portableModel(militaryObject));
        drawHomeMesh(renderer_,HomeMesh::Rpg,rpg,{1.12f,1.09f,1.0f});
    }
}

void HomeRoom::drawPicture(const glm::mat4& model) const {
    using namespace home_picture;
    drawHomeMesh(renderer_, HomeMesh::PictureDoor, glm::translate(model, framePosition));
    const auto& painting = art();
    const auto first = static_cast<GLint>(fractureMesh().vertices.size() + std::size(homeGeometry));
    const auto outlineCount = static_cast<GLsizei>(painting.vertices.size()) - painting.outlineFirst;
    renderer_.drawMovableMesh(first, painting.outlineFirst, model, glm::vec3{1.0f});
    const glm::vec3 ink{.65f, .52f, .36f};
    const glm::vec3 glint{.97f, .86f, .62f};
    renderer_.drawMovableMesh(first + painting.outlineFirst, outlineCount, model,
                              glm::mix(ink, glint, pictureGlow_));

    if (pictureReveal_ > 0.0f && pictureReveal_ < 1.0f) {
        const float progress = glm::smoothstep(0.0f, 1.0f, pictureReveal_);
        const auto pose = glm::translate(model, glm::mix(plateStart, plateEnd, progress));
        drawHomeMesh(renderer_, HomeMesh::LockSocket,
                     glm::scale(pose, glm::vec3{glm::mix(.12f, 1.0f, progress)}));
    }
}

void HomeRoom::drawRoom(const glm::mat4& model, bool miniature) const {
    drawHomeMesh(renderer_, HomeMesh::Interior, model);
    drawPicture(model);
    const auto liftFirst = static_cast<GLint>(fractureMesh().vertices.size() +
        std::size(homeGeometry) + home_picture::art().vertices.size());
    home_lift_visual::draw(renderer_,liftFirst,model,liftExtension_,liftTop_,liftEnabled_,miniature);
    drawHomeMesh(renderer_, miniature ? HomeMesh::MiniWalls : HomeMesh::Shell, model);
    const bool wallBroken = exitState_ >= ExitState::WallBroken;
    const HomeMesh rearWall = wallBroken
        ? (miniature ? HomeMesh::MiniBrokenWall : HomeMesh::BrokenWall) : HomeMesh::IntactWall;
    drawHomeMesh(renderer_, rearWall, model);
    if (!miniature) {
        auto left = glm::translate(model,leftHinge);
        auto right = glm::translate(model,rightHinge);
        left = glm::rotate(left,swingAngle(cabinetSwing_),{0.0f,1.0f,0.0f});
        right = glm::rotate(right,-swingAngle(cabinetSwing_),{0.0f,1.0f,0.0f});
        if (cabinetSwing_ > 0.0f) {
            drawHomeMesh(renderer_,HomeMesh::CabinetMiniatures,model);
        }
        drawHomeMesh(renderer_,HomeMesh::CabinetLeftDoor,left);
        drawHomeMesh(renderer_,HomeMesh::CabinetRightDoor,right);

    }
    const auto shelfModel = glm::translate(model, shelfAt(shelfOffset_).center);
    drawHomeMesh(renderer_, HomeMesh::Shelf, shelfModel);
    if (exitState_ <= ExitState::WallBroken && held_ != HeldItem::Door) {
        auto storedModel = glm::translate(model, shelvedDoor.center);
        storedModel = glm::rotate(storedModel, glm::radians(-90.0f), {1.0f, 0.0f, 0.0f});
        storedModel = glm::scale(storedModel, glm::vec3{miniatureScale});
        drawHomeMesh(renderer_, HomeMesh::Door, storedModel);

        const auto latchPart = [&](glm::vec3 center, glm::vec3 size, glm::vec3 tint) {
            renderer_.drawMovableMesh(liftFirst, 36, glm::scale(glm::translate(model, center), size), tint);
        };
        const float retract = .46f * glm::smoothstep(0.0f, 1.0f, shelfLatchRelease_);
        latchPart({4.98f, 2.245f, -3.69f}, {.14f, .09f, .24f}, {.31f, .26f, .20f});
        latchPart({4.99f, 2.30f, -3.28f - retract}, {.08f, .06f, .54f}, {.61f, .47f, .25f});
        latchPart({4.94f, 2.32f, -3.09f - retract}, {.08f, .065f, .08f}, {.77f, .62f, .36f});
    }
    const float rattle = exitState_ == ExitState::DoorPlaced && !doorOpening_
        ? std::sin(doorRattleTime_ * 55.0f) * doorRattleTime_ * 0.04f : 0.0f;
    const auto doorParent = model * hingeModel(exitHinge,-swingAngle(doorSwing_)+rattle);
    if (exitState_ >= ExitState::DoorPlaced) {
        drawDoor(renderer_, door, glm::vec3{1.0f}, doorParent);
    }
    if (doorSwing_ > 0.0f) {
        const auto outside = [&](const glm::vec3& center, const glm::vec3& size, const glm::vec3& color) {
            const auto exteriorModel = glm::scale(glm::translate(model, center), size);
            renderer_.drawMovableMesh(liftFirst,36,exteriorModel,color);
        };
        // The distant morning fills the opening while the door rotates out of it.
        outside({0.0f,1.35f,-8.40f},{3.60f,2.70f,.025f},{.86f,.93f,1.0f});
        outside({0.0f,.92f,-8.36f},{3.60f,.85f,.025f},{1.0f,.90f,.68f});
        outside({-.95f,.58f,-8.31f},{1.70f,.75f,.025f},{.54f,.69f,.55f});
        outside({1.02f,.51f,-8.30f},{1.60f,.62f,.025f},{.65f,.76f,.56f});
        outside({.14f,.22f,-8.26f},{.78f,.44f,.025f},{1.0f,.92f,.71f});
        outside({0.0f,.005f,-7.40f},{1.78f,.01f,2.05f},{.95f,.88f,.67f});
        outside({.52f,1.85f,-8.24f},{.25f,.25f,.025f},{1.0f,.98f,.82f});
    }
    if (exitState_ >= ExitState::DoorPlaced) {
        auto latchModel = glm::translate(doorParent, latchCenter);
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
    if (portable_[socketObject].active) {
        drawHomeMesh(renderer_,HomeMesh::LockSocket,portableModel(socketObject));
        if (cabinetOpen_) drawHomeMesh(renderer_,HomeMesh::LockSocket,homeModel()*portableModel(socketObject));
    }
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
        // The source RPG now uses F2 meter dimensions; retain its hand-view size.
        drawRpg(renderer_, center, 0.37f);
        if (shotFlashTime_ > 0.0f) {
            renderer_.drawBox(center + direction * 0.30f, {0.13f, 0.13f, 0.13f}, {1.0f, 0.79f, 0.30f});
        }
    } else if (held_ == HeldItem::Door) {
        const glm::vec3 center = eye + direction * 1.15f + carrySide(direction) * 0.32f - glm::vec3{0.0f, 0.23f, 0.0f};
        const float scale = std::clamp(doorRatio_ * doorWheelScale_ * 1.15f, 0.08f, 0.32f);
        drawDoor(renderer_, {center, door.size * scale}, glm::vec3{1.0f});
    }
    if (computerCube_) {
        auto model = glm::translate(glm::mat4{1.0f}, computerCube_->center);
        model = glm::rotate(model, computerCube_->yaw, {0.0f,1.0f,0.0f});
        model = glm::scale(model, glm::vec3{computerCube_->edge});
        drawHomeMesh(renderer_,HomeMesh::Crate,model);
        if (cabinetOpen_) drawHomeMesh(renderer_,HomeMesh::Crate,homeModel()*model);
    }
    if (keyPiece_) {
        auto model = glm::translate(glm::mat4{1.0f},keyPiece_->center);
        model = glm::rotate(model,keyPiece_->yaw,{0.0f,1.0f,0.0f});
        computer_.drawKeyPiece(viewProjection,model);
        if (cabinetOpen_) computer_.drawKeyPiece(viewProjection,homeModel()*model);
    }
    if (portable_[completeKeyObject].active) {
        computer_.drawCompleteKey(viewProjection,portableModel(completeKeyObject));
        if (cabinetOpen_) computer_.drawCompleteKey(viewProjection,homeModel()*portableModel(completeKeyObject));
    }
    computer_.draw(viewProjection);
    if (doorSwing_ > 0.0f) {
        // Thin translucent trapezoids soften the daylight over the existing floor.
        const GLboolean wasBlending = glIsEnabled(GL_BLEND);
        GLboolean depthMask;
        GLint sourceRgb, destinationRgb, sourceAlpha, destinationAlpha;
        GLfloat blendColor[4];
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
        glGetIntegerv(GL_BLEND_SRC_RGB,&sourceRgb);
        glGetIntegerv(GL_BLEND_DST_RGB,&destinationRgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA,&sourceAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA,&destinationAlpha);
        glGetFloatv(GL_BLEND_COLOR,blendColor);
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);
        glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
        renderer_.begin(viewProjection);
        for (int band = 0; band < 4; ++band) {
            const float reach = 3.50f - .45f * static_cast<float>(band);
            const float width = 1.80f - .23f * static_cast<float>(band);
            const std::array<glm::vec2,4> light{{
                {-.86f,wallZ+.18f},{.86f,wallZ+.18f},
                {width,wallZ+reach},{-width,wallZ+reach},
            }};
            glBlendColor(0.0f,0.0f,0.0f,.09f*doorSwing_);
            renderer_.drawFlatPolygon(light,.012f+.001f*static_cast<float>(band),{1.0f,.83f,.48f});
        }
        glBlendColor(blendColor[0],blendColor[1],blendColor[2],blendColor[3]);
        glBlendFuncSeparate(sourceRgb,destinationRgb,sourceAlpha,destinationAlpha);
        glDepthMask(depthMask);
        if (!wasBlending) glDisable(GL_BLEND);
    }
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
