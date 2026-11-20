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
constexpr float roomMiniatureScale = 0.075f;
constexpr glm::vec3 roomMiniatureOrigin{4.65f, 1.52f, 5.65f};
constexpr float debrisDuration = 1.4f;
constexpr float projectileSpeed = 12.0f;
constexpr float impactDuration = 0.20f;
constexpr float latchOpenTravel = 0.27f;
constexpr glm::vec3 latchCenter{0.70f, 1.45f, -6.58f};
constexpr glm::vec3 plaster{0.856f, 0.837f, 0.777f};

constexpr Box desk{{2.90f, 0.50f, 0.80f}, {3.30f, 1.00f, 1.40f}};
constexpr Box shelf{{0.0f, 1.30f, -5.95f}, {2.05f, 2.60f, 0.60f}};
constexpr Box miniShelf{{4.20375f, 1.6175f, 5.65f}, {0.095f, 0.235f, 0.20375f}};
constexpr Box miniRpg{{2.85f, 0.91f, 5.55f}, {0.47f, 0.24f, 0.21f}};
constexpr Box shelvedDoor{{5.20f, 2.245f, -3.25f}, {0.31f, 0.09f, 0.48f}};
constexpr Box wallPanel{{0.0f, 1.30f, wallZ}, {1.80f, 2.60f, 0.31f}};
constexpr Box door{{0.0f, 1.30f, wallZ}, {1.76f, 2.60f, 0.105f}};
constexpr Box openDoor{{-0.85f, 1.30f, wallZ + 0.86f}, {0.105f, 2.60f, 1.76f}};
constexpr glm::vec3 wood{0.53f, 0.32f, 0.20f};
constexpr float eyeHeight = 1.65f;
constexpr float headClearance = 0.15f;
constexpr Box monitor{{5.505f, 1.85f, 3.0f}, {0.05f, 1.80f, 3.20f}};
constexpr Box monitorBody{{5.61f, 1.85f, 3.0f}, {0.20f, 2.0f, 3.40f}};
constexpr Box cabinetBody{{3.75f, 1.35f, 5.65f}, {3.60f, 2.70f, 1.28f}};
constexpr Box cabinetFront{{3.75f, 1.35f, 4.99f}, {3.60f, 2.50f, 0.08f}};
constexpr glm::vec3 leftHinge{1.95f, 1.35f, 4.99f};
constexpr glm::vec3 rightHinge{5.55f, 1.35f, 4.99f};
constexpr glm::vec3 observationEye{3.80f, 1.65f, 2.0f};
constexpr glm::vec3 goalCube{3.07f, 1.25f, 4.0f};
constexpr glm::vec3 goalKey{4.03f, 1.25f, 4.55f};
constexpr float guidePlane = 4.94f;
constexpr std::array keyParts{
    Box{{0.0f, .10f, 0.0f}, {1.30f, .20f, .12f}},
    Box{{.25f, -.10f, 0.0f}, {.13f, .20f, .12f}},
    Box{{.55f, -.10f, 0.0f}, {.13f, .20f, .12f}},
};
constexpr std::array cabinetFrame{
    Box{{1.99f,1.35f,5.65f},{.08f,2.70f,1.28f}},
    Box{{5.51f,1.35f,5.65f},{.08f,2.70f,1.28f}},
    Box{{3.75f,1.35f,6.25f},{3.44f,2.70f,.08f}},
    Box{{3.75f,2.66f,5.65f},{3.60f,.08f,1.28f}},
    Box{{3.75f,1.4665f,5.65f},{3.44f,.08f,1.20f}},
    Box{{3.75f,.58f,5.65f},{3.44f,.08f,1.20f}},
    Box{{3.75f,1.35f,5.65f},{.20f,2.70f,1.28f}},
};

