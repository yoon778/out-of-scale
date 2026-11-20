#include "SchoolRoom.h"
#include "SchoolRenderer.h"
#include <GL/glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace school {
namespace {
constexpr float galleryRailHeight = 1.05f;
constexpr float galleryRailPostY = SchoolRoom::landingHeight + galleryRailHeight * .5f;
constexpr glm::vec3 gallerySideRail{2.10f, SchoolRoom::landingHeight + galleryRailHeight, -4.38f};
constexpr glm::vec3 gallerySideRailSize{.075f, .075f, 1.24f};
constexpr glm::vec3 galleryGateSize{1.86f, .075f, .075f};
glm::mat4 fitReflection(const glm::mat4& viewProjection, glm::vec3 glass, glm::vec3 right, glm::vec3 up) {
    glm::vec2 low{1.f}, high{-1.f};
    for (float x : {-1.f, 1.f})
        for (float y : {-1.f, 1.f}) {
            const auto clip = viewProjection * glm::vec4(glass + right * x + up * y, 1);
            if (clip.w <= .05f)
                return viewProjection; // Glass crosses the near plane. Keep the full view.
            const auto point = glm::vec2(clip) / clip.w;
            low = glm::min(low, point);
            high = glm::max(high, point);
        }
    low = glm::max(low - glm::vec2(.012f), glm::vec2(-1.015f));
    high = glm::min(high + glm::vec2(.012f), glm::vec2(1.015f));
    const auto extent = high - low;
    if (extent.x < .001f || extent.y < .001f)
        return viewProjection;
    // Give the mirror's visible area the whole reflection texture rather than a small crop of it.
    glm::mat4 crop(1);
    crop[0][0] = 2 / extent.x;
    crop[1][1] = 2 / extent.y;
    crop[3][0] = -(high.x + low.x) / extent.x;
    crop[3][1] = -(high.y + low.y) / extent.y;
    return crop * viewProjection;
}
constexpr float boardHalf = .695f;
constexpr float boardTravel = .13f;
constexpr float boardMoveSeconds = .24f;
constexpr std::array<float, 3> deskColumns{-3.05f, -0.95f, 1.15f};
constexpr std::array<float, 4> deskRows{0.65f, 2.55f, 4.45f, 6.35f};
struct ChairPose {
    glm::vec3 center;
    float yaw;
};
ChairPose chairPose(int seat, float x, float z) {
    const float pull = seat == 4 ? 0.20f : (seat == 7 ? 0.18f : (seat == 10 ? 0.18f : 0));
    const float shift = seat == 4 ? -0.08f : (seat == 7 ? 0.09f : 0);
    const float yaw = seat == 4 ? -0.12f : (seat == 7 ? 0.10f : (seat == 10 ? -0.08f : 0));
    return {{x + shift, 0.47f, z + 0.52f + pull}, yaw};
}
struct Fixture {
    glm::vec3 center, size;
};
constexpr Fixture artSuppliesCabinet{{SchoolRoom::notebookRest.x, .44f, SchoolRoom::notebookRest.z},
                                     {.65f, .88f, 2.00f}};
// Floor furnishings share their placement with collision; small tabletop details stay within them.
constexpr std::array<Fixture, 9> fixtures{{
    {{-3.45f, 0.70f, 8.58f}, {4.70f, 1.40f, 0.72f}},
    {{5.98f, 1.08f, 6.15f}, {0.70f, 2.16f, 2.25f}},
    {{0.65f, 0.27f, 8.35f}, {2.00f, 0.54f, 0.62f}},
    artSuppliesCabinet,
    {{-5.95f, 0.38f, 7.80f}, {0.45f, 0.76f, 0.45f}},
    {{5.90f, 0.30f, 8.25f}, {0.46f, 0.60f, 0.46f}},
    {{-5.98f, 1.00f, -3.20f}, {0.72f, 2.00f, 0.95f}},
    {{5.95f, 0.40f, -0.80f}, {0.46f, 0.80f, 0.46f}},
    {{4.80f, 0.40f, 5.00f}, {0.82f, 0.80f, 0.62f}},
}};
constexpr glm::vec3 chalkWhite{0.96f, 0.95f, 0.80f};
constexpr glm::vec3 timber{0.52f, 0.32f, 0.16f};
constexpr glm::vec3 mint{0.32f, 0.46f, 0.38f};
constexpr glm::vec3 cream{0.88f, 0.84f, 0.71f};
constexpr glm::vec3 brass{0.75f, 0.55f, 0.25f};
constexpr glm::vec3 notebookSize{.42f, .32f, .064f};
constexpr glm::vec3 notebookCover{.52f, .20f, .18f};
constexpr Fixture notebookStand{{-6.20f, 1.148f, SchoolRoom::notebookStart.z}, {.49f, .036f, .74f}};
constexpr float readableNotebookScale = 2.65f;
constexpr float maxNotebookScale = 5.5f;
constexpr float minNotebookScale = .30f;
constexpr float notebookHalfHeight = notebookSize.y * .5f;
constexpr const wchar_t* notebookClues[]{
    L"숫자 1~4를 하나씩",
    L"둘째는 4, 셋째는 1",
    L"넷째는 첫째보다 크게",
    L"위쪽 숫자를 보고 맞춰요",
};
constexpr const wchar_t* notebookPanelLabels[]{L"?", L"4", L"1", L"?"};
constexpr const wchar_t* notebookControls[]{
    L"누른 판의 숫자 +1",
    L"오른쪽 판의 숫자 +2",
    L"끝판 다음은 첫째 판",
    L"4 다음은 다시 1",
};
constexpr float mirrorStartYaw = .10f;
constexpr float mirrorMaxYaw = 1.32f;
constexpr float mirrorHalfWidth = .683f, mirrorHalfHeight = .965f;
constexpr float maxStandNotebookScale = 3.60f;
bool onReflectionStand(const perspective::Pose& pose) {
    const auto rest = SchoolRoom::notebookRest;
    return std::abs(pose.center.x - rest.x) < .002f && std::abs(pose.center.z - rest.z) < .002f &&
           std::abs(pose.center.y - pose.scale * notebookHalfHeight - rest.y) < .004f;
}
glm::vec3 notebookNormal(glm::vec3 center) {
    auto toward = SchoolRoom::mirrorCenter - center;
    toward.y = 0;
    return glm::dot(toward, toward) > .01f ? glm::normalize(toward) : glm::vec3(0, 0, -1);
}
struct NotebookAxes {
    glm::vec3 right, up, normal;
};
NotebookAxes notebookAxes(glm::vec3 center, bool onStand) {
    const auto facing = onStand ? glm::normalize(glm::vec3(1, 0, .23f)) : notebookNormal(center);
    const glm::vec3 right{facing.z, 0, -facing.x};
    // Lean back into the wooden display stand. Floor placement keeps the readable page vertical.
    constexpr glm::vec3 worldUp{0, 1, 0};
    return onStand ? NotebookAxes{right, worldUp * .94f - facing * .342f, facing * .94f + worldUp * .342f}
                   : NotebookAxes{right, worldUp, facing};
}
Box bounds(glm::vec3 c, glm::vec3 s) {
    return {c - s / 2.0f, c + s / 2.0f};
}
float ceilingAt(float z) {
    return z < -8.3f ? 5.1f : (z < -5.f ? 4.325f : 4.5f);
}
bool overlapsXZ(const Box& a, const Box& b) {
    return a.min.x < b.max.x && a.max.x > b.min.x && a.min.z < b.max.z && a.max.z > b.min.z;
}
bool footprint(glm::vec3 p, const Box& b, float radius) {
    const float dx = p.x - std::clamp(p.x, b.min.x, b.max.x);
    const float dz = p.z - std::clamp(p.z, b.min.z, b.max.z);
    return dx * dx + dz * dz < radius * radius;
}
bool overlap(glm::vec3 p, const Box& b) {
    return footprint(p, b, SchoolRoom::radius) && p.y + SchoolRoom::bodyHeight > b.min.y + 0.002f &&
           p.y < b.max.y - 0.002f;
}
float boardZ(int index) {
    return -4.68f + index * 0.07f;
}
glm::mat3 projectorBasis(float yaw, float pitch) {
    glm::vec3 forward{std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch)};
    glm::vec3 right{std::cos(yaw), 0, std::sin(yaw)};
    return {right, glm::cross(right, forward), forward};
}
struct ProjectionSurface {
    int axis;
    float coordinate;
    glm::vec3 low, high;
};
const std::vector<ProjectionSurface>& projectionSurfaces() {
    // The same receiving planes serve the light, slide strokes and puzzle checks.
    static const std::vector<ProjectionSurface> surfaces = [] {
        std::vector<ProjectionSurface> result;
        for (int i = 0; i < static_cast<int>(SchoolRoom::boardX.size()); ++i) {
            const float x = SchoolRoom::boardX[i];
            result.push_back({2, boardZ(i) + .055f, {x - boardHalf, 1.26f, 0}, {x + boardHalf, 3.18f, 0}});
        }
        result.push_back({1, 0.012f, {-6.5f, 0, -5}, {6.5f, 0, 9}});
        result.push_back({2, -3.777f, {2.1f, 0, 0}, {6.5f, 1.8f, 0}});
        result.push_back({2, -4.94f, {-6.5f, 0, 0}, {2.4f, 4.5f, 0}});
        result.push_back({2, -4.94f, {3.7f, 0, 0}, {6.5f, 4.5f, 0}});
        result.push_back({2, -4.94f, {2.4f, 3.94f, 0}, {3.7f, 4.5f, 0}});
        result.push_back({2, -2.89f, {0.45f, 0, 0}, {1.55f, 1.04f, 0}});
        result.push_back({0, 1.56f, {0, 0, -3.6f}, {0, 1.04f, -2.9f}});
        result.push_back({1, 1.045f, {0.45f, 0, -3.6f}, {1.55f, 0, -2.9f}});
        result.push_back({1, 4.5f, {-6.5f, 0, -5}, {6.5f, 0, 9}});
        result.push_back({0, -6.5f, {0, 0, -5}, {0, 4.5f, 9}});
        result.push_back({0, 6.5f, {0, 0, -5}, {0, 4.5f, 9}});
        result.push_back({2, -6.985f, {2.3f, 1.8f, 0}, {3.8f, 4.3f, 0}});
        return result;
    }();
    return surfaces;
}
void plant(Renderer& r, glm::vec3 base, float size) {
    r.box(base + glm::vec3(0, 0.15f, 0) * size, glm::vec3(0.34f, 0.30f, 0.34f) * size, {0.57f, 0.31f, 0.20f});
    r.box(base + glm::vec3(0, 0.30f, 0) * size, glm::vec3(0.38f, 0.06f, 0.38f) * size, {0.66f, 0.39f, 0.25f});
    for (int leaf = 0; leaf < 7; ++leaf) {
        float angle = leaf * 2.4f;
        glm::vec3 tip =
            base +
            glm::vec3(std::cos(angle) * 0.26f, 0.50f + (leaf % 3) * 0.16f, std::sin(angle) * 0.26f) * size;
        r.line(base + glm::vec3(0, 0.30f, 0) * size, tip, 0.018f * size, {0.27f, 0.40f, 0.22f});
        r.box(tip, glm::vec3(0.26f, 0.055f, 0.12f) * size, {0.32f + 0.04f * (leaf % 2), 0.49f, 0.26f}, angle);
    }
}
void bookStack(Renderer& r, glm::vec3 base, int count) {
    for (int i = 0; i < count; ++i) {
        glm::vec3 p = base + glm::vec3(0.015f * (i % 2), 0.03f + i * 0.065f, 0);
        r.box(p, {0.36f, 0.06f, 0.26f}, i % 2 ? mint : glm::vec3(0.55f, 0.29f, 0.23f), 0.07f * i);
        r.box(p + glm::vec3(0, 0.002f, 0.006f), {0.32f, 0.038f, 0.25f}, cream, 0.07f * i);
    }
}
} // namespace
glm::vec3 Camera::direction() const {
    return {std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch)};
}
void Camera::face(glm::vec3 target) {
    focusTime = 0;
    glm::vec3 d = glm::normalize(target - eye());
    yaw = std::atan2(d.x, -d.z);
    pitch = std::asin(d.y);
}
void Camera::focusOn(glm::vec3 target) {
    const auto d = glm::normalize(target - eye());
    focusStart = {yaw, pitch};
    focusDelta = {std::remainder(std::atan2(d.x, -d.z) - yaw, 6.283185307f), std::asin(d.y) - pitch};
    focusTime = .20f;
}
void Camera::updateFocus(float dt) {
    if (focusTime <= 0 || dt <= 0)
        return;
    focusTime = std::max(0.f, focusTime - dt);
    const auto angles = focusStart + focusDelta * glm::smoothstep(0.f, 1.f, 1.f - focusTime / .20f);
    yaw = angles.x;
    pitch = angles.y;
}
SchoolRoom::SchoolRoom() {
    reset();
    makeChalk();
    slideBounds_ = slideBounds();
    makeSlideMask();
}
void SchoolRoom::reset() {
    corridor_.reset();
    stairsReady_ = escaped_ = false;
    alignmentHeld_ = formation_ = 0;
    materializing_ = false;
    time_ = 0;
    auto direction = glm::normalize(projectorStartAim - projectorOrigin);
    projectorYaw_ = std::atan2(direction.x, -direction.z);
    projectorPitch_ = std::asin(direction.y);
    adjustingProjector_ = projectorAligned_ = false;
    boardPositions_ = boardStart;
    for (int i = 0; i < static_cast<int>(boardX.size()); ++i)
        boardOffsets_[i] = (boardPositions_[i] - boardGoal[i]) * boardTravel;
    boardFrom_ = boardOffsets_;
    boardMotion_ = 0;
    mirrorGuide_ = adjustingMirror_ = false;
    mirrorYaw_ = mirrorStartYaw;
    notebook_ = {notebookStart, 1.f};
    notebookRatio_ = notebookFallSpeed_ = 0;
    notebookFalling_ = false;
}
glm::vec3 SchoolRoom::mirrorNormal() const {
    return {std::sin(mirrorYaw_), 0, std::cos(mirrorYaw_)};
}
float SchoolRoom::rayBox(glm::vec3 o, glm::vec3 d, const Box& b) {
    float near = 0, far = 10000;
    for (int i = 0; i < 3; ++i) {
        if (std::abs(d[i]) < 0.00001f) {
            if (o[i] < b.min[i] || o[i] > b.max[i])
                return -1;
        } else {
            float a = (b.min[i] - o[i]) / d[i], c = (b.max[i] - o[i]) / d[i];
            if (a > c)
                std::swap(a, c);
            near = std::max(near, a);
            far = std::min(far, c);
            if (near > far)
                return -1;
        }
    }
    return far >= 0 ? near : -1;
}
glm::vec3 SchoolRoom::galleryGateCenter() const {
    const float slide = 1.87f * glm::smoothstep(.64f, 1.f, formation_);
    return {3.025f + slide, gallerySideRail.y, -3.80f};
}
std::vector<Box> SchoolRoom::collisionBoxes(bool includeFixtures, bool includeNotebook) const {
    std::vector<Box> boxes = {bounds({-6.6f, 2.3f, 2}, {0.2f, 4.6f, 14.2f}),
                              bounds({6.6f, 2.3f, 2}, {0.2f, 4.6f, 14.2f}),
                              bounds({0, 2.3f, 9.1f}, {13.2f, 4.6f, 0.2f}),
                              bounds({-2.05f, 2.3f, -5.05f}, {8.9f, 4.6f, 0.2f}),
                              bounds({5.1f, 2.3f, -5.05f}, {2.8f, 4.6f, 0.2f}),
                              bounds({3.05f, 4.3f, -5.05f}, {1.3f, 0.6f, 0.2f}),
                              bounds({4.3f, 0.9f, -4.43f}, {4.4f, 1.8f, 1.2f}),
                              bounds({1.0f, 0.51f, -3.25f}, {1.1f, 1.02f, 0.7f}),
                              bounds({mirrorCenter.x, 0.12f, 0.7f}, {1.7f, 0.24f, 0.9f}),
                              bounds({2.27f, 3.1f, -6.05f}, {0.2f, 2.6f, 2.1f}),
                              bounds({3.83f, 3.1f, -6.05f}, {0.2f, 2.6f, 2.1f}),
                              bounds({3.05f, 0.9f, -6.0f}, {1.35f, 1.8f, 2.1f})};
    for (const auto& fixture : fixtures)
        boxes.push_back(bounds(fixture.center, fixture.size));
    boxes.push_back(bounds({3.48f, 2.55f, -6.45f}, {0.42f, 1.45f, 0.95f}));
    boxes.push_back(bounds(notebookStand.center, notebookStand.size));
    if (includeFixtures) {
        // Fixed clearance around moving fixtures prevents rotation/sliding from trapping a player.
        boxes.push_back(bounds(mirrorCenter, {1.6f, 2.1f, 1.6f}));
        if (includeNotebook && notebookRatio_ == 0)
            boxes.push_back(notebookBounds(notebook_));
        boxes.push_back(bounds({-0.85f, 2.23f, -4.49f}, {5.96f, 2.22f, 0.82f}));
    }
    int seat = 0;
    for (float z : deskRows)
        for (float x : deskColumns) {
            boxes.push_back(bounds({x, 0.39f, z}, {1.06f, 0.78f, 0.66f}));
            const auto chair = chairPose(seat++, x, z);
            const float c = std::abs(std::cos(chair.yaw)), s = std::abs(std::sin(chair.yaw));
            boxes.push_back(bounds(chair.center, {0.48f * c + 0.52f * s, 0.94f, 0.52f * c + 0.48f * s}));
        }
    boxes.push_back(bounds({5.2f, galleryRailPostY, -3.78f}, {2.5f, galleryRailHeight, 0.08f}));
    boxes.push_back(bounds(gallerySideRail, gallerySideRailSize));
    boxes.push_back(bounds(galleryGateCenter(), galleryGateSize));
    if (stairsReady_)
        for (int i = 0; i < 6; ++i) {
            float height = (i + 1) * 0.3f;
            boxes.push_back(bounds({2.7f, height / 2, -0.5f - i * 0.6f}, {1.8f, height, 0.6f}));
            boxes.push_back(bounds({1.80f, height + 0.625f, -0.2f - i * 0.6f}, {0.06f, 1.25f, 0.06f}));
        }
    const auto corridorBoxes = corridor_.collisionBoxes();
    boxes.insert(boxes.end(), corridorBoxes.begin(), corridorBoxes.end());
    return boxes;
}
void SchoolRoom::move(Camera& camera, const Input& input, float dt) {
    const auto boxes = collisionBoxes(true, false);
    // The upright paper notebook obstructs movement but cannot support the player as a step.
    const auto book = notebookRatio_ == 0 ? std::optional{notebookBounds(notebook_)} : std::nullopt;
    glm::vec3 f{std::sin(camera.yaw), 0, -std::cos(camera.yaw)};
    glm::vec3 velocity = f * input.forward + glm::vec3(-f.z, 0, f.x) * input.right;
    velocity /= std::max(1.0f, glm::length(velocity));
    velocity *= 3.0f;
    const float response = glm::length(velocity) > .01f ? 26.0f : 38.0f;
    const float blend = -std::expm1(-response * dt);
    const glm::vec3 displacement = velocity * dt + (camera.movement - velocity) * (blend / response);
    camera.movement += (velocity - camera.movement) * blend;
    if (glm::length(velocity) < .01f && glm::length(camera.movement) < .015f)
        camera.movement = {};
    camera.stepOffset *= std::exp(-14.0f * dt);
    if (std::abs(camera.stepOffset) < .0001f)
        camera.stepOffset = 0;
    for (int axis : {0, 2}) {
        glm::vec3 next = camera.feet;
        next[axis] += displacement[axis];
        float step = next.y;
        for (const Box& b : boxes)
            if (overlap(next, b) && camera.grounded && b.max.y - camera.feet.y <= 0.31f)
                step = std::max(step, b.max.y);
        next.y = step;
        const bool blocked = (book && overlap(next, *book)) ||
                             std::any_of(boxes.begin(), boxes.end(), [&](const Box& b) { return overlap(next, b); });
        if (!blocked) {
            camera.stepOffset = std::clamp(camera.stepOffset + camera.feet.y - next.y, -.35f, .30f);
            camera.feet = next;
        } else
            camera.movement[axis] = 0;
    }
    float oldY = camera.feet.y;
    camera.verticalSpeed -= 9.8f * dt;
    float newY = oldY + camera.verticalSpeed * dt;
    float support = 0;
    for (const Box& b : boxes)
        if (footprint(camera.feet, b, radius - 0.02f)) {
            if (b.max.y <= oldY + 0.02f)
                support = std::max(support, b.max.y);
            if (camera.verticalSpeed > 0 && oldY + bodyHeight <= b.min.y && newY + bodyHeight >= b.min.y) {
                newY = b.min.y - bodyHeight;
                camera.verticalSpeed = 0;
            }
        }
    if (book && footprint(camera.feet, *book, radius - .02f) && camera.verticalSpeed > 0 &&
        oldY + bodyHeight <= book->min.y && newY + bodyHeight >= book->min.y) {
        newY = book->min.y - bodyHeight;
        camera.verticalSpeed = 0;
    }
    const float ceiling = ceilingAt(camera.feet.z);
    if (newY + bodyHeight > ceiling) {
        newY = ceiling - bodyHeight;
        camera.verticalSpeed = 0;
    }
    const bool downStep = camera.grounded && camera.verticalSpeed <= 0 && oldY - support <= .31f;
    if ((newY <= support || downStep) && camera.verticalSpeed <= 0) {
        if (downStep)
            camera.stepOffset = std::clamp(camera.stepOffset + oldY - support, -.35f, .30f);
        camera.feet.y = support;
        camera.verticalSpeed = 0;
        camera.grounded = true;
    } else {
        camera.feet.y = newY;
        camera.grounded = false;
    }
    camera.stepOffset = std::min(camera.stepOffset, ceiling - .08f - camera.feet.y - 1.65f);
}
void SchoolRoom::update(Camera& camera, const Input& input, float elapsed, bool active) {
    if (!active || elapsed <= 0)
        return;
    // Captures and test fixtures can reposition a camera; never carry its old motion across a teleport.
    if (!camera.motionTracked || glm::distance(camera.feet, camera.motionFeet) > .12f)
        camera.resetMotion();
    float dt = std::clamp(elapsed, 0.0f, 0.1f);
    time_ += dt;
    if (input.jump && camera.grounded) {
        camera.verticalSpeed = 4.8f;
        camera.grounded = false;
    }
    int count = std::max(1, static_cast<int>(std::ceil(dt * 120)));
    for (int i = 0; i < count; ++i) {
        corridor_.update(camera, dt / count);
        move(camera, input, dt / count);
        if (notebookFalling_)
            updateNotebookFall(camera, dt / count);
    }
    camera.motionFeet = camera.feet;
    camera.motionTracked = true;
    if (glm::distance(camera.eye(), projectorOrigin) > 2.4f || input.jump)
        adjustingProjector_ = false;
    if (input.jump || glm::distance(camera.eye(), mirrorCenter) > 3.1f)
        adjustingMirror_ = false;
    if (adjustingProjector_ || adjustingMirror_)
        camera.updateFocus(dt);
    else
        camera.focusTime = 0;
    boardMotion_ = std::max(0.f, boardMotion_ - dt);
    for (int i = 0; i < static_cast<int>(boardX.size()); ++i) {
        const float target = (boardPositions_[i] - boardGoal[i]) * boardTravel;
        boardOffsets_[i] = glm::mix(boardFrom_[i], target,
                                   glm::smoothstep(0.f, 1.f, 1.f - boardMotion_ / boardMoveSeconds));
    }
    if (mirrorGuide_ && !canReadNotebook(camera))
        mirrorGuide_ = false;
    const auto projectorTarget = glm::normalize(projectorPaper - projectorOrigin);
    // Endpoint hits jump between receiver faces at their edges. Judge the continuous aim instead.
    projectorAligned_ = boardsReady() &&
                        glm::dot(projectorTarget, projectorRotation() * projectorTarget) >=
                            std::cos(glm::radians(projectorAimToleranceDegrees)) &&
                        boardVisibility() > .85f;
    // Recognition needs a brief steady view. Once committed, the transition can finish while moving.
    if (materializing_) {
        formation_ = std::min(1.0f, formation_ + dt / formationDuration);
        if (formation_ >= 1) {
            stairsReady_ = true;
            materializing_ = false;
        }
    } else if (!stairsReady_) {
        const float score = projectorAligned_ ? alignment(camera) : 0;
        alignmentHeld_ = advanceAlignmentHold(alignmentHeld_, score, stairAlignmentThreshold, dt);
        if (score >= stairAlignmentThreshold && alignmentHeld_ >= recognitionDuration) {
            materializing_ = true;
            adjustingProjector_ = false;
        }
    }
    if (corridor_.finished(camera))
        escaped_ = true;
}
bool SchoolRoom::boardsReady() const {
    return boardPositions_ == boardGoal && boardMotion_ == 0;
}
float SchoolRoom::boardShift(int index) const {
    return boardOffsets_[index];
}
glm::vec3 SchoolRoom::boardHandle(int index) const {
    return {boardX[index], 1.39f + boardShift(index), boardZ(index) + .115f};
}
void SchoolRoom::advanceBoards(std::array<int, 4>& positions, int index) {
    positions[index] = (positions[index] + 1) % 4;
    const int neighbor = (index + 1) % static_cast<int>(positions.size());
    positions[neighbor] = (positions[neighbor] + 2) % 4;
}
int SchoolRoom::aimedBoard(const Camera& camera) const {
    if (!camera.grounded || notebookRatio_ > 0 || adjustingProjector_ || adjustingMirror_ || materializing_ || stairsReady_)
        return -1;
    const auto eye = camera.eye(), direction = camera.direction();
    int index = -1;
    float distance = 2.75f;
    for (int i = 0; i < static_cast<int>(boardX.size()); ++i) {
        const auto target = glm::vec3(boardX[i], 2.22f + boardShift(i), boardZ(i) + .08f);
        const float hit = rayBox(eye, direction, bounds(target, {1.40f, 2.04f, .12f}));
        if (hit > .01f && hit < distance) {
            index = i;
            distance = hit;
        }
    }
    if (index >= 0)
        for (const auto& b : collisionBoxes(false)) {
            const float hit = rayBox(eye, direction, b);
            if (hit > 0 && hit < distance - .035f)
                return -1;
        }
    return index;
}
bool SchoolRoom::moveBoard(const Camera& camera) {
    const int index = aimedBoard(camera);
    if (index < 0)
        return false;
    boardFrom_ = boardOffsets_;
    advanceBoards(boardPositions_, index);
    boardMotion_ = boardMoveSeconds;
    projectorAligned_ = false;
    alignmentHeld_ = 0;
    return true;
}
Box SchoolRoom::notebookBounds(const perspective::Pose& pose) const {
    const auto axes = notebookAxes(pose.center, pose.center == notebookStart);
    const auto extent = notebookSize.x * glm::abs(axes.right) + notebookSize.y * glm::abs(axes.up) +
                        notebookSize.z * glm::abs(axes.normal);
    return bounds(pose.center, pose.scale * extent);
}
bool SchoolRoom::canGrabNotebook(const Camera& camera) const {
    if (notebookRatio_ > 0 || adjustingProjector_ || adjustingMirror_ || !camera.grounded)
        return false;
    const auto look = camera.direction(), eye = camera.eye();
    const float t = rayBox(eye, look, notebookBounds(notebook_));
    if (t < 0 || t > 1.85f)
        return false;
    for (const auto& b : collisionBoxes(false)) {
        const float hit = rayBox(eye, look, b);
        if (hit > 0 && hit < t - .025f)
            return false;
    }
    return true;
}
float SchoolRoom::notebookSupport(const Box& area, const std::vector<Box>& solids) const {
    const bool aboveClassroom = area.min.x >= -6.50f && area.max.x <= 6.50f &&
                                area.min.z >= -4.95f && area.max.z <= 9.f;
    float support = aboveClassroom ? 0.f : -std::numeric_limits<float>::infinity();
    for (const auto& solid : solids)
        if (solid.max.y <= area.min.y + .002f && overlapsXZ(solid, area))
            support = std::max(support, solid.max.y);
    return support;
}
std::optional<perspective::Pose> SchoolRoom::notebookPlacement(const Camera& camera) const {
    if (notebookRatio_ <= 0)
        return std::nullopt;
    const auto eye = camera.eye(), direction = camera.direction();
    const auto solids = collisionBoxes(true, false);
    auto fits = [&](const perspective::Pose& pose) {
        const auto area = notebookBounds(pose);
        if (pose.scale < minNotebookScale || pose.scale > maxNotebookScale + .001f ||
            area.max.y > std::min(ceilingAt(area.min.z), ceilingAt(area.max.z)) ||
            footprint(camera.feet, area, radius + .12f))
            return false;
        // Keep the only staircase approach open while the drawing is incomplete.
        if (!stairsReady_ && area.max.x > 1.55f && area.min.z < .33f && area.max.z > -4.6f && area.min.x < 4.f)
            return false;
        const float support = notebookSupport(area, solids);
        if (!std::isfinite(support) || support > camera.feet.y + 1.30f || area.min.y < support - .003f)
            return false;
        return std::none_of(solids.begin(), solids.end(), [&](const Box& solid) {
            return overlapsXZ(solid, area) && area.min.y < solid.max.y - .002f &&
                   area.max.y > solid.min.y + .002f;
        });
    };

    // Start with the aimed surface. Search backward along the ray if the growing book clips a solid.
    constexpr float maxAimReach = 12.f;
    const float range = std::min(maxAimReach, maxNotebookScale / notebookRatio_);
    float depth = maxAimReach;
    const Box* aimedSurface = nullptr;
    for (const auto& solid : solids) {
        const float hit = rayBox(eye, direction, solid);
        if (hit > .001f && hit < depth) {
            depth = hit;
            aimedSurface = &solid;
        }
    }
    const auto receiver = bounds(notebookRest + glm::vec3(0, .045f, 0), {.65f, .26f, 1.92f});
    const float standHit = rayBox(eye, direction, receiver);
    if (standHit > .02f && standHit <= depth + .02f) {
        const float scale = std::clamp(
            perspective::scaleAtSupport(eye, direction, notebookRest, notebookRatio_, notebookHalfHeight),
            minNotebookScale, maxStandNotebookScale);
        const perspective::Pose pose{notebookRest + glm::vec3(0, notebookHalfHeight * scale, 0), scale};
        if (fits(pose))
            return pose;
    }
    if (depth > range) {
        depth = range;
        aimedSurface = nullptr;
    }
    if (aimedSurface) {
        const auto anchor = eye + direction * depth;
        if (direction.y < 0 && std::abs(anchor.y - aimedSurface->max.y) < .008f) {
            // A small adjustment on the same surface keeps the book clear of things like chair backs.
            for (const auto offset : {glm::vec3(0), glm::vec3(0, 0, -.16f), glm::vec3(0, 0, .16f),
                                      glm::vec3(-.16f, 0, 0), glm::vec3(.16f, 0, 0)}) {
                const auto point = anchor + offset;
                if (point.x < aimedSurface->min.x || point.x > aimedSurface->max.x ||
                    point.z < aimedSurface->min.z || point.z > aimedSurface->max.z)
                    continue;
                const float scale =
                    perspective::scaleAtSupport(eye, direction, point, notebookRatio_, notebookHalfHeight);
                const perspective::Pose pose{point + glm::vec3(0, notebookHalfHeight * scale, 0), scale};
                if (fits(pose))
                    return pose;
            }
        }
        depth -= .023f;
    }
    if (const auto floor = perspective::onFloor(eye, direction, notebookRatio_, notebookHalfHeight)) {
        const float floorDepth = glm::dot(floor->center - eye, direction);
        if (floorDepth <= depth && fits(*floor))
            return floor;
        depth = std::min(depth, floorDepth);
    }
    for (int attempt = 0; attempt < 14 && depth * notebookRatio_ >= minNotebookScale; ++attempt, depth *= .84f) {
        const perspective::Pose pose{eye + direction * depth, notebookRatio_ * depth};
        if (fits(pose))
            return pose;
    }

    // Even a wall or an upward glance leaves the player a nearby way to put the notebook down.
    const glm::vec3 forward{std::sin(camera.yaw), 0, -std::cos(camera.yaw)};
    const glm::vec3 right{-forward.z, 0, forward.x};
    constexpr float diagonal = .70710678f;
    const std::array headings{forward, (forward + right) * diagonal, (forward - right) * diagonal,
                              right, -right, (-forward + right) * diagonal, (-forward - right) * diagonal,
                              -forward};
    for (float distance : {.70f, 1.05f, 1.45f}) {
        const float scale = std::clamp(notebookRatio_ * distance, minNotebookScale, .85f);
        for (const auto heading : headings) {
            const auto center = camera.feet + heading * distance;
            const float height = scale * notebookHalfHeight;
            perspective::Pose pose{{center.x, camera.feet.y + height, center.z}, scale};
            const float support = notebookSupport(notebookBounds(pose), solids);
            if (!std::isfinite(support))
                continue;
            pose.center.y = support + height;
            if (!fits(pose))
                continue;
            const auto toward = pose.center - eye;
            const float reach = glm::length(toward);
            const auto ray = toward / reach;
            const bool blocked = std::any_of(solids.begin(), solids.end(), [&](const Box& solid) {
                const float hit = rayBox(eye, ray, solid);
                return hit > .001f && hit < reach - .025f;
            });
            if (!blocked)
                return pose;
        }
    }
    return std::nullopt;
}
void SchoolRoom::updateNotebookFall(const Camera& camera, float dt) {
    const auto area = notebookBounds(notebook_);
    const auto solids = collisionBoxes(true, false);
    const float support = notebookSupport(area, solids);
    if (!std::isfinite(support)) {
        notebookFalling_ = false;
        return;
    }
    notebookFallSpeed_ += 9.8f * dt;
    float bottom = std::max(support, area.min.y - notebookFallSpeed_ * dt);
    const float playerTop = camera.feet.y + bodyHeight;
    if (footprint(camera.feet, area, radius) && playerTop <= area.min.y + .002f && bottom < playerTop) {
        bottom = playerTop;
        notebookFallSpeed_ = 0;
    }
    notebook_.center.y += bottom - area.min.y;
    notebookFalling_ = bottom > support + .002f;
}
bool SchoolRoom::toggleNotebook(const Camera& camera) {
    if (notebookRatio_ > 0) {
        if (const auto pose = notebookPlacement(camera)) {
            notebook_ = *pose;
            notebookRatio_ = notebookFallSpeed_ = 0;
            notebookFalling_ = true;
        }
        return true;
    }
    if (!canGrabNotebook(camera))
        return false;
    notebookRatio_ = perspective::grabRatio(camera.eye(), camera.direction(), notebook_.center,
                                            notebook_.scale, notebookSize.x);
    notebookFalling_ = mirrorGuide_ = false;
    return true;
}
std::optional<float> SchoolRoom::mirrorHit(glm::vec3 eye, glm::vec3 ray) const {
    const auto normal = mirrorNormal();
    const auto glass = mirrorCenter + normal * .006f;
    const float angle = glm::dot(ray, normal);
    if (angle > -.10f)
        return std::nullopt;
    const float depth = glm::dot(glass - eye, normal) / angle;
    if (depth < .12f || depth > 3.15f)
        return std::nullopt;
    const auto at = eye + ray * depth - glass;
    const glm::vec3 right{normal.z, 0, -normal.x};
    if (std::abs(glm::dot(at, right)) > mirrorHalfWidth || std::abs(at.y) > mirrorHalfHeight)
        return std::nullopt;
    return depth;
}
bool SchoolRoom::canTurnMirror(const Camera& camera) const {
    if (!camera.grounded || adjustingProjector_ || notebookRatio_ > 0)
        return false;
    const auto eye = camera.eye(), ray = camera.direction();
    if (glm::distance(eye, mirrorCenter) > 3.1f)
        return false;
    const auto normal = mirrorNormal();
    const glm::vec3 right{normal.z, 0, -normal.x};
    auto local = [&](glm::vec3 v) {
        return glm::vec3(glm::dot(v, right), v.y, glm::dot(v, normal));
    };
    // The frame can be turned from either side, even after the glass has rotated away.
    const float depth = rayBox(local(eye - mirrorCenter), local(ray), bounds({0, 0, -.024f}, {1.51f, 2.06f, .15f}));
    if (depth < .12f || depth > 2.8f)
        return false;
    const auto solids = collisionBoxes(false);
    return std::none_of(solids.begin(), solids.end(), [&](const Box& b) {
        const float hit = rayBox(eye, ray, b);
        return hit > .01f && hit < depth - .025f;
    });
}
bool SchoolRoom::toggleMirror(const Camera& camera) {
    if (!adjustingMirror_ && !canTurnMirror(camera))
        return false;
    adjustingMirror_ = !adjustingMirror_;
    mirrorGuide_ = false;
    return true;
}
void SchoolRoom::rotateMirror(float yaw) {
    if (adjustingMirror_)
        mirrorYaw_ = std::clamp(mirrorYaw_ + yaw, mirrorStartYaw, mirrorMaxYaw);
}
bool SchoolRoom::canReadNotebook(const Camera& camera) const {
    if (notebookRatio_ > 0 || notebook_.scale < readableNotebookScale || !camera.grounded || adjustingProjector_ || adjustingMirror_)
        return false;
    const auto eye = camera.eye(), look = camera.direction(), normal = mirrorNormal();
    if (!mirrorHit(eye, look))
        return false;
    const auto solids = collisionBoxes(false);
    auto occluded = [&](glm::vec3 origin, glm::vec3 ray, float depth) {
        return std::any_of(solids.begin(), solids.end(), [&](const Box& b) {
            const float hit = rayBox(origin, ray, b);
            return hit > .01f && hit < depth - .025f;
        });
    };
    const auto page = notebookAxes(notebook_.center, false);
    const auto paper = notebook_.center + page.normal * (.043f * notebook_.scale);
    const auto right = glm::normalize(glm::cross(look, glm::vec3(0, 1, 0)));
    const auto up = glm::cross(right, look);
    // A small group of nearby sight rays gives thin pages a forgiving read target.
    for (const auto offset : {glm::vec3(0), right * .045f, right * -.045f, up * .045f, up * -.045f}) {
        const auto ray = glm::normalize(look + offset);
        const auto hit = mirrorHit(eye, ray);
        if (!hit || occluded(eye, ray, *hit))
            continue;
        const auto bounced = glm::reflect(ray, normal);
        const float angle = glm::dot(bounced, page.normal);
        if (angle > -.15f)
            continue;
        const auto origin = eye + ray * *hit + bounced * .025f;
        const float depth = glm::dot(paper - origin, page.normal) / angle;
        const auto local = origin + bounced * depth - paper;
        if (depth > 0 && std::abs(glm::dot(local, page.right)) <= notebookSize.x * notebook_.scale * .51f &&
            std::abs(local.y) <= notebookSize.y * notebook_.scale * .51f && !occluded(origin, bounced, depth))
            return true;
    }
    return false;
}
bool SchoolRoom::inspectMirror(const Camera& camera) {
    if (!canReadNotebook(camera))
        return false;
    mirrorGuide_ = !mirrorGuide_;
    return true;
}
bool SchoolRoom::canAdjustProjector(const Camera& camera) const {
    return projectorInReach(camera);
}
bool SchoolRoom::projectorInReach(const Camera& camera) const {
    if (stairsReady_ || materializing_ || adjustingMirror_ || notebookRatio_ > 0 || !camera.grounded)
        return false;
    const auto direction = projectorOrigin - camera.eye();
    float distance = glm::length(direction);
    if (distance < 0.2f || distance > 2.0f || glm::dot(glm::normalize(direction), camera.direction()) < 0.94f)
        return false;
    // Furniture between the player and the device prevents remote interaction.
    for (const auto& box : collisionBoxes(false)) {
        float hit = rayBox(camera.eye(), glm::normalize(direction), box);
        if (hit > 0 && hit < distance - 0.15f)
            return false;
    }
    return true;
}
bool SchoolRoom::toggleProjector(const Camera& camera) {
    if (adjustingProjector_) {
        adjustingProjector_ = false;
        return true;
    }
    if (!canAdjustProjector(camera))
        return false;
    adjustingProjector_ = true;
    return true;
}
void SchoolRoom::rotateProjector(float yaw, float pitch) {
    if (!adjustingProjector_ || stairsReady_ || materializing_)
        return;
    projectorYaw_ = std::clamp(projectorYaw_ + yaw, -0.82f, 0.10f);
    projectorPitch_ = std::clamp(projectorPitch_ + pitch, -0.15f, 0.30f);
}
glm::mat3 SchoolRoom::projectorRotation() const {
    auto target = glm::normalize(projectorPaper - projectorOrigin);
    return projectorBasis(projectorYaw_, projectorPitch_) *
           glm::transpose(projectorBasis(std::atan2(target.x, -target.z), std::asin(target.y)));
}
SchoolRoom::ProjectionHit SchoolRoom::projectSlide(glm::vec3 point) const {
    auto direction = glm::normalize(projectorRotation() * (point - projectorOrigin));
    ProjectionHit result;
    float nearest = 1000;
    const auto& surfaces = projectionSurfaces();
    for (int i = 0; i < static_cast<int>(surfaces.size()); ++i) {
        auto surface = surfaces[i];
        if (i < static_cast<int>(boardX.size())) {
            surface.low.y += boardShift(i);
            surface.high.y += boardShift(i);
        }
        if (std::abs(direction[surface.axis]) < 0.00001f)
            continue;
        float distance = (surface.coordinate - projectorOrigin[surface.axis]) / direction[surface.axis];
        if (distance <= 0.01f || distance >= nearest)
            continue;
        auto hit = projectorOrigin + direction * distance;
        bool inside = true;
        for (int axis = 0; axis < 3; ++axis)
            if (axis != surface.axis &&
                (hit[axis] < surface.low[axis] - 0.0001f || hit[axis] > surface.high[axis] + 0.0001f))
                inside = false;
        if (inside) {
            nearest = distance;
            result = {hit, {}, i};
            result.normal[surface.axis] = direction[surface.axis] > 0 ? -1.0f : 1.0f;
        }
    }
    return result;
}
bool SchoolRoom::isSlideStroke(const Chalk& stroke) const {
    // All lower stair fragments belong to the same slide, including floor and lectern strokes.
    return stroke.panel < 0;
}
glm::vec4 SchoolRoom::slideBounds() const {
    glm::vec4 bounds{1000, 1000, -1000, -1000};
    for (const auto& stroke : chalk_) {
        if (!isSlideStroke(stroke))
            continue;
        for (auto p : {stroke.projectedA, stroke.projectedB}) {
            auto ray = p - projectorOrigin;
            auto source = projectorOrigin + ray * ((projectorPaper.z - projectorOrigin.z) / ray.z);
            bounds.x = std::min(bounds.x, source.x);
            bounds.y = std::min(bounds.y, source.y);
            bounds.z = std::max(bounds.z, source.x);
            bounds.w = std::max(bounds.w, source.y);
        }
    }
    // Include the whole introductory artwork, not just the smaller puzzle-stroke bounds.
    // Transform its four corners back into the calibrated slide plane.
    auto toSlide = glm::transpose(projectorRotation());
    for (float x : {artworkFrame.x, artworkFrame.z})
        for (float y : {artworkFrame.y, artworkFrame.w}) {
            auto ray = toSlide * (glm::vec3(x, y, -4.50f) - projectorOrigin);
            auto source = projectorOrigin + ray * ((projectorPaper.z - projectorOrigin.z) / ray.z);
            bounds.x = std::min(bounds.x, source.x);
            bounds.y = std::min(bounds.y, source.y);
            bounds.z = std::max(bounds.z, source.x);
            bounds.w = std::max(bounds.w, source.y);
        }
    // The slide has a soft unprinted border around the drawing.
    return bounds + glm::vec4(-0.45f, -0.45f, 0.45f, 0.45f);
}
void SchoolRoom::makeSlideMask() {
    // A dark slide with soft light around its pencil drawing has no opaque rectangular backing.
    slideMask_.assign(slideResolution * slideResolution, 0);
    const glm::vec2 low{slideBounds_.x, slideBounds_.y};
    const glm::vec2 size = glm::vec2(slideBounds_.z, slideBounds_.w) - low;
    auto sourcePoint = [&](glm::vec3 p) {
        auto ray = p - projectorOrigin;
        return glm::vec2(projectorOrigin + ray * ((projectorPaper.z - projectorOrigin.z) / ray.z));
    };
    for (const auto& stroke : chalk_) {
        if (!isSlideStroke(stroke) || stroke.detail)
            continue;
        auto a = sourcePoint(stroke.projectedA), b = sourcePoint(stroke.projectedB);
        glm::vec2 ab = b - a;
        float squared = glm::dot(ab, ab);
        auto lowPixel =
            glm::ivec2(glm::floor((glm::min(a, b) - glm::vec2(0.29f) - low) / size * float(slideResolution)));
        auto highPixel =
            glm::ivec2(glm::ceil((glm::max(a, b) + glm::vec2(0.29f) - low) / size * float(slideResolution)));
        for (int y = std::max(0, lowPixel.y); y <= std::min(slideResolution - 1, highPixel.y); ++y)
            for (int x = std::max(0, lowPixel.x); x <= std::min(slideResolution - 1, highPixel.x); ++x) {
                auto p = low + (glm::vec2(x, y) + glm::vec2(0.5f)) / float(slideResolution) * size;
                float t = squared > 0.000001f ? glm::clamp(glm::dot(p - a, ab) / squared, 0.0f, 1.0f) : 0;
                float weight = 1 - glm::smoothstep(0.06f, 0.28f, glm::distance(p, a + ab * t));
                auto& pixel = slideMask_[y * slideResolution + x];
                pixel = std::max(pixel, weight);
            }
    }
}
float SchoolRoom::slideLight(glm::vec2 source) const {
    const glm::vec2 low{slideBounds_.x, slideBounds_.y}, high{slideBounds_.z, slideBounds_.w};
    auto uv = (source - low) / (high - low);
    if (uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1 || slideMask_.empty())
        return 0;
    auto p = glm::clamp(uv * float(slideResolution) - glm::vec2(0.5f), glm::vec2(0),
                        glm::vec2(slideResolution - 1));
    glm::ivec2 a(p), b = glm::min(a + glm::ivec2(1), glm::ivec2(slideResolution - 1));
    auto f = glm::fract(p);
    float bottom =
        glm::mix(slideMask_[a.y * slideResolution + a.x], slideMask_[a.y * slideResolution + b.x], f.x);
    float top =
        glm::mix(slideMask_[b.y * slideResolution + a.x], slideMask_[b.y * slideResolution + b.x], f.x);
    return glm::mix(bottom, top, f.y);
}
bool SchoolRoom::projectorCovers(glm::vec3 point) const {
    return projectorLight(point) > 0.5f;
}
glm::mat3 SchoolRoom::artworkProjection() const {
    auto initial = glm::normalize(projectorStartAim - projectorOrigin);
    return projectorBasis(std::atan2(initial.x, -initial.z), std::asin(initial.y)) *
           glm::transpose(projectorBasis(projectorYaw_, projectorPitch_));
}
float SchoolRoom::artworkCoverage(glm::vec3 point) const {
    auto ray = artworkProjection() * (point - projectorOrigin);
    if (ray.z >= -0.001f)
        return 0;
    auto art = projectorOrigin + ray * ((-4.50f - projectorOrigin.z) / ray.z);
    glm::vec2 uv = (glm::vec2(art) - glm::vec2(artworkFrame)) /
                   (glm::vec2(artworkFrame.z, artworkFrame.w) - glm::vec2(artworkFrame));
    auto margin = glm::min(uv, glm::vec2(1) - uv);
    return glm::smoothstep(0.0f, 0.025f, margin.x) * glm::smoothstep(0.0f, 0.025f, margin.y);
}
float SchoolRoom::projectorLight(glm::vec3 point) const {
    auto relative = glm::transpose(projectorRotation()) * (point - projectorOrigin);
    if (relative.z >= -0.01f)
        return 0;
    auto source = projectorOrigin + relative * ((projectorPaper.z - projectorOrigin.z) / relative.z);
    const auto bounds = slideBounds_;
    if (source.x < bounds.x || source.x > bounds.z || source.y < bounds.y || source.y > bounds.w)
        return 0;
    // Board artwork is drawn on each panel without projector shadow maps.
    // Use that same coverage here so nearby storage cannot leave ghost chalk over the image.
    if (point.z < -4.3f)
        return artworkCoverage(point);
    auto hit = projectSlide(source);
    if (hit.surface < 0 || glm::distance(hit.point, point) >= 0.06f)
        return 0;
    return slideLight(glm::vec2(source));
}
float SchoolRoom::boardVisibility() const {
    float visible = 0, length = 0;
    for (const auto& stroke : chalk_) {
        if (stroke.panel < 0)
            continue;
        float part = glm::length(stroke.projectedB - stroke.projectedA) / 4;
        for (int i = 0; i < 4; ++i) {
            length += part;
            auto point = glm::mix(stroke.projectedA, stroke.projectedB, (i + 0.5f) / 4);
            point.y += boardShift(stroke.panel);
            if (!projectorCovers(point))
                visible += part;
        }
    }
    return length > 0 ? visible / length : 0;
}
float SchoolRoom::projectorError() const {
    float worst = 0;
    bool found = false;
    for (const auto& stroke : chalk_) {
        if (!isSlideStroke(stroke))
            continue;
        found = true;
        for (auto point : {stroke.projectedA, stroke.projectedB}) {
            auto hit = projectSlide(point);
            if (hit.surface < 0)
                return 1000;
            worst = std::max(worst, glm::distance(hit.point, point));
        }
    }
    return found ? worst : 1000;
}
float SchoolRoom::alignment(const Camera& camera) const {
    return stairViewAlignment(camera).value_or(0);
}
std::optional<float> SchoolRoom::stairViewAlignment(const Camera& camera) const {
    if (stairsReady_ || !camera.grounded)
        return std::nullopt;
    const auto eye = camera.eye();
    if (glm::dot(camera.direction(), glm::normalize(alignmentFocus - eye)) < .94f)
        return std::nullopt;
    float error = 0;
    int count = 0;
    for (const Chalk& c : chalk_) {
        if (c.detail)
            continue;
        glm::vec3 point = c.projectedA;
        if (c.panel >= 0)
            point.y += boardShift(c.panel);
        glm::vec3 a = glm::normalize(point - eye), b = glm::normalize(c.originalA - eye);
        error += 1.0f - glm::clamp(glm::dot(a, b), -1.0f, 1.0f);
        ++count;
    }
    float angular = std::sqrt(error / std::max(count, 1));
    // Judge the visible projection, not proximity to a hidden world-space activation spot.
    return std::clamp(1.0f - angular * 7.0f, 0.0f, 1.0f);
}
void SchoolRoom::makeChalk() {
    chalk_.clear();
    pageFaces_.clear();
    // Project the future stair edges onto real room surfaces from the observation point.
    // Each fragment remains attached to its surface when the camera moves.
    auto project = [&](glm::vec3 p, int& panel) -> glm::vec3 {
        glm::vec3 d = glm::normalize(p - alignmentEye);
        float nearest = 100;
        glm::vec3 result = p;
        panel = -2;
        auto plane = [&](int axis, float coordinate, glm::vec3 low, glm::vec3 high, int id) {
            if (std::abs(d[axis]) < 0.0001f)
                return;
            float t = (coordinate - alignmentEye[axis]) / d[axis];
            glm::vec3 point = alignmentEye + d * t;
            if (t <= 0.05f || t >= nearest)
                return;
            for (int k = 0; k < 3; ++k)
                if (k != axis && (point[k] < low[k] || point[k] > high[k]))
                    return;
            nearest = t;
            // Slide rays must start from the true receiving surface. A view-facing depth bias
            // would move grazing rays far across the floor or lectern when projected again.
            result = id >= 0 ? point - d * 0.012f : point;
            panel = id;
        };
        plane(1, 0.012f, {-6.5f, 0, -5}, {6.5f, 0, 9}, -1);
        for (int i = 0; i < static_cast<int>(boardX.size()); ++i)
            plane(2, boardZ(i) + 0.055f, {boardX[i] - boardHalf, 1.26f, 0}, {boardX[i] + boardHalf, 3.18f, 0},
                  i);
        plane(2, -3.777f, {2.1f, 0, 0}, {4.5f, 1.8f, 0}, -1);
        plane(2, -2.89f, {0.45f, 0, 0}, {1.55f, 1.04f, 0}, -1);
        plane(0, 1.56f, {0, 0, -3.6f}, {0, 1.04f, -2.9f}, -1);
        plane(1, 1.045f, {0.45f, 0, -3.6f}, {1.55f, 0, -2.9f}, -1);
        // Keep the projected floor study inside the front teaching area, on ordinary wooden tiles.
        if (result.y < 0.04f && (result.x < 0.4f || result.x > 3.95f || result.z < -3.85f || result.z > 0.1f))
            panel = -2;
        return result;
    };
    auto edge = [&](glm::vec3 a, glm::vec3 b, bool detail = false) {
        constexpr int segments = 28;
        for (int i = 0; i < segments; ++i) {
            glm::vec3 p = glm::mix(a, b, i / float(segments)), q = glm::mix(a, b, (i + 1) / float(segments));
            int panelA, panelB;
            glm::vec3 pa = project(p, panelA), pb = project(q, panelB);
            if (panelA != -2 && panelA == panelB && glm::distance(pa, pb) < 0.9f)
                chalk_.push_back({pa, pb, p, q, panelA, detail});
        }
    };
    for (int i = 0; i < 6; ++i) {
        float y = (i + 1) * 0.3f, z = -0.2f - i * 0.6f;
        edge({1.8f, y, z}, {3.6f, y, z});
        // Project the page block through the exact same observation rays as its cover edges.
        // Small cells split at receiving surfaces, so the book drawing follows the lectern and floor.
        for (int column = 0; column < 18; ++column)
            for (int row = 0; row < 3; ++row) {
                std::array<glm::vec3, 4> face;
                int receiver = -2;
                bool valid = true;
                int corner = 0;
                for (auto uv : {glm::vec2(0, 0), {1, 0}, {1, 1}, {0, 1}}) {
                    auto p = glm::vec3(1.85f + (column + uv.x) / 18 * 1.70f,
                                       y - 0.10f + (row + uv.y) / 3 * 0.09f, z);
                    int panel;
                    face[corner++] = project(p, panel);
                    if (corner == 1)
                        receiver = panel;
                    valid = valid && panel == -1 && panel == receiver;
                }
                if (valid)
                    pageFaces_.push_back(face);
            }
        // The future treads are sketchbook covers. Page edges sit just below that same outline.
        for (float page : {0.028f, 0.056f, 0.084f})
            edge({1.85f, y - page, z}, {3.55f, y - page, z}, true);
        edge({1.83f, y - 0.10f, z - 0.02f}, {1.83f, y - 0.10f, z - 0.53f}, true);
        for (float x : {1.8f, 3.6f}) {
            edge({x, y, z}, {x, y, z - 0.6f});
            edge({x, y - 0.3f, z}, {x, y, z});
        }
        // The left handrail projects across the chalkboards; lower edges cross the lectern and floor.
        edge({1.8f, y, z}, {1.8f, y + 1.25f, z});
        if (i < 5)
            edge({1.8f, y + 1.25f, z}, {1.8f, y + 1.55f, z - 0.6f});
    }
    // An art-class construction line spans all the rails, even where the real stair edges
    // miss a panel. Lay it out on a common image plane so its segments meet in perspective.
    constexpr float guidePlane = -4.50f;
    for (int i = 0; i < static_cast<int>(boardX.size()); ++i) {
        const float z = boardZ(i) + .073f;
        const float scale = (guidePlane - alignmentEye.z) / (z - alignmentEye.z);
        auto point = [&](float x) {
            const float imageX = alignmentEye.x + (x - alignmentEye.x) * scale;
            const float imageY = 1.92f + .16f * (imageX + 3.70f);
            return glm::vec3(x, alignmentEye.y + (imageY - alignmentEye.y) / scale, z);
        };
        const auto a = point(boardX[i] - .677f), b = point(boardX[i] + .677f);
        chalk_.push_back({a, b, a, b, i, true});
    }
    edge({1.8f, 0.015f, -0.2f}, {3.6f, 0.015f, -0.2f});
}

