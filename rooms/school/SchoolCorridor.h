#pragma once

#include "../shared/PerspectiveCarry.h"

#include <GL/glm/glm.hpp>
#include <array>
#include <optional>
#include <vector>

namespace school {
struct Box;
struct Camera;
class Renderer;

struct AlignmentFeedback {
    float score, threshold;
    bool aligned;
    float holdProgress = 0;
};

// Near-threshold jitter spends progress; an invalid view resets it immediately.
float advanceAlignmentHold(float held, float score, float threshold, float dt);

// School-owned continuation. The shared inter-stage connector is unchanged.
class SchoolCorridor {
  public:
    SchoolCorridor();
    void reset();
    void update(const Camera& camera, float dt);
    bool canInteract(const Camera& camera) const;
    bool interact(const Camera& camera);
    bool interactClassroom(const Camera& camera);
    const wchar_t* classroomInteractionLabel(const Camera& camera) const;
    bool holdingClassroomKey() const { return classroomKeyTaken_ && !classroomDoorUnlocked_; }
    const wchar_t* classroomKeyStatus(const Camera& camera) const;
    void drawHeldKey(Renderer& renderer, const Camera& camera) const;
    bool finished(const Camera& camera) const;
    bool doorOpen() const { return targetOpen_; }
    float doorAngle() const { return angle_; }
    float alignment(const Camera& camera) const { return doorViewAlignment(camera).value_or(0); }
    std::optional<AlignmentFeedback> alignmentFeedback(const Camera& camera) const;
    bool doorwayForming() const { return forming_; }
    bool doorwayReady() const { return opening_ >= 1; }
    bool bridgeReady() const { return bridgeFormation_ >= 1; }
    float bridgeAlignment(const Camera& camera) const { return bridgeViewAlignment(camera).value_or(0); }
    float doorwayFormation() const { return formation_; }
    float doorwayOpening() const { return opening_; }
    std::vector<Box> collisionBoxes() const;
    void draw(Renderer& renderer) const;
    static constexpr float floorHeight = 1.8f;
    static constexpr glm::vec3 classroomKeySpot{9.44f, 3.231f, -9.79f};
    static constexpr glm::vec3 classroomRearDoor{11.675f, 2.951f, -11.73f};
    static constexpr glm::vec3 classroomLock{12.125f, 3.251f, -11.81f};
    static constexpr glm::vec3 hinge{19.30f, floorHeight, -11.48f};
    static constexpr float doorWidth = 1.20f;
    static constexpr float doorHeight = 2.30f;
    static constexpr glm::vec3 illusionEye{14.50f, 3.45f, -10.95f};
    static constexpr glm::vec3 illusionFocus{20.45f, 3.0f, -9.90f};
    static constexpr float recognitionDuration = .55f;
    static constexpr float formationDuration = 1.8f;
    static constexpr float openingDuration = .9f;
    static constexpr glm::vec3 bridgeEye{23.95f, 3.45f, -10.45f};
    static constexpr glm::vec3 bridgeFocus{23.95f, 1.8f, -13.825f};
    static constexpr float exitZ = -21.85f;

  private:
    friend int runTests();
    enum class ClassroomAction { none, takeKey, unlockDoor };
    struct Solid {
        glm::vec3 center, size, color;
        bool visible = true;
        float bevel = 0;
    };
    struct Window {
        float start, end;
    };
    enum class DoorMaterial { infill, timber, paper, ruling };
    struct InkPatch {
        std::array<glm::vec3, 4> target, receiver;
        glm::vec3 color;
        bool onLeaf, frame;
        DoorMaterial material = DoorMaterial::infill;
    };
    void build();
    void buildDrawing();
    std::optional<float> doorViewAlignment(const Camera& camera) const;
    std::optional<float> bridgeViewAlignment(const Camera& camera) const;
    void updateIllusion(const Camera& camera, float dt);
    void drawIllusion(Renderer& renderer) const;
    void drawCupboardNotices(Renderer& renderer) const;
    void buildBridgeDrawing();
    void drawFalseHall(Renderer& renderer) const;
    void drawClassroomLock(Renderer& renderer) const;
    ClassroomAction classroomAction(const Camera& camera) const;
    Box classroomDoorBox(float opening) const;
    std::optional<perspective::Pose> keyAtLock(const Camera& camera) const;
    perspective::Pose carriedKeyPose(const Camera& camera) const;
    void updateKey(const Camera& camera, float dt);
    void drawBridge(Renderer& renderer) const;
    void updateBridge(const Camera& camera, float dt);
    glm::vec3 paintedPoint(const InkPatch& patch, int corner) const;
    Box passageBlocker() const;
    std::vector<Box> doorBoxes(float angle) const;
    std::vector<Solid> solids_, details_;
    std::vector<glm::vec3> furnitureBases_;
    std::vector<glm::vec4> foliage_;
    std::vector<InkPatch> drawing_, bridgeDrawing_;
    float angle_ = 0;
    float angularSpeed_ = 0;
    bool targetOpen_ = false;
    bool classroomKeyTaken_ = false, classroomDoorUnlocked_ = false;
    float keyRatio_ = 0;
    perspective::Pose keyDisplay_{classroomKeySpot, 1.f};
    float classroomDoorOpening_ = 0;
    float recognition_ = 0, formation_ = 0, opening_ = 0;
    bool forming_ = false;
    float bridgeHeld_ = 0, bridgeFormation_ = 0;
    bool bridgeForming_ = false;
};
} // namespace school
