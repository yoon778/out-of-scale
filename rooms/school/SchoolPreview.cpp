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
        state.camera.movement = {};
    }
    state.sample = false;
    state.jump = false;
    state.mouseResumeAt = glfwGetTime() + 0.12;
    glfwSetInputMode(window, GLFW_CURSOR, paused ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    glfwGetCursorPos(window, &state.mouseX, &state.mouseY);
}
void settle(Application& state) {
    for (int i = 0; i < 180; ++i)
        state.room.update(state.camera, {}, 1.0f / 120);
}
void aimProjector(Application& state, float fraction = 1) {
    state.camera.feet = {4.8f, 0, 6.4f};
    state.camera.face(school::SchoolRoom::projectorOrigin);
    if (!state.room.toggleProjector(state.camera))
        throw std::runtime_error("Capture could not operate the projector");
    auto start = glm::normalize(school::SchoolRoom::projectorStartAim - school::SchoolRoom::projectorOrigin);
    auto target = glm::normalize(school::SchoolRoom::projectorPaper - school::SchoolRoom::projectorOrigin);
    state.room.rotateProjector((std::atan2(target.x, -target.z) - std::atan2(start.x, -start.z)) * fraction,
                               (std::asin(target.y) - std::asin(start.y)) * fraction);
    state.room.toggleProjector(state.camera);
    state.camera.face({0.0f, 1.9f, -4.0f});
    state.room.update(state.camera, {}, 1.0f / 120);
}
void stageCapture(Application& state, const std::string& stage) {
    if (stage.rfind("corridor-", 0) == 0) {
        aimProjector(state);
        state.camera.feet = school::SchoolRoom::alignmentEye - glm::vec3(0, 1.65f, 0);
        state.camera.face(school::SchoolRoom::alignmentFocus);
        settle(state);
        if (stage == "corridor-connection") {
            state.camera.feet = {2.9f, 1.8f, -6.1f};
            state.camera.face({3.1f, 3.4f, -9.8f});
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
            state.camera.feet = {7.45f, 1.8f, -10.65f};
            state.camera.face({8.0f, 3.25f, -9.9f});
        } else if (stage == "corridor-class-exit") {
            state.camera.feet = {10.13f, 1.8f, -14.2f};
            state.camera.face({11.67f, 3.15f, -11.55f});
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
                state.camera.face(school::SchoolCorridor::illusionFocus);
                if (stage == "corridor-illusion-forming") {
                    for (int frame = 0; frame < 105; ++frame)
                        state.room.update(state.camera, {}, 1.0f / 120);
                } else if (stage != "corridor-illusion-wrong" && stage != "corridor-illusion-aligned") {
                    settle(state);
                    if (stage != "corridor-illusion-formed") {
                        state.camera.feet = {19.6f, 1.8f, -9.9f};
                        state.camera.face(school::SchoolCorridor::illusionFocus);
                        if (!state.room.openCorridorDoor(state.camera))
                            throw std::runtime_error("Capture could not open the formed door");
                        settle(state);
                    }
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
    } else if (stage == "projector") {
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
    } else {
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
            else {
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
        if (key == GLFW_KEY_E && a.room.toggleProjector(a.camera)) {
            if (a.room.adjustingProjector())
                a.camera.face({0.0f, 1.9f, -4.0f});
            a.sample = false;
            a.mouseResumeAt = glfwGetTime() + 0.12;
        } else if (key == GLFW_KEY_E && !a.room.openCorridorDoor(a.camera))
            a.room.toggleCupboard(a.camera);
    });
    if (captureFolder.empty())
        pause(window, false);
    if (!captureFolder.empty()) {
        std::filesystem::create_directories(captureFolder);
        for (const std::string stage : {"entrance",
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
                                        "corridor-false-close",
                                        "corridor-class-exit",
                                        "corridor-window-a",
                                        "corridor-window-b",
                                        "corridor-door-open",
                                        "corridor-door-closed",
                                        "corridor-stairwell",
                                        "corridor-illusion-wrong",
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
            for (int i = 0; i < 11; ++i)
                settle(state);
            stageCapture(state, stage);
            state.room.draw(renderer, state.camera, 1280, 800);
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
        if (state.paused) {
            float s = std::min(width / 1280.0f, height / 800.0f);
            renderer.overlay(width, height);
            renderer.panel(width / 2.0f - 230 * s, height / 2.0f - 108 * s, 460 * s, 216 * s,
                           {0.10f, 0.19f, 0.15f});
            renderer.text(L"잠시 쉬는 시간", width / 2.0f - 192 * s, height / 2.0f - 88 * s, 44 * s);
            renderer.text(L"Esc   계속하기", width / 2.0f - 192 * s, height / 2.0f - 26 * s, 31 * s);
            renderer.text(L"R      학교 처음부터", width / 2.0f - 192 * s, height / 2.0f + 17 * s, 31 * s);
            renderer.text(L"Q      종료", width / 2.0f - 192 * s, height / 2.0f + 60 * s, 31 * s);
        }
        glfwSwapBuffers(window);
    }
    state.room.stopAdjustingProjector();
    state.camera.movement = {};
    glEnable(GL_DEPTH_TEST);
    glfwSetCursorPosCallback(window, nullptr);
    glfwSetKeyCallback(window, nullptr);
    glfwSetWindowFocusCallback(window, nullptr);
    glfwSetWindowUserPointer(window, nullptr);
    if (navigation)
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    return 0;
}
