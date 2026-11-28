#include "src/core/Audio.h"
#include "src/core/MovementAudio.h"
#include "src/core/Player.h"
#include "src/core/PauseMenu.h"
#include "../../rooms/home/HomeRoom.h"
#include "../../rooms/military/MilitaryRoom.h"
#include "../../rooms/shared/MapPreview.h"
#include "../../rooms/school/SchoolPreview.h"
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
constexpr char lockedTitle[] = "Out of Scale | Return the cube | E Grab/Place | WASD Move | Esc Pause";

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

RoomMusic musicForMap(Map map) {
    switch (map) {
    case Map::School: return RoomMusic::School;
    case Map::Military: return RoomMusic::Military;
    case Map::Office: return RoomMusic::Office;
    case Map::Home: return RoomMusic::Home;
    case Map::Test: return RoomMusic::None;
    }
    return RoomMusic::None;
}

bool restoreFullscreen(GLFWwindow* window) {
    // GLFW makes a fullscreen window windowed when its monitor disconnects.
    if (glfwGetWindowMonitor(window)) {
        return false;
    }
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
    if (!mode) {
        return false;
    }
    glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    return glfwGetWindowMonitor(window) == monitor;
}

bool updateAudioAndRestoreFullscreen(GLFWwindow* window) {
    const double now = glfwGetTime();
    static double previous = now;
    const auto elapsed = static_cast<float>(std::clamp(now - previous, 0.0, 0.10));
    previous = now;
    Audio::instance().update(elapsed);
    return restoreFullscreen(window);
}

void playMovement(MovementAudio::Cue cue) {
    if (cue == MovementAudio::Cue::Step) {
        Audio::instance().playEffect(SoundEffect::Footstep, 0.7f);
    } else if (cue == MovementAudio::Cue::Land) {
        Audio::instance().playEffect(SoundEffect::Land, 0.9f);
    }
}

void playCarryChange(bool before, bool after) {
    if (before != after) {
        Audio::instance().playEffect(after ? SoundEffect::Grab : SoundEffect::Place);
    }
}

struct SchoolAudio {
    PauseMenu& pause;
    const school::PreviewState& scene;
    MovementAudio movement;
    std::optional<bool> carryingBeforeE;

    void flushCarry() {
        if (carryingBeforeE.has_value()) {
            playCarryChange(*carryingBeforeE, scene.room.isCarrying());
            carryingBeforeE.reset();
        }
    }

    bool keyPressed(GLFWwindow* window, int key) {
        flushCarry();
        const bool consumed = pause.keyPressed(window, key);
        if (!consumed && key == GLFW_KEY_E && !pause.isPaused()) {
            carryingBeforeE = scene.room.isCarrying();
        }
        return consumed;
    }

    void reset() {
        carryingBeforeE.reset();
        movement.reset(scene.camera.feet, scene.camera.grounded);
    }

    void frame(GLFWwindow* window) {
        if (pause.isPaused() || !glfwGetWindowAttrib(window, GLFW_FOCUSED)) {
            reset();
        } else {
            flushCarry();
            playMovement(movement.sample(scene.camera.feet, scene.camera.grounded));
        }
    }
};

const char* windowTitle(TestRoom::ExitState state) {
    switch (state) {
    case TestRoom::ExitState::Open:
        return "Out of Scale | Exit open | Walk through the door | E Grab/Place | Esc Pause";
    case TestRoom::ExitState::Escaped:
        return "Out of Scale | Escaped | Esc Pause";
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
        return "Out of Scale | E Grab moon | Scroll down to grow | E Place | Esc Pause";
    case MilitaryRoom::ExitState::BridgeReady:
        return "Out of Scale | Follow the shadow | E Move moon | Esc Pause";
    case MilitaryRoom::ExitState::Escaped:
        return "Out of Scale | Military escaped | Esc Pause";
    case MilitaryRoom::ExitState::Locked:
        return "Out of Scale | Barracks | E Grab/Place";
    }
    return "Out of Scale | Barracks";
}

