#pragma once

#include "SchoolCorridor.h"

#include <GL/glm/glm.hpp>
#include <array>
#include <string>
#include <vector>

namespace school {
class Renderer;
struct Box {
    glm::vec3 min, max;
};
struct Input {
    float forward = 0, right = 0;
    bool jump = false;
};
struct Camera {
    glm::vec3 feet{4.8f, 0, 7.6f};
    float yaw = -0.46f, pitch = 0.005f, verticalSpeed = 0;
    bool grounded = true;
    glm::vec3 movement{}, motionFeet{};
    float stepOffset = 0;
    bool motionTracked = false;
    glm::vec3 eye() const { return feet + glm::vec3(0, 1.65f + stepOffset, 0); }
    void resetMotion() {
        movement = {};
        stepOffset = 0;
        motionTracked = false;
    }
    glm::vec3 direction() const;
    void face(glm::vec3 target);
};
class SchoolRoom {
  public:
    SchoolRoom();
    void reset();
    void update(Camera& camera, const Input& input, float elapsed, bool active = true);
    float alignment(const Camera& camera) const;
    bool canAdjustProjector(const Camera& camera) const;
    bool toggleProjector(const Camera& camera);
    void rotateProjector(float yaw, float pitch);
    void stopAdjustingProjector() { adjustingProjector_ = false; }
    bool adjustingProjector() const { return adjustingProjector_; }
    bool projectorAligned() const { return projectorAligned_; }
    float boardVisibility() const;
    void draw(Renderer& renderer, const Camera& camera, int width, int height) const;
    bool stairsReady() const { return stairsReady_; }
    bool materializing() const { return materializing_; }
    float stairFormation() const { return glm::smoothstep(0.0f, 1.0f, formation_); }
    bool escaped() const { return escaped_; }
    bool canUseCupboard(const Camera& camera) const { return corridor_.canInteract(camera); }
    bool toggleCupboard(const Camera& camera) { return corridor_.interact(camera); }
    bool cupboardOpen() const { return corridor_.doorOpen(); }
    float cupboardAngle() const { return corridor_.doorAngle(); }
    bool corridorDoorReady() const { return corridor_.doorwayReady(); }
    bool corridorDoorForming() const { return corridor_.doorwayForming(); }
    float corridorAlignment(const Camera& camera) const { return corridor_.alignment(camera); }
    bool canOpenCorridorDoor(const Camera& camera) const { return corridor_.canOpenPassage(camera); }
    bool openCorridorDoor(const Camera& camera) { return corridor_.openPassage(camera); }
    bool corridorBridgeReady() const { return corridor_.bridgeReady(); }
    float corridorBridgeAlignment(const Camera& camera) const { return corridor_.bridgeAlignment(camera); }
    glm::vec3 mirrorNormal() const;
    std::vector<Box> collisionBoxes(bool includeFixtures = true) const;
    static float rayBox(glm::vec3 origin, glm::vec3 direction, const Box& box);
    static constexpr float radius = 0.22f;
    static constexpr float bodyHeight = 1.75f;
    static constexpr float landingHeight = 1.8f;
    static constexpr glm::vec3 alignmentEye{4.05f, 1.65f, 2.4f};
    static constexpr glm::vec3 alignmentFocus{2.65f, 1.55f, -2.1f};
    static constexpr glm::vec3 mirrorCenter{-5.05f, 1.65f, 0.7f};
    static constexpr glm::vec3 projectorOrigin{4.8f, 1.14f, 4.65f};
    static constexpr glm::vec3 projectorPaper{3.30f, 0.10f, -3.757f};
    static constexpr glm::vec3 projectorStartAim{-0.85f, 2.30f, -4.50f};
    static constexpr glm::vec4 artworkFrame{-4.05f, 1.08f, 2.30f, 3.38f};
    static constexpr float recognitionDuration = 0.4f;
    static constexpr float formationDuration = 0.65f;

  private:
    friend int runTests();
    struct Chalk {
        glm::vec3 projectedA, projectedB, originalA, originalB;
        int panel = -1;
        bool detail = false;
    };
    void makeChalk();
    void move(Camera& camera, const Input& input, float dt);
    void scene(Renderer& renderer, const Camera& camera, bool reflection) const;
    void hud(Renderer& renderer, const Camera& camera, int width, int height) const;
    struct ProjectionHit {
        glm::vec3 point{}, normal{};
        int surface = -1;
    };
    glm::mat3 projectorRotation() const;
    ProjectionHit projectSlide(glm::vec3 point) const;
    bool isSlideStroke(const Chalk& stroke) const;
    bool projectorCovers(glm::vec3 point) const;
    float projectorLight(glm::vec3 point) const;
    glm::mat3 artworkProjection() const;
    float artworkCoverage(glm::vec3 point) const;
    float projectorError() const;
    glm::vec4 slideBounds() const;
    void makeSlideMask();
    float slideLight(glm::vec2 source) const;
    void drawProjector(Renderer& renderer) const;
    void drawStairs(Renderer& renderer) const;
    std::vector<Chalk> chalk_;
    std::vector<std::array<glm::vec3, 4>> pageFaces_;
    glm::vec4 slideBounds_{};
    static constexpr int slideResolution = 256;
    std::vector<float> slideMask_;
    float time_ = 0;
    float alignmentHeld_ = 0, formation_ = 0;
    bool materializing_ = false;
    float projectorYaw_ = 0, projectorPitch_ = 0;
    bool adjustingProjector_ = false, projectorAligned_ = false;
    bool stairsReady_ = false, escaped_ = false;
    SchoolCorridor corridor_;
};
int runTests();
} // namespace school