void SchoolRoom::drawProjector(Renderer& r) const {
    // A fixed classroom AV cart with a rotating projector and a powered lens.
    r.box({4.8f, 0.77f, 5.0f}, {0.86f, 0.06f, 0.67f}, timber);
    r.box({4.8f, 0.27f, 5.0f}, {0.76f, 0.05f, 0.56f}, timber);
    for (float x : {4.46f, 5.14f})
        for (float z : {4.76f, 5.24f}) {
            r.box({x, 0.39f, z}, {0.045f, 0.74f, 0.045f}, mint * 0.7f);
            r.box({x, 0.055f, z}, {0.10f, 0.11f, 0.07f}, {0.20f, 0.24f, 0.22f});
        }
    const glm::vec3 heading{std::sin(projectorYaw_), 0, -std::cos(projectorYaw_)};
    const glm::vec3 right{-heading.z, 0, heading.x};
    const auto body = projectorOrigin - heading * .24f - glm::vec3(0, .10f, 0);
    r.box({4.8f, .817f, 4.97f}, {.32f, .034f, .31f}, mint * .60f);
    r.box({4.8f, .874f, 4.97f}, {.21f, .10f, .18f}, mint * .45f, -projectorYaw_);
    r.box(body, {.56f, .24f, .50f}, {.70f, .72f, .65f}, -projectorYaw_);
    for (int vent = 0; vent < 5; ++vent)
        r.box(body - heading * .251f + right * (-.19f + vent * .060f), {.024f, .09f, .008f}, mint * .50f,
              -projectorYaw_);
    r.box(body - heading * .255f + right * .18f, {.041f, .020f, .01f}, {.38f, .65f, .37f},
          -projectorYaw_);

    const glm::vec3 direction = heading * std::cos(projectorPitch_) + glm::vec3(0, std::sin(projectorPitch_), 0);
    const auto up = glm::cross(right, direction);
    const auto lens = projectorOrigin + direction * .064f;
    for (int i = 0; i < 12; ++i) {
        const float a = i * 6.2831853f / 12, b = (i + 1) * 6.2831853f / 12;
        const auto rimA = (right * std::cos(a) + up * std::sin(a)) * .092f;
        const auto rimB = (right * std::cos(b) + up * std::sin(b)) * .092f;
        const auto neck = direction * .178f;
        const glm::vec3 casing{.25f, .29f, .28f};
        r.triangle(lens + rimA, lens - neck + rimB, lens - neck + rimA, casing);
        r.triangle(lens + rimA, lens + rimB, lens - neck + rimB, casing);
        r.triangle(lens, lens + rimB, lens + rimA, casing);
        const auto glass = lens + direction * .001f;
        r.triangle(glass, glass + rimB * .73f, glass + rimA * .73f, {.92f, .96f, .82f});
    }
    const auto socket = body - heading * .253f + right * .22f - glm::vec3(0, .065f, 0);
    const glm::vec3 cableGuide{5.16f, .806f, 5.25f};
    r.line(socket, cableGuide, .016f, timber * .5f);
    r.line(cableGuide, {5.25f, 0.025f, 5.2f}, .018f, timber * .5f);
    r.line({5.25f, 0.025f, 5.2f}, {6.44f, 0.025f, 5.4f}, 0.018f, timber * 0.5f);
    r.line({6.44f, 0.025f, 5.4f}, {6.44f, 0.30f, 5.4f}, 0.018f, timber * 0.5f);
    r.box({6.44f, 0.30f, 5.4f}, {0.04f, 0.15f, 0.12f}, cream);
    // Clip the light to receiving surfaces; the shader leaves unprinted areas transparent.
    // The same soft slide mask controls the chalk being covered and uncovered.
    const auto inverse = glm::transpose(projectorRotation());
    const float depth = projectorPaper.z - projectorOrigin.z;
    r.beginProjection(projectorOrigin, inverse, slideBounds_, projectorPaper.z, slideMask_, slideResolution,
                      artworkProjection(), artworkFrame, 1);
    auto distanceToEdge = [&](glm::vec3 point, int edge) {
        auto q = inverse * (point - projectorOrigin);
        switch (edge) {
        case 0:
            return -(projectorOrigin.x - slideBounds_.x) * q.z - depth * q.x;
        case 1:
            return (projectorOrigin.x - slideBounds_.z) * q.z + depth * q.x;
        case 2:
            return -(projectorOrigin.y - slideBounds_.y) * q.z - depth * q.y;
        case 3:
            return (projectorOrigin.y - slideBounds_.w) * q.z + depth * q.y;
        default:
            return -q.z - 0.01f;
        }
    };
    const auto& surfaces = projectionSurfaces();
    for (std::size_t surfaceIndex = 0; surfaceIndex < surfaces.size(); ++surfaceIndex) {
        auto surface = surfaces[surfaceIndex];
        if (surfaceIndex < boardX.size()) {
            surface.low.y += boardShift(static_cast<int>(surfaceIndex));
            surface.high.y += boardShift(static_cast<int>(surfaceIndex));
        }
        int u = (surface.axis + 1) % 3, v = (surface.axis + 2) % 3;
        std::vector<glm::vec3> polygon;
        for (glm::vec2 uv : {glm::vec2(0, 0), {1, 0}, {1, 1}, {0, 1}}) {
            auto point = surface.low;
            point[surface.axis] = surface.coordinate;
            point[u] = glm::mix(surface.low[u], surface.high[u], uv.x);
            point[v] = glm::mix(surface.low[v], surface.high[v], uv.y);
            polygon.push_back(point);
        }
        for (int edge = 0; edge < 5 && !polygon.empty(); ++edge) {
            std::vector<glm::vec3> clipped;
            auto previous = polygon.back();
            float previousDistance = distanceToEdge(previous, edge);
            for (auto point : polygon) {
                float distance = distanceToEdge(point, edge);
                if ((distance >= 0) != (previousDistance >= 0))
                    clipped.push_back(
                        glm::mix(previous, point, previousDistance / (previousDistance - distance)));
                if (distance >= 0)
                    clipped.push_back(point);
                previous = point;
                previousDistance = distance;
            }
            polygon = std::move(clipped);
        }
        if (polygon.size() < 3)
            continue;
        glm::vec3 normal{};
        normal[surface.axis] = projectorOrigin[surface.axis] > surface.coordinate ? 1.0f : -1.0f;
        if (glm::dot(glm::cross(polygon[1] - polygon[0], polygon[2] - polygon[0]), normal) < 0)
            std::reverse(polygon.begin(), polygon.end());
        // Keep the translucent light above the board and just above the paper surface.
        float offset = surfaceIndex < boardX.size() ? 0.050f : 0.003f;
        for (std::size_t i = 1; i + 1 < polygon.size(); ++i)
            r.triangle(polygon[0] + normal * offset, polygon[i] + normal * offset,
                       polygon[i + 1] + normal * offset, {0.98f, 0.96f, 0.79f});
    }
    r.projectionWash(1 - stairFormation());
    for (const auto& face : pageFaces_) {
        std::array<ProjectionHit, 4> hits;
        bool visible = true;
        for (int i = 0; i < 4; ++i) {
            hits[i] = projectSlide(face[i]);
            visible = visible && hits[i].surface >= 0 && hits[i].surface == hits[0].surface;
        }
        if (visible) {
            auto offset = hits[0].normal * 0.020f;
            r.triangle(hits[0].point + offset, hits[1].point + offset, hits[2].point + offset,
                       {0.75f, 0.70f, 0.57f});
            r.triangle(hits[0].point + offset, hits[2].point + offset, hits[3].point + offset,
                       {0.75f, 0.70f, 0.57f});
        }
    }
    r.projectionStrokes(1 - stairFormation());
    // Soft strokes stay on the receiving plane, with the light's grain and faded margins.
    for (const auto& stroke : chalk_) {
        if (!isSlideStroke(stroke))
            continue;
        auto a = projectSlide(stroke.projectedA), b = projectSlide(stroke.projectedB);
        if (a.surface >= 0 && a.surface == b.surface && glm::distance(a.point, b.point) < 0.9f)
            r.projectedStroke(a.point + a.normal * 0.037f, b.point + b.normal * 0.037f, a.normal,
                              stroke.detail ? 0.018f : 0.040f, {0.32f, 0.29f, 0.22f});
    }
    r.endProjection();
}
void SchoolRoom::drawNotebook(Renderer& r, const Camera& camera,
                              const std::optional<perspective::Pose>& placement) const {
    auto pose = notebook_;
    const bool onStand = notebookRatio_ == 0 && pose.center == notebookStart;
    bool preview = false;
    if (notebookRatio_ > 0) {
        if (placement) {
            pose = *placement;
        } else {
            const float depth = std::min(1.14f, maxNotebookScale / notebookRatio_);
            pose = {camera.eye() + camera.direction() * depth, depth * notebookRatio_};
            preview = true;
        }
    }
    auto [right, up, normal] = notebookAxes(pose.center, onStand);
    if (preview) {
        auto toEye = camera.eye() - pose.center;
        toEye.y = 0;
        if (glm::dot(toEye, toEye) > .01f) {
            normal = glm::normalize(toEye);
            right = {normal.z, 0, -normal.x};
        }
    }
    const float scale = pose.scale, yaw = std::atan2(normal.x, normal.z);
    auto at = [&](float x, float y, float z) {
        return pose.center + scale * (right * x + up * y + normal * z);
    };
    auto layer = [&](glm::vec3 center, glm::vec3 size, glm::vec3 color) {
        if (!onStand) {
            r.box(center, size * scale, color, yaw);
            return;
        }
        const auto x = right * (size.x * scale * .5f);
        const auto y = up * (size.y * scale * .5f);
        const auto z = normal * (size.z * scale * .5f);
        r.quad(center + z, x, y, color);
        r.quad(center - z, -x, y, color);
        r.quad(center + x, -z, y, color);
        r.quad(center - x, z, y, color);
        r.quad(center + y, x, -z, color);
        r.quad(center - y, x, z, color);
    };
    const bool readable = scale >= readableNotebookScale;
    layer(pose.center, notebookSize, notebookCover);
    for (float side : {-1.f, 1.f}) {
        layer(at(side * .103f, 0, .026f), {.191f, .295f, .024f}, cream * .86f);
        layer(at(side * .103f, .002f, .038f), {.186f, .284f, .008f}, side > 0 ? cream : cream * .96f);
        const int lineCount = readable ? 0 : 6;
        for (int line = 0; line < lineCount; ++line) {
            const float lineY = .081f - line * .035f;
            r.line(at(side * .103f - .077f, lineY, .044f), at(side * .103f + .077f, lineY, .044f),
                   .0013f * scale, mint * 1.10f);
        }
    }
    for (float y : {-.106f, -.054f, -.002f, .050f, .102f})
        layer(at(0, y, .041f), {.018f, .007f, .022f}, brass * .71f);
    r.quad(at(.156f, -.117f, .046f), right * (.022f * scale), up * (.015f * scale), notebookCover * 1.16f);
    if (!readable) {
        // A broad study sketch stays legible at the notebook's original size.
        const glm::vec3 pencil{.33f, .39f, .35f};
        for (float x : {-.15f, -.11f, -.07f})
            r.line(at(x, -.052f, .046f), at(x + .025f, .028f, .046f), .003f * scale, pencil);
        r.line(at(-.154f, -.055f, .046f), at(-.047f, -.055f, .046f), .003f * scale, pencil);
        return;
    }
    const auto ink = mint * .35f;
    r.label(L"숫자 맞추기", at(.104f, .123f, .049f), right * (-.082f * scale), up * (.014f * scale), ink);
    r.label(L"E로 밀기", at(-.104f, .123f, .049f), right * (-.082f * scale), up * (.014f * scale), ink);
    // Logical left-to-right is reversed on the paper, then restored by the mirror.
    for (int i = 0; i < 4; ++i) {
        const float x = .164f - i * .04f;
        r.quad(at(x, .064f, .050f), right * (.016f * scale), up * (.026f * scale), mint * .65f);
        r.label(notebookPanelLabels[i], at(x, .064f, .052f),
                right * (-.012f * scale), up * (.017f * scale), chalkWhite);
    }
    for (int i = 0; i < 2; ++i) {
        const float x = -.061f - i * .086f;
        r.quad(at(x, .064f, .050f), right * (.034f * scale), up * (.030f * scale), mint * .65f);
        r.label(i == 0 ? L"+1" : L"+2", at(x, .064f, .052f),
                right * (-.025f * scale), up * (.018f * scale), chalkWhite);
    }
    for (int i = 0; i < 4; ++i) {
        const float y = .004f - i * .038f;
        const auto textRight = right * (-.088f * scale);
        const auto textUp = up * (.013f * scale);
        r.label(notebookClues[i], at(.104f, y, .052f), textRight, textUp, ink);
        r.label(notebookControls[i], at(-.104f, y, .052f), textRight, textUp, ink);
    }
}