const char* windowTitle(HomeRoom::ExitState state) {
    return state == HomeRoom::ExitState::Escaped
        ? "Out of Scale | Home escaped | Esc Pause"
        : "Out of Scale | Home | E Grab/Place | Wheel Scale | R Rotate | Esc Pause";
}

template<class Room>
struct Scene {
    Player player;
    Room room;
    bool initialized = false;
    std::optional<Map> destination;
    PauseMenu* pause = nullptr;
};

template<class Room>
Scene<Room>& sceneFor(GLFWwindow* window) {
    return *static_cast<Scene<Room>*>(glfwGetWindowUserPointer(window));
}

template<class Room>
void updatePauseCursor(GLFWwindow* window, Scene<Room>& scene, bool wasPaused) {
    const bool pointer = scene.pause->isPaused();
    const int cursor = pointer ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED;
    if (scene.pause->isPaused() != wasPaused || glfwGetInputMode(window,GLFW_CURSOR) != cursor) {
        scene.player.resetMouseSample();
        if constexpr (std::is_same_v<Room, HomeRoom>) scene.room.computerClearInput();
        glfwSetInputMode(window, GLFW_CURSOR, cursor);
    }
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
    if constexpr (std::is_same_v<Room, HomeRoom>) {
        room.drawEndingOverlay();
    }
}

