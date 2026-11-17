#include "../SchoolRoom.h"
#include "../SchoolRenderer.h"
#include <cmath>
#include <iostream>
#include <string>

namespace school {
namespace {
int failures = 0, checks = 0;
void check(bool value, const std::string& description) {
    ++checks;
    if (!value) {
        ++failures;
        std::cerr << "FAIL: " << description << '\n';
    }
}
void tick(SchoolRoom& room, Camera& camera, Input input = {}, int frames = 120) {
    for (int i = 0; i < frames; ++i) {
        room.update(camera, input, 1.0f / 120);
        input.jump = false;
    }
}
void viewStudy(Camera& camera) {
    camera.resetMotion();
    camera.feet = SchoolRoom::alignmentEye - glm::vec3(0, 1.65f, 0);
    camera.face(SchoolRoom::alignmentFocus);
    camera.grounded = true;
    camera.verticalSpeed = 0;
}
void aimProjector(SchoolRoom& room, float fraction = 1) {
    auto start = glm::normalize(SchoolRoom::projectorStartAim - SchoolRoom::projectorOrigin);
    auto target = glm::normalize(SchoolRoom::projectorPaper - SchoolRoom::projectorOrigin);
    room.rotateProjector((std::atan2(target.x, -target.z) - std::atan2(start.x, -start.z)) * fraction,
                         (std::asin(target.y) - std::asin(start.y)) * fraction);
}
void positionProjector(SchoolRoom& room, Camera& camera) {
    camera = Camera();
    camera.feet = {4.8f, 0, 6.4f};
    camera.face(SchoolRoom::projectorOrigin);
    check(room.toggleProjector(camera), "projector can be rotated from behind its cart");
    aimProjector(room);
    room.toggleProjector(camera);
    camera.face(SchoolRoom::projectorPaper);
    room.update(camera, {}, 1.0f / 120);
    check(room.projectorAligned() && room.boardVisibility() > 0.98f && !room.stairsReady(),
          "rotation uncovers board and completes the slide without a reveal timer");
}
bool walkTo(SchoolRoom& room, Camera& camera, glm::vec2 destination) {
    for (int frame = 0; frame < 1200; ++frame) {
        glm::vec2 delta = destination - glm::vec2(camera.feet.x, camera.feet.z);
        if (glm::length(delta) < 0.04f)
            return true;
        camera.yaw = std::atan2(delta.x, -delta.y);
        room.update(camera, {1, 0, false}, 1.0f / 120);
    }
    std::cerr << "walk blocked at " << camera.feet.x << "," << camera.feet.y << "," << camera.feet.z
              << " moving toward " << destination.x << "," << destination.y << '\n';
    return false;
}
} // namespace
int runTests() {
    failures = checks = 0;
    SchoolRoom room;
    Camera camera;
    check(!room.stairsReady() && !room.escaped(),
          "reset starts with an incomplete drawing and an open destination");
    std::array<int, 3> boardFragments{};
    int floorFragments = 0, lecternFragments = 0, uncovered = 0, blocked = 0;
    float projectionError = 0;
    for (const auto& fragment : room.chalk_) {
        if (fragment.panel >= 0)
            ++boardFragments[fragment.panel];
        for (glm::vec3 point : {fragment.projectedA, fragment.projectedB}) {
            if (point.y < 0.04f) {
                ++floorFragments;
                if (point.x < 0.4f || point.x > 3.95f || point.z < -3.85f || point.z > 0.1f) {
                    ++uncovered;
                    if (uncovered < 3)
                        std::cerr << "paper misses " << point.x << ", " << point.z << '\n';
                }
            }
            if (point.x > 0.4f && point.x < 1.6f && point.z < -2.85f && point.z > -3.65f && point.y < 1.1f)
                ++lecternFragments;
            glm::vec3 direction = glm::normalize(point - SchoolRoom::alignmentEye);
            float distance = glm::distance(point, SchoolRoom::alignmentEye);
            for (const auto& box : room.collisionBoxes(false)) {
                float hit = SchoolRoom::rayBox(SchoolRoom::alignmentEye, direction, box);
                if (hit > 0 && hit < distance - 0.06f) {
                    ++blocked;
                    if (blocked < 4)
                        std::cerr << "blocked " << point.x << "," << point.y << "," << point.z
                                  << " box=" << box.min.x << "," << box.min.y << "," << box.min.z
                                  << " hit=" << hit << " distance=" << distance << '\n';
                }
            }
        }
        auto a = glm::normalize(fragment.projectedA - SchoolRoom::alignmentEye);
        auto b = glm::normalize(fragment.originalA - SchoolRoom::alignmentEye);
        projectionError = std::max(projectionError, glm::length(a - b));
    }
    for (int count : boardFragments)
        check(count > 0, "board study contains real projected stair fragments");
    check(floorFragments > 0 && lecternFragments > 0, "study spans the classroom floor and lectern");
    check(uncovered == 0, "projected floor strokes remain in the front teaching area");
    check(blocked == 0, "furniture and gallery do not hide the aligned study");
    check(projectionError < 0.00001f, "surface drawing preserves the stair projection");
    float initialVisible = room.boardVisibility();
    std::vector<glm::vec3> coveredInitially;
    for (const auto& stroke : room.chalk_)
        if (stroke.panel >= 0 && room.projectorCovers(stroke.projectedA))
            coveredInitially.push_back(stroke.projectedA);
    check(initialVisible < 0.001f && !room.projectorAligned(),
          "initial image hides every board stair stroke");
    float leastCornerCoverage = 1;
    for (int board = 0; board < 3; ++board)
        for (float x : {-2.80f + board * 1.95f - 0.86f, -2.80f + board * 1.95f + 0.86f})
            for (float y : {1.28f, 3.16f})
                leastCornerCoverage = std::min(leastCornerCoverage,
                                               room.projectorLight({x, y, -4.68f + board * 0.105f + 0.065f}));
    check(leastCornerCoverage > 0.98f, "projection covers all four corners of all three boards");
    check(!room.toggleProjector(camera), "projector cannot be controlled from the entrance");
    float initialYaw = room.projectorYaw_, initialPitch = room.projectorPitch_;
    room.rotateProjector(1, 1);
    check(room.projectorYaw_ == initialYaw && room.projectorPitch_ == initialPitch,
          "mouse rotation does nothing outside projector mode");
    int slideCount = 0, initialOnBoard = 0;
    for (const auto& stroke : room.chalk_) {
        if (!room.isSlideStroke(stroke))
            continue;
        ++slideCount;
        auto hit = room.projectSlide(stroke.projectedA);
        if (hit.surface >= 0 && hit.surface < 3)
            ++initialOnBoard;
    }
    check(slideCount > 20 && initialOnBoard > 20, "actual missing stair drawing starts projected on board");
    int floorStrokes = 0, movedFloorStrokes = 0, lecternStrokes = 0, fixedLowerStrokes = 0;
    for (const auto& stroke : room.chalk_) {
        if (stroke.panel >= 0)
            continue;
        if (!room.isSlideStroke(stroke))
            ++fixedLowerStrokes;
        if (stroke.projectedA.y < 0.04f && stroke.projectedB.y < 0.04f) {
            ++floorStrokes;
            auto hit = room.projectSlide(stroke.projectedA);
            if (hit.surface >= 0 && glm::distance(hit.point, stroke.projectedA) > 0.20f)
                ++movedFloorStrokes;
        } else if (stroke.projectedA.x < 1.6f) {
            ++lecternStrokes;
        }
    }
    check(fixedLowerStrokes == 0, "non-board stair marks always belong to the projector");
    check(floorStrokes > 20 && movedFloorStrokes == floorStrokes,
          "every lower floor fragment moves away from its solution position with the initial projection");
    check(lecternStrokes > 0, "lectern fragments are also projected rather than fixed ink");
    check(room.slideLight({room.slideBounds_.x, room.slideBounds_.y}) == 0,
          "unprinted slide margin has no rectangular light backing");
    viewStudy(camera);
    tick(room, camera, {}, 240);
    check(!room.stairsReady(), "stair viewpoint alone cannot bypass incorrect projection");
    camera.feet = {4.8f, 0, 6.4f};
    camera.face(SchoolRoom::projectorOrigin);
    check(room.toggleProjector(camera), "enter projector rotation");
    const auto fixedOrigin = SchoolRoom::projectorOrigin;
    auto startDirection = glm::normalize(SchoolRoom::projectorStartAim - fixedOrigin);
    auto targetDirection = glm::normalize(SchoolRoom::projectorPaper - fixedOrigin);
    float horizontalTurn =
        std::atan2(targetDirection.x, -targetDirection.z) - std::atan2(startDirection.x, -startDirection.z);
    room.rotateProjector(horizontalTurn, 0);
    room.update(camera, {}, 1.0f / 120);
    check(!room.projectorAligned(), "horizontal angle alone with incorrect pitch leaves drawing incomplete");
    room.rotateProjector(-horizontalTurn, 0);
    aimProjector(room, 0.5f);
    tick(room, camera);
    float halfwayVisible = room.boardVisibility();
    int newlyVisible = 0;
    for (auto point : coveredInitially)
        if (!room.projectorCovers(point))
            ++newlyVisible;
    check(newlyVisible > 0 && halfwayVisible < 0.99f,
          "rotating halfway exposes previously covered chalk while other chalk remains covered");
    check(!room.projectorAligned() && room.projectorError() > 0.075f,
          "partial uncovering is a clue rather than an automatic solution");
    check(room.toggleProjector(camera) && !room.adjustingProjector(),
          "E leaves rotation at its current angle");
    float stoppedYaw = room.projectorYaw_;
    room.rotateProjector(1, 1);
    check(room.projectorYaw_ == stoppedYaw, "released projector keeps its rotation");
    check(room.toggleProjector(camera), "rotation may be resumed");
    aimProjector(room, -0.5f);
    tick(room, camera);
    check(std::abs(room.boardVisibility() - initialVisible) < 0.02f,
          "turning back covers the same board strokes again");
    aimProjector(room);
    room.update(camera, {}, 0.1f, false);
    check(!room.projectorAligned(), "inactive update cannot accept a new alignment");
    room.update(camera, {}, 1.0f / 120);
    check(room.projectorAligned() && room.adjustingProjector() && room.boardVisibility() > 0.98f,
          "correct projection joins immediately without focus adjustment or automatic control lock");
    if (room.projectorError() >= 0.075f) {
        int shown = 0;
        for (const auto& stroke : room.chalk_) {
            if (!room.isSlideStroke(stroke))
                continue;
            for (auto point : {stroke.projectedA, stroke.projectedB}) {
                auto hit = room.projectSlide(point);
                if (glm::distance(hit.point, point) >= 0.075f && shown++ < 6)
                    std::cerr << "slide miss " << point.x << "," << point.y << "," << point.z
                              << " hit surface=" << hit.surface << " at " << hit.point.x << "," << hit.point.y
                              << "," << hit.point.z << '\n';
            }
        }
    }
    check(room.projectorError() < 0.075f,
          "projected slide endpoints meet their actual missing stair fragments");
    room.rotateProjector(0.03f, 0);
    room.update(camera, {}, 1.0f / 120);
    check(!room.projectorAligned(), "rotating away breaks projection alignment before materialization");
    room.rotateProjector(-0.03f, 0);
    room.update(camera, {}, 1.0f / 120);
    check(room.projectorAligned(), "correct angle can be restored");
    room.rotateProjector(-10, 10);
    room.update(camera, {}, 1.0f / 120);
    check(!room.projectorAligned() && std::isfinite(room.projectorError()),
          "rotation limits remain valid and do not falsely solve");
    room.rotateProjector(std::atan2(targetDirection.x, -targetDirection.z) - room.projectorYaw_,
                         std::asin(targetDirection.y) - room.projectorPitch_);
    room.update(camera, {}, 1.0f / 120);
    check(room.projectorAligned() && SchoolRoom::projectorOrigin == fixedOrigin,
          "projector can rotate back from limits without moving its origin");
    room.stopAdjustingProjector();
    stoppedYaw = room.projectorYaw_;
    room.rotateProjector(0.1f, 0);
    check(!room.adjustingProjector() && room.projectorYaw_ == stoppedYaw,
          "focus loss or pause stops further rotation input");
    check(room.toggleProjector(camera), "aligned projector can be adjusted again until stairs materialize");
    camera.feet = {4.8f, 0, 8.0f};
    room.update(camera, {}, 1.0f / 120);
    check(!room.adjustingProjector(), "walking away exits projector controls");
    room.reset();
    positionProjector(room, camera);
    viewStudy(camera);
    check(room.alignment(camera) > 0.94f, "combined drawing matches without a symbol code");
    auto originalFeet = camera.feet;
    room.update(camera, {1, 0, true}, 0.1f, false);
    check(!room.stairsReady() && camera.feet == originalFeet,
          "paused or unfocused update cannot move or materialize");
    room.update(camera, {}, 0);
    check(!room.stairsReady(), "zero elapsed time cannot materialize");
    room.update(camera, {0, 0, true}, 1.0f / 120);
    check(!room.stairsReady() && !camera.grounded, "jump frame cannot materialize");
    room.reset();
    positionProjector(room, camera);
    viewStudy(camera);
    camera.yaw += 1.0f;
    tick(room, camera, {}, 150);
    check(!room.stairsReady(), "looking away does not materialize");
    for (glm::vec3 position : {glm::vec3(4.8f, 0, 7.6f), glm::vec3(-4, 0, -1.3f), glm::vec3(0, 0, -1.3f),
                               glm::vec3(5, 0, 6), glm::vec3(2.7f, 0, -1.5f)}) {
        room.reset();
        positionProjector(room, camera);
        camera.feet = position;
        camera.face(SchoolRoom::alignmentFocus);
        camera.grounded = true;
        tick(room, camera, {}, 150);
        check(!room.stairsReady(), "misaligned classroom views do not trigger the study");
    }
    room.reset();
    positionProjector(room, camera);
    viewStudy(camera);
    room.update(camera, {}, 1.0f / 120);
    check(!room.stairsReady() && !room.materializing(), "a passing aligned glance does not create stairs");
    tick(room, camera, {}, 24);
    camera.yaw += 1;
    room.update(camera, {}, 1.0f / 120);
    check(room.alignmentHeld_ == 0 && !room.materializing(), "looking away clears recognition progress");
    viewStudy(camera);
    tick(room, camera, {}, 45);
    check(!room.materializing(), "recognition requires a continuous 0.4 second view");
    auto beforeFormation = room.collisionBoxes().size();
    tick(room, camera, {}, 6);
    check(room.materializing() && !room.stairsReady(),
          "steady view starts formation automatically without E");
    check(room.collisionBoxes().size() == beforeFormation, "unfinished stairs have no invisible collision");
    auto interrupted = room;
    interrupted.reset();
    check(!interrupted.materializing() && interrupted.stairFormation() == 0 &&
              interrupted.alignmentHeld_ == 0 && !interrupted.stairsReady(),
          "restart during formation clears both animation and recognition");
    float progress = room.stairFormation();
    room.update(camera, {1, 0, true}, 1, false);
    room.update(camera, {}, 0);
    check(room.stairFormation() == progress && camera.feet == originalFeet,
          "pause and zero time freeze formation and movement");
    camera.yaw += 1;
    tick(room, camera, {0, 0, true}, 12);
    check(room.stairFormation() > progress && !room.stairsReady(),
          "committed formation continues when looking away or jumping");
    tick(room, camera, {}, 90);
    check(room.stairsReady() && room.stairFormation() == 1 && !room.materializing(),
          "formation finishes with solid walkable stairs");
    tick(room, camera, {}, 240);
    check(room.stairsReady() && !room.escaped(), "repeated frames preserve stairs without skipping escape");
    camera.feet = {2.9f, 0, 0.3f};
    camera.yaw = 0;
    camera.pitch = 0;
    tick(room, camera, {1, 0, false}, 255);
    check(camera.feet.z < -5.75f && !room.escaped(),
          "classroom doorway leads onward, not to early completion");
    check(std::abs(camera.feet.y - 1.8f) < 0.01f, "gallery and corridor share height");
    camera.yaw = 3.14159265f;
    tick(room, camera, {1, 0, false}, 280);
    check(camera.feet.y < 0.01f && room.stairsReady(), "return to classroom with stable stairs");
    camera.feet = {2.9f, 1.2f, -2.3f};
    camera.yaw = 1.5707963f;
    tick(room, camera, {1, 0, false}, 90);
    tick(room, camera);
    check(camera.feet.y < 0.01f, "falling off the stairs lands on classroom floor");
    room.reset();
    camera = Camera();
    check(!room.stairsReady() && !room.escaped() && !room.projectorAligned() &&
              std::abs(room.boardVisibility() - initialVisible) < 0.02f,
          "restart clears both puzzle stages");
    camera.feet = {2.9f, 0, -3.4f};
    camera.yaw = 0;
    tick(room, camera, {1, 0, true}, 160);
    check(!room.escaped() && camera.feet.z > -3.85f, "floor jump cannot pass through built-in storage");
    // No artificial remote lock: reaching the doorway by a valid route permits walking out.
    camera.feet = {3.05f, 1.8f, -4.5f};
    camera.grounded = true;
    tick(room, camera, {1, 0, false}, 100);
    check(camera.feet.z < -5.75f && !room.escaped(), "open doorway has no code or stair-state lock");
    room.reset();
    camera.feet = {4.8f, 0, -1.5f};
    camera.yaw = 1.5707963f;
    tick(room, camera, {1, 0, false}, 400);
    check(camera.feet.x > 6.20f && camera.feet.x <= 6.29f, "east wall collision");
    camera.feet = {-4.8f, 0, -1.5f};
    camera.yaw = -1.5707963f;
    tick(room, camera, {1, 0, false}, 150);
    check(camera.feet.x < -6.20f && camera.feet.x >= -6.29f, "west wall collision");
    camera.feet = {4.8f, 0, 7.6f};
    camera.yaw = 3.14159265f;
    tick(room, camera, {1, 0, false}, 150);
    check(camera.feet.z > 8.70f && camera.feet.z <= 8.79f, "rear wall collision");
    camera.feet = {4.9f, 0, 6.1f};
    camera.yaw = 1.5707963f;
    tick(room, camera, {1, 0, false}, 100);
    check(camera.feet.x > 5.35f && camera.feet.x <= 5.42f, "bookshelf collision");
    camera.feet = {-0.95f, 0, -0.2f};
    camera.yaw = 3.14159265f;
    tick(room, camera, {1, 0, false}, 100);
    check(camera.feet.z < 0.2f, "desk collision");
    camera.feet = {0.1f, 0, 1};
    camera.verticalSpeed = 0;
    camera.grounded = true;
    float top = 0;
    room.update(camera, {0, 0, true}, 1.0f / 120);
    for (int i = 0; i < 180; ++i) {
        room.update(camera, {}, 1.0f / 120);
        top = std::max(top, camera.feet.y);
    }
    check(top > 1.0f && top < 1.2f && camera.grounded && camera.feet.y == 0, "jump height and landing");
    auto normal = room.mirrorNormal();
    glm::vec3 point{1, 2, 3};
    auto reflected = Renderer::reflectPoint(point, SchoolRoom::mirrorCenter, normal);
    check(glm::distance(Renderer::reflectPoint(reflected, SchoolRoom::mirrorCenter, normal), point) <
              0.00001f,
          "decorative mirror still uses a physical reflection");
    // Full route uses normal movement, projector controls, mouse look and automatic materialization.
    room.reset();
    camera = Camera();
    check(walkTo(room, camera, {-4.0f, 7.6f}), "walk rear aisle");
    check(walkTo(room, camera, {-4.0f, -1.3f}), "walk window aisle");
    check(walkTo(room, camera, {2.7f, -1.3f}), "cross teaching aisle");
    check(walkTo(room, camera, {2.7f, 7.6f}), "walk desk side aisle");
    check(!room.stairsReady(), "ordinary exploration does not solve a misaligned image");
    check(walkTo(room, camera, {4.8f, 6.4f}), "walk behind the projector cart");
    camera.face(SchoolRoom::projectorOrigin);
    check(room.toggleProjector(camera), "operate projector during continuous play");
    aimProjector(room);
    room.toggleProjector(camera);
    camera.face(SchoolRoom::projectorPaper);
    tick(room, camera, {}, 240);
    check(room.projectorAligned() && !room.stairsReady(), "finish new puzzle before approaching the drawing");
    check(walkTo(room, camera, {3.8f, 6.4f}), "step around the solid projector cart");
    check(walkTo(room, camera, {3.8f, 2.4f}), "continue along the clear side aisle");
    check(walkTo(room, camera, {4.05f, 2.4f}), "approach the art study naturally");
    camera.face(SchoolRoom::alignmentFocus);
    tick(room, camera, {}, 150);
    check(room.stairsReady(), "discover and materialize with mouse look alone");
    check(walkTo(room, camera, {2.7f, 0.6f}), "walk onto the materialized study");
    check(walkTo(room, camera, {2.9f, -6.1f}) && !room.escaped(),
          "continuous classroom-to-materials-bay route");
    check(walkTo(room, camera, {2.9f, -9.9f}), "materials bay rear opens into the connector");
    check(std::abs(camera.feet.y - 1.8f) < .01f, "connector preserves the existing exit floor height");
    check(walkTo(room, camera, {6.25f, -9.9f}), "turn from connector into school corridor");
    camera.yaw = 1.570796327f;
    tick(room, camera, {1, 0, true}, 125);
    check(camera.feet.x < 7.80f, "the painted view of a hallway is a real solid wall");
    check(walkTo(room, camera, {6.25f, -9.9f}), "step back to the ordinary classroom entrance");
    check(walkTo(room, camera, {6.25f, -12.4f}) && walkTo(room, camera, {9.7f, -12.4f}),
          "enter classroom A and reach observation window before the first obstruction");
    camera.yaw = 3.14159265f;
    tick(room, camera, {1, 0, true}, 90);
    check(camera.feet.z < -11.8f, "closed classroom glass prevents jumping through as a shortcut");
    check(walkTo(room, camera, {9.7f, -12.4f}) && walkTo(room, camera, {11.67f, -12.55f}) &&
              walkTo(room, camera, {11.67f, -9.9f}) && walkTo(room, camera, {13.45f, -9.9f}),
          "walk through the real rear classroom door to bypass the false hall");
    check(walkTo(room, camera, {13.45f, -12.4f}) && walkTo(room, camera, {16.5f, -12.4f}),
          "classroom B preserves a side view of the cupboard and partition");
    camera.face(SchoolCorridor::hinge + glm::vec3(.8f, 1.2f, 0));
    check(!room.canUseCupboard(camera), "cupboard cannot be operated remotely through classroom glass");
    check(walkTo(room, camera, {13.45f, -12.4f}) && walkTo(room, camera, {13.45f, -10.9f}) &&
              walkTo(room, camera, {17.0f, -10.9f}),
          "return to the corridor to operate the painted leaf");
    check(walkTo(room, camera, {19.7f, -9.9f}), "walk up to the painted partition");
    camera.yaw = 1.570796327f;
    tick(room, camera, {1, 0, true}, 130);
    check(camera.feet.x < 20.29f && !room.corridorDoorForming(),
          "painted door is a solid wall before recognition, including a jump attempt");
    check(walkTo(room, camera, {17.0f, -10.9f}), "return from the flat door to the cupboard");
    camera.face(SchoolCorridor::hinge + glm::vec3(.8f, 1.2f, 0));
    check(room.canUseCupboard(camera) && room.toggleCupboard(camera), "E opens the near painted surface");
    float doorAngle = room.cupboardAngle();
    room.update(camera, {}, 1, false);
    room.update(camera, {}, 0);
    check(room.cupboardAngle() == doorAngle, "pause and zero time freeze cupboard motion");
    bool clearSweep = true;
    for (int frame = 0; frame < 120; frame++) {
        room.update(camera, {}, 1.0f / 120);
        const auto boxes = room.collisionBoxes();
        for (size_t leaf = boxes.size() - 12; leaf < boxes.size(); leaf++)
            for (size_t fixture = 0; fixture < boxes.size() - 12; fixture++) {
                const auto& a = boxes[leaf];
                const auto& b = boxes[fixture];
                clearSweep &=
                    !(a.min.x < b.max.x - .002f && a.max.x > b.min.x + .002f && a.min.y < b.max.y - .002f &&
                      a.max.y > b.min.y + .002f && a.min.z < b.max.z - .002f && a.max.z > b.min.z + .002f);
            }
    }
    check(clearSweep, "door sweep clears furniture, partition, walls and cupboard body");
    check(room.cupboardAngle() > 1.56f && !room.corridorDoorForming(),
          "opening the cupboard alone does not unlock a remote door");
    check(walkTo(room, camera, {15.3f, -10.7f}), "step back to connect the near and far fragments");
    tick(room, camera, {}, 30);
    camera.face(SchoolCorridor::illusionFocus);
    check(room.corridorAlignment(camera) >= .80f, "normal walking can reach a visually aligned door");
    tick(room, camera, {}, 30);
    check(!room.corridorDoorForming(), "a brief aligned glance does not materialize the door");
    camera.yaw += 1.0f;
    tick(room, camera, {}, 30);
    camera.face(SchoolCorridor::illusionFocus);
    tick(room, camera, {}, 30);
    check(!room.corridorDoorForming(), "looking away clears incomplete recognition");
    tick(room, camera, {}, 45);
    check(room.corridorDoorForming() && !room.corridorDoorReady(),
          "holding the assembled image starts materialization");
    const float frozenFormation = room.corridor_.doorwayFormation();
    room.update(camera, {}, 1, false);
    check(room.corridor_.doorwayFormation() == frozenFormation, "pause freezes doorway materialization");
    camera.yaw += 1.0f;
    tick(room, camera, {}, 240);
    check(!room.corridorDoorReady() && !room.cupboardOpen() && !room.openCorridorDoor(camera),
          "the real door remains closed and cannot be operated from a distance");
    check(walkTo(room, camera, {19.7f, -9.9f}), "approach the real door after materialization");
    camera.face(SchoolCorridor::illusionFocus);
    check(room.openCorridorDoor(camera), "E opens the physically accessible sliding door");
    tick(room, camera, {}, 130);
    check(room.corridorDoorReady() && walkTo(room, camera, {21.2f, -9.9f}) &&
              walkTo(room, camera, {SchoolCorridor::bridgeEye.x, -10.65f}),
          "walk through the materialized doorway to the service stairwell");
    check(walkTo(room, camera, {SchoolCorridor::bridgeEye.x, -11.86f}) && camera.grounded,
          "reach the near lip of the gap with a running start");
    camera.yaw = 0;
    camera.pitch = 0;
    tick(room, camera, {1, 0, true}, 200);
    check(camera.feet.y < .02f && camera.feet.z > -15.47f && !room.corridorBridgeReady(),
          "a running jump cannot cross the unformed grating or bypass the far ledge below");
    camera.yaw = 0;
    tick(room, camera, {1, 0, true}, 145);
    check(camera.feet.z > -15.47f && camera.feet.y < .02f,
          "jumping from the lower floor cannot climb the far ledge");
    check(walkTo(room, camera, {22.39f, -15.15f}) && walkTo(room, camera, {22.39f, -11.5f}) &&
              camera.feet.y > 1.78f,
          "return stairs recover a fall without resetting");
    check(walkTo(room, camera, {22.75f, -10.45f}), "reach an off-axis view of the grating");
    tick(room, camera, {}, 30);
    camera.face(SchoolCorridor::bridgeFocus);
    tick(room, camera, {}, 130);
    check(!room.corridorBridgeReady() && room.corridorBridgeAlignment(camera) < .75f,
          "off-axis marks cannot support a player or solve by waiting");
    check(walkTo(room, camera, {SchoolCorridor::bridgeEye.x, SchoolCorridor::bridgeEye.z}),
          "walk back to line up the painted floor and riser");
    tick(room, camera, {}, 30);
    camera.face(SchoolCorridor::bridgeFocus);
    check(room.corridorBridgeAlignment(camera) >= .75f, "matching rays recognize the complete grating");
    room.update(camera, {}, 1, false);
    check(!room.corridorBridgeReady(), "pause cannot form a bridge");
    tick(room, camera, {}, 165);
    check(room.corridorBridgeReady(), "the assembled grating becomes a physical crossing");
    check(walkTo(room, camera, {SchoolCorridor::bridgeEye.x, -13.75f}) && camera.feet.y > 1.78f,
          "the formed grating supports walking above the lower level");
    check(walkTo(room, camera, {SchoolCorridor::bridgeEye.x, -16.0f}) &&
              walkTo(room, camera, {23.55f, SchoolCorridor::exitZ}),
          "walk across the grating and down the stairwell without teleporting");
    tick(room, camera, {}, 60);
    check(room.escaped() && std::abs(camera.feet.y - .6f) < .01f,
          "completion occurs only on final stairwell landing");
    check(walkTo(room, camera, {23.55f, -16.02f}) &&
              walkTo(room, camera, {SchoolCorridor::bridgeEye.x, -16.02f}) &&
              walkTo(room, camera, {SchoolCorridor::bridgeEye.x, -9.9f}) &&
              walkTo(room, camera, {21.2f, -9.9f}) && walkTo(room, camera, {19.9f, -9.9f}) &&
              walkTo(room, camera, {19.9f, -10.9f}),
          "materialized passage stays open for the return trip");
    camera.face(SchoolCorridor::hinge + glm::vec3(1.1f, 1.2f, 0));
    check(room.toggleCupboard(camera), "cupboard can still open from its far side after materialization");
    tick(room, camera, {}, 120);
    check(room.cupboardAngle() > 1.56f && room.corridorDoorReady(),
          "later cupboard movement cannot undo the real doorway");
    // Swept door collision must stop before reaching the player, including between rendered frames.
    camera.feet = {17.0f, 1.8f, -10.9f};
    camera.face(SchoolCorridor::hinge + glm::vec3(0, 1.2f, 1));
    check(room.toggleCupboard(camera), "close cupboard for sweep recovery check");
    tick(room, camera, {}, 120);
    camera.feet = {18.72f, 1.8f, -10.9f};
    camera.face(SchoolCorridor::hinge + glm::vec3(1.1f, 1.2f, 0));
    check(room.toggleCupboard(camera), "open from within the swing area");
    tick(room, camera, {}, 120);
    check(room.cupboardAngle() < .001f && !room.cupboardOpen(),
          "door reverses its sweep rather than pinning the player beside the stack");
    bool intersects = false;
    for (const auto& b : room.collisionBoxes()) {
        auto p = camera.feet;
        intersects |= p.x + SchoolRoom::radius > b.min.x && p.x - SchoolRoom::radius < b.max.x &&
                      p.z + SchoolRoom::radius > b.min.z && p.z - SchoolRoom::radius < b.max.z &&
                      p.y + SchoolRoom::bodyHeight > b.min.y + .002f && p.y < b.max.y - .002f;
    }
    check(!intersects, "blocked sweep leaves the player outside all solids");
    check(walkTo(room, camera, {19.9f, -10.9f}), "player can retreat from the stopped sweep");
    camera.face(SchoolCorridor::hinge + glm::vec3(1.1f, 1.2f, 0));
    check(room.toggleCupboard(camera), "reopen after retreating from the swing area");
    tick(room, camera, {}, 120);
    check(room.cupboardAngle() > 1.56f, "door opens safely when its swing area clears");
    room.reset();
    check(!room.cupboardOpen() && room.cupboardAngle() == 0 && !room.corridorDoorForming() &&
              !room.corridorDoorReady() && !room.corridorBridgeReady() && !room.escaped() &&
              !room.stairsReady(),
          "restart restores both classroom and corridor puzzles");
    camera.feet = {19.7f, 1.8f, -9.9f};
    camera.grounded = true;
    camera.verticalSpeed = 0;
    camera.yaw = 1.570796327f;
    tick(room, camera, {1, 0, true}, 160);
    check(camera.feet.x < 20.29f, "reset restores solid collision at the former passage");
    // Actual angular agreement, not proximity or an observation flag, controls recognition.
    auto prepareCorridor = [&]() {
        room.reset();
        camera = Camera();
        camera.feet = {17.0f, 1.8f, -10.9f};
        camera.face(SchoolCorridor::hinge + glm::vec3(.8f, 1.2f, 0));
        check(room.toggleCupboard(camera), "prepare the rotating projection surface");
        tick(room, camera, {}, 150);
        camera.feet = SchoolCorridor::illusionEye - glm::vec3(0, 1.65f, 0);
        camera.resetMotion();
        camera.face(SchoolCorridor::illusionFocus);
    };
    prepareCorridor();
    check(room.corridorAlignment(camera) > .999f,
          "reference eye gives matching rays on both receiving planes");
    camera.feet.z += 1.1f;
    camera.face(SchoolCorridor::illusionFocus);
    tick(room, camera, {}, 150);
    check(room.corridorAlignment(camera) < .8f && !room.corridorDoorForming(),
          "sideways view breaks the picture and cannot solve it by waiting");
    camera.feet = {19.5f, 1.8f, -9.9f};
    camera.face(SchoolCorridor::illusionFocus);
    tick(room, camera, {}, 150);
    check(!room.corridorDoorForming(),
          "standing close to the distant fragment cannot substitute for alignment");
    camera.feet = SchoolCorridor::illusionEye - glm::vec3(0, 1.65f, 0);
    camera.resetMotion();
    camera.face(SchoolCorridor::illusionFocus);
    camera.grounded = false;
    check(room.corridorAlignment(camera) == 0, "airborne views cannot recognize the door");
    camera.grounded = true;
    camera.yaw += 1.2f;
    check(room.corridorAlignment(camera) == 0, "position alone cannot recognize an offscreen picture");
    prepareCorridor();
    camera.feet = {17.0f, 1.8f, -10.9f};
    camera.face(SchoolCorridor::hinge + glm::vec3(0, 1.2f, 1));
    check(room.toggleCupboard(camera), "rotate the near fragment away again before solving");
    tick(room, camera, {}, 150);
    camera.feet = SchoolCorridor::illusionEye - glm::vec3(0, 1.65f, 0);
    camera.resetMotion();
    camera.face(SchoolCorridor::illusionFocus);
    tick(room, camera, {}, 180);
    check(!room.corridorDoorForming() && room.corridorAlignment(camera) < .8f,
          "correct viewing position cannot solve a rotated-away fragment");
    for (int rate : {30, 144}) {
        prepareCorridor();
        float elapsed = 0;
        while (room.corridor_.doorwayFormation() < 1 && elapsed < 3) {
            room.update(camera, {}, 1.0f / rate);
            elapsed += 1.0f / rate;
        }
        const float expected = SchoolCorridor::recognitionDuration + SchoolCorridor::formationDuration;
        check(room.corridor_.doorwayFormation() == 1 && !room.corridorDoorReady() && elapsed >= expected &&
                  elapsed < expected + .08f,
              "recognition and materialization stay stable across frame rates");
    }
    for (float rate : {30.0f, 144.0f}) {
        room.reset();
        positionProjector(room, camera);
        viewStudy(camera);
        float elapsed = 0;
        while (!room.stairsReady() && elapsed < 1.3f) {
            room.update(camera, {}, 1.0f / rate);
            elapsed += 1.0f / rate;
        }
        check(room.stairsReady() &&
                  elapsed >= SchoolRoom::recognitionDuration + SchoolRoom::formationDuration &&
                  elapsed < SchoolRoom::recognitionDuration + SchoolRoom::formationDuration + 2.1f / rate,
              "recognition and formation duration remain stable across frame rates");
    }
    // Smoothing is bounded: responsive starts, short stopping distance and no frame-rate drift.
    room.reset();
    camera = Camera();
    camera.feet = {14.0f, 1.8f, -9.9f};
    camera.yaw = 1.570796327f;
    room.update(camera, {1, 0, false}, 1.0f / 60);
    check(camera.feet.x > 14.0f && camera.feet.x < 14.025f,
          "walking eases into motion without an instant speed jump");
    tick(room, camera, {1, 0, false}, 118);
    check(glm::length(camera.movement) > 2.99f, "eased movement preserves the original maximum walk speed");
    auto stoppedAt = camera.feet;
    tick(room, camera, {}, 30);
    check(glm::distance(stoppedAt, camera.feet) < .09f && glm::length(camera.movement) == 0,
          "release stops within nine centimetres without lingering drift");
    glm::vec3 ratePositions[2];
    int rateIndex = 0;
    for (int rate : {30, 144}) {
        room.reset();
        camera = Camera();
        camera.feet = {14.0f, 1.8f, -9.9f};
        camera.yaw = 1.570796327f;
        for (int i = 0; i < rate; i++)
            room.update(camera, {1, 0, false}, 1.0f / rate);
        for (int i = 0; i < rate; i++)
            room.update(camera, {}, 1.0f / rate);
        ratePositions[rateIndex++] = camera.feet;
    }
    check(glm::distance(ratePositions[0], ratePositions[1]) < .015f,
          "walk and braking differ by under 1.5 cm at 30 and 144 FPS");
    camera = Camera();
    camera.feet = {8.92f, 1.8f, -10.28f};
    camera.yaw = 1.570796327f;
    tick(room, camera, {1, 0, false}, 8);
    check(camera.feet.x > 8.96f, "round player footprint slides past the empty corner of a desk stack");
    room.reset();
    room.stairsReady_ = true;
    camera = Camera();
    camera.feet = {2.9f, 0, .3f};
    camera.yaw = 0;
    camera.pitch = 0;
    float maxRise = 0, previousEye = camera.eye().y;
    for (int i = 0; i < 240; i++) {
        room.update(camera, {1, 0, false}, 1.0f / 120);
        maxRise = std::max(maxRise, std::abs(camera.eye().y - previousEye));
        previousEye = camera.eye().y;
    }
    check(camera.feet.y > 1.79f && maxRise < .08f,
          "stair collision stays solid while eye height avoids 30 cm jumps");
    tick(room, camera, {}, 120);
    check(std::abs(camera.stepOffset) < .001f, "stair view settles to the exact ordinary eye height");
    camera.yaw = 3.14159265f;
    float maxDrop = 0;
    previousEye = camera.eye().y;
    for (int i = 0; i < 270; i++) {
        room.update(camera, {1, 0, false}, 1.0f / 120);
        maxDrop = std::max(maxDrop, std::abs(camera.eye().y - previousEye));
        previousEye = camera.eye().y;
    }
    check(camera.feet.y < .01f && maxDrop < .08f,
          "walking back down steps keeps a continuous eye trajectory");
    for (int rate : {30, 144}) {
        room.reset();
        camera = Camera();
        camera.feet = {17.0f, 1.8f, -10.9f};
        camera.face(SchoolCorridor::hinge + glm::vec3(.8f, 1.2f, 0));
        check(room.toggleCupboard(camera), "operate cupboard for eased motion probe");
        float first = 0, middle = 0, previous = room.cupboardAngle();
        bool bounded = true;
        for (int i = 0; i < rate * 2; i++) {
            room.update(camera, {}, 1.0f / rate);
            float delta = std::abs(room.cupboardAngle() - previous);
            if (i == 0)
                first = delta;
            if (i == rate / 3)
                middle = delta;
            bounded &=
                room.cupboardAngle() >= 0 && room.cupboardAngle() <= 1.570797f && delta <= 2.21f / rate;
            previous = room.cupboardAngle();
        }
        check(first < middle * .3f && room.cupboardAngle() > 1.57079f && bounded,
              "cupboard accelerates gently, respects speed cap and settles exactly at both frame rates");
    }
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
} // namespace school