void SchoolRoom::scene(Renderer& r, const Camera& camera, bool reflection,
                       const std::optional<perspective::Pose>& placement) const {
    r.box(notebookStand.center, notebookStand.size, timber * 1.10f);
    r.box({-6.36f, 1.195f, notebookStart.z}, {.16f, .105f, .46f}, timber * .86f);
    r.box({-6.005f, 1.172f, notebookStart.z}, {.030f, .046f, .56f}, timber);
    r.sign(L"관찰 노트", {-5.95f, 1.105f, notebookStart.z}, {0, 0, -.23f}, {0, .066f, 0});
    drawNotebook(r, camera, placement);
    // Architecture. Every visible walkable solid has a corresponding collision box.
    r.box({0, -0.16f, 2}, {13.2f, 0.24f, 14.2f}, {0.30f, 0.22f, 0.15f});
    for (int x = 0; x < 26; ++x)
        for (int z = 0; z < 14; ++z) {
            float shade = 0.91f + 0.025f * ((x * 13 + z * 7) % 6);
            r.box({-6.25f + x * 0.5f, -0.015f, -4.5f + z}, {0.492f, 0.03f, 0.988f},
                  glm::vec3(0.59f, 0.40f, 0.23f) * shade);
        }
    r.box({6.6f, 2.25f, 2}, {0.2f, 4.5f, 14.2f}, cream);
    r.box({0, 2.25f, 9.1f}, {13.2f, 4.5f, 0.2f}, cream);
    r.box({-2.05f, 2.25f, -5.1f}, {8.9f, 4.5f, 0.2f}, cream);
    r.box({5.1f, 2.25f, -5.1f}, {2.8f, 4.5f, 0.2f}, cream);
    r.box({3.05f, 4.3f, -5.1f}, {1.3f, 0.6f, 0.2f}, cream);
    r.box({-6.6f, 0.55f, 2}, {0.2f, 1.1f, 14.2f}, mint);
    r.box({-6.6f, 4.0f, 2}, {0.2f, 1.0f, 14.2f}, cream);
    for (float z : {-4.95f, -1.5f, 2.0f, 5.5f, 8.95f})
        r.box({-6.6f, 2.3f, z}, {0.2f, 2.4f, 0.22f}, cream);
    r.box({0, 4.6f, 2}, {13.2f, 0.2f, 14.2f}, {0.93f, 0.91f, 0.83f});
    r.box({6.48f, 0.56f, 2}, {0.035f, 1.12f, 14}, mint);
    r.box({0, 0.56f, 8.98f}, {13, 1.12f, 0.035f}, mint);
    for (float y : {0.10f, 1.14f, 4.32f}) {
        r.box({6.45f, y, 2}, {0.08f, 0.07f, 14}, timber * 0.85f);
        r.box({0, y, 8.94f}, {13, 0.07f, 0.08f}, timber * 0.85f);
        r.box({0, y, -4.94f}, {13, 0.07f, 0.08f}, timber * 0.85f);
    }
    for (float z : {-3.25f, 0.25f, 3.75f, 7.25f}) {
        r.quad({-6.56f, 2.30f, z}, {0, 0, 1.64f}, {0, 1.16f, 0}, {0.64f, 0.80f, 0.82f});
        // Layered courtyard silhouettes outside the window.
        for (int j = 0; j < 6; ++j)
            r.box({-6.54f, 1.12f + (j % 3) * 0.14f, z - 1.3f + j * 0.52f},
                  {0.02f, 0.7f + (j % 3) * 0.28f, 0.62f}, {0.44f + 0.035f * (j % 2), 0.56f, 0.37f});
        for (float offset : {-1.65f, 0.0f, 1.65f})
            r.box({-6.43f, 2.3f, z + offset}, {0.16f, 2.45f, 0.08f}, timber);
        for (float y : {1.14f, 2.3f, 3.5f})
            r.box({-6.43f, y, z}, {0.16f, 0.08f, 3.38f}, timber);
        r.box({-6.30f, 1.08f, z}, {0.42f, 0.10f, 3.46f}, timber);
        // Short gathered curtains stay beside the glass, leaving the view open.
        for (float edge : {-1.45f, 1.45f})
            for (int fold = 0; fold < 4; ++fold) {
                float height = 1.45f + 0.18f * std::sin(z * 0.8f + edge) + 0.025f * (fold % 3);
                r.box({-6.28f + (fold % 2) * 0.045f, 3.45f - height / 2, z + edge + fold * 0.065f},
                      {0.09f, height, 0.08f}, glm::vec3(0.72f, 0.70f, 0.55f) * (1 - fold * 0.025f));
            }
        // Warm window-shaped light patches, kept below feet and puzzle strokes.
        for (int j = 0; j < 2; ++j)
            r.quad({-4.8f, 0.003f, z + 0.3f + j * 0.66f}, {1.35f, 0, -0.27f}, {0, 0, 0.29f},
                   {0.74f, 0.56f, 0.34f});
    }
    for (float z : {-2.7f, 1.4f, 5.5f}) {
        r.box({0, 4.40f, z}, {9.6f, 0.12f, 0.16f}, mint);
        for (float x : {-3.9f, -0.5f, 3.0f}) {
            r.box({x, 4.23f, z}, {1.15f, 0.15f, 0.32f}, {0.38f, 0.41f, 0.36f});
            r.box({x, 4.14f, z}, {1.02f, 0.025f, 0.25f}, {1.0f, 0.96f, 0.77f});
        }
    }

    // Twelve seats face the board; keep the front teaching area and side aisles open.
    int seat = 0;
    for (float z : deskRows)
        for (float x : deskColumns) {
            r.box({x, 0.74f, z}, {1.06f, 0.08f, 0.66f}, {0.68f, 0.44f, 0.23f});
            r.box({x, 0.58f, z}, {0.85f, 0.20f, 0.46f}, timber);
            for (float dx : {-0.42f, 0.42f})
                for (float dz : {-0.23f, 0.23f})
                    r.box({x + dx, 0.32f, z + dz}, {0.045f, 0.64f, 0.045f}, {0.27f, 0.33f, 0.31f});
            const auto chair = chairPose(seat, x, z);
            auto chairBox = [&](glm::vec3 offset, glm::vec3 size, glm::vec3 color) {
                glm::vec3 rotated{std::cos(chair.yaw) * offset.x + std::sin(chair.yaw) * offset.z, offset.y,
                                  -std::sin(chair.yaw) * offset.x + std::cos(chair.yaw) * offset.z};
                r.box(chair.center + rotated, size, color, chair.yaw);
            };
            chairBox({0, -0.01f, 0}, {0.48f, 0.07f, 0.48f}, timber);
            chairBox({0, 0.31f, 0.23f}, {0.48f, 0.30f, 0.055f}, timber);
            for (float dx : {-0.20f, 0.20f})
                for (float dz : {-0.20f, 0.20f})
                    chairBox({dx, -0.24f, dz}, {0.038f, 0.46f, 0.038f}, {0.27f, 0.33f, 0.31f});
            const glm::vec3 cover = seat % 3 == 0 ? glm::vec3(0.57f, 0.28f, 0.22f)
                                                  : (seat % 3 == 1 ? glm::vec3(0.24f, 0.40f, 0.42f)
                                                                   : glm::vec3(0.48f, 0.49f, 0.29f));
            if (seat % 4 != 3) {
                float angle = 0.22f * std::sin(seat * 2.3f + 0.4f);
                auto book = glm::vec3(x - 0.13f + 0.06f * std::sin(seat * 1.7f), 0.803f, z - 0.07f);
                r.box(book, {0.32f, 0.045f, 0.24f}, cover, angle);
                r.box(book + glm::vec3(0, 0.005f, 0), {0.28f, 0.028f, 0.21f}, {0.86f, 0.83f, 0.70f}, angle);
            }
            if (seat % 3 == 0) {
                r.box({x + 0.27f, 0.81f, z + 0.07f}, {0.25f, 0.065f, 0.10f}, {0.40f, 0.38f, 0.32f});
                r.line({x + 0.08f, 0.787f, z - 0.20f}, {x + 0.34f, 0.787f, z - 0.12f}, 0.012f, brass);
            } else if (seat % 3 == 1) {
                r.box({x + 0.30f, 0.90f, z - 0.15f}, {0.09f, 0.24f, 0.09f}, {0.44f, 0.59f, 0.55f});
                r.box({x + 0.30f, 1.03f, z - 0.15f}, {0.07f, 0.025f, 0.07f}, timber);
            } else {
                r.box({x + 0.15f, 0.786f, z + 0.11f}, {0.37f, 0.008f, 0.20f}, cream, -0.1f);
            }
            if (seat % 2 == 0) {
                chairBox({0.03f, 0.10f, 0.09f}, {0.31f, 0.36f, 0.16f}, cover * 0.7f);
                chairBox({0.03f, 0.04f, 0.18f}, {0.24f, 0.14f, 0.035f}, cover);
            }
            ++seat;
        }

    // The moving boards sit in a single frame. The numbered detents belong to each rail.
    r.box({-.85f, 2.23f, -4.86f}, {5.96f, 2.98f, .11f}, timber);
    for (float x : {-3.78f, -2.31f, -.85f, .61f, 2.08f})
        r.box({x, 2.23f, -4.78f}, {.036f, 2.89f, .064f}, {.38f, .42f, .38f});
    for (int i = 0; i < static_cast<int>(boardX.size()); ++i) {
        const float shift = boardShift(i), x = boardX[i], z = boardZ(i);
        r.box({x, 2.22f + shift, z}, {1.42f, 1.97f, .09f}, timber);
        r.box({x, 2.22f + shift, z + .055f}, {2 * boardHalf, 1.92f, .018f}, {.13f, .29f, .23f});
        r.box(boardHandle(i), {.20f, .095f, .10f}, {.40f, .45f, .40f});
        r.box({x, 3.67f, -4.27f}, {.42f, .25f, .03f}, cream * .91f);
        const wchar_t* values[] = {L"1", L"2", L"3", L"4"};
        r.label(values[boardPositions_[i]], {x, 3.67f, -4.25f}, {.062f, 0, 0}, {0, .083f, 0}, mint * .42f);
    }
    r.box({-.85f, .71f, -4.37f}, {5.96f, .07f, .44f}, {.39f, .42f, .33f});
    r.box({-2.75f, 1.25f, -4.17f}, {0.24f, 0.075f, 0.13f}, {0.27f, 0.28f, 0.24f});
    r.box({-2.3f, 1.24f, -4.16f}, {0.10f, 0.025f, 0.025f}, chalkWhite);
    r.box({-2.1f, 1.24f, -4.16f}, {0.09f, 0.025f, 0.025f}, {0.78f, 0.65f, 0.44f});
    // Teacher's lectern.
    r.box({1.0f, 0.48f, -3.25f}, {1.0f, 0.96f, 0.62f}, timber);
    r.box({1.0f, 1.0f, -3.25f}, {1.1f, 0.08f, 0.70f}, {0.66f, 0.44f, 0.23f});
    r.box({1.0f, 0.53f, -2.925f}, {0.75f, 0.68f, 0.018f}, {0.59f, 0.37f, 0.18f});
    r.box({1.0f, 0.10f, -3.25f}, {1.10f, 0.12f, 0.70f}, timber * 0.8f);

    // Teaching materials make the lectern read as a working desk, not a puzzle pedestal.
    r.box({0.79f, 1.06f, -3.22f}, {0.36f, 0.035f, 0.26f}, {0.88f, 0.86f, 0.76f});
    r.box({1.28f, 1.10f, -3.35f}, {0.12f, 0.16f, 0.12f}, {0.37f, 0.44f, 0.39f});
    for (int i = 0; i < 3; ++i)
        r.line({1.25f + i * 0.025f, 1.13f, -3.35f}, {1.24f + i * 0.028f, 1.29f, -3.32f}, 0.012f, timber);

    // A raised materials gallery rests on built-in storage, rather than an isolated exit pedestal.
    r.box({4.30f, 0.90f, -4.43f}, {4.40f, 1.80f, 1.20f}, timber * 0.83f);
    for (float x : {2.65f, 3.75f, 4.85f, 5.95f}) {
        r.box({x, 0.87f, -3.812f}, {1.04f, 1.64f, 0.025f}, {0.58f, 0.47f, 0.31f});
        if (x > 4.5f)
            r.box({x + 0.34f, 0.93f, -3.786f}, {0.035f, 0.13f, 0.03f}, timber * 0.65f);
    }
    r.box({4.30f, 1.79f, -4.43f}, {4.42f, 0.05f, 1.22f}, timber);
    for (float x : {3.95f, 4.75f, 5.55f, 6.35f})
        r.line({x, 1.8f, -3.78f}, {x, gallerySideRail.y, -3.78f}, 0.05f, timber);
    r.line({3.95f, gallerySideRail.y, -3.78f}, {6.40f, gallerySideRail.y, -3.78f}, 0.075f, timber);
    for (float x : {2.33f, 3.77f})
        r.box({x, 2.87f, -4.95f}, {0.12f, 2.22f, 0.15f}, timber);
    r.box({3.05f, 3.94f, -4.95f}, {1.56f, 0.12f, 0.15f}, timber);
    // A sliding safety rail protects the edge until the new stair flight meets the gallery.
    r.box(gallerySideRail, gallerySideRailSize, timber);
    for (float z : {-3.80f, -4.38f, -4.96f})
        r.box({gallerySideRail.x, galleryRailPostY, z}, {.055f, galleryRailHeight, .055f}, timber);
    r.box({4.0f, 1.822f, -3.834f}, {3.78f, .012f, .037f}, timber * .66f);
    const auto gate = galleryGateCenter();
    r.box(gate, galleryGateSize, timber);
    for (float x : {-.86f, 0.f, .86f})
        r.box({gate.x + x, galleryRailPostY, gate.z}, {.052f, galleryRailHeight, .045f}, timber);
    // The materials room door remains open behind the gallery.
    r.box({4.05f, 2.85f, -4.65f}, {0.09f, 2.08f, 1.3f}, mint);
    r.box({4.11f, 3.24f, -4.65f}, {0.015f, 0.72f, 0.87f}, {0.64f, 0.76f, 0.71f});
    r.box({3.05f, 1.77f, -6.05f}, {1.35f, 0.06f, 2.1f}, timber);
    for (float x : {2.27f, 3.83f})
        r.box({x, 3.1f, -6.05f}, {0.2f, 2.6f, 2.1f}, cream);
    for (int canvas = 0; canvas < 3; ++canvas) {
        r.box({3.58f - canvas * 0.10f, 2.55f, -6.45f}, {0.08f, 1.45f, 0.85f}, timber, 0.12f);
        r.box({3.53f - canvas * 0.10f, 2.55f, -6.45f}, {0.018f, 1.32f, 0.72f}, cream, 0.12f);
    }
    r.box({3.05f, 4.20f, -6.1f}, {0.80f, 0.06f, 0.30f}, {1.0f, 0.96f, 0.77f});
    r.box({3.05f, 4.4f, -6.05f}, {1.76f, 0.15f, 2.2f}, cream);
    corridor_.draw(r);
    if (stairsReady_)
        drawStairs(r);

    // The classroom's wooden floor continues through the projection area.
    const glm::vec3 paper{0.83f, 0.77f, 0.62f};
    r.box({1.0f, 0.52f, -2.91f}, {0.99f, 0.96f, 0.018f}, paper);
    r.box({1.552f, 0.52f, -3.25f}, {0.008f, 0.96f, 0.65f}, paper);
    // A pinned study covers the storage front at the same depth used by the projection.
    r.quad({3.30f, 0.90f, -3.777f}, {1.19f, 0, 0}, {0, 0.88f, 0}, paper);
    for (float x : {2.18f, 4.42f})
        r.box({x, 1.74f, -3.765f}, {0.07f, 0.06f, 0.018f}, timber);
    r.box({0.91f, 0.025f, 0.55f}, {0.24f, 0.045f, 0.15f}, {0.40f, 0.34f, 0.27f});
    for (int pencil = 0; pencil < 3; ++pencil)
        r.line({1.04f, 0.025f, 1.0f + pencil * 0.045f}, {1.30f, 0.025f, 1.04f + pencil * 0.045f}, 0.013f,
               pencil % 2 ? timber : brass);
    // A loose plant study reads as classwork, not another row of combination symbols.
    const float shift = boardShift(0);
    const float sketchZ = boardZ(0) + 0.08f;
    const glm::vec3 sketch = chalkWhite * 0.58f;
    r.line({-3.35f, 1.42f + shift, sketchZ}, {-2.98f, 1.42f + shift, sketchZ}, 0.010f, sketch);
    r.line({-3.35f, 1.42f + shift, sketchZ}, {-3.42f, 1.72f + shift, sketchZ}, 0.010f, sketch);
    r.line({-2.98f, 1.42f + shift, sketchZ}, {-2.91f, 1.72f + shift, sketchZ}, 0.010f, sketch);
    r.line({-3.42f, 1.72f + shift, sketchZ}, {-2.91f, 1.72f + shift, sketchZ}, 0.010f, sketch);
    for (int leaf = 0; leaf < 5; ++leaf) {
        float x = -3.43f + leaf * 0.12f, y = 1.91f + shift + (leaf % 2) * 0.20f;
        r.line({-3.17f, 1.70f + shift, sketchZ}, {x, y, sketchZ}, 0.009f, sketch);
        r.line({x, y, sketchZ}, {x - 0.10f, y - 0.035f, sketchZ}, 0.016f, sketch);
    }
    // Only board marks are permanent; the whole lower stair drawing rotates with the projector.
    if (!stairsReady_) {
        for (const Chalk& c : chalk_) {
            if (isSlideStroke(c))
                continue;
            for (int part = 0; part < 4; ++part) {
                auto a = glm::mix(c.projectedA, c.projectedB, part / 4.0f);
                auto b = glm::mix(c.projectedA, c.projectedB, (part + 1) / 4.0f);
                a.y += boardShift(c.panel);
                b.y += boardShift(c.panel);
                float visibility = (1 - projectorLight((a + b) / 2.0f)) * (1 - stairFormation());
                if (visibility > 0.01f)
                    r.line(a, b, c.detail ? 0.006f : 0.014f,
                           glm::mix(glm::vec3(0.13f, 0.29f, 0.23f), chalkWhite * 0.83f, visibility));
            }
        }
    }
    drawProjector(r);
    // Keep the support feet behind the glass and beside the stiles.
    const auto normal = mirrorNormal();
    const glm::vec3 right{normal.z, 0, -normal.x};
    const glm::vec3 baseRight{std::cos(mirrorStartYaw), 0, -std::sin(mirrorStartYaw)};
    if (!reflection) {
        for (float x : {-.74f, .74f}) {
            const auto side = baseRight * x;
            r.box(mirrorCenter + side + glm::vec3(0, -1.56f, 0), {.12f, .18f, .54f}, timber,
                  mirrorStartYaw);
        }
        r.box(mirrorCenter + glm::vec3(0, -1.51f, 0), {1.48f, .12f, .16f}, timber, mirrorStartYaw);
        r.box(mirrorCenter + glm::vec3(0, -1.24f, 0), {.14f, .56f, .14f}, brass * .70f);
        r.box(mirrorCenter - normal * .055f, {1.42f, 1.96f, .09f}, timber, mirrorYaw_);
        for (float sign : {-1.f, 1.f}) {
            r.box(mirrorCenter + right * (sign * .72f), {.065f, 2.02f, .09f}, timber, mirrorYaw_);
            r.box(mirrorCenter + glm::vec3(0, sign * 1.f, 0), {1.50f, .06f, .09f}, timber, mirrorYaw_);
        }
        const auto plaque = mirrorCenter + glm::vec3(0, -.92f, 0) + normal * .018f;
        r.box(plaque, {1.05f, .19f, .05f}, mint * .62f, mirrorYaw_);
        r.label(L"공책을 비춰 보기", plaque + normal * .028f, right * .43f, {0, .063f, 0}, chalkWhite);
    }
    // Rear cubbies, a reading bench and a stocked bookshelf establish the classroom's daily use.
    r.box(fixtures[0].center, fixtures[0].size, timber * 0.8f);
    for (int row = 0; row < 2; ++row)
        for (int col = 0; col < 6; ++col) {
            float x = -5.39f + col * 0.775f, y = 0.37f + row * 0.67f;
            r.box({x, y, 8.205f}, {0.72f, 0.60f, 0.035f},
                  glm::vec3(0.55f, 0.43f, 0.29f) * (0.93f + (col % 3) * 0.05f));
            r.box({x + 0.23f, y, 8.175f}, {0.045f, 0.10f, 0.03f}, mint * 0.7f);
            r.box({x, y + 0.17f, 8.18f}, {0.19f, 0.075f, 0.015f}, cream);
        }
    bookStack(r, {-4.9f, 1.4f, 8.5f}, 4);
    bookStack(r, {-4.25f, 1.4f, 8.5f}, 2);
    r.box(fixtures[2].center, fixtures[2].size, timber);
    r.box({0.65f, 0.56f, 8.35f}, {2.04f, 0.07f, 0.64f}, {0.61f, 0.47f, 0.28f});
    bookStack(r, {1.30f, 0.60f, 8.35f}, 2);
    // The open side faces into the room; books are varied in height and cover colour.
    r.box({6.29f, 1.08f, 6.15f}, {0.08f, 2.16f, 2.25f}, timber);
    for (float z : {5.06f, 7.24f})
        r.box({5.98f, 1.08f, z}, {0.70f, 2.16f, 0.07f}, timber);
    for (int shelf = 0; shelf < 4; ++shelf) {
        float y = 0.12f + shelf * 0.65f;
        r.box({5.98f, y, 6.15f}, {0.70f, 0.07f, 2.25f}, timber);
        if (shelf < 3)
            for (int book = 0; book < 12; ++book) {
                if ((book + shelf) % 7 == 0)
                    continue;
                float h = 0.30f + (book % 4) * 0.055f;
                glm::vec3 color = book % 3 == 0 ? glm::vec3(0.57f, 0.30f, 0.23f)
                                                : (book % 3 == 1 ? mint : glm::vec3(0.38f, 0.46f, 0.52f));
                r.box({5.90f, y + 0.04f + h / 2, 5.25f + book * 0.16f}, {0.38f, h, 0.115f}, color);
                r.box({5.703f, y + 0.13f, 5.25f + book * 0.16f}, {0.009f, 0.035f, 0.08f}, cream);
            }
    }
    r.box(artSuppliesCabinet.center, artSuppliesCabinet.size, timber);
    for (float z : {1.95f, 2.60f, 3.25f}) {
        r.box({5.68f, 0.46f, z}, {0.025f, 0.70f, 0.60f}, mint);
        r.box({5.65f, 0.55f, z + 0.17f}, {0.025f, 0.09f, 0.035f}, brass);
    }
    // A worn open portfolio tray sits on the existing supply cabinet.
    r.quad(notebookRest + glm::vec3(0, .003f, 0), {.251f, 0, 0}, {0, 0, -.81f}, {.38f, .36f, .26f});
    for (float z : {notebookRest.z - .82f, notebookRest.z + .82f})
        r.box({notebookRest.x, notebookRest.y + .01f, z}, {.52f, .022f, .025f}, timber * .90f);
    r.sign(L"미술 준비물", {5.65f, .75f, 2.60f}, {0, 0, .23f}, {0, .045f, 0});
    plant(r, {-5.95f, 0, 7.8f}, 1.25f);
    plant(r, {5.95f, 0, -0.8f}, 1.25f);
    for (float z : {-1.9f, 3.02f, 6.7f})
        plant(r, {-6.27f, 1.13f, z}, 0.65f);
    // Cleaning cupboard, broom and sorting bin sit against the walls, away from puzzle access.
    r.box(fixtures[6].center, fixtures[6].size, {0.53f, 0.57f, 0.48f});
    r.box({-5.60f, 1.0f, -3.2f}, {0.035f, 1.87f, 0.83f}, mint);
    r.box({-5.57f, 1.0f, -2.93f}, {0.04f, 0.16f, 0.04f}, timber);
    r.line({-6.05f, 0.15f, -2.3f}, {-6.27f, 1.72f, -2.3f}, 0.035f, timber);
    r.box({-6.05f, 0.12f, -2.3f}, {0.15f, 0.20f, 0.44f}, {0.63f, 0.52f, 0.29f});
    r.box(fixtures[5].center, fixtures[5].size, {0.31f, 0.42f, 0.40f});
    r.box({5.9f, 0.61f, 8.25f}, {0.49f, 0.045f, 0.49f}, mint);
    r.box({5.9f, 0.64f, 8.25f}, {0.28f, 0.012f, 0.22f}, {0.17f, 0.23f, 0.20f});
    r.box({-1.0f, 2.35f, 8.94f}, {3.15f, 1.12f, 0.07f}, timber);
    r.box({-1.0f, 2.35f, 8.89f}, {3.0f, 0.98f, 0.025f}, {0.90f, 0.86f, 0.72f});
    for (float x : {-2.40f, 0.40f})
        r.box({x, 2.76f, 8.865f}, {0.05f, 0.05f, 0.025f}, brass);
    // Small noticeboard, clock, room plate and rear entrance establish a familiar classroom.
    r.box({6.45f, 2.35f, 2.6f}, {0.05f, 1.65f, 3.3f}, timber);
    r.box({6.41f, 2.35f, 2.6f}, {0.025f, 1.53f, 3.18f}, {0.68f, 0.60f, 0.43f});
    for (int i = 0; i < 6; ++i) {
        float z = 1.35f + (i % 3) * 1.15f, y = 1.95f + (i / 3) * 0.74f;
        float angle = 0.045f * std::sin(i * 2.1f + 0.7f);
        glm::vec3 paperRight{0, std::sin(angle), std::cos(angle)}, up{0, std::cos(angle), -std::sin(angle)};
        r.quad({6.38f, y, z}, paperRight * 0.39f, up * 0.29f, cream);
        r.box(glm::vec3(6.35f, y, z) + up * 0.22f, {0.02f, 0.045f, 0.06f}, i % 2 ? mint : brass);
        // Student landscapes: sky, hills and a small house on pinned sheets.
        r.quad(glm::vec3(6.36f, y, z) - up * 0.04f, paperRight * 0.31f, up * 0.17f, {0.58f, 0.73f, 0.72f});
        r.quad(glm::vec3(6.34f, y, z) - up * 0.14f, paperRight * 0.31f, up * 0.07f, {0.43f, 0.57f, 0.33f});
        if (i % 3 == 0) {
            r.box({6.32f, y - 0.05f, z + 0.06f}, {0.012f, 0.14f, 0.16f}, {0.65f, 0.42f, 0.27f});
            r.line({6.31f, y + 0.02f, z - 0.04f}, {6.31f, y + 0.12f, z + 0.06f}, 0.025f, timber);
            r.line({6.31f, y + 0.12f, z + 0.06f}, {6.31f, y + 0.02f, z + 0.16f}, 0.025f, timber);
        } else if (i % 3 == 1) {
            for (float offset : {-0.14f, 0.13f}) {
                r.box({6.32f, y - 0.06f, z + offset}, {0.012f, 0.20f, 0.028f}, timber);
                r.box({6.31f, y + 0.03f, z + offset}, {0.012f, 0.17f, 0.14f}, mint);
            }
        } else {
            r.line({6.32f, y - 0.10f, z - 0.26f}, {6.32f, y + 0.11f, z - 0.04f}, 0.07f, mint);
            r.line({6.32f, y + 0.11f, z - 0.04f}, {6.32f, y - 0.10f, z + 0.22f}, 0.07f, mint);
        }
        r.box({6.30f, y + 0.08f, z + (i % 2 ? 0.22f : -0.22f)}, {0.012f, 0.07f, 0.07f}, brass);
    }
    // Timetable and duty chart on the rear wall.
    for (float x : {1.40f, 2.95f}) {
        r.box({x, 2.35f, 8.94f}, {1.32f, 1.45f, 0.06f}, timber);
        r.box({x, 2.35f, 8.90f}, {1.20f, 1.33f, 0.025f}, cream);
        for (int row = 0; row < 5; ++row)
            for (int col = 0; col < 4; ++col)
                r.box({x - 0.43f + col * 0.285f, 1.89f + row * 0.21f, 8.88f}, {0.23f, 0.13f, 0.008f},
                      (row + col) % 3 ? glm::vec3(0.71f, 0.74f, 0.61f) : mint);
    }
    r.box({4.8f, 1.2f, 8.94f}, {1.3f, 2.4f, 0.08f}, mint);
    r.box({4.8f, 1.65f, 8.88f}, {0.86f, 0.78f, 0.025f}, {0.65f, 0.76f, 0.71f});
    r.box({5.25f, 1.08f, 8.85f}, {0.08f, 0.08f, 0.06f}, brass);
    // Wall speaker and pin rail are decorative, with no extra interaction prompts.
    r.box({5.65f, 3.75f, -4.86f}, {0.50f, 0.65f, 0.16f}, {0.39f, 0.43f, 0.36f});
    for (int i = 0; i < 6; ++i)
        r.box({5.65f, 3.55f + i * 0.075f, -4.77f}, {0.36f, 0.018f, 0.018f}, mint * 0.55f);
    for (int i = 0; i < 48; ++i) {
        float a = i * 6.2831853f / 48, b = (i + 1) * 6.2831853f / 48;
        r.line({-3.75f + std::cos(a) * 0.28f, 3.80f + std::sin(a) * 0.28f, -4.87f},
               {-3.75f + std::cos(b) * 0.28f, 3.80f + std::sin(b) * 0.28f, -4.87f}, 0.045f, timber);
    }
    r.line({-3.75f, 3.8f, -4.84f}, {-3.75f, 4.0f, -4.84f}, 0.025f, timber);
    r.line({-3.75f, 3.8f, -4.84f}, {-3.59f, 3.73f, -4.84f}, 0.028f, timber);
    r.flush();
    // These are ordinary lesson headings, not instructions or a combination code.
    r.label(L"미술 · 공간 스케치", {-2.1f, 3.54f, -4.86f}, {0.93f, 0, 0}, {0, 0.12f, 0}, mint * 0.65f);
    r.label(L"교실에서 찾은 형태", {boardX[0], 2.90f + boardShift(0), boardZ(0) + 0.081f}, {0.69f, 0, 0},
            {0, 0.075f, 0}, chalkWhite * 0.8f);
    r.label(L"공간 드로잉", {boardX[1], 2.9f + boardShift(1), boardZ(1) + 0.081f}, {0.60f, 0, 0},
            {0, 0.075f, 0}, chalkWhite * 0.8f);
    r.sign(L"자료실", {3.05f, 4.17f, -4.89f}, {.54f, 0, 0}, {0, .17f, 0});
    r.label(L"미술 작품전", {-1.0f, 2.63f, 8.865f}, {-0.65f, 0, 0}, {0, 0.12f, 0}, mint * 0.65f);
    r.label(L"우리 학교의 공간", {-1.0f, 2.30f, 8.865f}, {-0.95f, 0, 0}, {0, 0.13f, 0}, mint * 0.65f);
    r.label(L"스케치 · 종이 · 색연필", {-1.0f, 1.99f, 8.865f}, {-0.94f, 0, 0}, {0, 0.09f, 0}, mint * 0.65f);
    r.sign(L"미술실", {4.80f, 2.70f, 8.89f}, {-.55f, 0, 0}, {0, .17f, 0});
    r.label(L"시간표", {1.40f, 2.91f, 8.87f}, {-0.33f, 0, 0}, {0, 0.095f, 0}, mint * 0.65f);
    r.label(L"우리 반 약속", {2.95f, 2.91f, 8.87f}, {-0.48f, 0, 0}, {0, 0.095f, 0}, mint * 0.65f);
    r.label(L"우리들의 그림", {6.36f, 3.36f, 2.60f}, {0, 0, 0.70f}, {0, 0.11f, 0}, mint * 0.65f);
    r.label(L"학급 문고", {6.28f, 2.45f, 6.15f}, {0, 0, 0.48f}, {0, 0.11f, 0}, mint * 0.65f);
    // Translucent formation follows opaque room geometry, including the reflection pass.
    if (materializing_)
        drawStairs(r);
}
void SchoolRoom::drawStairs(Renderer& r) const {
    const float progress = stairFormation();
    r.opacity(progress * 0.18f);
    r.quad({2.7f, 0.014f, -2.0f}, {0.93f, 0, 0}, {0, 0, -1.80f}, {0.18f, 0.14f, 0.10f});
    r.opacity(progress);
    // Each tread first reads as a thin cover, then gains its full supporting volume.
    for (int i = 0; i < 6; ++i) {
        float h = (i + 1) * 0.3f, thickness = std::max(0.014f, h * progress);
        r.box({2.7f, h - thickness / 2, -0.5f - i * 0.6f}, {1.8f, thickness, 0.6f}, timber * 0.86f);
        r.box({2.7f, h + 0.004f, -0.5f - i * 0.6f}, {1.82f, 0.018f, 0.60f}, {0.66f, 0.49f, 0.29f});
        r.line({1.8f, h, -0.2f - i * 0.6f}, {1.8f, h + 1.25f, -0.2f - i * 0.6f}, 0.014f + progress * 0.036f,
               timber);
        if (i < 5)
            r.line({1.8f, h + 1.25f, -0.2f - i * 0.6f}, {1.8f, h + 1.55f, -0.8f - i * 0.6f},
                   0.014f + progress * 0.051f, timber);
    }
    r.opacity(1);
}
std::optional<AlignmentFeedback> SchoolRoom::alignmentFeedback(const Camera& camera) const {
    if (projectorAligned_ && !adjustingProjector_ && !materializing_ && camera.feet.z >= -4.9f) {
        if (const auto score = stairViewAlignment(camera))
            return AlignmentFeedback{*score, stairAlignmentThreshold, *score >= stairAlignmentThreshold,
                                     std::clamp(alignmentHeld_ / recognitionDuration, 0.f, 1.f)};
    }
    return corridor_.alignmentFeedback(camera);
}
const wchar_t* SchoolRoom::interactionLabel(const Camera& camera) const {
    if (adjustingProjector_ || adjustingMirror_)
        return L"마침";
    if (canAdjustProjector(camera))
        return L"돌리기";
    if (notebookRatio_ > 0)
        return nullptr;
    if (canGrabNotebook(camera))
        return L"집기";
    if (canReadNotebook(camera))
        return L"읽기";
    if (canTurnMirror(camera))
        return L"돌리기";
    if (aimedBoard(camera) >= 0)
        return L"밀기";
    if (const auto action = corridor_.classroomInteractionLabel(camera))
        return action;
    if (canUseCupboard(camera))
        return cupboardOpen() ? L"닫기" : L"열기";
    return nullptr;
}
void SchoolRoom::drawNotebookPages(Renderer& r, int width, int height, float scale) const {
    const float x = width / 2.f - 470 * scale, y = height / 2.f - 240 * scale;
    const glm::vec3 ink{.15f, .24f, .22f}, rule{.75f, .79f, .75f};
    r.panel(x - 10 * scale, y + 8 * scale, 960 * scale, 486 * scale, {.14f, .19f, .16f});
    r.panel(x - 7 * scale, y - 4 * scale, 954 * scale, 488 * scale, {.27f, .37f, .28f});
    r.panel(x, y + 6 * scale, 940 * scale, 480 * scale, {.78f, .75f, .61f});
    r.panel(x, y, 470 * scale, 480 * scale, {.96f, .94f, .85f});
    r.panel(x + 470 * scale, y, 470 * scale, 480 * scale, {.94f, .92f, .83f});
    r.panel(x + 466 * scale, y, 4 * scale, 480 * scale, {.84f, .80f, .68f});
    r.panel(x + 470 * scale, y, 2 * scale, 480 * scale, {.78f, .75f, .64f});

    const float left = x + 38 * scale, right = x + 506 * scale;
    for (float page : {left, right}) {
        r.panel(page - 12 * scale, y + 24 * scale, scale, 403 * scale, {.85f, .70f, .69f});
        for (int line = 0; line < 4; ++line)
            r.panel(page - 6 * scale, y + (315 + line * 38) * scale, 398 * scale, scale, rule);
    }
    r.text(L"칠판 숫자 맞추기", left, y + 24 * scale, 34 * scale, ink);
    r.text(L"E로 칠판 밀기", right, y + 24 * scale, 34 * scale, ink);
    r.text(L"칠판을 바라보고, 왼쪽부터", left, y + 70 * scale, 22 * scale, ink);
    r.text(L"한 번 누르면 두 판이 움직여요", right, y + 70 * scale, 22 * scale, ink);
    const wchar_t* order[]{L"첫째", L"둘째", L"셋째", L"넷째"};
    for (int i = 0; i < 4; ++i) {
        const float panelX = left + (8 + i * 96) * scale;
        r.panel(panelX, y + 142 * scale, 74 * scale, 74 * scale, timber);
        r.panel(panelX + 4 * scale, y + 146 * scale, 66 * scale, 66 * scale, mint * .65f);
        r.panel(panelX + 16 * scale, y + 103 * scale, 42 * scale, 32 * scale, rule);
        const float numberWidth = r.textWidth(notebookPanelLabels[i], 27 * scale);
        r.text(notebookPanelLabels[i], panelX + (74 * scale - numberWidth) * .5f,
               y + 103 * scale, 27 * scale, ink);
        const float labelWidth = r.textWidth(order[i], 20 * scale);
        r.text(order[i], panelX + (74 * scale - labelWidth) * .5f, y + 237 * scale, 20 * scale, ink);
    }
    for (int i = 0; i < 2; ++i) {
        const float panelX = right + (8 + i * 190) * scale;
        const auto label = i == 0 ? L"선택한 판" : L"오른쪽 판";
        const float labelWidth = r.textWidth(label, 22 * scale);
        r.text(label, panelX + (156 * scale - labelWidth) * .5f, y + 103 * scale, 22 * scale, ink);
        r.panel(panelX, y + 132 * scale, 156 * scale, 84 * scale, timber);
        r.panel(panelX + 4 * scale, y + 136 * scale, 148 * scale, 76 * scale, mint * .65f);
        const auto amount = i == 0 ? L"+1" : L"+2";
        const float amountWidth = r.textWidth(amount, 44 * scale);
        r.text(amount, panelX + (156 * scale - amountWidth) * .5f, y + 149 * scale, 44 * scale);
    }
    r.text(L"→", right + 166 * scale, y + 155 * scale, 28 * scale, ink);
    r.panel(right + 68 * scale, y + 228 * scale, 36 * scale, 29 * scale, ink);
    r.text(L"E", right + 79 * scale, y + 229 * scale, 23 * scale);
    for (int i = 0; i < 4; ++i) {
        const float lineY = y + (282 + i * 38) * scale;
        r.text(notebookClues[i], left, lineY, 30 * scale, ink);
        r.text(notebookControls[i], right, lineY, 28 * scale, ink);
    }
    const float closeX = x + 826 * scale, closeY = y + 441 * scale;
    r.panel(closeX, closeY, 25 * scale, 25 * scale, {.25f, .35f, .29f});
    r.text(L"E", closeX + 5 * scale, closeY + scale, 23 * scale);
    r.text(L"닫기", closeX + 32 * scale, closeY + scale, 23 * scale, ink);
}
void SchoolRoom::hud(Renderer& r, const Camera& camera, int width, int height,
                     const std::optional<perspective::Pose>& placement) const {
    const float scale = std::min(width / 1280.0f, height / 800.0f);
    r.overlay(width, height);
    if (mirrorGuide_ && canReadNotebook(camera)) {
        drawNotebookPages(r, width, height, scale);
        glEnable(GL_DEPTH_TEST);
        return;
    }
    if (escaped_) {
        r.panel(24 * scale, 24 * scale, 254 * scale, 45 * scale, {.12f, .22f, .18f});
        r.text(L"학교를 마쳤습니다.", 39 * scale, 31 * scale, 27 * scale);
    }
    const auto action = placement ? (onReflectionStand(*placement) ? L"거치" : L"놓기") : interactionLabel(camera);
    if (action) {
        const float actionWidth = r.textWidth(action, 23 * scale);
        const float x = width / 2.f - (39 * scale + actionWidth) / 2.f;
        const float y = height / 2.f + 11 * scale;
        r.panel(x, y, 39 * scale + actionWidth, 29 * scale, {.12f, .22f, .18f});
        r.panel(x + 3 * scale, y + 3 * scale, 23 * scale, 23 * scale, {.89f, .89f, .80f});
        r.text(L"E", x + 7 * scale, y + 4 * scale, 21 * scale, {.12f, .22f, .18f});
        r.text(action, x + 32 * scale, y + 3 * scale, 23 * scale);
    } else if (const auto feedback = alignmentFeedback(camera)) {
        const float trackWidth = 58 * scale;
        const float trackHeight = std::max(2.f, 3 * scale);
        const float x = (width - trackWidth) / 2.f, y = height / 2.f + 11 * scale;
        const float fill = feedback->aligned ? 1.f : std::min(feedback->score / feedback->threshold, .98f);
        r.panel(x, y, trackWidth, trackHeight, {.12f, .22f, .18f});
        if (fill > 0)
            r.panel(x, y, trackWidth * fill, trackHeight, {.96f, .96f, .86f});
        if (feedback->holdProgress > 0)
            r.panel(x, y, trackWidth * feedback->holdProgress, trackHeight, {.43f, .86f, .65f});
    }
    if (boardsReady() && !stairsReady_ && camera.feet.z >= -4.9f) {
        const wchar_t* status = materializing_ ? L"계단이 만들어지고 있어요"
                                : !projectorAligned_ ? L"칠판 완료 · 프로젝터 방향을 맞춰 주세요"
                                : adjustingProjector_ ? L"투사 완료 · E로 조정을 마쳐 주세요"
                                : L"투사 완료 · 오른쪽 통로에서 계단 그림을 바라보세요";
        const float textWidth = r.textWidth(status, 23 * scale);
        const float x = (width - textWidth) * .5f;
        r.panel(x - 12 * scale, height - 60 * scale, textWidth + 24 * scale, 36 * scale,
                {.12f, .22f, .18f});
        r.text(status, x, height - 54 * scale, 23 * scale);
    } else if (time_ < 5.0f) {
        r.panel(24 * scale, height - 60 * scale, 255 * scale, 36 * scale, {.12f, .22f, .18f});
        r.text(L"WASD 이동 · 마우스 시점", 36 * scale, height - 54 * scale, 23 * scale);
    }
    const glm::vec3 dot{0.96f, 0.96f, 0.86f};
    r.panel(width / 2.0f - 2.5f, height / 2.0f - 2.5f, 5, 5, {0.12f, 0.22f, 0.18f});
    r.panel(width / 2.0f - 1.0f, height / 2.0f - 1.0f, 2, 2, dot);
    glEnable(GL_DEPTH_TEST);
}
void SchoolRoom::draw(Renderer& r, const Camera& camera, int width, int height) const {
    glm::vec3 eye = camera.eye(), normal = mirrorNormal();
    glm::mat4 projection = glm::perspective(glm::radians(68.0f), float(width) / height, 0.05f, 50.0f);
    const auto target = eye + camera.direction();
    const auto viewProjection = projection * glm::lookAt(eye, target, glm::vec3(0, 1, 0));
    const auto glass = mirrorCenter + normal * .006f;
    const glm::vec3 mirrorRight{normal.z * mirrorHalfWidth, 0, -normal.x * mirrorHalfWidth};
    const glm::vec3 mirrorUp{0, mirrorHalfHeight, 0};
    const bool reflect = glm::dot(eye - mirrorCenter, normal) > 0 &&
                         Renderer::visible(viewProjection, glass, glm::abs(mirrorRight) + mirrorUp);
    const auto placement = notebookPlacement(camera);
    glm::mat4 reflectedVP{1};
    glEnable(GL_DEPTH_TEST);
    if (reflect) {
        const auto reflectedEye = Renderer::reflectPoint(eye, mirrorCenter, normal);
        const auto reflectedTarget = Renderer::reflectPoint(target, mirrorCenter, normal);
        const auto reflectedView = glm::lookAt(reflectedEye, reflectedTarget, glm::vec3(0, 1, 0));
        // Reflection reverses handedness. Flip clip X so the view maps back onto the mirror plane.
        glm::mat4 flip(1);
        flip[0][0] = -1;
        reflectedVP = fitReflection(flip * projection * reflectedView, glass, mirrorRight, mirrorUp);
        r.beginReflection();
        r.begin(reflectedVP, reflectedEye, glm::vec4(normal, -glm::dot(normal, mirrorCenter) - 0.01f));
        scene(r, camera, true, placement);
        r.flush();
    }
    r.endReflection(width, height);
    glClearColor(0.63f, 0.74f, 0.76f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    r.begin(viewProjection, eye);
    scene(r, camera, false, placement);
    if (reflect)
        r.mirror(glass, mirrorRight, mirrorUp, reflectedVP);
    hud(r, camera, width, height, placement);
}
} // namespace school