template<class Room>
int run(GLFWwindow* window, Scene<Room>& scene, Map& activeMap, PauseMenu& pause) {
    if (!scene.initialized) {
        if (!scene.room.initialize()) {
            return 1;
        }
        scene.initialized = true;
    }
    scene.destination.reset();
    scene.pause = &pause;
    scene.player.resetMouseSample();
    MovementAudio movement;
    movement.reset(scene.player.eyePosition(), scene.player.isGrounded());
    updatePauseCursor(window,scene,pause.isPaused());
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
        auto& scene = sceneFor<Room>(target);
        if (!scene.pause->isPaused()) {
            scene.player.look(x, y);
        }
    });
    glfwSetWindowFocusCallback(window, [](GLFWwindow* target, int) {
        sceneFor<Room>(target).player.resetMouseSample();
        if constexpr (std::is_same_v<Room, HomeRoom>) sceneFor<Room>(target).room.computerClearInput();
    });
    glfwSetKeyCallback(window, [](GLFWwindow* target, int key, int, int action, int mods) {
        if (glfwGetWindowAttrib(target, GLFW_FOCUSED) != GLFW_TRUE) {
            return;
        }
        auto& scene = sceneFor<Room>(target);
        if (action != GLFW_PRESS) {
            if constexpr (std::is_same_v<Room, HomeRoom>) {
                if (!scene.pause->isPaused()) scene.room.computerKey(key,action,mods);
            }
            return;
        }
        const bool wasPaused = scene.pause->isPaused();
        if (scene.pause->keyPressed(target, key)) {
            updatePauseCursor(target, scene, wasPaused);
            return;
        }
        if (const auto map = mapForKey(key)) {
            scene.destination = map;
            return;
        }
        if (scene.pause->isPaused()) {
            return;
        }
        if constexpr (std::is_same_v<Room, HomeRoom>) {
            scene.room.computerKey(key,action,mods);
        }
        if (key == GLFW_KEY_SPACE) {
            scene.player.jump();
        } else if constexpr (!std::is_same_v<Room, MapPreview>) {
            if (key == GLFW_KEY_E) {
                const bool wasCarrying = scene.room.isCarrying();
                scene.room.interact(scene.player.eyePosition(), scene.player.viewDirection());
                playCarryChange(wasCarrying, scene.room.isCarrying());
            }
        }
    });
    glfwSetMouseButtonCallback(window, [](GLFWwindow* target, int button, int action, int mods) {
        if (glfwGetWindowAttrib(target, GLFW_FOCUSED) != GLFW_TRUE) {
            return;
        }
        auto& scene = sceneFor<Room>(target);
        const bool wasPaused = scene.pause->isPaused();
        if (wasPaused) {
            if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) scene.pause->click(target);
            updatePauseCursor(target, scene, wasPaused);
        }
    });
    if constexpr (std::is_same_v<Room, MilitaryRoom> || std::is_same_v<Room, HomeRoom>) {
        glfwSetScrollCallback(window, [](GLFWwindow* target, double, double yOffset) {
            auto& scene = sceneFor<Room>(target);
            if (!scene.pause->isPaused() && glfwGetWindowAttrib(target, GLFW_FOCUSED) == GLFW_TRUE) {
                scene.room.scroll(yOffset);
            }
        });
    }
    double previous = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        const bool wasPaused = pause.isPaused();
        glfwPollEvents();
        if (glfwWindowShouldClose(window)) {
            break;
        }
        if (updateAudioAndRestoreFullscreen(window)) {
            scene.player.resetMouseSample();
        }
        // F2 can return from the training ground within the military map.
        if (scene.destination && (*scene.destination != activeMap || *scene.destination == Map::Military)) {
            if constexpr (std::is_same_v<Room, HomeRoom>) {
                scene.room.closeComputer();
            }
            scene.player.resetMouseSample();
            glfwSetInputMode(window,GLFW_CURSOR,GLFW_CURSOR_DISABLED);
            activeMap = *scene.destination;
            break;
        }
        scene.destination.reset();

        const double now = glfwGetTime();
        const float elapsed = static_cast<float>(std::min(now - previous, 0.05));
        previous = now;
        const bool updating = !wasPaused && !pause.isPaused();
        if (updating) {
            const glm::vec3 previousEye = scene.player.eyePosition();
            scene.player.update(window, elapsed, Room::halfSize,
                                std::is_same_v<Room, MilitaryRoom> || std::is_same_v<Room, HomeRoom>);
            const glm::vec3 desiredEye = scene.player.eyePosition();
            if constexpr (std::is_same_v<Room, MilitaryRoom>) {
                scene.room.update(elapsed, previousEye);
                bool grounded = false;
                const glm::vec3 resolved = scene.room.resolveMovement(previousEye, desiredEye,
                                                                     scene.player.viewDirection(), &grounded);
                scene.player.setRoomPosition(resolved, grounded);
            } else if constexpr (std::is_same_v<Room, HomeRoom>) {
                scene.room.update(elapsed, previousEye, scene.player.viewDirection());
                bool grounded = false;
                const glm::vec3 resolved = scene.room.resolveMovement(previousEye, desiredEye, &grounded);
                scene.player.setRoomPosition(resolved, grounded);
                auto& audio = Audio::instance();
                if (scene.room.repriseActive()) audio.selectMusic(RoomMusic::Finale);
                audio.setSceneMusicGain(1.0f - scene.room.endingFade());
            } else if constexpr (std::is_same_v<Room, TestRoom>) {
                scene.player.setHorizontalPosition(scene.room.resolveMovement(previousEye, desiredEye));
            }
        }
        const bool playRoomAudio = updating && glfwGetWindowAttrib(window, GLFW_FOCUSED);
        if (playRoomAudio) {
            playMovement(movement.sample(scene.player.eyePosition(), scene.player.isGrounded()));
        } else {
            movement.reset(scene.player.eyePosition(), scene.player.isGrounded());
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
            if (pause.isPaused()) {
                pause.draw(window, width, height);
            }
            glfwSwapBuffers(window);
        } else {
            glfwWaitEventsTimeout(0.05);
        }
    }

    glfwSetCursorPosCallback(window, nullptr);
    glfwSetWindowFocusCallback(window, nullptr);
    glfwSetKeyCallback(window, nullptr);
    glfwSetMouseButtonCallback(window, nullptr);
    glfwSetScrollCallback(window, nullptr);
    glfwSetWindowUserPointer(window, nullptr);
    scene.pause = nullptr;
    return 0;
}