constexpr std::array staticInterior{
    desk,
    Box{{-3.75f, 0.38f, 1.80f}, {2.35f, 0.76f, 3.65f}},
    Box{{-4.85f, 0.37f, -0.55f}, {0.85f, 0.74f, 0.78f}},
    Box{{4.90f, 0.71f, -3.25f}, {1.20f, 1.42f, 1.50f}},
    Box{{-3.65f, 0.48f, 4.65f}, {1.25f, 0.96f, 1.05f}},
    Box{{3.07f,.035f,4.0f},{.62f,.07f,.46f}},
    Box{{3.07f,.485f,4.0f},{.14f,.83f,.14f}},
    Box{{3.07f,.925f,4.0f},{.34f,.05f,.22f}},
    Box{{4.03f,.035f,4.55f},{.60f,.07f,.35f}},
    Box{{4.03f,.635f,4.55f},{.14f,1.13f,.14f}},
    Box{{4.03f,1.225f,4.55f},{.30f,.05f,.18f}},
};

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
    MiniBrokenWall, LatchMounts, LatchBolt, CabinetLeftDoor, CabinetRightDoor, CabinetMiniatures
};

#include "HomeGeometry.inc"

static_assert(std::size(homeMeshes) == 15);

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
    const glm::vec3 center{box.center.z, box.center.y, -box.center.x};
    const glm::vec3 size{box.size.z, box.size.y, box.size.x};
    return {roomMiniatureOrigin + center * roomMiniatureScale, size * roomMiniatureScale};
}

glm::mat4 miniatureModel() {
    auto model = glm::translate(glm::mat4{1.0f}, roomMiniatureOrigin);
    model = glm::rotate(model, glm::radians(90.0f), {0.0f, 1.0f, 0.0f});
    return glm::scale(model, glm::vec3{roomMiniatureScale});
}

