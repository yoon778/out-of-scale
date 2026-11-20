#include "SchoolPreview.h"
#include "SchoolRenderer.h"
#include <GL/glfw3.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

namespace {
struct Application {
    school::SchoolRoom& room;
    school::Camera& camera;
    school::PreviewNavigation* navigation;
    bool paused = false, jump = false, sample = false;
    double mouseX = 0, mouseY = 0, mouseResumeAt = 0;
};
Application& app(GLFWwindow* window) {
    return *static_cast<Application*>(glfwGetWindowUserPointer(window));
}
void pause(GLFWwindow* window, bool paused) {
    auto& state = app(window);
    state.paused = paused;
    if (paused) {
        state.room.stopAdjustingProjector();
        state.room.stopAdjustingMirror();
        state.camera.movement = {};
        state.camera.focusTime = 0;
    }
    state.sample = false;
    state.jump = false;
    state.mouseResumeAt = glfwGetTime() + 0.12;
    glfwSetInputMode(window, GLFW_CURSOR, paused ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    glfwGetCursorPos(window, &state.mouseX, &state.mouseY);
}
void drawPauseMenu(school::Renderer& r, int width, int height) {
    const float scale = std::min(width / 1280.f, height / 800.f);
    const float x = width / 2.f - 230 * scale, y = height / 2.f - 108 * scale;
    r.overlay(width, height);
    r.panel(x, y, 460 * scale, 216 * scale, {.10f, .19f, .15f});
    r.text(L"잠시 쉬는 시간", x + 38 * scale, y + 20 * scale, 44 * scale);
    r.text(L"Esc   계속하기", x + 38 * scale, y + 82 * scale, 31 * scale);
    r.text(L"R      학교 처음부터", x + 38 * scale, y + 125 * scale, 31 * scale);
    r.text(L"Q      종료", x + 38 * scale, y + 168 * scale, 31 * scale);
}
void drawSchoolControls(school::Renderer& r, int width, int height) {
    const float scale = std::min(width / 1280.f, height / 800.f);
    const float x = width / 2.f - 250 * scale, y = height / 2.f + 166 * scale;
    r.overlay(width, height);
    r.panel(x, y, 500 * scale, 138 * scale, {.12f, .22f, .18f});
    r.text(L"조작법", x + 22 * scale, y + 9 * scale, 28 * scale);
    r.text(L"WASD 이동   ·   마우스 시점   ·   E 조작   ·   Space 점프",
           x + 22 * scale, y + 43 * scale, 23 * scale);
    r.text(L"공책은 E로 놓습니다. 멀리 놓으면 커집니다.",
           x + 22 * scale, y + 77 * scale, 21 * scale);
    r.text(L"프로젝터와 거울의 방향은 마우스로 돌립니다.",
           x + 22 * scale, y + 104 * scale, 21 * scale);
}
void settle(Application& state) {
    for (int i = 0; i < 180; ++i)
        state.room.update(state.camera, {}, 1.0f / 120);
}
void stageMirrorCapture(Application& state, const std::string& stage) {
    state.camera.feet = {-4.83f, 0.f, 3.80f};
    state.camera.face(state.room.notebookCenter());
    if (!state.room.toggleNotebook(state.camera))
        throw std::runtime_error("Capture could not pick up the notebook");
    const auto stand = school::SchoolRoom::notebookRest;
    state.camera.face(stand);
    if (stage == "notebook-at-stand")
        return;
    state.room.toggleNotebook(state.camera);
    state.camera.feet = {-5.64f, 0.f, 2.61f};
    const auto mirror = school::SchoolRoom::mirrorCenter;
    state.camera.face(school::SchoolRoom::mirrorAim);
    if (stage == "mirror-unaligned")
        return;
    if (!state.room.toggleMirror(state.camera))
        throw std::runtime_error("Capture could not adjust the mirror");
    auto horizontal = [](glm::vec3 v) { return glm::normalize(glm::vec2(v.x, v.z)); };
    const auto bisector = horizontal(state.camera.eye() - mirror) + horizontal(state.room.notebookCenter() - mirror);
    const auto normal = state.room.mirrorNormal();
    const float turn = std::atan2(bisector.x, bisector.y) - std::atan2(normal.x, normal.z);
    state.room.rotateMirror(stage == "mirror-rotating" ? turn * .76f : turn);
    if (stage == "mirror-rotating")
        return;
    state.room.toggleMirror(state.camera);
    state.camera.face(school::Renderer::reflectPoint(
        state.room.notebookCenter(), mirror, state.room.mirrorNormal()));
}
void changeBoards(Application& state, bool solve) {
    constexpr std::array presses{0, 0, 1, 1, 1, 2, 3, 3, 3};
    const size_t count = solve ? presses.size() : 1;
    for (size_t i = 0; i < count; ++i) {
        int index = presses[i];
        state.camera.feet = {school::SchoolRoom::boardX[index], 0, -2.18f};
        state.camera.resetMotion();
        state.camera.face(state.room.boardHandle(index) + glm::vec3(0, .55f, 0));
        if (!state.room.moveBoard(state.camera))
            throw std::runtime_error("Capture could not move the chalkboard");
        for (int frame = 0; frame < 36; ++frame)
            state.room.update(state.camera, {}, 1.f / 120);
    }
    if (solve && !state.room.boardsReady())
        throw std::runtime_error("Capture did not align the chalkboards");
}
void turnProjectorTowardsPaper(school::SchoolRoom& room, float fraction) {
    const auto start = glm::normalize(school::SchoolRoom::projectorStartAim - school::SchoolRoom::projectorOrigin);
    const auto target = glm::normalize(school::SchoolRoom::projectorPaper - school::SchoolRoom::projectorOrigin);
    room.rotateProjector((std::atan2(target.x, -target.z) - std::atan2(start.x, -start.z)) * fraction,
                         (std::asin(target.y) - std::asin(start.y)) * fraction);
}
void aimProjector(Application& state, float fraction = 1) {
    changeBoards(state, true);
    state.camera.feet = {4.8f, 0, 6.4f};
    state.camera.face(school::SchoolRoom::projectorOrigin);
    if (!state.room.toggleProjector(state.camera))
        throw std::runtime_error("Capture could not operate the projector");
    turnProjectorTowardsPaper(state.room, fraction);
    state.room.toggleProjector(state.camera);
    state.camera.face({0.0f, 1.9f, -4.0f});
    state.room.update(state.camera, {}, 1.0f / 120);
}
void stageCapture(Application& state, const std::string& stage) {
    if (stage.rfind("focus-", 0) == 0) {
        const bool mirror = stage.find("mirror") != std::string::npos;
        if (mirror) {
            state.camera.feet = {-4.83f, 0.f, 2.50f};
            state.camera.face(school::SchoolRoom::mirrorCenter + glm::vec3(.35f, -.5f, 0));
            if (!state.room.toggleMirror(state.camera))
                throw std::runtime_error("Capture could not start mirror focus");
            state.camera.focusOn(school::SchoolRoom::mirrorAim);
        } else {
            state.camera.face(school::SchoolRoom::projectorOrigin);
            if (!state.room.toggleProjector(state.camera))
                throw std::runtime_error("Capture could not start projector focus");
            state.camera.focusOn({0, 1.9f, -4.f});
        }
        const int frames = stage.ends_with("end") ? 26 : stage.ends_with("middle") ? 12 : 0;
        for (int i = 0; i < frames; ++i)
            state.room.update(state.camera, {}, 1.f / 120);
        return;
    }
    if (stage.rfind("corridor-", 0) == 0) {
        aimProjector(state);
        state.camera.feet = school::SchoolRoom::alignmentEye - glm::vec3(0, 1.65f, 0);
        state.camera.face(school::SchoolRoom::alignmentFocus);
        settle(state);
        if (stage == "corridor-connection") {
            state.camera.feet = {2.9f, 1.8f, -6.1f};
            state.camera.face({3.1f, 3.4f, -9.8f});
        } else if (stage == "corridor-class-sign") {
            state.camera.feet = {4.8f, 1.8f, -9.15f};
            state.camera.face({6.25f, 3.70f, -11.48f});
        } else if (stage == "corridor-window-a") {
            state.camera.feet = {9.8f, 1.8f, -12.55f};
            state.camera.face({10.0f, 2.5f, -9.5f});
        } else if (stage == "corridor-window-b") {
            state.camera.feet = {16.5f, 1.8f, -12.4f};
            state.camera.face({18.1f, 2.8f, -10.85f});
        } else if (stage == "corridor-door-closed") {
            state.camera.feet = {18.7f, 1.8f, -10.1f};
            state.camera.face(school::SchoolCorridor::hinge + glm::vec3(.8f, 1.2f, 0));
        } else if (stage == "corridor-false-close") {
            state.camera.feet = {9.0f, 1.8f, -10.65f};
            state.camera.face({9.55f, 3.25f, -9.9f});
        } else if (stage == "corridor-class-key" || stage == "corridor-class-exit" ||
                   stage == "corridor-class-unlocked") {
            state.camera.feet = {8.78f, 1.8f, -10.65f};
            state.camera.face(school::SchoolCorridor::classroomKeySpot);
            if (stage != "corridor-class-key") {
                if (stage == "corridor-class-unlocked" && !state.room.interactCorridor(state.camera))
                    throw std::runtime_error("Capture could not find the classroom key");
                state.camera.feet = {11.60f, 1.8f, -13.0f};
                state.camera.face(school::SchoolCorridor::classroomRearDoor);
                if (stage == "corridor-class-unlocked") {
                    if (!state.room.interactCorridor(state.camera))
                        throw std::runtime_error("Capture could not unlock the classroom rear door");
                    settle(state);
                }
            }
        } else if (stage == "corridor-door-open" || stage.rfind("corridor-illusion-", 0) == 0 ||
                   stage.rfind("corridor-bridge-", 0) == 0 || stage == "corridor-stairwell") {
            state.camera.feet = {17.0f, 1.8f, -10.9f};
            state.camera.face(school::SchoolCorridor::hinge + glm::vec3(.8f, 1.2f, 0));
            if (!state.room.toggleCupboard(state.camera))
                throw std::runtime_error("Capture cannot open cupboard");
            settle(state);
            if (stage != "corridor-door-open") {
                state.camera.feet = school::SchoolCorridor::illusionEye - glm::vec3(0, 1.65f, 0);
                state.camera.resetMotion();
                if (stage == "corridor-illusion-wrong")
                    state.camera.feet.z += 1.1f;
                else if (stage == "corridor-illusion-partial")
                    state.camera.feet.z += .45f;
                state.camera.face(school::SchoolCorridor::illusionFocus);
                if (stage == "corridor-illusion-forming") {
                    for (int frame = 0; frame < 105; ++frame)
                        state.room.update(state.camera, {}, 1.0f / 120);
                } else if (stage == "corridor-illusion-formed") {
                    for (int frame = 0; frame < 180 && state.room.corridorDoorFormation() < 1; ++frame)
                        state.room.update(state.camera, {}, 1.0f / 120);
                    if (state.room.corridorDoorFormation() < 1)
                        throw std::runtime_error("Capture could not form the sliding door");
                } else if (stage != "corridor-illusion-wrong" && stage != "corridor-illusion-partial" &&
                           stage != "corridor-illusion-aligned") {
                    settle(state);
                    settle(state);
                    if (!state.room.corridorDoorReady())
                        throw std::runtime_error("Capture could not wait for the sliding door to open");
                    if (stage == "corridor-illusion-through") {
                        state.camera.feet = {21.8f, 1.8f, -9.8f};
                        state.camera.face({18.8f, 3.1f, -10.4f});
                    } else if (stage.rfind("corridor-bridge-", 0) == 0 || stage == "corridor-stairwell") {
                        state.camera.feet = school::SchoolCorridor::bridgeEye - glm::vec3(0, 1.65f, 0);
                        state.camera.resetMotion();
                        if (stage == "corridor-bridge-wrong")
                            state.camera.feet.x -= 1.20f;
                        state.camera.face(school::SchoolCorridor::bridgeFocus);
                        if (stage == "corridor-bridge-forming") {
                            for (int frame = 0; frame < 104; ++frame)
                                state.room.update(state.camera, {}, 1.0f / 120);
                            state.camera.feet.x -= .5f;
                            state.camera.face(school::SchoolCorridor::bridgeFocus);
                        }
                        if (stage == "corridor-bridge-complete" || stage == "corridor-stairwell") {
                            settle(state);
                            if (!state.room.corridorBridgeReady())
                                throw std::runtime_error("Capture could not form the landing grating");
                        }
                        if (stage == "corridor-stairwell") {
                            state.camera.feet = {23.55f, 1.8f, -16.05f};
                            state.camera.face({23.55f, 1.65f, -20.1f});
                        }
                    } else if (stage == "corridor-illusion-complete") {
                        state.camera.feet = {17.2f, 1.8f, -9.9f};
                        state.camera.face(school::SchoolCorridor::illusionFocus);
                    }
                }
            }
        } else {
            state.camera.feet = {4.8f, 1.8f, -9.9f};
            state.camera.face({22.5f, 3.3f, -9.9f});
        }
    } else if (stage == "notebook-start") {
        state.camera.feet = {-4.83f, 0, 3.80f};
        state.camera.face(state.room.notebookCenter());
    } else if (stage == "notebook-on-desk") {
        state.camera.feet = {-4.83f, 0, 3.80f};
        state.camera.face(state.room.notebookCenter());
        if (!state.room.toggleNotebook(state.camera))
            throw std::runtime_error("Capture could not pick up the notebook for the desk");
        state.camera.feet = {3.10f, 0, 1.86f};
        state.camera.face({1.15f, .78f, .65f});
        state.room.toggleNotebook(state.camera);
        settle(state);
        state.camera.face(state.room.notebookCenter());
    } else if (stage.rfind("mirror-", 0) == 0 || stage == "notebook-at-stand") {
        stageMirrorCapture(state, stage);
        if (stage == "mirror-guide" && !state.room.inspectMirror(state.camera))
            throw std::runtime_error("Capture could not read the enlarged notebook in the mirror");
    } else if (stage == "boards-moving" || stage == "boards-aligned") {
        changeBoards(state, stage == "boards-aligned");
        state.camera.feet = {-.8f, 0, -.95f};
        state.camera.face({-.85f, 2.1f, -4.60f});
    } else if (stage == "projector-first-turn") {
        state.camera.face(school::SchoolRoom::projectorOrigin);
        if (!state.room.toggleProjector(state.camera))
            throw std::runtime_error("Capture could not operate the initial projector");
        turnProjectorTowardsPaper(state.room, .5f);
        state.camera.face({0.0f, 1.9f, -4.0f});
    } else if (stage == "projector") {
        changeBoards(state, true);
        state.camera.feet = {4.8f, 0, 6.4f};
        state.camera.face(school::SchoolRoom::projectorOrigin);
        state.room.toggleProjector(state.camera);
        state.camera.face({0.0f, 1.9f, -4.0f});
    } else if (stage == "turning" || stage == "positioned") {
        aimProjector(state, stage == "turning" ? 0.5f : 1.0f);
        state.camera.feet = school::Camera().feet;
        state.camera.face({-0.6f, 1.7f, -3.0f});
    } else if (stage == "aligned" || stage == "complete" || stage == "overview" || stage == "recognizing" ||
               stage.rfind("forming-", 0) == 0) {
        aimProjector(state);
        state.camera.feet = school::SchoolRoom::alignmentEye - glm::vec3(0, 1.65f, 0);
        state.camera.face(school::SchoolRoom::alignmentFocus);
        // Aligned captures the instant before an active update recognizes the complete drawing.
        if (stage == "recognizing" || stage.rfind("forming-", 0) == 0) {
            int frames = stage == "recognizing"
                             ? 24
                             : (stage == "forming-start" ? 56 : (stage == "forming-middle" ? 86 : 120));
            for (int i = 0; i < frames; ++i)
                state.room.update(state.camera, {}, 1.0f / 120);
        } else if (stage != "aligned")
            settle(state);
        if (stage == "overview") {
            state.camera.feet = {5.2f, 0, 7.5f};
            state.camera.face({-0.3f, 1.6f, -2.8f});
        }
    } else if (stage == "rear") {
        state.camera.feet = {-4.0f, 0, 3.4f};
        state.camera.face({1.0f, 1.8f, 8.4f});
    } else if (stage == "details") {
        state.camera.feet = {3.8f, 0, 4.5f};
        state.camera.face({6.0f, 1.7f, 4.7f});
    } else if (stage != "entrance") {
        state.camera.feet = school::Camera().feet;
        state.camera.face({-0.6f, 1.7f, -3.0f});
    }
}
} // namespace

int school::runPreview(GLFWwindow* window, PreviewState& preview, const std::string& captureFolder,
                       PreviewNavigation* navigation) {
    Application state{preview.room, preview.camera, navigation};
    school::Renderer renderer;
    if (!renderer.initialize())
        return 2;
    glfwSetWindowUserPointer(window, &state);
    glfwSetCursorPosCallback(window, [](GLFWwindow* w, double x, double y) {
        auto& a = app(w);
        if (a.paused || !glfwGetWindowAttrib(w, GLFW_FOCUSED)) {
            a.sample = false;
            return;
        }
        if (a.sample && glfwGetTime() >= a.mouseResumeAt) {
            if (a.room.adjustingProjector())
                a.room.rotateProjector(float(x - a.mouseX) * 0.0014f, -float(y - a.mouseY) * 0.0014f);
            else if (a.room.adjustingMirror())
                a.room.rotateMirror(float(x - a.mouseX) * 0.003f);
            else {
                a.camera.focusTime = 0;
                a.camera.yaw += float(x - a.mouseX) * 0.0025f;
                a.camera.pitch = std::clamp(a.camera.pitch - float(y - a.mouseY) * 0.0025f, -1.54f, 1.54f);
            }
        }
        a.mouseX = x;
        a.mouseY = y;
        a.sample = true;
    });
    glfwSetWindowFocusCallback(window, [](GLFWwindow* w, int focused) {
        if (!focused)
            pause(w, true);
        app(w).sample = false;
    });
    glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int, int action, int) {
        if (action != GLFW_PRESS || !glfwGetWindowAttrib(w, GLFW_FOCUSED))
            return;
        auto& a = app(w);
        if (a.navigation && key >= GLFW_KEY_F2 && key <= GLFW_KEY_F4) {
            a.navigation->requestedKey = key;
            return;
        }
        if (key == GLFW_KEY_ESCAPE) {
            pause(w, !a.paused);
            return;
        }
        if (a.paused) {
            if (key == GLFW_KEY_Q)
                glfwSetWindowShouldClose(w, true);
            if (key == GLFW_KEY_R) {
                a.room.reset();
                a.camera = school::Camera();
                a.jump = false;
                pause(w, false);
            }
            return;
        }
        if (key == GLFW_KEY_SPACE)
            a.jump = true;
        if (key != GLFW_KEY_E)
            return;
        if (a.room.toggleProjector(a.camera)) {
            if (a.room.adjustingProjector())
                a.camera.focusOn({0.0f, 1.9f, -4.0f});
        } else {
            if (a.room.toggleNotebook(a.camera) || a.room.inspectMirror(a.camera))
                return;
            if (!a.room.toggleMirror(a.camera)) {
                if (!a.room.moveBoard(a.camera))
                    a.room.interactCorridor(a.camera);
                return;
            }
            if (a.room.adjustingMirror())
                a.camera.focusOn(school::SchoolRoom::mirrorAim);
        }
        a.sample = false;
        a.mouseResumeAt = glfwGetTime();
    });
    if (captureFolder.empty())
        pause(window, false);
    if (!captureFolder.empty()) {
        std::filesystem::create_directories(captureFolder);
        for (const std::string stage : {"entrance",
                                        "focus-projector-start",
                                        "focus-projector-middle",
                                        "focus-projector-end",
                                        "focus-mirror-start",
                                        "focus-mirror-middle",
                                        "focus-mirror-end",
                                        "projector-first-turn",
                                        "notebook-start",
                                        "notebook-on-desk",
                                        "pause-controls",
                                        "notebook-at-stand",
                                        "mirror-unaligned",
                                        "mirror-rotating",
                                        "mirror-notebook",
                                        "mirror-guide",
                                        "boards-moving",
                                        "boards-aligned",
                                        "projector",
                                        "turning",
                                        "positioned",
                                        "aligned",
                                        "recognizing",
                                        "forming-start",
                                        "forming-middle",
                                        "forming-end",
                                        "complete",
                                        "overview",
                                        "rear",
                                        "details",
                                        "corridor-connection",
                                        "corridor-entrance",
                                        "corridor-class-sign",
                                        "corridor-false-close",
                                        "corridor-class-key",
                                        "corridor-class-exit",
                                        "corridor-class-unlocked",
                                        "corridor-window-a",
                                        "corridor-window-b",
                                        "corridor-door-open",
                                        "corridor-door-closed",
                                        "corridor-stairwell",
                                        "corridor-illusion-wrong",
                                        "corridor-illusion-partial",
                                        "corridor-illusion-aligned",
                                        "corridor-illusion-forming",
                                        "corridor-illusion-complete",
                                        "corridor-illusion-through",
                                        "corridor-illusion-formed",
                                        "corridor-bridge-wrong",
                                        "corridor-bridge-aligned",
                                        "corridor-bridge-forming",
                                        "corridor-bridge-complete"}) {
            state.room.reset();
            state.camera = school::Camera();
            if (stage != "entrance")
                for (int i = 0; i < 11; ++i)
                    settle(state);
            stageCapture(state, stage);
            state.room.draw(renderer, state.camera, 1280, 800);
            if (stage == "pause-controls") {
                drawPauseMenu(renderer, 1280, 800);
                drawSchoolControls(renderer, 1280, 800);
            }
            glFinish();
            if (!renderer.saveFrame(captureFolder + "/" + stage + ".ppm", 1280, 800))
                return 3;
            GLenum error = glGetError();
            if (error != GL_NO_ERROR) {
                std::cerr << "OpenGL error: " << error << '\n';
                return 4;
            }
            std::cout << stage << " projector=" << state.room.projectorAligned()
                      << " board-visible=" << state.room.boardVisibility()
                      << " stairs=" << state.room.stairsReady()
                      << " formation=" << state.room.stairFormation();
            if (stage.rfind("corridor-", 0) == 0)
                std::cout << " corridor-alignment=" << state.room.corridorAlignment(state.camera)
                          << " corridor-door=" << state.room.corridorDoorReady()
                          << " bridge-alignment=" << state.room.corridorBridgeAlignment(state.camera)
                          << " bridge=" << state.room.corridorBridgeReady();
            std::cout << '\n';
        }
        glfwSetWindowUserPointer(window, nullptr);
        return 0;
    }
    double previous = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (navigation && navigation->requestedKey)
            break;
        if (navigation && navigation->restoreFullscreen && navigation->restoreFullscreen(window))
            pause(window, state.paused);
        double now = glfwGetTime();
        float dt = float(std::min(now - previous, 0.05));
        previous = now;
        if (!state.paused && glfwGetWindowAttrib(window, GLFW_FOCUSED)) {
            school::Input input;
            input.forward = float(glfwGetKey(window, GLFW_KEY_W) - glfwGetKey(window, GLFW_KEY_S));
            input.right = float(glfwGetKey(window, GLFW_KEY_D) - glfwGetKey(window, GLFW_KEY_A));
            input.jump = state.jump;
            state.jump = false;
            state.room.update(state.camera, input, dt);
        }
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        if (width <= 0 || height <= 0) {
            glfwWaitEventsTimeout(0.05);
            continue;
        }
        state.room.draw(renderer, state.camera, width, height);
        if (state.paused)
            drawPauseMenu(renderer, width, height);
        if (state.paused)
            drawSchoolControls(renderer, width, height);
        glfwSwapBuffers(window);
    }
    state.room.stopAdjustingProjector();
    state.room.stopAdjustingMirror();
    state.camera.movement = {};
    state.camera.focusTime = 0;
    glEnable(GL_DEPTH_TEST);
    glfwSetCursorPosCallback(window, nullptr);
    glfwSetKeyCallback(window, nullptr);
    glfwSetWindowFocusCallback(window, nullptr);
    glfwSetWindowUserPointer(window, nullptr);
    if (navigation)
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    return 0;
}