int runGame(GLFWwindow* window, Map activeMap) {
    using Theme = MapPreview::Theme;
    PauseMenu pause;
    if (!pause.initialize()) {
        return 1;
    }
    school::PreviewState schoolState;
    SchoolAudio schoolAudio{pause, schoolState};
    const school::PreviewPause schoolPause{
        &schoolAudio,
        [](const void* context) { return static_cast<const SchoolAudio*>(context)->pause.isPaused(); },
        [](void* context, bool paused) {
            auto& adapter = *static_cast<SchoolAudio*>(context);
            adapter.pause.setPaused(paused);
            adapter.reset();
        },
        [](void* context, GLFWwindow* target, int key) { return static_cast<SchoolAudio*>(context)->keyPressed(target, key); },
        [](void* context, GLFWwindow* target) { static_cast<SchoolAudio*>(context)->pause.click(target); },
        [](void* context, GLFWwindow* target, int width, int height) {
            static_cast<SchoolAudio*>(context)->pause.draw(target, width, height);
        },
    };
    Scene<MilitaryRoom> military{Player{MilitaryRoom::spawnEye, MilitaryRoom::spawnYaw}, {}};
    Scene<MapPreview> office{Player{}, MapPreview{Theme::Office}};
    Scene<HomeRoom> home{Player{HomeRoom::spawnEye, HomeRoom::spawnYaw}, {}};
    Scene<TestRoom> test{Player{}, {}};

    glEnable(GL_DEPTH_TEST);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    auto& audio = Audio::instance();
    while (!glfwWindowShouldClose(window)) {
        audio.selectMusic(activeMap == Map::Home && home.room.repriseActive()
            ? RoomMusic::Finale : musicForMap(activeMap));
        audio.setSceneMusicGain(activeMap == Map::Home ? 1.0f - home.room.endingFade() : 1.0f);
        int result = 0;
        switch (activeMap) {
        case Map::School: {
            glfwSetWindowTitle(window, "Out of Scale | School");
            schoolAudio.reset();
            school::PreviewNavigation navigation{
                0, updateAudioAndRestoreFullscreen, &schoolPause, &schoolAudio,
                [](void* context, GLFWwindow* target) { static_cast<SchoolAudio*>(context)->frame(target); }
            };
            result = school::runPreview(window, schoolState, {}, &navigation);
            if (const auto map = mapForKey(navigation.requestedKey)) {
                activeMap = *map;
            } else if (result == 0 && schoolState.room.escaped() && !glfwWindowShouldClose(window)) {
                // Walking out of school enters the existing barracks flow without a hotkey.
                // Reset only the arrival pose; keep the military puzzle state on revisits.
                military.player = Player{MilitaryRoom::spawnEye, MilitaryRoom::spawnYaw};
                activeMap = Map::Military;
            }
            break;
        }
        case Map::Military:
            if (military.room.returnToBarracks()) {
                military.player = Player{MilitaryRoom::spawnEye, MilitaryRoom::spawnYaw};
            }
            result = run(window, military, activeMap, pause);
            break;
        case Map::Office: result = run(window, office, activeMap, pause); break;
        case Map::Home: result = run(window, home, activeMap, pause); break;
        case Map::Test: result = run(window, test, activeMap, pause); break;
        }
        if (result != 0) {
            return result;
        }
        if (pause.isPaused() && !glfwWindowShouldClose(window)) {
            pause.setPaused(false);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
    return 0;
}
}

int main(int argc, char* argv[]) {
    const std::string_view modeArgument = argc > 1 ? argv[1] : "";
    const Map startMap = modeArgument == "--test-room" ? Map::Test : Map::School;
    glfwSetErrorCallback([](int, const char* message) { std::cerr << message << '\n'; });
    if (!glfwInit()) {
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    const char* initialTitle = startMap == Map::Test ? lockedTitle : "Out of Scale | School";
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
    if (mode) {
        glfwWindowHint(GLFW_RED_BITS, mode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
    }
    GLFWwindow* window = glfwCreateWindow(mode ? mode->width : 1024, mode ? mode->height : 640,
                                         initialTitle, mode ? monitor : nullptr, nullptr);
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
        auto& audio = Audio::instance();
        if (!audio.initialize()) {
            std::cerr << "Audio initialization failed; continuing without audio\n";
        }
        result = runGame(window, startMap);
        audio.shutdown();
    } else {
        std::cerr << glewGetErrorString(status) << '\n';
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}