std::array<Box, 2> cabinetDoors(bool open) {
    if (open) return {Box{{1.95f,1.35f,4.14f},{.08f,2.50f,1.70f}},
                      Box{{5.55f,1.35f,4.14f},{.08f,2.50f,1.70f}}};
    return {Box{{2.80f,1.35f,4.99f},{1.70f,2.50f,.08f}},
            Box{{4.70f,1.35f,4.99f},{1.70f,2.50f,.08f}}};
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

glm::vec3 guidePoint(const glm::vec3& point) {
    return observationEye + (point - observationEye) * ((guidePlane - observationEye.z) / (point.z - observationEye.z));
}

template<std::size_t Count>
std::array<glm::vec3, Count> projectedPoints(const std::array<glm::vec3, Count>& points) {
    auto result = points;
    for (auto& point : result) point = guidePoint(point);
    return result;
}

const std::vector<MeshVertex>& alignmentGuideMesh() {
    static const auto mesh = [] {
        const auto cube = projectedPoints(cubePoints(goalCube, .60f, .35f));
        std::vector<glm::vec3> points{cube.begin(), cube.end()};
        std::sort(points.begin(), points.end(), [](const auto& a, const auto& b) {
            return a.x < b.x || (a.x == b.x && a.y < b.y);
        });
        const auto turn = [](const auto& a, const auto& b, const auto& c) {
            return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
        };
        std::vector<glm::vec3> hull;
        for (const auto& point : points) {
            while (hull.size() >= 2 && turn(hull[hull.size()-2], hull.back(), point) <= 0.0f) hull.pop_back();
            hull.push_back(point);
        }
        const auto lower = hull.size();
        for (int index = static_cast<int>(points.size()) - 2; index >= 0; --index) {
            while (hull.size() > lower && turn(hull[hull.size()-2], hull.back(), points[index]) <= 0.0f) hull.pop_back();
            hull.push_back(points[index]);
        }
        hull.pop_back();
        std::vector<MeshVertex> result;
        const auto stroke = [&](const auto& a, const auto& b) {
            const auto delta = b - a;
            const glm::vec3 side = glm::normalize(glm::vec3{-delta.y, delta.x, 0.0f}) * .009f;
            constexpr glm::vec3 color{.87f,.71f,.39f};
            result.insert(result.end(), {{a-side,color},{b-side,color},{b+side,color},
                                          {a-side,color},{b+side,color},{a+side,color}});
        };
        for (std::size_t index = 0; index < hull.size(); ++index) stroke(hull[index], hull[(index+1)%hull.size()]);
        const auto key = projectedPoints(keyPoints(goalKey, 0.0f));
        for (std::size_t index = 0; index < key.size(); ++index) stroke(key[index], key[(index+1)%key.size()]);
        return result;
    }();
    return mesh;
}

bool matchingRays(std::span<const glm::vec3> actual, std::span<const glm::vec3> guide,
                  const glm::vec3& eye) {
    const auto matches = [&](auto from, auto to) {
        for (const auto& point : from) {
            const auto ray = glm::normalize(point - eye);
            bool found = false;
            for (const auto& target : to) {
                if (glm::dot(ray, glm::normalize(target - eye)) >= std::cos(.035f)) {
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }
        return true;
    };
    return matches(actual, guide) && matches(guide, actual);
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
    const auto& guide = alignmentGuideMesh();
    movable.insert(movable.end(), guide.begin(), guide.end());
    return renderer_.initialize({}, movable) && computer_.initialize();
}

bool HomeRoom::computerActive() const { return computerActive_; }
void HomeRoom::closeComputer() { computerActive_ = false; }
void HomeRoom::computerInput(float horizontal, float vertical, float turn, float elapsed) {
    if (computerActive_) computer_.input(horizontal, vertical, turn, elapsed);
}
void HomeRoom::computerSelect(int number) {
    if (computerActive_) computer_.selectItem(number);
}

bool HomeRoom::aimsAtComputer(const glm::vec3& eye, const glm::vec3& direction) const {
    return eye.x < monitor.center.x && aimsAt(eye, direction, monitor, visibleRange(eye, direction, reach));
}

bool HomeRoom::cubeFits(const ComputerCube& cube, const glm::vec3& eye) const {
    const float half = cube.edge * 0.5f;
    const float extent = half * (std::abs(std::cos(cube.yaw)) + std::abs(std::sin(cube.yaw)));
    if (std::abs(cube.center.x) + extent > 5.69f || cube.center.z + extent > 6.33f ||
        cube.center.z - extent < wallZ + 0.16f || cube.center.y < half - 0.001f ||
        cube.center.y + half > 3.22f || cubeOverlapsPlayer(cube.center, cube.edge, cube.yaw, eye)) return false;
    for (const auto& obstacle : staticInterior) {
        if (cubeOverlapsBox(cube.center, cube.edge, cube.yaw, obstacle)) return false;
    }
    for (const auto& obstacle : doorShelf) {
        if (cubeOverlapsBox(cube.center, cube.edge, cube.yaw, obstacle)) return false;
    }
    for (const auto& obstacle : cabinetFrame) {
        if (cubeOverlapsBox(cube.center, cube.edge, cube.yaw, obstacle)) return false;
    }
    for (const auto& obstacle : cabinetDoors(cabinetOpen_)) {
        if (cubeOverlapsBox(cube.center, cube.edge, cube.yaw, obstacle)) return false;
    }
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
        cube.center.y = std::clamp(cube.center.y, half, 3.22f - half);
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
            cube.center.y = std::clamp(eye.y - .45f, half, 3.22f - half);
            if (reachable()) return cube;
        }
    }
    return std::nullopt;
}

bool HomeRoom::keyFits(const KeyPiece& key, const glm::vec3& eye) const {
    const float c = std::abs(std::cos(key.yaw));
    const float s = std::abs(std::sin(key.yaw));
    if (std::abs(key.center.x) + .65f*c + .06f*s > 5.69f ||
        key.center.z + .65f*s + .06f*c > 6.33f ||
        key.center.z - .65f*s - .06f*c < wallZ + .16f ||
        key.center.y < .199f || key.center.y > 3.02f ||
        keyOverlapsPlayer(key.center,key.yaw,eye)) return false;
    for (const auto& part : keyParts) {
        const auto world = worldKeyPart(key.center,key.yaw,part);
        const auto blockedBy = [&](const Box& obstacle) { return rotatedBoxesOverlap(world,key.yaw,obstacle); };
        for (const auto& obstacle : staticInterior) if (blockedBy(obstacle)) return false;
        for (const auto& obstacle : doorShelf) if (blockedBy(obstacle)) return false;
        for (const auto& obstacle : cabinetFrame) if (blockedBy(obstacle)) return false;
        for (const auto& obstacle : cabinetDoors(cabinetOpen_)) if (blockedBy(obstacle)) return false;
        if (blockedBy(shelfAt(shelfOffset_)) || blockedBy(monitorBody)) return false;
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
        key.center.y = std::clamp(key.center.y,.20f,3.02f);
        if (reachable()) return key;
    }
    const float clearance = .65f+.25f+.08f;
    for (const float extra : {0.0f,.45f,.90f}) {
        for (int sample = 0; sample < 16; ++sample) {
            const float angle = static_cast<float>(sample)*.39269908f;
            key.center = eye+glm::vec3{std::cos(angle),0.0f,std::sin(angle)}*(clearance+extra);
            key.center.y = std::clamp(eye.y-.45f,.20f,3.02f);
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
        for (const auto& obstacle : cabinetDoors(cabinetOpen_)) supportedBy(obstacle);
        supportedBy(shelfAt(shelfOffset_));
        supportedBy(monitorBody);
        supportedBy({{eye.x,eye.y+headClearance-.005f,eye.z},{playerRadius*2.0f,.01f,playerRadius*2.0f}});
        if (computerCube_ && held_ != HeldItem::Cube)
            supportedBy({computerCube_->center,glm::vec3{computerCube_->edge}},computerCube_->yaw);
        if (next <= supportCenter) {
            key.center.y = supportCenter;
            key.verticalSpeed = 0.0f;
        } else key.center.y = next;
    }
}

void HomeRoom::updateAlignment(float elapsed, const glm::vec3& eye, const glm::vec3& direction) {
    if (cabinetUnlocked_) return;
    bool aligned = computer_.cubeSolved() && computer_.keySolved() && computerCube_ && keyPiece_ &&
                   held_ != HeldItem::Cube && held_ != HeldItem::Key &&
                   eye.z < guidePlane && direction.z > 0.0f;
    if (aligned) {
        const auto cube = cubePoints(computerCube_->center,computerCube_->edge,computerCube_->yaw);
        const auto key = keyPoints(keyPiece_->center,keyPiece_->yaw);
        const auto cubeGuide = projectedPoints(cubePoints(goalCube,.60f,.35f));
        const auto keyGuide = projectedPoints(keyPoints(goalKey,0.0f));
        const auto center = (guidePoint(goalCube)+guidePoint(goalKey))*.5f;
        aligned = std::abs(computerCube_->verticalSpeed) < .04f && std::abs(keyPiece_->verticalSpeed) < .04f &&
                  glm::dot(direction,glm::normalize(center-eye)) > std::cos(.30f) &&
                  matchingRays(cube,cubeGuide,eye) && matchingRays(key,keyGuide,eye);
    }
    alignmentTime_ = aligned ? alignmentTime_+elapsed : 0.0f;
    if (alignmentTime_ >= .60f) cabinetUnlocked_ = true;
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
    for (const auto& obstacle : cabinetDoors(cabinetOpen_)) {
        const auto hit = perspective::rayBoxDistance(eye, direction, obstacle.center, obstacle.size * .5f);
        if (hit) maxDistance = std::min(maxDistance, *hit);
    }
    const auto shelfHit = perspective::rayBoxDistance(
        eye, direction, shelfAt(shelfOffset_).center, shelf.size * 0.5f);
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

bool HomeRoom::canTakeShelvedDoor(const glm::vec3& eye, const glm::vec3& direction) const {
    if (!cabinetOpen_ || !computerCube_ || held_ != HeldItem::None || !standingOnCube_) return false;
    const auto& cube = *computerCube_;
    const float cubeTop = cube.center.y + cube.edge * .5f;
    const float visible = visibleRange(eye, direction, reach);
    if (std::abs(cube.verticalSpeed) >= .05f ||
        std::abs(eye.y - eyeHeight - cubeTop) >= .03f ||
        !cubeFootprint(eye, cube.center, cube.edge, cube.yaw) ||
        !aimsAt(eye, direction, shelvedDoor, visible)) return false;
    auto model = glm::translate(glm::mat4{1.0f}, shelvedDoor.center);
    model = glm::rotate(model, glm::radians(-90.0f), {1.0f, 0.0f, 0.0f});
    model = glm::scale(model, glm::vec3{miniatureScale});
    return visibleMeshRange(HomeMesh::Door, model, eye, direction, reach + .01f) <= visible;
}

bool HomeRoom::canReachMiniatureLatch(const glm::vec3& eye, const glm::vec3& direction) const {
    const Box latch = miniatureBox({latchCenter, {0.50f, 0.35f, 0.28f}});
    float visible = visibleRange(eye, direction, 2.3f);
    if (!cabinetOpen_ || eye.x >= latch.center.x - 0.02f || !aimsAt(eye, direction, latch, visible)) {
        return false;
    }
    const auto model = miniatureModel();
    visible = visibleMeshRange(HomeMesh::Interior,model,eye,direction,visible);
    visible = visibleMeshRange(HomeMesh::MiniWalls, model, eye, direction, visible);
    visible = visibleMeshRange(HomeMesh::MiniBrokenWall, model, eye, direction, visible);
    visible = visibleMeshRange(HomeMesh::Door, glm::translate(model, door.center), eye, direction, visible);
    visible = visibleMeshRange(HomeMesh::Shelf, glm::translate(model, shelfAt(shelfOffset_).center),
                               eye, direction, visible);
    return aimsAt(eye, direction, latch, visible);
}

bool HomeRoom::canReachMiniatureShelf(const glm::vec3& eye, const glm::vec3& direction) const {
    if (!cabinetOpen_ || !aimsAt(eye,direction,miniShelf,visibleRange(eye,direction,reach))) return false;
    const auto model = miniatureModel();
    float visible = visibleRange(eye,direction,reach);
    visible = visibleMeshRange(HomeMesh::Interior,model,eye,direction,visible);
    visible = visibleMeshRange(HomeMesh::MiniWalls,model,eye,direction,visible);
    visible = visibleMeshRange(HomeMesh::IntactWall,model,eye,direction,visible);
    const auto shelfModel = glm::translate(model,shelfAt(shelfOffset_).center);
    return visibleMeshRange(HomeMesh::Shelf,shelfModel,eye,direction,reach+.01f) <= visible;
}

bool HomeRoom::canTakeMiniatureRpg(const glm::vec3& eye, const glm::vec3& direction) const {
    if (!cabinetOpen_ || !aimsAt(eye,direction,miniRpg,visibleRange(eye,direction,reach))) return false;
    auto rack = glm::translate(glm::mat4{1.0f},{2.85f,.90f,5.65f});
    rack = glm::rotate(rack,glm::radians(180.0f),{0.0f,1.0f,0.0f});
    rack = glm::scale(rack,glm::vec3{.8f});
    float visible = visibleMeshRange(HomeMesh::Rack,rack,eye,direction,visibleRange(eye,direction,reach));
    visible = visibleMeshRange(HomeMesh::CabinetMiniatures,glm::mat4{1.0f},eye,direction,visible);
    const auto model = glm::scale(glm::translate(glm::mat4{1.0f},miniRpg.center),glm::vec3{.8f});
    return visibleMeshRange(HomeMesh::Rpg,model,eye,direction,reach+.01f) <= visible;
}

void HomeRoom::interact(const glm::vec3& eye, const glm::vec3& direction) {
    if (computerActive_) {
        const auto action = computer_.confirm();
        if (action == ComputerScreen::Action::TakeCube && !computerCube_) {
            computerCube_ = ComputerCube{glm::vec3{0.0f}, computer_.edge(), computer_.yaw()};
            if (const auto pose = carriedCube(eye, direction)) {
                computerCube_ = *pose;
                held_ = HeldItem::Cube;
                computerActive_ = false;
                computer_.setCubeVisible(false);
            } else computerCube_.reset();
        } else if (action == ComputerScreen::Action::TakeKey && !keyPiece_) {
            keyPiece_ = KeyPiece{glm::vec3{0.0f},computer_.keyYaw()};
            if (const auto pose = carriedKey(eye,direction)) {
                keyPiece_ = *pose;
                held_ = HeldItem::Key;
                computerActive_ = false;
                computer_.setKeyVisible(false);
            } else keyPiece_.reset();
        }
        return;
    }
    if (held_ == HeldItem::Cube) {
        if (aimsAtComputer(eye, direction)) {
            computer_.setCube(computerCube_->edge, computerCube_->yaw);
            computerCube_.reset();
            held_ = HeldItem::None;
        } else if (const auto pose = carriedCube(eye, direction)) {
            computerCube_ = *pose;
            held_ = HeldItem::None;
        } else if (cubeFits(*computerCube_, eye)) {
            held_ = HeldItem::None;
        }
        return;
    }
    if (held_ == HeldItem::Key) {
        if (aimsAtComputer(eye,direction)) {
            computer_.setKey(keyPiece_->yaw);
            keyPiece_.reset();
            held_ = HeldItem::None;
        } else if (const auto pose = carriedKey(eye,direction)) {
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
    if (held_ == HeldItem::None && aimsAtComputer(eye, direction)) {
        computerActive_ = true;
        return;
    }
    const float nearby = visibleRange(eye, direction, reach);
    if (held_ == HeldItem::None && cabinetUnlocked_ && !cabinetOpen_ &&
        aimsAt(eye,direction,cabinetFront,nearby)) {
        for (const auto& leaf : cabinetDoors(true)) {
            if (overlapsPlayer(eye,leaf)) return;
            if (computerCube_ && cubeOverlapsBox(computerCube_->center,computerCube_->edge,computerCube_->yaw,leaf)) return;
            if (keyPiece_) {
                for (const auto& part : keyParts) {
                    if (rotatedBoxesOverlap(worldKeyPart(keyPiece_->center,keyPiece_->yaw,part),keyPiece_->yaw,leaf)) return;
                }
            }
        }
        cabinetOpen_ = true;
        return;
    }
    switch (exitState_) {
        case ExitState::Explore:
            if (canReachMiniatureShelf(eye,direction)) {
                shelfMoving_ = true;
            }
            break;
        case ExitState::WallRevealed:
            if (canTakeMiniatureRpg(eye,direction)) {
                held_ = held_ == HeldItem::Rpg ? HeldItem::None : HeldItem::Rpg;
            }
            break;
        case ExitState::WallBroken:
            if (held_ == HeldItem::Door) {
                if (canFitDoor(eye, direction)) {
                    held_ = HeldItem::None;
                    exitState_ = ExitState::DoorPlaced;
                }
            } else if (canTakeShelvedDoor(eye, direction)) {
                held_ = HeldItem::Door;
                doorRatio_ = perspective::grabRatio(eye, direction, shelvedDoor.center, miniatureScale, door.size.y);
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
    if (!cabinetOpen_ || held_ != HeldItem::Rpg || exitState_ != ExitState::WallRevealed || projectile_) {
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
    for (const auto& obstacle : doorShelf) hit(obstacle);
    for (const auto& obstacle : cabinetFrame) hit(obstacle);
    for (const auto& obstacle : cabinetDoors(cabinetOpen_)) hit(obstacle);
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
    for (const auto& obstacle : doorShelf) hit(obstacle, false, true);
    for (const auto& obstacle : cabinetFrame) hit(obstacle,false,true);
    hit(shelfAt(shelfOffset_), false, true);
    hit({{0.0f, -0.09f, 0.0f}, {11.7f, 0.18f, 13.1f}}, false, true);
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
        const auto smallImpact = (projectile.center-roomMiniatureOrigin)/roomMiniatureScale;
        const glm::vec3 realImpact = projectile.miniature
            ? glm::vec3{-smallImpact.z,smallImpact.y,smallImpact.x} : projectile.center;
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
    if (computerActive_) {
        computer_.scroll(yOffset);
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
        const auto supportedBy = [&](const Box& obstacle) {
            const float top = obstacle.center.y + obstacle.size.y * .5f;
            Box verticalProbe = obstacle;
            verticalProbe.center.y = cube.center.y;
            verticalProbe.size.y = cube.edge + 2.0f;
            if (bottom >= top - .005f && cubeOverlapsBox(cube.center, cube.edge, cube.yaw, verticalProbe)) {
                support = std::max(support, top);
            }
        };
        for (const auto& obstacle : staticInterior) supportedBy(obstacle);
        for (const auto& obstacle : doorShelf) supportedBy(obstacle);
        for (const auto& obstacle : cabinetFrame) supportedBy(obstacle);
        for (const auto& obstacle : cabinetDoors(cabinetOpen_)) supportedBy(obstacle);
        supportedBy(shelfAt(shelfOffset_));
        supportedBy(monitorBody);
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
    if (elapsed <= 0.0f) {
        return;
    }
    updateCube(elapsed, eye, direction);
    updateKey(elapsed,eye,direction);
    updateAlignment(elapsed,eye,direction);
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
        if (!cubeBlocks && !keyBlocks && !overlapsPlayer(eye, shelfAt(nextOffset))) {
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
    const auto blocked = [&](const glm::vec3& position) {
        if (overlapsPlayer(position, shelfAt(shelfOffset_))) {
            return true;
        }
        for (const auto& obstacle : staticInterior) {
            if (overlapsPlayer(position, obstacle)) {
                return true;
            }
        }
        if (overlapsPlayer(position, monitorBody)) return true;
        if (overlapsPlayer(position,cabinetBody)) return true;
        for (const auto& leaf : cabinetDoors(cabinetOpen_)) if (overlapsPlayer(position,leaf)) return true;
        if (keyPiece_ && held_ != HeldItem::Key) {
            const float top = keyPiece_->center.y+.20f;
            if (!(previous.y-eyeHeight >= top-.02f && desired.y <= previous.y) &&
                keyOverlapsPlayer(keyPiece_->center,keyPiece_->yaw,position)) return true;
        }
        if (computerCube_ && held_ != HeldItem::Cube) {
            const auto& cube = *computerCube_;
            const float top = cube.center.y + cube.edge * .5f;
            if (!(previous.y - eyeHeight >= top - .02f && desired.y <= previous.y) &&
                cubeOverlapsPlayer(cube.center, cube.edge, cube.yaw, position)) return true;
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
    const bool landed = position.y - eyeHeight <= support + .001f;
    if (landed) position.y = support + eyeHeight;
    position.y = std::min(position.y, 3.22f - headClearance);
    if (grounded) *grounded = landed;
    standingOnCube_ = landed && support > .01f && computerCube_ && held_ != HeldItem::Cube &&
        std::abs(support - (computerCube_->center.y + computerCube_->edge * .5f)) < .01f;
    if (exitState_ == ExitState::DoorOpen && position.z < wallZ - 0.55f) {
        exitState_ = ExitState::Escaped;
    }
    return position;
}

HomeRoom::ExitState HomeRoom::exitState() const {
    return exitState_;
}

void HomeRoom::drawMiniature() const {
    if (!cabinetOpen_) return;
    drawRoom(miniatureModel(), true);
    auto rack = glm::translate(glm::mat4{1.0f},{2.85f,.90f,5.65f});
    rack = glm::rotate(rack,glm::radians(180.0f),{0.0f,1.0f,0.0f});
    rack = glm::scale(rack,glm::vec3{.8f});
    drawHomeMesh(renderer_,HomeMesh::Rack,rack);
    if ((exitState_ == ExitState::Explore || exitState_ == ExitState::WallRevealed) && held_ != HeldItem::Rpg) {
        drawRpg(renderer_, miniRpg.center, .8f);
    }
}

void HomeRoom::drawRoom(const glm::mat4& model, bool miniature) const {
    drawHomeMesh(renderer_, HomeMesh::Interior, model);
    drawHomeMesh(renderer_, miniature ? HomeMesh::MiniWalls : HomeMesh::Shell, model);
    const bool wallBroken = exitState_ >= ExitState::WallBroken;
    const HomeMesh rearWall = wallBroken
        ? (miniature ? HomeMesh::MiniBrokenWall : HomeMesh::BrokenWall) : HomeMesh::IntactWall;
    drawHomeMesh(renderer_, rearWall, model);
    if (!miniature) {
        auto left = glm::translate(model,leftHinge);
        auto right = glm::translate(model,rightHinge);
        if (cabinetOpen_) {
            left = glm::rotate(left,glm::radians(90.0f),{0.0f,1.0f,0.0f});
            right = glm::rotate(right,glm::radians(-90.0f),{0.0f,1.0f,0.0f});
            drawHomeMesh(renderer_,HomeMesh::CabinetMiniatures,model);
        }
        drawHomeMesh(renderer_,HomeMesh::CabinetLeftDoor,left);
        drawHomeMesh(renderer_,HomeMesh::CabinetRightDoor,right);
        if (!cabinetOpen_) {
            const auto first = static_cast<GLint>(fractureMesh().vertices.size()+std::size(homeGeometry));
            renderer_.drawMovableMesh(first,static_cast<GLsizei>(alignmentGuideMesh().size()),model,
                                      cabinetUnlocked_ ? glm::vec3{.60f,1.0f,.72f} : glm::vec3{1.0f});
        }
    }
    const auto shelfModel = glm::translate(model, shelfAt(shelfOffset_).center);
    drawHomeMesh(renderer_, HomeMesh::Shelf, shelfModel);
    if (exitState_ <= ExitState::WallBroken && held_ != HeldItem::Door) {
        auto storedModel = glm::translate(model, shelvedDoor.center);
        storedModel = glm::rotate(storedModel, glm::radians(-90.0f), {1.0f, 0.0f, 0.0f});
        storedModel = glm::scale(storedModel, glm::vec3{miniatureScale});
        drawHomeMesh(renderer_, HomeMesh::Door, storedModel);
    }
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
        drawHomeMesh(renderer_, HomeMesh::LatchMounts, latchModel,
                     latchReleased_ && latchTravel_ >= latchOpenTravel
                         ? glm::vec3{0.45f, 1.30f, 0.70f} : glm::vec3{1.0f});
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
    if (computerCube_) {
        auto model = glm::translate(glm::mat4{1.0f}, computerCube_->center);
        model = glm::rotate(model, computerCube_->yaw, {0.0f,1.0f,0.0f});
        model = glm::scale(model, glm::vec3{computerCube_->edge});
        computer_.drawCube(viewProjection, model);
        if (cabinetOpen_) computer_.drawCube(viewProjection, miniatureModel() * model);
    }
    if (keyPiece_) {
        auto model = glm::translate(glm::mat4{1.0f},keyPiece_->center);
        model = glm::rotate(model,keyPiece_->yaw,{0.0f,1.0f,0.0f});
        computer_.drawKeyPiece(viewProjection,model);
        if (cabinetOpen_) computer_.drawKeyPiece(viewProjection,miniatureModel()*model);
    }
    computer_.draw(viewProjection);
    computer_.drawGuide(computerActive_, held_ == HeldItem::Cube, held_ == HeldItem::Key);
}
