#include "../SchoolRoom.h"
#include "../SchoolRenderer.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string_view>
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
void tickFor(SchoolRoom& room, Camera& camera, float seconds) {
    tick(room, camera, {}, static_cast<int>(std::ceil(seconds * 120)) + 2);
}
void aimProjector(SchoolRoom& room, float fraction = 1) {
    auto start = glm::normalize(SchoolRoom::projectorStartAim - SchoolRoom::projectorOrigin);
    auto target = glm::normalize(SchoolRoom::projectorPaper - SchoolRoom::projectorOrigin);
    room.rotateProjector((std::atan2(target.x, -target.z) - std::atan2(start.x, -start.z)) * fraction,
                         (std::asin(target.y) - std::asin(start.y)) * fraction);
}
bool placeNotebook(SchoolRoom& room, Camera& camera) {
    camera.face(room.notebookCenter());
    if (!room.toggleNotebook(camera))
        return false;
    camera.face(SchoolRoom::notebookRest);
    room.toggleNotebook(camera);
    const auto center = room.notebookCenter();
    return glm::distance(glm::vec2(center.x, center.z),
                         glm::vec2(SchoolRoom::notebookRest.x, SchoolRoom::notebookRest.z)) < .002f;
}
void faceNotebookReflection(SchoolRoom& room, Camera& camera) {
    camera.face(Renderer::reflectPoint(room.notebookCenter(), SchoolRoom::mirrorCenter, room.mirrorNormal()));
}
bool turnMirrorToNotebook(SchoolRoom& room, Camera& camera) {
    const auto mirror = SchoolRoom::mirrorCenter;
    camera.face(SchoolRoom::mirrorAim);
    if (!room.toggleMirror(camera))
        return false;
    auto horizontal = [](glm::vec3 v) { return glm::normalize(glm::vec2(v.x, v.z)); };
    const auto bisector = horizontal(camera.eye() - mirror) + horizontal(room.notebookCenter() - mirror);
    const auto normal = room.mirrorNormal();
    room.rotateMirror(std::atan2(bisector.x, bisector.y) - std::atan2(normal.x, normal.z));
    return room.toggleMirror(camera);
}
void solveBoards(SchoolRoom& room, Camera& camera) {
    for (int index : {0, 0, 1, 1, 1, 2, 3, 3, 3}) {
        camera.feet = {SchoolRoom::boardX[index], 0, -2.18f};
        camera.grounded = true;
        camera.resetMotion();
        camera.face(room.boardHandle(index) + glm::vec3(0, .55f, 0));
        if (!room.moveBoard(camera))
            throw std::runtime_error("test setup cannot move chalkboard");
        tick(room, camera, {}, 36);
    }
}
void positionProjector(SchoolRoom& room, Camera& camera) {
    camera = Camera();
    solveBoards(room, camera);
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
    std::array<int, 4> boardFragments{};
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
    auto packed = [](const std::array<int, 4>& state) {
        return state[0] + 4 * state[1] + 16 * state[2] + 64 * state[3];
    };
    std::vector<std::array<int, 4>> states{SchoolRoom::boardGoal};
    std::array<int, 256> distance;
    distance.fill(-1);
    distance[packed(states.front())] = 0;
    for (size_t i = 0; i < states.size(); ++i) {
        for (int board = 0; board < 4; ++board) {
            auto previous = states[i];
            // Three forward presses undo a single press of that board.
            for (int step = 0; step < 3; ++step)
                SchoolRoom::advanceBoards(previous, board);
            if (distance[packed(previous)] < 0) {
                distance[packed(previous)] = distance[packed(states[i])] + 1;
                states.push_back(previous);
            }
        }
    }
    check(states.size() == 256, "every four-board arrangement can return to the goal");
    check(distance[packed(SchoolRoom::boardStart)] == 9,
          "the new scramble requires at least nine board moves");
    // The printed height clues determine one arrangement among all permutations of the four heights.
    std::array<int, 4> permutation{1, 2, 3, 4};
    int matching = 0;
    bool matchesGoal = false;
    do {
        const auto [first, second, third, fourth] = permutation;
        if (second == 4 && third == 1 && fourth > first) {
            ++matching;
            matchesGoal |= std::array{first - 1, second - 1, third - 1, fourth - 1} == SchoolRoom::boardGoal;
        }
    } while (std::next_permutation(permutation.begin(), permutation.end()));
    check(matching == 1 && matchesGoal, "the simple number clues imply exactly the existing board goal");
    float initialVisible = room.boardVisibility();
    std::vector<glm::vec3> coveredInitially;
    for (const auto& stroke : room.chalk_)
        if (stroke.panel >= 0 && room.projectorCovers(stroke.projectedA))
            coveredInitially.push_back(stroke.projectedA);
    check(initialVisible < 0.001f && !room.projectorAligned(),
          "initial image hides every board stair stroke");
    float leastCornerCoverage = 1;
    for (int board = 0; board < 4; ++board)
        for (float x : {SchoolRoom::boardX[board] - .69f, SchoolRoom::boardX[board] + .69f})
            for (float y : {1.28f + room.boardShift(board), 3.16f + room.boardShift(board)})
                leastCornerCoverage = std::min(leastCornerCoverage,
                                               room.projectorLight({x, y, -4.68f + board * 0.07f + 0.065f}));
    check(leastCornerCoverage > 0.98f, "projection covers all four corners of all four boards");
    Camera distantView;
    distantView.feet.z = 7.6f;
    distantView.face(SchoolRoom::projectorOrigin);
    check(!room.toggleProjector(distantView), "projector cannot be controlled from outside its reach");
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
        if (hit.surface >= 0 && hit.surface < 4)
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
    camera = Camera();
    const auto initialProjectorLabel = room.interactionLabel(camera);
    check(initialProjectorLabel && std::wstring_view(initialProjectorLabel) == L"돌리기",
          "the starting view offers the projector as the first action");
    check(room.toggleProjector(camera), "scrambled boards still allow the first projector turn");
    aimProjector(room, .5f);
    const float partialReveal = room.boardVisibility();
    check(partialReveal > .10f && partialReveal < .90f,
          "turning the projector partly reveals the hidden board strokes before any board moves");
    aimProjector(room, .5f);
    room.toggleProjector(camera);
    viewStudy(camera);
    tick(room, camera, {}, 150);
    check(room.boardVisibility() > .98f && !room.projectorAligned() && !room.stairsReady(),
          "turning all the way reveals the board but cannot bypass the board order");
    room.reset();
    Camera mirrorView;
    mirrorView.feet = {-5.64f, 0, 2.61f};
    faceNotebookReflection(room, mirrorView);
    check(!room.inspectMirror(mirrorView), "the initial small notebook cannot reveal the guide");
    mirrorView.face(SchoolRoom::mirrorCenter);
    const auto turnLabel = room.interactionLabel(mirrorView);
    check(turnLabel && std::wstring_view(turnLabel) == L"돌리기", "the empty mirror offers turning");
    mirrorView.feet = {-4.83f, 0, 3.80f};
    const auto startNotebookPosition = room.notebookCenter();
    mirrorView.face(startNotebookPosition);
    const auto pickupLabel = room.interactionLabel(mirrorView);
    check(pickupLabel && std::wstring_view(pickupLabel) == L"집기",
          "a reachable notebook offers the pickup prompt");
    check(placeNotebook(room, mirrorView),
          "aiming the held notebook at the stand seats it in the cradle");
    check(room.notebook_.scale > 3.f && room.notebook_.scale < 5.5f &&
              glm::distance(room.notebookCenter(), startNotebookPosition) > 5.f &&
              std::abs(room.notebookBounds(room.notebook_).min.y - SchoolRoom::notebookRest.y) < .004f,
          "the enlarged notebook rests on the existing supply cabinet");
    mirrorView.face(room.notebookCenter());
    check(!room.inspectMirror(mirrorView), "a direct view of the notebook cannot open the mirror guide");
    mirrorView.feet = {-5.64f, 0, 2.61f};
    mirrorView.face(SchoolRoom::mirrorCenter);
    check(!room.canReadNotebook(mirrorView), "the starting mirror angle does not show the stand");
    const auto startMirrorNormal = room.mirrorNormal();
    room.rotateMirror(.2f);
    check(room.mirrorNormal() == startMirrorNormal, "mouse motion outside mirror adjustment cannot turn it");
    check(turnMirrorToNotebook(room, mirrorView) && !room.adjustingMirror(),
          "turn the mirror about its base and leave adjustment");
    check(room.canReadNotebook(mirrorView), "the adjusted mirror makes the notebook readable without moving the player");
    faceNotebookReflection(room, mirrorView);
    const auto readLabel = room.interactionLabel(mirrorView);
    check(readLabel && std::wstring_view(readLabel) == L"읽기", "a readable reflection offers the read prompt");
    Camera edgeView = mirrorView;
    auto justAbovePage = room.notebookCenter();
    justAbovePage.y = room.notebookBounds(room.notebook_).max.y + .04f;
    edgeView.face(Renderer::reflectPoint(justAbovePage, SchoolRoom::mirrorCenter, room.mirrorNormal()));
    check(room.canReadNotebook(edgeView), "looking just outside the reflected page still offers reading");
    check(room.inspectMirror(mirrorView) && room.mirrorGuide_,
          "read the enlarged notebook through the mirror");
    mirrorView.yaw += 1.5f;
    tick(room, mirrorView, {}, 1);
    check(!room.mirrorGuide_, "looking away from the reflection closes the guide");
    mirrorView.face(SchoolRoom::mirrorCenter);
    const bool enteredMirror = room.toggleMirror(mirrorView);
    room.rotateMirror(2.f);
    check(enteredMirror && room.toggleMirror(mirrorView) && !room.canReadNotebook(mirrorView) &&
              room.toggleMirror(mirrorView),
          "turning past the reflection remains recoverable from the back of the mirror");
    room.stopAdjustingMirror();

    SchoolRoom dropRoom;
    Camera carrier;
    auto grabForDrop = [&] {
        dropRoom.reset();
        carrier = Camera();
        carrier.feet = {-4.83f, 0.f, 3.8f};
        carrier.face(dropRoom.notebookCenter());
        return dropRoom.toggleNotebook(carrier);
    };
    check(grabForDrop(), "pick up the notebook for a nearby stand placement");
    carrier.feet = {4.55f, 0, 2.60f};
    carrier.face(SchoolRoom::notebookRest);
    check(dropRoom.toggleNotebook(carrier) && dropRoom.notebookRatio_ == 0 &&
              dropRoom.notebook_.scale < 2.65f &&
              std::abs(dropRoom.notebookBounds(dropRoom.notebook_).min.y - SchoolRoom::notebookRest.y) < .004f,
          "placing the notebook from close to the stand leaves it small");
    carrier.feet = {-5.64f, 0, 2.61f};
    check(turnMirrorToNotebook(dropRoom, carrier) && !dropRoom.canReadNotebook(carrier),
          "the adjusted mirror still requires an enlarged notebook");
    dropRoom.reset();
    check(dropRoom.mirrorNormal() == startMirrorNormal && !dropRoom.adjustingMirror(),
          "reset restores the starting mirror angle");
    check(grabForDrop(), "pick up the notebook for level dropping");
    carrier.feet = {3.8f, 0, 2.4f};
    carrier.pitch = 0;
    carrier.yaw = 3.14159265f;
    const auto levelDrop = dropRoom.notebookPlacement(carrier);
    check(levelDrop && levelDrop->center.y > 1.5f && dropRoom.toggleNotebook(carrier) &&
              dropRoom.notebookRatio_ == 0,
          "the notebook can release along a level sightline");
    tick(dropRoom, carrier, {}, 120);
    check(std::abs(dropRoom.notebookBounds(dropRoom.notebook_).min.y) < .003f && !dropRoom.notebookFalling_,
          "the released notebook falls to a stable position on the classroom floor");
    check(grabForDrop(), "pick up the notebook for blocked-ray dropping");
    carrier.feet = {6.22f, 0, -.10f};
    carrier.yaw = 1.5707963f;
    carrier.pitch = 0;
    const auto wallDrop = dropRoom.notebookPlacement(carrier);
    check(wallDrop && wallDrop->center.x < 6.50f && std::abs(wallDrop->center.z - carrier.feet.z) > .3f &&
              dropRoom.toggleNotebook(carrier) && dropRoom.notebookRatio_ == 0,
          "a wall immediately ahead redirects the drop to nearby free space");
    check(grabForDrop(), "pick up the notebook for desk placement");
    carrier.feet = {3.10f, 0, 1.86f};
    carrier.face({1.15f, .78f, .65f});
    const auto deskDrop = dropRoom.notebookPlacement(carrier);
    check(deskDrop && std::abs(dropRoom.notebookBounds(*deskDrop).min.y - .78f) < .004f &&
              dropRoom.toggleNotebook(carrier) && dropRoom.notebookRatio_ == 0,
          "the aimed tabletop supports the notebook");
    check(grabForDrop(), "pick up the notebook for an upward drop");
    carrier.feet = {3.8f, 0, 2.4f};
    carrier.yaw = 0;
    carrier.pitch = 1.1f;
    const auto overheadDrop = dropRoom.notebookPlacement(carrier);
    check(overheadDrop && overheadDrop->center.y > carrier.eye().y &&
              dropRoom.notebookBounds(*overheadDrop).max.y <= 4.5f &&
              dropRoom.toggleNotebook(carrier) && dropRoom.notebookRatio_ == 0,
          "an upward drop stays below the classroom ceiling");
    check(grabForDrop(), "pick up the notebook for corridor placement");
    carrier.feet = {6.2f, 1.8f, -10.45f};
    carrier.face({5.2f, 1.8f, -9.4f});
    const auto corridorDrop = dropRoom.notebookPlacement(carrier);
    check(corridorDrop && std::abs(dropRoom.notebookBounds(*corridorDrop).min.y - 1.8f) < .004f &&
              dropRoom.toggleNotebook(carrier) && dropRoom.notebookRatio_ == 0,
          "the notebook can be left on the elevated corridor floor");
    solveBoards(room, camera);
    check(room.boardsReady(), "linked physical board controls reach the target order");
    check(room.boardVisibility() < .10f, "board movement alone keeps the projected artwork in place");
    camera.feet = {4.8f, 0, 6.4f};
    camera.face(SchoolRoom::projectorOrigin);
    check(room.toggleProjector(camera), "enter projector rotation");
    tick(room, camera, {}, 30);
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
    room.rotateProjector(0.12f, 0);
    room.update(camera, {}, 1.0f / 120);
    check(!room.projectorAligned(), "rotating away breaks projection alignment before materialization");
    room.rotateProjector(-0.12f, 0);
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
    camera.feet = {SchoolRoom::boardX[0], 0, -2.18f};
    camera.face(room.boardHandle(0) + glm::vec3(0, .55f, 0));
    check(room.moveBoard(camera), "a solved board can be changed before materialization");
    viewStudy(camera);
    tick(room, camera, {}, 150);
    check(!room.boardsReady() && !room.projectorAligned() && !room.stairsReady(),
          "disturbing a solved board blocks even a correctly aimed projector and alignment view");
    for (const auto offset : {glm::vec2(-3.5f, 0), glm::vec2(3.5f, 0), glm::vec2(0, 3.5f),
                              glm::vec2(0, -1.f), glm::vec2(2.f, 2.f)}) {
        room.reset();
        positionProjector(room, camera);
        camera.face(SchoolRoom::projectorOrigin);
        check(room.toggleProjector(camera), "reopen projector for an approximate player adjustment");
        room.rotateProjector(glm::radians(offset.x), glm::radians(offset.y));
        room.toggleProjector(camera);
        room.update(camera, {}, 1.f / 120);
        check(room.projectorAligned(), "small horizontal and vertical aim errors remain accepted");
        viewStudy(camera);
        camera.feet = {4.25f, 0, 2.8f};
        camera.face(SchoolRoom::alignmentFocus);
        camera.yaw += glm::radians(18.f);
        tickFor(room, camera, SchoolRoom::recognitionDuration + SchoolRoom::formationDuration);
        check(room.stairsReady(), "approximate projection and a nearby off-center view create walkable stairs");
    }
    for (const auto offset : {glm::vec2(6.f, 0), glm::vec2(0, 8.f), glm::vec2(3.5f, 3.5f)}) {
        room.reset();
        positionProjector(room, camera);
        room.projectorYaw_ += glm::radians(offset.x);
        room.projectorPitch_ += glm::radians(offset.y);
        viewStudy(camera);
        tick(room, camera, {}, 150);
        check(!room.projectorAligned() && !room.materializing() && !room.stairsReady(),
              "large or combined aim errors still block stairs from the correct viewpoint");
    }
    for (const auto position : {glm::vec3(4.05f, 0, 2.2f), glm::vec3(4.05f, 0, 2.8f),
                                glm::vec3(5.f, 0, 6.f)}) {
        room.reset();
        positionProjector(room, camera);
        viewStudy(camera);
        camera.feet = position;
        camera.face(SchoolRoom::alignmentFocus);
        tickFor(room, camera, SchoolRoom::recognitionDuration + SchoolRoom::formationDuration);
        if (position.z == 6.f)
            check(!room.stairsReady() && !room.materializing(),
                  "the formerly accepted distant rear view now needs closer stair alignment");
        else
            check(room.stairsReady(), "nearby views in front of and behind the reference point remain accepted");
    }
    room.reset();
    positionProjector(room, camera);
    viewStudy(camera);
    check(room.alignment(camera) > 0.94f, "combined drawing matches without a symbol code");
    const auto stairFeedback = room.alignmentFeedback(camera);
    check(stairFeedback && stairFeedback->aligned && stairFeedback->score == room.alignment(camera),
          "stair HUD uses the same alignment score as recognition");
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
    check(!room.stairsReady() && !room.materializing(), "looking away does not materialize");
    for (glm::vec3 position : {glm::vec3(4.8f, 0, 7.6f), glm::vec3(-4, 0, -1.3f), glm::vec3(0, 0, -1.3f),
                               glm::vec3(-4, 0, 6), glm::vec3(2.7f, 0, -1.5f)}) {
        room.reset();
        positionProjector(room, camera);
        camera.feet = position;
        camera.face(SchoolRoom::alignmentFocus);
        camera.grounded = true;
        tick(room, camera, {}, 150);
        check(!room.stairsReady() && !room.materializing(), "misaligned classroom views do not trigger the study");
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
    check(!room.alignmentFeedback(camera), "the alignment HUD hides when the player looks away");
    viewStudy(camera);
    tick(room, camera, {}, 27);
    check(!room.materializing(), "recognition still requires a brief continuous view before 0.25 seconds");
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
    tickFor(room, camera, SchoolRoom::formationDuration);
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
    Camera lecternJump;
    lecternJump.feet = {1.30f, 1.02f, -3.34f};
    lecternJump.face({2.9f, lecternJump.eye().y, -4.43f});
    tick(room, lecternJump, {1, 0, true}, 85);
    tick(room, lecternJump, {}, 80);
    check(lecternJump.feet.y < 1.79f, "the safety rail stops the diagonal jump from the lectern");
    Camera frontJump;
    frontJump.feet = {1.4f, 1.02f, -3.18f};
    frontJump.face({3.05f, frontJump.eye().y, -4.10f});
    tick(room, frontJump, {1, 0, true}, 100);
    tick(room, frontJump, {}, 80);
    check(frontJump.feet.y < 1.79f, "the safety rail also blocks a jump around its front corner");
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
    check(room.toggleProjector(camera), "begin the continuous route by using the projector");
    aimProjector(room, .5f);
    room.toggleProjector(camera);
    check(walkTo(room, camera, {4.8f, 7.6f}) && walkTo(room, camera, {-4.0f, 7.6f}), "walk rear aisle");
    check(walkTo(room, camera, {-4.83f, 3.80f}), "walk to the notebook on the windowsill");
    check(placeNotebook(room, camera), "carry and enlarge the notebook during the continuous route");
    check(walkTo(room, camera, {-5.64f, 2.61f}), "walk to the reflection sightline");
    check(turnMirrorToNotebook(room, camera), "turn the mirror toward the stand during the continuous route");
    faceNotebookReflection(room, camera);
    check(room.inspectMirror(camera), "read the enlarged notebook during the continuous route");
    check(walkTo(room, camera, {-4.0f, 2.59f}) && walkTo(room, camera, {-4.0f, -2.18f}),
          "walk around the mirror along the window aisle");
    for (int index : {0, 0, 1, 1, 1, 2, 3, 3, 3}) {
        check(walkTo(room, camera, {SchoolRoom::boardX[index], -2.18f}),
              "reach the corresponding board rail");
        camera.face(room.boardHandle(index) + glm::vec3(0, .55f, 0));
        check(room.moveBoard(camera), "use chalkboard control while walking under normal movement");
        tick(room, camera, {}, 36);
    }
    check(room.boardsReady(), "the continuous route aligns all four boards");
    check(walkTo(room, camera, {2.7f, -1.3f}), "cross teaching aisle");
    check(walkTo(room, camera, {2.7f, 7.6f}), "walk desk side aisle");
    check(!room.stairsReady(), "ordinary exploration does not solve a misaligned image");
    check(walkTo(room, camera, {4.8f, 6.4f}), "walk behind the projector cart");
    camera.face(SchoolRoom::projectorOrigin);
    check(room.toggleProjector(camera), "return to the projector after arranging the boards");
    aimProjector(room, .5f);
    room.toggleProjector(camera);
    camera.face(SchoolRoom::projectorPaper);
    tick(room, camera, {}, 240);
    check(room.projectorAligned() && !room.stairsReady(), "finish new puzzle before approaching the drawing");
    check(walkTo(room, camera, {3.8f, 6.4f}), "step around the solid projector cart");
    check(walkTo(room, camera, {3.8f, 2.4f}), "continue along the clear side aisle");
    check(walkTo(room, camera, {4.05f, 2.4f}), "approach the art study naturally");
    camera.face(SchoolRoom::alignmentFocus);
    tickFor(room, camera, SchoolRoom::recognitionDuration + SchoolRoom::formationDuration);
    check(room.stairsReady(), "discover and materialize with mouse look alone");
    check(walkTo(room, camera, {2.7f, 0.6f}), "walk onto the materialized study");
    check(walkTo(room, camera, {2.9f, -6.1f}) && !room.escaped(),
          "continuous classroom-to-materials-bay route");
    check(walkTo(room, camera, {2.9f, -9.9f}), "materials bay rear opens into the connector");
    check(std::abs(camera.feet.y - 1.8f) < .01f, "connector preserves the existing exit floor height");
    check(walkTo(room, camera, {6.25f, -9.9f}), "turn from connector into school corridor");
    camera.yaw = 1.570796327f;
    tick(room, camera, {1, 0, true}, 200);
    check(camera.feet.x > 9.25f && camera.feet.x < 9.40f,
          "the extended approach still stops at the solid painted wall");
    check(walkTo(room, camera, {6.25f, -9.9f}), "step back to the ordinary classroom entrance");
    camera.face(SchoolCorridor::classroomKeySpot);
    check(!room.interactCorridor(camera), "the key is out of reach from the first classroom entrance");
    check(walkTo(room, camera, {6.25f, -12.4f}) && walkTo(room, camera, {9.7f, -12.4f}),
          "enter classroom A and reach its privacy window before the first obstruction");
    int privacyRays = 0;
    bool concealedHall = true;
    for (float x : {7.8f, 8.8f, 9.8f, 10.8f})
        for (float z : {-12.05f, -12.55f, -14.f})
            for (float y : {3.45f, 4.2f})
                for (const auto target : {glm::vec3(10.3f, 2.6f, -10.9f), glm::vec3(11.8f, 3.4f, -9.4f),
                                           glm::vec3(14.5f, 3.45f, -10.95f)}) {
                    const glm::vec3 eye{x, y, z};
                    const auto delta = target - eye;
                    const auto paneHit = eye + delta * ((-11.6f - z) / delta.z);
                    if (paneHit.x <= 7.6f || paneHit.x >= 11.05f || paneHit.y <= 2.85f || paneHit.y >= 4.2f)
                        continue;
                    ++privacyRays;
                    bool opaque = false;
                    for (const auto& solid : room.corridor_.solids_) {
                        if (!solid.visible)
                            continue; // Collision-only glass must not count as visual occlusion.
                        const float hit = SchoolRoom::rayBox(eye, glm::normalize(delta),
                                                            {solid.center - solid.size * .5f,
                                                             solid.center + solid.size * .5f});
                        opaque = opaque || (hit >= 0 && hit < glm::length(delta));
                    }
                    concealedHall = concealedHall && opaque;
                }
    check(privacyRays > 30 && concealedHall,
          "classroom A conceals the post-key hall through its window from low, high and oblique views");
    bool clearSecondWindow = true;
    const glm::vec3 secondEye{16.5f, 3.45f, -12.4f}, secondTarget{16.5f, 3.45f, -10.9f};
    for (const auto& solid : room.corridor_.solids_) {
        if (!solid.visible)
            continue;
        const float hit = SchoolRoom::rayBox(secondEye, glm::normalize(secondTarget - secondEye),
                                            {solid.center - solid.size * .5f, solid.center + solid.size * .5f});
        clearSecondWindow = clearSecondWindow && (hit < 0 || hit >= glm::distance(secondEye, secondTarget));
    }
    check(clearSecondWindow, "classroom B keeps the later observation window clear");
    camera.yaw = 3.14159265f;
    tick(room, camera, {1, 0, true}, 90);
    check(camera.feet.z < -11.8f, "closed classroom glass prevents jumping through as a shortcut");
    check(walkTo(room, camera, {9.7f, -12.4f}) && walkTo(room, camera, {11.67f, -12.55f}),
          "reach the locked classroom rear door");
    camera.face(SchoolCorridor::classroomRearDoor);
    check(!room.interactCorridor(camera), "the rear door requires the corridor key");
    camera.yaw = 3.14159265f;
    tick(room, camera, {1, 0, true}, 120);
    check(camera.feet.z < -11.97f, "jumping does not bypass the locked rear door");
    check(walkTo(room, camera, {11.67f, -12.55f}) && walkTo(room, camera, {6.25f, -12.55f}) &&
              walkTo(room, camera, {6.25f, -10.65f}) && walkTo(room, camera, {8.78f, -10.65f}),
          "return around the classroom to inspect the false hall from the side");
    camera.face(SchoolCorridor::classroomKeySpot);
    check(room.interactCorridor(camera) && room.corridor_.holdingClassroomKey(),
          "take the key hanging over the painted latch");
    check(walkTo(room, camera, {6.25f, -10.65f}) && walkTo(room, camera, {6.25f, -12.55f}) &&
              walkTo(room, camera, {11.67f, -12.55f}),
          "carry the key back to the rear door");
    camera.face(SchoolCorridor::classroomLock);
    tick(room, camera, {}, 35);
    check(room.corridor_.key_.scale == 1.f && !room.corridor_.classroomDoorUnlocked_,
          "carrying the oversized key to the door does not shrink or use it");
    check(walkTo(room, camera, {12.05f, -12.28f}), "walk to a nearby release surface");
    camera.face(SchoolCorridor::classroomLock);
    const auto keyDrop = room.corridor_.keyPlacement(camera, room.collisionBoxes());
    check(keyDrop && keyDrop->scale >= .39f && keyDrop->scale <= .51f && room.corridor_.key_.scale == 1.f,
          "the release preview shows a small key without committing its physical size");
    check(room.interactCorridor(camera) && room.corridor_.looseClassroomKey() && !room.corridor_.classroomDoorUnlocked_,
          "E releases the key into the world instead of unlocking with the preview");
    tick(room, camera, {}, 100);
    camera.face(room.classroomKeyCenter());
    check(room.interactCorridor(camera) && room.corridor_.holdingClassroomKey(),
          "pick up the smaller key after it falls onto the corridor floor");
    camera.face(SchoolCorridor::classroomLock);
    check(room.interactCorridor(camera) && !room.corridor_.holdingClassroomKey(),
          "use the released and recovered key's actual size to unlock the door");
    tick(room, camera, {}, 100);
    check(walkTo(room, camera, {11.67f, -9.9f}) && walkTo(room, camera, {13.45f, -9.9f}),
          "walk through the opened rear classroom door to bypass the false hall");
    check(walkTo(room, camera, {13.45f, -12.4f}) && walkTo(room, camera, {16.5f, -12.4f}),
          "classroom B preserves a side view of the cupboard and partition");
    camera.face(SchoolCorridor::hinge + glm::vec3(-.8f, 1.2f, 0));
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
    camera.face(SchoolCorridor::hinge + glm::vec3(-.8f, 1.2f, 0));
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
                const bool overlaps =
                    a.min.x < b.max.x - .002f && a.max.x > b.min.x + .002f && a.min.y < b.max.y - .002f &&
                    a.max.y > b.min.y + .002f && a.min.z < b.max.z - .002f && a.max.z > b.min.z + .002f;
                if (clearSweep && overlaps)
                    std::cerr << "Door sweep obstruction at angle " << room.cupboardAngle() << ": ["
                              << b.min.x << ',' << b.min.y << ',' << b.min.z << "]-["
                              << b.max.x << ',' << b.max.y << ',' << b.max.z << "]\n";
                clearSweep &= !overlaps;
            }
    }
    check(clearSweep, "door sweep clears furniture, partition, walls and cupboard body");
    check(room.cupboardAngle() > 1.56f && !room.corridorDoorForming(),
          "opening the cupboard alone does not unlock a remote door");
    check(walkTo(room, camera, {SchoolCorridor::illusionEye.x, SchoolCorridor::illusionEye.z}),
          "walk back to the classroom entrance to connect the everyday fittings");
    tick(room, camera, {}, 30);
    camera.face(SchoolCorridor::illusionFocus);
    check(room.corridorAlignment(camera) >= .88f, "normal walking can reach a visually aligned door");
    const auto doorFeedback = room.alignmentFeedback(camera);
    Camera offAxis = camera;
    offAxis.feet.z += .75f;
    offAxis.face(SchoolCorridor::illusionFocus);
    const auto partialDoorFeedback = room.alignmentFeedback(offAxis);
    check(doorFeedback && doorFeedback->aligned && partialDoorFeedback && !partialDoorFeedback->aligned &&
              partialDoorFeedback->score > 0 && partialDoorFeedback->score < doorFeedback->threshold,
          "the door HUD displays partial alignment before the recognition threshold");
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
    check(!room.alignmentFeedback(camera), "the door HUD clears when materialization begins");
    const float frozenFormation = room.corridor_.doorwayFormation();
    room.update(camera, {}, 1, false);
    check(room.corridor_.doorwayFormation() == frozenFormation, "pause freezes doorway materialization");
    camera.yaw += 1.0f;
    tickFor(room, camera, SchoolCorridor::formationDuration + SchoolCorridor::openingDuration);
    check(room.corridorDoorReady() && !room.cupboardOpen(),
          "the real door opens automatically even after looking away once materialization starts");
    check(walkTo(room, camera, {19.7f, -9.9f}), "approach the automatically opened door");
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
    check(room.corridorBridgeAlignment(camera) >= .85f, "matching rays recognize the complete grating");
    const auto bridgeFeedback = room.alignmentFeedback(camera);
    check(bridgeFeedback && bridgeFeedback->aligned && bridgeFeedback->score == room.corridorBridgeAlignment(camera),
          "the bridge HUD uses its own recognition threshold and alignment score");
    room.update(camera, {}, 1, false);
    check(!room.corridorBridgeReady(), "pause cannot form a bridge");
    tickFor(room, camera, .5f + SchoolCorridor::formationDuration);
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
    camera.face(SchoolCorridor::hinge + glm::vec3(-1.1f, 1.2f, 0));
    check(room.toggleCupboard(camera), "cupboard can still open from its far side after materialization");
    tick(room, camera, {}, 120);
    check(room.cupboardAngle() > 1.56f && room.corridorDoorReady(),
          "later cupboard movement cannot undo the real doorway");
    // Swept door collision must stop before reaching the player, including between rendered frames.
    camera.feet = {19.9f, 1.8f, -10.9f};
    camera.face(SchoolCorridor::hinge + glm::vec3(0, 1.2f, 1));
    check(room.toggleCupboard(camera), "close cupboard for sweep recovery check");
    tick(room, camera, {}, 120);
    camera.feet = {18.68f, 1.8f, -10.9f};
    camera.face(SchoolCorridor::hinge + glm::vec3(-1.1f, 1.2f, 0));
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
    camera.face(SchoolCorridor::hinge + glm::vec3(-1.1f, 1.2f, 0));
    check(room.toggleCupboard(camera), "reopen after retreating from the swing area");
    tick(room, camera, {}, 120);
    check(room.cupboardAngle() > 1.56f, "door opens safely when its swing area clears");
    room.reset();
    check(!room.cupboardOpen() && room.cupboardAngle() == 0 && !room.corridorDoorForming() &&
              !room.corridorDoorReady() && !room.corridorBridgeReady() && !room.escaped() &&
              !room.stairsReady(),
          "restart restores both classroom and corridor puzzles");
    camera.feet = {11.67f, 1.8f, -12.55f};
    camera.face(SchoolCorridor::classroomRearDoor);
    check(!room.interactCorridor(camera), "reset puts the lock back on the classroom rear door");
    camera.feet = {10.05f, 1.8f, -9.9f};
    camera.face(SchoolCorridor::classroomKeySpot);
    check(!room.interactCorridor(camera), "the key cannot be reached through the painted wall");
    camera.feet = {8.78f, 1.8f, -10.65f};
    camera.face(SchoolCorridor::classroomKeySpot);
    check(room.interactCorridor(camera), "reset returns the classroom key to its hook");
    camera.feet = {19.7f, 1.8f, -9.9f};
    camera.grounded = true;
    camera.verticalSpeed = 0;
    camera.yaw = 1.570796327f;
    tick(room, camera, {1, 0, true}, 160);
    check(camera.feet.x < 20.29f, "reset restores solid collision at the former passage");
    // Inspect the actual rendered clue points against the physical leaf, not a separate proxy drawing.
    SchoolCorridor innerFace;
    auto clueOccluded = [&](glm::vec3 eye, glm::vec3 point) {
        const auto ray = point - eye;
        for (const auto& b : innerFace.doorBoxes(innerFace.angle_)) {
            const float hit = SchoolRoom::rayBox(eye, glm::normalize(ray), b);
            if (hit >= 0 && hit < glm::length(ray) - .002f)
                return true;
        }
        return false;
    };
    bool shutConcealsClue = true, insideShowsClue = true, outsideConcealsClue = true, staysOnLeaf = true;
    for (float angle : {0.f, .25f, .75f, 1.25f, 1.570796327f}) {
        innerFace.angle_ = angle;
        const glm::vec3 along{-std::cos(angle), 0, std::sin(angle)};
        const glm::vec3 outside{std::sin(angle), 0, std::cos(angle)};
        for (const auto& patch : innerFace.drawing_) {
            if (!patch.onLeaf || patch.material == SchoolCorridor::DoorMaterial::infill)
                continue;
            glm::vec3 center{0};
            for (int i = 0; i < 4; ++i) {
                const auto p = innerFace.paintedPoint(patch, i), local = p - SchoolCorridor::hinge;
                const float distanceAlong = glm::dot(local, along);
                staysOnLeaf &= distanceAlong >= -.0001f && distanceAlong <= SchoolCorridor::doorWidth + .0001f &&
                               local.y >= 0 && local.y <= SchoolCorridor::doorHeight &&
                               glm::dot(local, outside) < -.035f;
                center += p * .25f;
            }
            if (angle == 0)
                shutConcealsClue &= clueOccluded({18.7f, 3.45f, -10.1f}, center) &&
                                    clueOccluded({15.7f, 3.45f, -9.4f}, center);
            if (angle > 1.57f) {
                insideShowsClue &= !clueOccluded(SchoolCorridor::illusionEye, center);
                outsideConcealsClue &= clueOccluded({20.0f, 3.45f, -10.5f}, center);
            }
        }
    }
    check(staysOnLeaf, "every visible clue remains within the inner face throughout the reverse swing");
    check(shutConcealsClue, "the solid closed door hides the inner chart from the corridor");
    check(insideShowsClue, "the open inner face exposes its actual clue geometry to the return view");
    check(outsideConcealsClue, "the open outer face still conceals the inner clue without a visibility toggle");
    Camera insideHandle, outsideHandle;
    insideHandle.feet = {18.5f, 1.8f, -10.9f};
    outsideHandle.feet = {19.9f, 1.8f, -10.9f};
    for (auto* view : {&insideHandle, &outsideHandle})
        view->face(SchoolCorridor::hinge + glm::vec3(0, 1.05f, SchoolCorridor::doorWidth * .82f));
    check(innerFace.canInteract(insideHandle) && innerFace.canInteract(outsideHandle),
          "the open cupboard remains reachable from both its inner and outer faces");
    // Actual angular agreement, not proximity or an observation flag, controls recognition.
    auto prepareCorridor = [&]() {
        room.reset();
        camera = Camera();
        camera.feet = {17.0f, 1.8f, -10.9f};
        camera.face(SchoolCorridor::hinge + glm::vec3(-.8f, 1.2f, 0));
        check(room.toggleCupboard(camera), "prepare the rotating projection surface");
        tick(room, camera, {}, 150);
        camera.feet = SchoolCorridor::illusionEye - glm::vec3(0, 1.65f, 0);
        camera.resetMotion();
        camera.face(SchoolCorridor::illusionFocus);
    };
    prepareCorridor();
    check(room.corridorAlignment(camera) > .999f,
          "reference eye gives matching rays on both receiving planes");
    bool forgivingView = true;
    for (float x : {-.20f, 0.f, .20f})
        for (float z : {-.12f, 0.f, .12f}) {
            Camera nearby = camera;
            nearby.feet += glm::vec3(x, 0, z);
            nearby.face(SchoolCorridor::illusionFocus);
            const auto feedback = room.corridor_.alignmentFeedback(nearby);
            forgivingView = forgivingView && feedback && feedback->threshold == .88f && feedback->aligned;
        }
    check(forgivingView, "the classroom return view retains a forgiving 40 by 24 cm recognition region");
    for (const auto feet : {glm::vec3(18.0f, 1.8f, -10.9f), glm::vec3(18.6f, 1.8f, -10.9f)}) {
        Camera closeView = camera;
        closeView.feet = feet;
        closeView.face(SchoolCorridor::illusionFocus);
        tick(room, closeView, {}, 150);
        check(room.corridorAlignment(closeView) < .80f && !room.corridorDoorForming(),
              "opening the cupboard and waiting at its handle cannot replace the distant observation");
    }
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
    camera.feet = {19.9f, 1.8f, -10.9f};
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
        bool stayedClosedWhileForming = true;
        while (room.corridorDoorFormation() < 1 && elapsed < 3) {
            room.update(camera, {}, 1.0f / rate);
            elapsed += 1.0f / rate;
            stayedClosedWhileForming &= room.corridorDoorFormation() == 1 || room.corridor_.doorwayOpening() == 0;
        }
        const float expected = SchoolCorridor::recognitionDuration + SchoolCorridor::formationDuration;
        check(room.corridorDoorFormation() == 1 && stayedClosedWhileForming && !room.corridorDoorReady() &&
                  elapsed >= expected && elapsed < expected + .08f,
              "materialization finishes before the sliding door starts opening");
        const float formedAt = elapsed;
        while (!room.corridorDoorReady() && elapsed < expected + SchoolCorridor::openingDuration + .1f) {
            room.update(camera, {}, 1.0f / rate);
            elapsed += 1.0f / rate;
        }
        const float openingTime = elapsed - formedAt;
        check(room.corridorDoorReady() && std::abs(openingTime - SchoolCorridor::openingDuration) <= 1.01f / rate,
              "the formed door opens automatically at the intended speed across frame rates");
    }
    for (float rate : {30.0f, 144.0f}) {
        room.reset();
        positionProjector(room, camera);
        viewStudy(camera);
        float elapsed = 0;
        while (!room.stairsReady() && elapsed < SchoolRoom::recognitionDuration + SchoolRoom::formationDuration + .1f) {
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
    check(glm::distance(ratePositions[0], ratePositions[1]) < .0005f,
          "walk and braking differ by under 0.5 mm at 30 and 144 FPS");
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
        camera.face(SchoolCorridor::hinge + glm::vec3(-.8f, 1.2f, 0));
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
    // Every physical press counts even if the previous linked panels are still moving.
    room.reset();
    camera = Camera();
    for (int index : {0, 0, 1, 1, 1, 2, 3, 3, 3}) {
        camera.feet = {SchoolRoom::boardX[index], 0, -2.18f};
        camera.resetMotion();
        camera.face(room.boardHandle(index) + glm::vec3(0, .55f, 0));
        const auto before = room.boardOffsets_;
        check(room.moveBoard(camera) && room.boardOffsets_ == before,
              "rapid board presses retarget from the visible pose without snapping");
        room.update(camera, {}, .05f);
    }
    check(room.boardPositions_ == SchoolRoom::boardGoal && !room.boardsReady(),
          "rapid solution accepts all nine presses but waits for panels to settle");
    tick(room, camera, {}, 32);
    check(room.boardsReady() && room.boardOffsets_ == std::array<float, 4>{},
          "retargeted panels finish exactly at the solved drawing");
    room.reset();
    camera = Camera();
    camera.feet = {SchoolRoom::boardX[0], 0, -2.18f};
    camera.face(room.boardHandle(0) + glm::vec3(0, .55f, 0));
    room.moveBoard(camera);
    auto boardAt30 = room, boardAt144 = room;
    auto camera30 = camera, camera144 = camera;
    for (int i = 0; i < 5; ++i)
        boardAt30.update(camera30, {}, 1.f / 30);
    for (int i = 0; i < 24; ++i)
        boardAt144.update(camera144, {}, 1.f / 144);
    check(std::abs(boardAt30.boardShift(0) - boardAt144.boardShift(0)) < .00001f,
          "moving chalkboard has the same visible pose at 30 and 144 FPS");

    // Short equipment focus uses the shortest yaw arc; ordinary mouse look remains direct.
    for (int fps : {30, 60, 144}) {
        Camera focusCamera;
        focusCamera.yaw = 3.10f;
        const auto target = focusCamera.eye() + glm::vec3(-.04f, .1f, 1.f);
        const auto before = focusCamera.direction();
        focusCamera.focusOn(target);
        check(focusCamera.direction() == before, "equipment interaction does not teleport the view");
        focusCamera.updateFocus(1.f / fps);
        check(focusCamera.yaw > 3.10f && focusCamera.yaw < 3.19f,
              "equipment view crosses the yaw seam by the shortest arc");
        for (int i = 1; i < fps; ++i)
            focusCamera.updateFocus(1.f / fps);
        check(focusCamera.focusTime == 0 &&
                  glm::distance(focusCamera.direction(), glm::normalize(target - focusCamera.eye())) < .00001f,
              "equipment focus finishes at the same exact direction across frame rates");
    }
    room.reset();
    camera = Camera();
    camera.face(SchoolRoom::projectorOrigin);
    room.toggleProjector(camera);
    camera.focusOn({0, 1.9f, -4.f});
    const auto focusBeforePause = camera.direction();
    room.update(camera, {}, .05f, false);
    check(camera.direction() == focusBeforePause && camera.focusTime == .20f,
          "paused room freezes equipment focus along with its puzzle");
    room.update(camera, {}, .05f);
    check(glm::distance(camera.direction(), focusBeforePause) > .01f && camera.focusTime > 0,
          "active room eases equipment focus while keeping its controls active");
    room.update(camera, {0, 0, true}, .02f);
    check(!room.adjustingProjector() && camera.focusTime == 0,
          "jump cancels both equipment control and camera focus");

    for (float threshold : {SchoolRoom::stairAlignmentThreshold, .88f, .85f}) {
        float held = 0;
        for (int i = 0; i < 120; ++i)
            held = advanceAlignmentHold(held, threshold - .01f, threshold, 1.f / 120);
        check(held == 0, "nearby but incorrect alignment never earns recognition time");
        held = advanceAlignmentHold(.2f, threshold - .01f, threshold, 1.f / 120);
        check(held > .18f && held < .2f, "one near-threshold wobble spends time instead of erasing it");
        check(advanceAlignmentHold(held, 0, threshold, 1.f / 120) == 0,
              "looking away or invalidating the puzzle immediately clears recognition");
    }
    room.reset();
    positionProjector(room, camera);
    viewStudy(camera);
    tick(room, camera, {}, 24);
    auto correctView = camera;
    bool nearThreshold = false;
    for (int i = 1; i < 1000; ++i) {
        camera = correctView;
        camera.feet.x += i * .001f;
        const float score = room.alignment(camera);
        if (score >= SchoolRoom::stairAlignmentThreshold - .03f && score < SchoolRoom::stairAlignmentThreshold) {
            nearThreshold = true;
            break;
        }
    }
    check(nearThreshold, "regression fixture finds an actual near-threshold classroom view");
    room.update(camera, {}, 1.f / 120);
    const auto wobbleFeedback = room.alignmentFeedback(camera);
    check(!room.materializing() && room.alignmentHeld_ > .18f && room.alignmentHeld_ < .20f &&
              wobbleFeedback && !wobbleFeedback->aligned && wobbleFeedback->holdProgress > 0,
          "real alignment wobble preserves partial HUD progress without creating stairs");
    tick(room, camera, {}, 120);
    check(room.alignmentHeld_ == 0 && !room.materializing(),
          "remaining outside the real threshold drains progress and never solves the puzzle");
    viewStudy(camera);
    tick(room, camera, {}, 15);
    const auto heldFeedback = room.alignmentFeedback(camera);
    check(heldFeedback && heldFeedback->aligned && heldFeedback->holdProgress > .49f &&
              heldFeedback->holdProgress < .51f,
          "green alignment fill reports actual hold time rather than an instant full bar");
    room.reset();
    check(room.alignmentHeld_ == 0 && !room.alignmentFeedback(camera),
          "reset clears retained alignment progress and its feedback");

    for (bool bridge : {false, true}) {
        prepareCorridor();
        if (bridge) {
            tickFor(room, camera, SchoolCorridor::recognitionDuration + SchoolCorridor::formationDuration +
                                      SchoolCorridor::openingDuration);
            camera.feet = SchoolCorridor::bridgeEye - glm::vec3(0, 1.65f, 0);
            camera.resetMotion();
            camera.face(SchoolCorridor::bridgeFocus);
        }
        tick(room, camera, {}, 24);
        const auto steadyView = camera;
        const float threshold = bridge ? .85f : .88f;
        bool found = false;
        for (int axis : {0, 2}) {
            for (float sign : {-1.f, 1.f}) {
                for (int i = 1; i < 1300 && !found; ++i) {
                    camera = steadyView;
                    camera.feet[axis] += sign * i * .001f;
                    const auto feedback = room.alignmentFeedback(camera);
                    if (feedback && feedback->score >= threshold - .03f && feedback->score < threshold)
                        found = true;
                }
            }
        }
        check(found, bridge ? "find the actual bridge alignment boundary" : "find the actual door alignment boundary");
        room.update(camera, {}, 1.f / 120);
        const auto wobble = room.alignmentFeedback(camera);
        check(wobble && !wobble->aligned && wobble->holdProgress > .30f,
              "corridor door and bridge retain only partial progress during a small wobble");
        tick(room, camera, {}, 120);
        const auto drained = room.alignmentFeedback(camera);
        check(drained && drained->holdProgress == 0 &&
                  (bridge ? !room.corridorBridgeReady() : !room.corridorDoorForming()),
              "incorrect corridor views drain progress without forming a passage");
        camera = steadyView;
        tick(room, camera, {}, 24);
        camera.yaw += 1;
        room.update(camera, {}, 1.f / 120);
        camera = steadyView;
        const auto returned = room.alignmentFeedback(camera);
        check(returned && returned->holdProgress == 0,
              "looking away resets both corridor puzzles before the next attempt");
    }

    // The former outer acceptance band must now stay a clue, even after a long steady look.
    // Also sample a small real area around each solution so tightening cannot leave a single-pixel answer.
    for (int puzzle = 0; puzzle < 3; ++puzzle) {
        if (puzzle == 0) {
            room.reset();
            positionProjector(room, camera);
            viewStudy(camera);
        } else {
            prepareCorridor();
            if (puzzle == 2) {
                tickFor(room, camera, SchoolCorridor::recognitionDuration + SchoolCorridor::formationDuration +
                                          SchoolCorridor::openingDuration);
                camera.feet = SchoolCorridor::bridgeEye - glm::vec3(0, 1.65f, 0);
                camera.resetMotion();
                camera.face(SchoolCorridor::bridgeFocus);
            }
        }
        const auto correct = camera;
        const auto focus = puzzle == 0 ? SchoolRoom::alignmentFocus :
                           puzzle == 1 ? SchoolCorridor::illusionFocus : SchoolCorridor::bridgeFocus;
        const float oldThreshold = puzzle == 0 ? .88f : puzzle == 1 ? .84f : .80f;
        const float newThreshold = 1.f - (1.f - oldThreshold) * .75f;
        bool nearSolution = true;
        for (float dx : {-.1f, 0.f, .1f})
            for (float dz : {-.1f, 0.f, .1f}) {
                auto nearby = correct;
                // The bridge's existing back-wall occlusion bounds the rear edge of its visible area.
                nearby.feet += glm::vec3(puzzle == 2 ? dx * .9f : dx, 0, puzzle == 2 ? (dz - .1f) * .4f : dz);
                nearby.face(focus);
                const auto feedback = room.alignmentFeedback(nearby);
                nearSolution = nearSolution && feedback && feedback->aligned &&
                               std::abs(feedback->threshold - newThreshold) < .0001f;
            }
        check(nearSolution, "tightened puzzles retain a 20 by 20 cm region (bridge 18 by 8 cm toward its gap): " +
                                std::to_string(puzzle));
        bool foundFormerBand = false;
        for (int axis : {0, 2}) {
            for (float sign : {-1.f, 1.f}) {
                for (int i = 1; i <= 1500 && !foundFormerBand; ++i) {
                    camera = correct;
                    camera.feet[axis] += sign * i * .001f;
                    camera.face(focus);
                    const auto feedback = room.alignmentFeedback(camera);
                    if (feedback && feedback->score >= oldThreshold + .005f &&
                        feedback->score < newThreshold - .005f)
                        foundFormerBand = !feedback->aligned;
                }
            }
        }
        check(foundFormerBand, "formerly accepted outer band now reports incomplete alignment: " +
                                   std::to_string(puzzle));
        tick(room, camera, {}, 180);
        const auto feedback = room.alignmentFeedback(camera);
        check(foundFormerBand && feedback && !feedback->aligned && feedback->holdProgress == 0 &&
                  (puzzle == 0 ? !room.materializing() && !room.stairsReady() :
                   puzzle == 1 ? !room.corridorDoorForming() : !room.corridorBridgeReady()),
              "waiting at an old outer-band view cannot materialize the tightened puzzle: " + std::to_string(puzzle));
    }

    room.reset();
    const auto solidOverlap = [](glm::vec3 feet, const Box& b) {
        const float dx = feet.x - std::clamp(feet.x, b.min.x, b.max.x);
        const float dz = feet.z - std::clamp(feet.z, b.min.z, b.max.z);
        return dx * dx + dz * dz < SchoolRoom::radius * SchoolRoom::radius &&
               feet.y + SchoolRoom::bodyHeight > b.min.y + .002f && feet.y < b.max.y - .002f;
    };
    const auto roomSolids = room.collisionBoxes(true, false);
    for (int fps : {30, 144}) {
        for (const auto landing : {glm::vec3(-4.f, 1.5f, 8.005f), glm::vec3(-5.95f, 1.5f, 8.065f),
                                   glm::vec3(-6.015f, 1.5f, 8.5f)}) {
            camera = Camera();
            camera.feet = landing;
            camera.grounded = false;
            camera.verticalSpeed = -1.f;
            for (int frame = 0; frame < fps; ++frame)
                room.update(camera, {}, 1.f / fps);
            check(camera.grounded && std::abs(camera.feet.y - 1.4f) < .002f &&
                      std::none_of(roomSolids.begin(), roomSolids.end(),
                                   [&](const Box& b) { return solidOverlap(camera.feet, b); }),
                  "landing on rear cabinet edges and corners cannot sink into its collision");
        }
        camera = Camera();
        camera.feet = {-4.f, 0, 8.005f};
        for (int frame = 0; frame < fps; ++frame)
            room.update(camera, {1, 0, false}, 1.f / fps);
        check(camera.feet.z < 7.5f &&
                  std::none_of(roomSolids.begin(), roomSolids.end(),
                               [&](const Box& b) { return solidOverlap(camera.feet, b); }),
              "walking away recovers an existing rear-cabinet overlap without restarting");
    }
    for (float ratio : {1.f, 2.3f}) {
        for (float yaw : {-3.14159265f, -1.5707963f}) {
            for (float pitch : {-1.4f, 0.f, 1.4f}) {
                room.reset();
                room.notebookRatio_ = ratio;
                camera = Camera();
                camera.feet = {-6.1f, 0, 8.75f};
                camera.yaw = yaw;
                camera.pitch = pitch;
                check(!room.notebookPlacement(camera), "rear-corner fixture has no safe drop position");
                const auto preview = room.heldNotebookPose(camera);
                const float half = preview.scale * (glm::length(glm::vec3(.42f, .32f, .064f)) * .5f + .025f);
                const Box area{preview.center - glm::vec3(half), preview.center + glm::vec3(half)};
                const auto toward = preview.center - camera.eye();
                const float reach = glm::length(toward);
                check(area.min.x >= -6.5f && area.max.x <= 6.5f && area.min.z >= -4.95f && area.max.z <= 9.f &&
                          area.min.y >= 0 && area.max.y <= 4.5f &&
                          glm::dot(toward, camera.direction()) - half > .055f &&
                          std::none_of(roomSolids.begin(), roomSolids.end(), [&](const Box& solid) {
                              const float hit = SchoolRoom::rayBox(camera.eye(), toward / reach, solid);
                              const bool overlap = area.min.x < solid.max.x && area.max.x > solid.min.x &&
                                                   area.min.z < solid.max.z && area.max.z > solid.min.z &&
                                                   area.min.y < solid.max.y && area.max.y > solid.min.y;
                              return overlap || (hit >= 0 && hit < reach);
                          }),
                      "blocked carry preview stays visible inside the walls with no clipping or occlusion");
                const auto oldBook = room.notebook_;
                room.toggleNotebook(camera);
                check(room.notebookRatio_ == ratio && room.notebook_.center == oldBook.center &&
                          room.notebook_.scale == oldBook.scale,
                      "pressing drop in the blocked corner keeps the notebook held and preserves its size ratio");
            }
        }
    }
    camera.feet = {-4.83f, 0, 3.8f};
    camera.face(SchoolRoom::notebookRest);
    room.toggleNotebook(camera);
    tick(room, camera, {}, 120);
    check(room.notebookRatio_ == 0 && !room.notebookFalling_ &&
              glm::distance(glm::vec2(room.notebook_.center.x, room.notebook_.center.z),
                            glm::vec2(SchoolRoom::notebookRest.x, SchoolRoom::notebookRest.z)) < .002f,
          "leaving the blocked corner permits normal notebook placement on its mirror stand");
    camera.feet = {5.4f, 0, 2.6f};
    camera.resetMotion();
    camera.face(room.notebookCenter());
    check(room.toggleNotebook(camera) && room.notebookRatio_ > 0,
          "the recovered notebook can be picked up from its stand again");
    for (int cycle = 0; cycle < 5; ++cycle) {
        camera.feet = {4.8f, 0, 2.4f};
        camera.resetMotion();
        camera.face({3.8f, 0, 2.4f});
        room.toggleNotebook(camera);
        tick(room, camera, {}, 120);
        const auto placed = room.notebookBounds(room.notebook_);
        check(room.notebookRatio_ == 0 && !room.notebookFalling_ && placed.min.x >= -6.5f &&
                  placed.max.x <= 6.5f && placed.min.z >= -4.95f && placed.max.z <= 9.f && placed.min.y >= -.003f,
              "repeated notebook drops settle inside the classroom");
        camera.face(room.notebookCenter());
        check(room.toggleNotebook(camera) && room.notebookRatio_ > 0,
              "repeated notebook pickups retain the book after recovery from a blocked drop");
    }

    room.reset();
    // A slow aim sweep across the floor/back-wall/ceiling must not quantize the held scale.
    camera = Camera();
    camera.feet = {-4.83f, 0, 3.8f};
    camera.face(room.notebookCenter());
    check(room.toggleNotebook(camera), "pick up the notebook for a continuous perspective sweep");
    camera.feet = {3.8f, 0, 2.4f};
    camera.yaw = 3.14159265f;
    float previousScale = 0, largestScaleStep = 0, minimumScale = 100, maximumScale = 0;
    bool continuousAim = true, clearBody = true;
    const auto sweepSolids = room.collisionBoxes(true, false);
    for (int sample = 0; sample <= 1000; ++sample) {
        camera.pitch = -1.f + sample * .0015f;
        const auto pose = room.notebookPlacement(camera);
        if (!pose) {
            continuousAim = false;
            continue;
        }
        const auto toward = pose->center - camera.eye(), ray = camera.direction();
        const float depth = glm::dot(toward, ray);
        continuousAim = continuousAim && depth > 0 && glm::length(toward - ray * depth) < .0001f &&
                        std::abs(pose->scale / depth - room.notebookRatio_) < .0001f;
        if (previousScale > 0)
            largestScaleStep = std::max(largestScaleStep, std::abs(pose->scale - previousScale));
        previousScale = pose->scale;
        minimumScale = std::min(minimumScale, pose->scale);
        maximumScale = std::max(maximumScale, pose->scale);
        const auto area = room.notebookBounds(*pose);
        for (const auto& solid : sweepSolids) {
            const bool overlap = area.min.x < solid.max.x && area.max.x > solid.min.x &&
                                 area.min.z < solid.max.z && area.max.z > solid.min.z &&
                                 area.min.y < solid.max.y - .002f && area.max.y > solid.min.y + .002f;
            const float hit = SchoolRoom::rayBox(camera.eye(), ray, solid);
            clearBody = clearBody && !overlap && (hit < 0 || hit >= depth - .002f);
        }
    }
    check(continuousAim, "a held notebook keeps its apparent scale and stays on the sight ray during a slow sweep");
    check(largestScaleStep < .03f && maximumScale - minimumScale > 2.f,
          "floor/wall contact changes size continuously instead of shrinking in discrete 16 percent steps");
    check(clearBody, "continuous scaling never places the notebook through a solid or behind an occluder");
    camera.feet = {3.10f, 0, 1.86f};
    camera.face({1.15f, .78f, .65f});
    const auto tablePose = room.notebookPlacement(camera);
    const float tableDepth = tablePose ? glm::dot(tablePose->center - camera.eye(), camera.direction()) : 0;
    check(tablePose && tableDepth > 0 &&
              glm::length(tablePose->center - camera.eye() - camera.direction() * tableDepth) < .0001f &&
              std::abs(tablePose->scale / tableDepth - room.notebookRatio_) < .0001f &&
              std::abs(room.notebookBounds(*tablePose).min.y - .78f) < .004f,
          "table placement keeps the same apparent size without shifting the book sideways or upward");
    // A steep upward aim must not send the book to an unseen fallback on the floor.
    for (float fps : {10.f, 30.f, 144.f}) {
        for (float pitch : {1.3f, 1.45f, 1.54f}) {
            room.reset();
            camera = Camera();
            camera.feet = {-4.83f, 0, 3.8f};
            camera.face(room.notebookCenter());
            check(room.toggleNotebook(camera), "pick up the original notebook for a steep upward release");
            const float carryRatio = room.notebookRatio_;
            const auto original = room.notebook_;
            camera.feet = {3.8f, 0, 2.4f};
            camera.yaw = 0;
            camera.pitch = pitch;
            const auto placement = room.notebookPlacement(camera);
            const auto shown = placement.value_or(room.heldNotebookPose(camera));
            check(!placement && glm::dot(glm::normalize(shown.center - camera.eye()), camera.direction()) > .9f &&
                      room.notebookBounds(shown).max.y < 4.5f,
                  "a blocked ceiling aim keeps a visible notebook below the roof instead of showing an unseen floor drop");
            room.toggleNotebook(camera);
            for (int frame = 0; frame < static_cast<int>(fps); ++frame)
                room.update(camera, {}, 1.f / fps);
            check(room.notebookRatio_ == carryRatio && !room.notebookFalling_ &&
                      room.notebook_.center == original.center && room.notebook_.scale == original.scale,
                  "pressing release at the blocked ceiling preserves the held book at low and high frame rates");
            camera.face({3.8f, 0, 1.1f});
            const auto floorPlacement = room.notebookPlacement(camera);
            check(floorPlacement && room.toggleNotebook(camera) && room.notebookRatio_ == 0,
                  "lowering the view after a blocked ceiling release permits a normal floor placement");
            for (int frame = 0; frame < static_cast<int>(fps * 2); ++frame)
                room.update(camera, {}, 1.f / fps);
            camera.face(room.notebookCenter());
            check(!room.notebookFalling_ && room.notebookBounds(room.notebook_).min.y >= -.003f &&
                      room.toggleNotebook(camera) && room.notebookRatio_ > 0,
                  "the notebook remains in the room and can be picked up again after lowering the view");
        }
    }
    room.reset();
    const auto sealedSolids = room.collisionBoxes(true, false);
    auto nearestSolid = [&](glm::vec3 origin, glm::vec3 ray) {
        float nearest = 1000;
        for (const auto& solid : sealedSolids) {
            const float hit = SchoolRoom::rayBox(origin, ray, solid);
            if (hit >= 0)
                nearest = std::min(nearest, hit);
        }
        return nearest;
    };
    for (const auto eye : {glm::vec3(22.22f, 2.65f, -18.5f), glm::vec3(24.87f, 2.65f, -18.5f)}) {
        for (float x : {22.9f, 23.55f, 24.8f}) {
            const auto ray = glm::normalize(glm::vec3(x, .8f, -12.f) - eye);
            const float hitDepth = nearestSolid(eye, ray);
            check(hitDepth < 8.f && (eye + ray * hitDepth).z <= -11.99f,
                  "looking up the stairs cannot see the world void below the near landing");
        }
    }
    check(std::abs(nearestSolid({3.8f, 1.65f, 2.4f}, {0, 1, 0}) - 2.85f) < .003f,
          "the visible classroom ceiling also stops placement rays");
    // Inspect the real body and support after repeated high-angle releases, including while airborne.
    for (const auto feet : {glm::vec3(3.8f, 0, 2.4f), glm::vec3(3.8f, 1.1f, 2.4f),
                            glm::vec3(3.05f, 1.8f, -6.f), glm::vec3(23.55f, .6f, SchoolCorridor::exitZ)}) {
        for (float ratio : {.3f, 1.f, 2.3f}) {
            for (float pitch : {.6f, 1.45f}) {
                room.reset();
                room.notebookRatio_ = ratio;
                camera = Camera();
                camera.feet = feet;
                camera.pitch = pitch;
                camera.grounded = feet.y != 1.1f;
                const auto pose = room.notebookPlacement(camera);
                const auto shown = pose.value_or(room.heldNotebookPose(camera));
                const auto area = room.notebookBounds(shown);
                const float localCeiling = feet.z < -20.6f ? 3.1f : (feet.z < -5.f ? 4.325f : 4.5f);
                const auto toward = shown.center - camera.eye();
                const float reach = glm::length(toward);
                check(area.max.y <= localCeiling + .002f &&
                          nearestSolid(camera.eye(), toward / reach) >= reach - .002f,
                      "upward notebook placement and held previews remain below the actual ceiling and in sight");
                room.toggleNotebook(camera);
                if (pose) {
                    tick(room, camera, {}, 180);
                    const auto settled = room.notebookBounds(room.notebook_);
                    check(room.notebookRatio_ == 0 && !room.notebookFalling_ && settled.min.y >= -.003f &&
                              settled.max.y <= localCeiling + .002f,
                          "an upward release settles inside the room instead of escaping above its roof");
                } else
                    check(room.notebookRatio_ == ratio, "a blocked upward release keeps the notebook in hand");
            }
        }
    }

    // Drop/recover is the only way to commit a new physical key size.
    for (int fps : {30, 60, 144}) {
        for (const auto pickup : {glm::vec3(8.78f, 1.8f, -10.65f), glm::vec3(9.29f, 1.8f, -9.9f),
                                   glm::vec3(8.28f, 1.8f, -9.9f)}) {
            SchoolRoom keyRoom;
            Camera keyCamera;
            keyCamera.feet = pickup;
            keyCamera.face(SchoolCorridor::classroomKeySpot);
            auto& keyCorridor = keyRoom.corridor_;
            auto settleKey = [&]() {
                keyCamera.resetMotion();
                for (int frame = 0; frame < fps; ++frame)
                    keyRoom.update(keyCamera, {}, 1.f / fps);
            };
            auto world = keyRoom.collisionBoxes();
            check(keyRoom.interactCorridor(keyCamera) && keyCorridor.holdingClassroomKey() &&
                      keyCorridor.classroomKeyStatus(keyCamera, world) && keyCorridor.key_.scale == 1.f,
                  "pickup preserves the physical key size and shows its held state");
            for (float z : {-13.f, -12.28f, -12.01f}) {
                keyCamera.feet = {12.05f, 1.8f, z};
                keyCamera.face(SchoolCorridor::classroomLock);
                settleKey();
                const auto atLock = keyCorridor.keyAtLock(keyCamera, world);
                check(atLock && atLock->scale == 1.f && keyCorridor.key_.scale == 1.f &&
                          keyCorridor.classroomAction(keyCamera, world) != SchoolCorridor::ClassroomAction::unlockDoor,
                      "moving closer to the lock never shrinks or unlocks an unmodified key");
            }
            // Ordinary release against the classroom's back wall, then gravity and a real pickup.
            // A point-blank initial grab may need an intermediate drop to reduce its apparent size.
            bool recoveredFit = false;
            for (int attempt = 0; attempt < 4 && !recoveredFit; ++attempt) {
                bool foundFit = false;
                for (int i = 0; i <= 400; ++i) {
                    keyCamera.feet = {11.5f, 1.8f, -16.28f + i * .01f};
                    keyCamera.face({11.5f, 3.45f, -16.6f});
                    const auto placement = keyCorridor.keyPlacement(keyCamera, world);
                    if (placement && placement->scale > .40f && placement->scale < .50f) {
                        foundFit = true;
                        break;
                    }
                }
                if (!foundFit) {
                    keyCamera.feet = {11.5f, 1.8f, -16.05f};
                    keyCamera.face({11.5f, 3.45f, -16.6f});
                }
                const auto preview = keyCorridor.keyPlacement(keyCamera, world);
                check(preview && keyRoom.interactCorridor(keyCamera) && keyCorridor.looseClassroomKey() &&
                          !keyCorridor.classroomDoorUnlocked_, "E commits a world key instead of unlocking with a preview");
                const auto released = keyCorridor.key_;
                keyRoom.update(keyCamera, {}, 1.f, false);
                check(keyCorridor.key_.center == released.center && keyCorridor.key_.scale == released.scale,
                      "pause freezes a released key before it falls");
                settleKey();
                check(keyCorridor.key_.center.y < released.center.y - .5f &&
                          std::abs(keyCorridor.key_.center.y - (1.806f + .006f * released.scale)) < .002f &&
                          keyCorridor.key_.scale == released.scale && keyCorridor.keyFallSpeed_ == 0,
                      "the key falls to the visible floor without tunnelling or changing its committed size");
                keyCamera.feet = {12.8f, 1.8f, keyCorridor.key_.center.z};
                keyCamera.face(keyCorridor.key_.center);
                check(!keyRoom.interactCorridor(keyCamera), "a dropped key cannot be picked up through the classroom wall");
                keyCamera.feet = {11.5f, 1.8f, keyCorridor.key_.center.z + 1.1f};
                keyCamera.face(keyCorridor.key_.center);
                check(keyRoom.interactCorridor(keyCamera) && keyCorridor.holdingClassroomKey() &&
                          keyCorridor.key_.scale == released.scale,
                      "picking up the fallen key preserves its released physical size");
                recoveredFit = foundFit && keyCorridor.key_.scale >= .39f && keyCorridor.key_.scale <= .51f;
            }
            check(recoveredFit, "the perspective release puzzle is solvable even after a point-blank pickup");
            for (float z : {-13.f, -12.01f}) {
                keyCamera.feet = {12.05f, 1.8f, z};
                keyCamera.face(SchoolCorridor::classroomLock);
                const auto actual = keyCorridor.keyAtLock(keyCamera, world);
                check(actual && actual->scale == keyCorridor.key_.scale &&
                          keyCorridor.classroomAction(keyCamera, world) == SchoolCorridor::ClassroomAction::unlockDoor,
                      "a recovered matching key stays the same size at different reachable lock distances");
            }
            keyCamera.feet = {12.05f, 1.8f, -11.3f};
            keyCamera.face(SchoolCorridor::classroomLock);
            check(!keyCorridor.keyAtLock(keyCamera, world), "the closed door blocks lock use from its back");
            keyCamera.feet = {13.4f, 1.8f, -12.7f};
            keyCamera.face(SchoolCorridor::classroomLock);
            check(!keyCorridor.keyAtLock(keyCamera, world), "the adjacent classroom wall blocks the lock");
            keyCamera.feet = {12.05f, 1.8f, -12.28f};
            keyCamera.face(SchoolCorridor::classroomLock);
            check(keyRoom.interactCorridor(keyCamera) && keyCorridor.classroomDoorUnlocked_ &&
                      !keyCorridor.holdingClassroomKey() && !keyCorridor.looseClassroomKey() &&
                      !keyCorridor.classroomKeyStatus(keyCamera, world),
                  "only the actual recovered key unlocks and leaves the held and loose states");
            check(!keyRoom.interactCorridor(keyCamera), "the inserted key cannot be consumed twice");
            keyRoom.reset();
            check(!keyCorridor.holdingClassroomKey() && !keyCorridor.looseClassroomKey() &&
                      !keyCorridor.classroomDoorUnlocked_ && keyCorridor.key_.center == SchoolCorridor::classroomKeySpot &&
                      keyCorridor.key_.scale == 1.f && keyCorridor.keyFallSpeed_ == 0,
                  "reset restores the original hook, scale, ownership, fall and lock state");
        }
    }

    // Like the military props, a dropped key can be picked up from a distance and shrunk on nearby floor.
    for (int fps : {30, 144}) {
        SchoolRoom keyRoom;
        Camera keyCamera;
        auto& key = keyRoom.corridor_;
        keyCamera.feet = {8.78f, 1.8f, -10.65f};
        keyCamera.face(SchoolCorridor::classroomKeySpot);
        check(keyRoom.interactCorridor(keyCamera), "discover the key at its original close pickup range");
        keyCamera.feet = {11.5f, 1.8f, -15.4f};
        keyCamera.face({11.5f, 3.45f, -16.6f});
        check(keyRoom.interactCorridor(keyCamera) && key.looseClassroomKey(), "release a key for distant recovery");
        keyCamera.resetMotion();
        for (int frame = 0; frame < fps; ++frame)
            keyRoom.update(keyCamera, {}, 1.f / fps);
        keyCamera.feet = {12.8f, 1.8f, -12.9f};
        keyCamera.face(key.key_.center);
        check(!keyRoom.interactCorridor(keyCamera), "distant recovery still rejects a key behind the adjacent wall");
        keyCamera.feet = {11.5f, 1.8f, -12.9f};
        keyCamera.face(key.key_.center);
        const auto before = key.key_;
        check(glm::distance(keyCamera.eye(), before.center) > 3.f && keyRoom.interactCorridor(keyCamera) &&
                  key.holdingClassroomKey() && key.key_.scale == before.scale,
              "recovering from beyond the old reach preserves size and changes only the held perspective ratio");
        const auto world = keyRoom.collisionBoxes();
        bool broadFloorRange = true;
        for (float degrees : {60.f, 65.f, 70.f, 75.f, 80.f, 85.f}) {
            keyCamera.pitch = -glm::radians(degrees);
            const auto preview = key.keyPlacement(keyCamera, world);
            broadFloorRange = broadFloorRange && preview && preview->scale >= .39f && preview->scale <= .51f &&
                              std::wstring_view(key.classroomKeyStatus(keyCamera, world)).find(L"놓으면 맞는") != std::wstring_view::npos;
        }
        check(broadFloorRange, "distant recovery gives a matching release across a broad downward aim range");
        const auto floorCamera = keyCamera;
        keyCamera.feet = {9.8f, 1.8f, -11.86f};
        keyCamera.yaw = 3.141592654f;
        keyCamera.pitch = glm::radians(-85.f);
        const auto atGlass = key.keyPlacement(keyCamera, world);
        check(!atGlass && keyRoom.interactCorridor(keyCamera) && key.holdingClassroomKey(),
              "near-feet fallback cannot move a key through glass when facing down against the wall");
        keyCamera = floorCamera;
        const auto preview = key.keyPlacement(keyCamera, world);
        check(preview && keyRoom.interactCorridor(keyCamera) && key.looseClassroomKey() &&
                  key.key_.scale == preview->scale && key.key_.scale < before.scale &&
                  glm::dot(key.key_.center - keyCamera.feet, glm::vec3(0, 0, -1)) > SchoolRoom::radius,
              "steep downward release places the smaller key in front of the feet instead of blocking or dropping behind");
        for (int frame = 0; frame < fps; ++frame)
            keyRoom.update(keyCamera, {}, 1.f / fps);
        keyCamera.face(key.key_.center);
        check(keyRoom.interactCorridor(keyCamera), "recover the floor-scaled key without moving against a wall");
        keyCamera.feet = {12.05f, 1.8f, -12.5f};
        keyCamera.face(SchoolCorridor::classroomLock);
        check(keyRoom.interactCorridor(keyCamera) && key.classroomDoorUnlocked_,
              "the key resized on nearby floor unlocks only after being picked up again");
    }

    // Enlarge, shrink too far, recover, and test releases against the real walls and ceiling.
    for (int fps : {30, 144}) {
        for (const auto aim : {glm::vec4(11.5f, 1.8f, -12.5f, 0.f),
                                glm::vec4(12.05f, 1.8f, -12.10f, 1.f),
                                glm::vec4(8.f, 1.8f, -13.f, 2.f),
                                glm::vec4(9.8f, 1.8f, -11.86f, 3.f)}) {
            SchoolRoom keyRoom;
            Camera keyCamera;
            keyCamera.feet = {8.78f, 1.8f, -10.65f};
            keyCamera.face(SchoolCorridor::classroomKeySpot);
            check(keyRoom.interactCorridor(keyCamera), "prepare the release boundary fixture");
            keyCamera.feet = glm::vec3(aim);
            if (aim.w == 0)
                keyCamera.face({11.5f, 3.45f, -16.6f});
            else if (aim.w == 1)
                keyCamera.face(SchoolCorridor::classroomLock);
            else if (aim.w == 2)
                keyCamera.face(keyCamera.eye() + glm::vec3(0, 1, -.12f));
            else
                keyCamera.face({9.8f, 3.45f, -10.f});
            const auto world = keyRoom.collisionBoxes();
            auto& key = keyRoom.corridor_;
            const auto preview = key.keyPlacement(keyCamera, world);
            const auto original = key.key_;
            check(keyRoom.interactCorridor(keyCamera), "release input is safely consumed even when no placement fits");
            if (!preview) {
                check(key.holdingClassroomKey() && key.key_.center == original.center && key.key_.scale == original.scale,
                      "a blocked release keeps the physical key and its size in hand");
                const auto pose = key.carriedKeyPose(keyCamera, world);
                const auto toward = pose.center - keyCamera.eye();
                bool visible = pose.scale > 0;
                for (const auto& solid : world) {
                    const float hit = SchoolRoom::rayBox(keyCamera.eye(), glm::normalize(toward), solid);
                    visible = visible && (hit < 0 || hit >= glm::length(toward));
                }
                check(visible, "the blocked held model stays on the camera side of thin glass");
                continue;
            }
            check(!key.holdingClassroomKey() && key.key_.scale == preview->scale,
                  "only release commits the preview scale to the physical key");
            bool clear = true;
            for (int frame = 0; frame < fps; ++frame) {
                keyRoom.update(keyCamera, {}, 1.f / fps);
                const auto center = key.key_.center;
                const auto half = glm::vec3(.025f, .006f, .048f) * key.key_.scale;
                for (const auto& solid : world)
                    clear = clear && !(center.x + half.x > solid.min.x + .002f && center.x - half.x < solid.max.x - .002f &&
                                       center.y + half.y > solid.min.y + .002f && center.y - half.y < solid.max.y - .002f &&
                                       center.z + half.z > solid.min.z + .002f && center.z - half.z < solid.max.z - .002f);
                clear = clear && center.y - half.y >= 1.8f && center.y + half.y < 5.1f;
            }
            check(clear && key.key_.scale == preview->scale,
                  "every falling-key frame stays clear of floor, ceiling, glass and furniture at both rates");
            if (aim.w == 0)
                check(key.key_.scale > 2.f, "a distant release enlarges the key without a wheel or lock interaction");
            if (aim.w == 1) {
                check(key.key_.scale < .39f, "a closer release can make the key too small");
                keyCamera.face(key.key_.center);
                check(keyRoom.interactCorridor(keyCamera), "recover the undersized key from the floor");
                keyCamera.face(SchoolCorridor::classroomLock);
                check(key.classroomAction(keyCamera, world) != SchoolCorridor::ClassroomAction::unlockDoor &&
                          std::wstring_view(key.classroomKeyStatus(keyCamera, world)).find(L"작아요") != std::wstring_view::npos,
                      "a recovered undersized key is still rejected by the lock");
            }
        }
    }

    room.reset();
    camera = Camera();
    camera.feet = {8.78f, 1.8f, -10.65f};
    camera.face(SchoolCorridor::classroomKeySpot);
    check(room.interactCorridor(camera), "prepare single held-item checks");
    camera.feet = {-4.83f, 0, 3.8f};
    camera.face(room.notebookCenter());
    check(!room.toggleNotebook(camera), "the notebook cannot be picked up while holding the key");
    camera.feet = {11.5f, 1.8f, -15.9f};
    camera.face({11.5f, 3.45f, -16.6f});
    check(room.interactCorridor(camera) && room.corridor_.looseClassroomKey(), "put the key down to free the hand");
    tick(room, camera, {}, 120);
    camera.feet = {-4.83f, 0, 3.8f};
    camera.face(room.notebookCenter());
    check(room.toggleNotebook(camera), "a dropped key no longer blocks notebook pickup");
    camera.feet = {11.5f, 1.8f, room.classroomKeyCenter().z + 1.1f};
    camera.face(room.classroomKeyCenter());
    check(!room.interactCorridor(camera) && !room.corridor_.holdingClassroomKey(),
          "a held notebook still prevents picking up the loose key");

    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
} // namespace school
