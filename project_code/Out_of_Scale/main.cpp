#include "src/core/Player.h"
#include "../../rooms/military/MilitaryRoom.h"
#include "../../rooms/shared/MapPreview.h"
#include "../../rooms/test/TestRoom.h"

#include <GL/glew.h>
#include <GL/glfw3.h>
#include <GL/glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <iostream>
#include <optional>
#include <string_view>
#include <type_traits>

namespace {
constexpr char lockedTitle[] = "Out of Scale | Return the cube | E Grab/Place | WASD Move | Esc Exit";

enum class Map { School, Military, Office, Home, Test };

std::optional<Map> mapForKey(int key) {
    switch (key) {
    case GLFW_KEY_F1: return Map::School;
    case GLFW_KEY_F2: return Map::Military;
    case GLFW_KEY_F3: return Map::Office;
    case GLFW_KEY_F4: return Map::Home;
    default: return std::nullopt;
    }
}

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
        return "Out of Scale | TV on | E Grab/Place | Enlarge and walk into the screen";
    case MilitaryRoom::ExitState::OutpostReached:
        return "Out of Scale | E Grab moon | Scroll down to grow | E Place | Esc Exit";
    case MilitaryRoom::ExitState::BridgeReady:
        return "Out of Scale | Bridge ready | Cross to the exit | Esc Exit";
    case MilitaryRoom::ExitState::Escaped:
        return "Out of Scale | Military escaped | Esc Exit";
    case MilitaryRoom::ExitState::Locked:
        return "Out of Scale | Barracks | Return the three belongings | E Grab/Place";
    }
    return "Out of Scale | Barracks";
}

template<class Room>
struct Scene {
    Player player;
    Room room;
    bool initialized = false;
    std::optional<Map> destination;
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
int run(GLFWwindow* window, Scene<Room>& scene, Map& activeMap) {
    if (!scene.initialized) {
        if (!scene.room.initialize()) {
            return 1;
        }
        scene.initialized = true;
    }
    scene.destination.reset();
    scene.player.resetMouseSample();
    const auto titleForRoom = [&scene] {
        if constexpr (std::is_same_v<Room, MapPreview>) {
            return scene.room.title();
        } else {
            return windowTitle(scene.room.exitState());
        }
    };
    const char* title = titleForRoom();
    glfwSetWindowTitle(window, title);

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
        if (const auto map = mapForKey(key)) {
            scene.destination = map;
        } else if (key == GLFW_KEY_SPACE) {
            scene.player.jump();
        } else if constexpr (!std::is_same_v<Room, MapPreview>) {
            if (key == GLFW_KEY_E) {
                scene.room.interact(scene.player.eyePosition(), scene.player.viewDirection());
            }
        }
    });
    if constexpr (std::is_same_v<Room, MilitaryRoom>) {
        glfwSetScrollCallback(window, [](GLFWwindow* target, double, double yOffset) {
            if (glfwGetWindowAttrib(target, GLFW_FOCUSED) == GLFW_TRUE) {
                sceneFor<Room>(target).room.scroll(yOffset);
            }
        });
    }
    double previous = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
        }
        if (scene.destination && *scene.destination != activeMap) {
            activeMap = *scene.destination;
            break;
        }
        scene.destination.reset();

        const double now = glfwGetTime();
        const float elapsed = static_cast<float>(std::min(now - previous, 0.05));
        previous = now;
        const glm::vec3 previousEye = scene.player.eyePosition();
        scene.player.update(window, elapsed, Room::halfSize, std::is_same_v<Room, MilitaryRoom>);
        const glm::vec3 desiredEye = scene.player.eyePosition();
        if constexpr (std::is_same_v<Room, MilitaryRoom>) {
            bool grounded = false;
            const glm::vec3 resolved = scene.room.resolveMovement(previousEye, desiredEye,
                                                                 scene.player.viewDirection(), &grounded);
            scene.player.setRoomPosition(resolved, grounded);
        } else if constexpr (std::is_same_v<Room, TestRoom>) {
            scene.player.setHorizontalPosition(scene.room.resolveMovement(previousEye, desiredEye));
        }

        if (const char* currentTitle = titleForRoom(); currentTitle != title) {
            title = currentTitle;
            glfwSetWindowTitle(window, title);
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

int runGame(GLFWwindow* window, Map activeMap) {
    using Theme = MapPreview::Theme;
    Scene<MapPreview> school{Player{}, MapPreview{Theme::School}};
    Scene<MilitaryRoom> military{Player{MilitaryRoom::spawnEye, MilitaryRoom::spawnYaw}, {}};
    Scene<MapPreview> office{Player{}, MapPreview{Theme::Office}};
    Scene<MapPreview> home{Player{}, MapPreview{Theme::Home}};
    Scene<TestRoom> test{Player{}, {}};

    glEnable(GL_DEPTH_TEST);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    while (!glfwWindowShouldClose(window)) {
        int result = 0;
        switch (activeMap) {
        case Map::School: result = run(window, school, activeMap); break;
        case Map::Military: result = run(window, military, activeMap); break;
        case Map::Office: result = run(window, office, activeMap); break;
        case Map::Home: result = run(window, home, activeMap); break;
        case Map::Test: result = run(window, test, activeMap); break;
        }
        if (result != 0) {
            return result;
        }
    }
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
        result = runGame(window, useTestRoom ? Map::Test : Map::Military);
    } else {
        std::cerr << glewGetErrorString(status) << '\n';
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}
