#pragma once

#include "SchoolCorridor.h"
#include "../shared/PerspectiveCarry.h"

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
    glm::vec3 feet{4.8f, 0, 6.45f};
    float yaw = 0, pitch = -.28f, verticalSpeed = 0;
    bool grounded = true;
    glm::vec3 movement{}, motionFeet{};
    float stepOffset = 0;
    bool motionTracked = false;
    float focusTime = 0;
    glm::vec2 focusStart{}, focusDelta{};
    glm::vec3 eye() const { return feet + glm::vec3(0, 1.65f + stepOffset, 0); }
    void resetMotion() {
        movement = {};
        stepOffset = 0;
        motionTracked = false;
    }
    glm::vec3 direction() const;
    void face(glm::vec3 target);
    void focusOn(glm::vec3 target);
    void updateFocus(float dt);
};
class SchoolRoom {
  public:
    SchoolRoom();
    void reset();
    void update(Camera& camera, const Input& input, float elapsed, bool active = true);
    float alignment(const Camera& camera) const;
    bool moveBoard(const Camera& camera);
    bool inspectMirror(const Camera& camera);
    bool toggleMirror(const Camera& camera);
    void rotateMirror(float yaw);
    bool adjustingMirror() const { return adjustingMirror_; }
    void stopAdjustingMirror() { adjustingMirror_ = false; }
    static constexpr glm::vec3 notebookRest{6.02f, .88f, 2.60f};
    bool toggleNotebook(const Camera& camera);
    static constexpr glm::vec3 notebookStart{-6.27f, 1.328f, 3.80f};
    glm::vec3 notebookCenter() const { return notebook_.center; }
    bool boardsReady() const;
    static constexpr std::array<float, 4> boardX{-3.04f, -1.58f, -.12f, 1.34f};
    static constexpr std::array<int, 4> boardStart{1, 0, 1, 1}, boardGoal{1, 3, 0, 2};
    glm::vec3 boardHandle(int index) const;
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
    bool interactCorridor(const Camera& camera) {
        return corridor_.interactClassroom(camera) || toggleCupboard(camera);
    }
    bool cupboardOpen() const { return corridor_.doorOpen(); }
    float cupboardAngle() const { return corridor_.doorAngle(); }
    bool corridorDoorReady() const { return corridor_.doorwayReady(); }
    bool corridorDoorForming() const { return corridor_.doorwayForming(); }
    float corridorDoorFormation() const { return corridor_.doorwayFormation(); }
    float corridorAlignment(const Camera& camera) const { return corridor_.alignment(camera); }
    bool corridorBridgeReady() const { return corridor_.bridgeReady(); }
    float corridorBridgeAlignment(const Camera& camera) const { return corridor_.bridgeAlignment(camera); }
    glm::vec3 mirrorNormal() const;
    std::vector<Box> collisionBoxes(bool includeFixtures = true, bool includeNotebook = true) const;
    static float rayBox(glm::vec3 origin, glm::vec3 direction, const Box& box);
    static constexpr float radius = 0.22f;
    static constexpr float bodyHeight = 1.75f;
    static constexpr float landingHeight = 1.8f;
    static constexpr glm::vec3 alignmentEye{4.05f, 1.65f, 2.4f};
    static constexpr glm::vec3 alignmentFocus{2.65f, 1.55f, -2.1f};
    static constexpr glm::vec3 mirrorCenter{-5.05f, 1.65f, 0.7f};
    static constexpr glm::vec3 mirrorAim{mirrorCenter.x, mirrorCenter.y - .035f, mirrorCenter.z};
    static constexpr glm::vec3 projectorOrigin{4.8f, 1.14f, 4.65f};
    static constexpr glm::vec3 projectorPaper{3.30f, 0.10f, -3.757f};
    static constexpr glm::vec3 projectorStartAim{-0.85f, 2.30f, -4.50f};
    static constexpr glm::vec4 artworkFrame{-4.05f, .73f, 2.30f, 3.75f};
    static constexpr float recognitionDuration = 0.25f;
    static constexpr float formationDuration = 0.65f;

  private:
    friend int runTests();
    static constexpr float stairAlignmentThreshold = .85f;
    static constexpr float projectorAimToleranceDegrees = 4.f;
    struct Chalk {
        glm::vec3 projectedA, projectedB, originalA, originalB;
        int panel = -1;
        bool detail = false;
    };
    int aimedBoard(const Camera& camera) const;
    std::optional<float> mirrorHit(glm::vec3 eye, glm::vec3 ray) const;
    bool canTurnMirror(const Camera& camera) const;
    bool canReadNotebook(const Camera& camera) const;
    bool canGrabNotebook(const Camera& camera) const;
    std::optional<perspective::Pose> notebookPlacement(const Camera& camera) const;
    perspective::Pose heldNotebookPose(const Camera& camera) const;
    float notebookSupport(const Box& area, const std::vector<Box>& solids) const;
    void updateNotebookFall(const Camera& camera, float dt);
    Box notebookBounds(const perspective::Pose& pose) const;
    void drawNotebook(Renderer& renderer, const Camera& camera,
                      const std::optional<perspective::Pose>& placement) const;
    bool projectorInReach(const Camera& camera) const;
    static void advanceBoards(std::array<int, 4>& positions, int index);
    float boardShift(int index) const;
    void makeChalk();
    void move(Camera& camera, const Input& input, float dt);
    void scene(Renderer& renderer, const Camera& camera, bool reflection,
               const std::optional<perspective::Pose>& placement) const;
    const wchar_t* interactionLabel(const Camera& camera) const;
    void drawNotebookPages(Renderer& renderer, int width, int height, float scale) const;
    void hud(Renderer& renderer, const Camera& camera, int width, int height,
             const std::optional<perspective::Pose>& placement) const;
    std::optional<float> stairViewAlignment(const Camera& camera) const;
    std::optional<AlignmentFeedback> alignmentFeedback(const Camera& camera) const;
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
    glm::vec3 galleryGateCenter() const;
    std::vector<Chalk> chalk_;
    std::vector<std::array<glm::vec3, 4>> pageFaces_;
    glm::vec4 slideBounds_{};
    static constexpr int slideResolution = 256;
    std::vector<float> slideMask_;
    std::array<int, 4> boardPositions_ = boardStart;
    std::array<float, 4> boardOffsets_{};
    std::array<float, 4> boardFrom_{};
    float boardMotion_ = 0;
    bool mirrorGuide_ = false, adjustingMirror_ = false;
    float mirrorYaw_ = 0;
    perspective::Pose notebook_{notebookStart, 1.f};
    float notebookRatio_ = 0.f, notebookFallSpeed_ = 0.f;
    bool notebookFalling_ = false;
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
