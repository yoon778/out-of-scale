#include "src/core/Player.h"
#include "../../rooms/military/MilitaryRoom.h"
#include "../../rooms/test/TestRoom.h"

#include <GL/glew.h>
#include <GL/glfw3.h>
#include <GL/glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <iostream>
#include <string_view>
#include <type_traits>

namespace {
constexpr char lockedTitle[] = "Out of Scale | Return the cube | E Grab/Place | WASD Move | Esc Exit";

const char* windowTitle(TestRoom::ExitState state) {
    switch (state) {
    case TestRoom::ExitState::Open:
        return "Out of Scale | Exit open | Walk through the door | E Grab/Place | Esc Exit";
    case TestRoom::ExitState::Escaped:
        return "Out of Scale | Escaped | Esc Exit";
    case TestRoom::ExitState::Locked:
        return lockedTitle;
    }
    return lockedTitle;
}

const char* windowTitle(MilitaryRoom::ExitState state) {
    switch (state) {
    case MilitaryRoom::ExitState::Open:
        return "Out of Scale | Barricade returned | Follow arrows to the night outpost";
    case MilitaryRoom::ExitState::OutpostReached:
        return "Out of Scale | E Grab moon | Scroll down to grow | E Place | Esc Exit";
    case MilitaryRoom::ExitState::BridgeReady:
        return "Out of Scale | Bridge ready | Cross to the exit | Esc Exit";
    case MilitaryRoom::ExitState::Escaped:
        return "Out of Scale | Military escaped | Esc Exit";
    case MilitaryRoom::ExitState::Locked:
        return "Out of Scale | Military supply | Return the barricade | E Grab/Place";
    }
    return "Out of Scale | Military supply";
}

template<class Room>
struct Scene {
    Player player;
    Room room;
};

template<class Room>
Scene<Room>& sceneFor(GLFWwindow* window) {
    return *static_cast<Scene<Room>*>(glfwGetWindowUserPointer(window));
}

void drawAimDot(int width, int height) {
    glEnable(GL_SCISSOR_TEST);
    glScissor(width / 2 - 3, height / 2 - 3, 7, 7);
    glClearColor(0.06f, 0.08f, 0.11f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glScissor(width / 2 - 2, height / 2 - 2, 5, 5);
    glClearColor(0.96f, 0.97f, 0.98f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
}

template<class Room>
void drawFrame(const Player& player, const Room& room, int width, int height) {
    glViewport(0, 0, width, height);
    glClearColor(0.12f, 0.18f, 0.25f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    const glm::mat4 projection = glm::perspective(
        glm::radians(68.0f), static_cast<float>(width) / static_cast<float>(height), 0.05f, 600.0f);
    room.draw(projection * player.view(), player.eyePosition(), player.viewDirection());
    drawAimDot(width, height);
}

template<class Room>
int run(GLFWwindow* window, const Player& player) {
    Scene<Room> scene{player, {}};
    if (!scene.room.initialize()) {
        return 1;
    }
    glEnable(GL_DEPTH_TEST);

    glfwSetWindowUserPointer(window, &scene);
    glfwSetCursorPosCallback(window, [](GLFWwindow* target, double x, double y) {
        sceneFor<Room>(target).player.look(x, y);
    });
    glfwSetWindowFocusCallback(window, [](GLFWwindow* target, int) {
        sceneFor<Room>(target).player.resetMouseSample();
    });
    glfwSetKeyCallback(window, [](GLFWwindow* target, int key, int, int action, int) {
        if (action != GLFW_PRESS || glfwGetWindowAttrib(target, GLFW_FOCUSED) != GLFW_TRUE) {
            return;
        }
        auto& scene = sceneFor<Room>(target);
        if (key == GLFW_KEY_SPACE) {
            scene.player.jump();
        } else if (key == GLFW_KEY_E) {
            scene.room.interact(scene.player.eyePosition(), scene.player.viewDirection());
        }
    });
    if constexpr (std::is_same_v<Room, MilitaryRoom>) {
        glfwSetScrollCallback(window, [](GLFWwindow* target, double, double yOffset) {
            if (glfwGetWindowAttrib(target, GLFW_FOCUSED) == GLFW_TRUE) {
                sceneFor<Room>(target).room.scroll(yOffset);
            }
        });
    }
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    auto exitState = scene.room.exitState();
    double previous = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            break;
        }

        const double now = glfwGetTime();
        const float elapsed = static_cast<float>(std::min(now - previous, 0.05));
        previous = now;
        const glm::vec3 previousEye = scene.player.eyePosition();
        scene.player.update(window, elapsed, Room::halfSize);
        const glm::vec3 desiredEye = scene.player.eyePosition();
        scene.player.setHorizontalPosition(scene.room.resolveMovement(previousEye, desiredEye));

        const auto currentExitState = scene.room.exitState();
        if (currentExitState != exitState) {
            exitState = currentExitState;
            glfwSetWindowTitle(window, windowTitle(exitState));
        }

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        if (width > 0 && height > 0) {
            drawFrame(scene.player, scene.room, width, height);
            glfwSwapBuffers(window);
        } else {
            glfwWaitEventsTimeout(0.05);
        }
    }

    glfwSetCursorPosCallback(window, nullptr);
    glfwSetWindowFocusCallback(window, nullptr);
    glfwSetKeyCallback(window, nullptr);
    if constexpr (std::is_same_v<Room, MilitaryRoom>) {
        glfwSetScrollCallback(window, nullptr);
    }
    glfwSetWindowUserPointer(window, nullptr);
    return 0;
}
}

int main(int argc, char* argv[]) {
    const bool useTestRoom = argc > 1 && std::string_view{argv[1]} == "--test-room";
    glfwSetErrorCallback([](int, const char* message) { std::cerr << message << '\n'; });
    if (!glfwInit()) {
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    const char* initialTitle = useTestRoom ? lockedTitle : windowTitle(MilitaryRoom::ExitState::Locked);
    GLFWwindow* window = glfwCreateWindow(1024, 640, initialTitle, nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glewExperimental = GL_TRUE;
    const GLenum status = glewInit();
    int result = 1;
    if (status == GLEW_OK) {
        result = useTestRoom
            ? run<TestRoom>(window, Player{})
            : run<MilitaryRoom>(window, Player{MilitaryRoom::spawnEye, MilitaryRoom::spawnYaw});
    } else {
        std::cerr << glewGetErrorString(status) << '\n';
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}
