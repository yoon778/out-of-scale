#pragma once

#include <GL/glm/glm.hpp>
#include <array>
#include <vector>

namespace school {
struct Box;
struct Camera;
class Renderer;

// School-owned continuation. The shared inter-stage connector is unchanged.
class SchoolCorridor {
  public:
    SchoolCorridor();
    void reset();
    void update(const Camera& camera, float dt);
    bool canInteract(const Camera& camera) const;
    bool interact(const Camera& camera);
    bool finished(const Camera& camera) const;
    bool doorOpen() const { return targetOpen_; }
    float doorAngle() const { return angle_; }
    float alignment(const Camera& camera) const;
    bool doorwayForming() const { return forming_; }
    bool doorwayReady() const { return opening_ >= 1; }
    bool canOpenPassage(const Camera& camera) const;
    bool openPassage(const Camera& camera);
    bool bridgeReady() const { return bridgeFormation_ >= 1; }
    float bridgeAlignment(const Camera& camera) const;
    float doorwayFormation() const { return formation_; }
    float doorwayOpening() const { return opening_; }
    std::vector<Box> collisionBoxes() const;
    void draw(Renderer& renderer) const;
    static constexpr float floorHeight = 1.8f;
    static constexpr glm::vec3 hinge{18.10f, floorHeight, -11.48f};
    static constexpr float doorWidth = 1.20f;
    static constexpr glm::vec3 illusionEye{15.30f, 3.45f, -10.70f};
    static constexpr glm::vec3 illusionFocus{20.45f, 3.0f, -9.90f};
    static constexpr float recognitionDuration = .55f;
    static constexpr float formationDuration = .75f;
    static constexpr float openingDuration = .9f;
    static constexpr glm::vec3 bridgeEye{23.95f, 3.45f, -10.45f};
    static constexpr glm::vec3 bridgeFocus{23.95f, 1.8f, -13.825f};
    static constexpr float exitZ = -21.85f;

  private:
    struct Solid {
        glm::vec3 center, size, color;
        bool visible = true;
        float bevel = 0;
    };
    struct Window {
        float start, end;
    };
    struct InkPatch {
        std::array<glm::vec3, 4> target, receiver;
        glm::vec3 color;
        bool onLeaf, frame;
    };
    void build();
    void buildDrawing();
    void updateIllusion(const Camera& camera, float dt);
    void drawIllusion(Renderer& renderer) const;
    void buildBridgeDrawing();
    void drawFalseHall(Renderer& renderer) const;
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
    float recognition_ = 0, formation_ = 0, opening_ = 0;
    bool forming_ = false, passageOpening_ = false;
    float bridgeHeld_ = 0, bridgeFormation_ = 0;
    bool bridgeForming_ = false;
};
} // namespace school
