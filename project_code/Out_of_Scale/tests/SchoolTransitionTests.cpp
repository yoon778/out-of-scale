// Exercise the real team and school loops with a hidden window and fixed input.
// Only the final school doorway is used as a fixture; the 474 school checks cover
// the preceding puzzle route. No puzzle completion flag is written by this test.
#include <GL/glew.h>
#include <GL/glfw3.h>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

void testPoll();
double testTime();
int testKey(GLFWwindow*, int);
int testAttrib(GLFWwindow*, int);
GLFWmonitor* testPrimaryMonitor();
void testTitle(GLFWwindow*, const char*);

#define glfwPollEvents testPoll
#define glfwGetTime testTime
#define glfwGetKey testKey
#define glfwGetWindowAttrib testAttrib
#define glfwGetPrimaryMonitor testPrimaryMonitor
#define glfwSetWindowTitle testTitle
#define main teamMain
#include "../main.cpp"
#undef main
#include "../../../rooms/school/SchoolPreview.cpp"
#include "../src/core/Player.cpp"
#undef glfwPollEvents
#undef glfwGetTime
#undef glfwGetKey
#undef glfwGetWindowAttrib
#undef glfwGetPrimaryMonitor
#undef glfwSetWindowTitle

namespace {
GLFWwindow* testWindow = nullptr;
int frames = 0, phaseFrames = 0, schoolVisits = 0, militaryVisits = 0;
int checks = 0, failures = 0;
bool walking = false, standalone = false;
enum class Phase { none, school, military } phase = Phase::none;
glm::vec3 stoppedSchool{}, stoppedMilitary{}, lastMilitary{};
std::filesystem::path output;

void check(bool condition, const char* message) {
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}
void press(int key) {
    auto callback = glfwSetKeyCallback(testWindow, nullptr);
    glfwSetKeyCallback(testWindow, callback);
    if (callback)
        callback(testWindow, key, 0, GLFW_PRESS, 0);
}
void approachExit(school::Camera& camera) {
    camera = school::Camera();
    camera.feet = {23.55f, .6f, -21.05f};
    camera.face({23.55f, 2.25f, -22.4f});
    camera.resetMotion();
}
}

double testTime() { return frames / 60.0; }
int testKey(GLFWwindow*, int key) { return walking && key == GLFW_KEY_W ? GLFW_PRESS : GLFW_RELEASE; }
int testAttrib(GLFWwindow* window, int attribute) {
    return attribute == GLFW_FOCUSED ? GLFW_TRUE : glfwGetWindowAttrib(window, attribute);
}
GLFWmonitor* testPrimaryMonitor() { return nullptr; } // Never move a hidden test onto a monitor.
void testTitle(GLFWwindow*, const char* title) {
    const std::string value(title);
    if (value == "Out of Scale | School") {
        phase = Phase::school;
        ++schoolVisits;
        phaseFrames = 0;
    } else if (value.find("Barracks") != std::string::npos) {
        phase = Phase::military;
        ++militaryVisits;
        phaseFrames = 0;
    }
}
void testPoll() {
    ++frames;
    ++phaseFrames;
    if (frames > 220) {
        check(false, "game loop reaches the expected room without timing out");
        glfwSetWindowShouldClose(testWindow, GLFW_TRUE);
        return;
    }
    if (standalone) {
        auto& state = app(testWindow);
        if (phaseFrames == 1) {
            approachExit(state.camera);
            walking = true;
        }
        if (phaseFrames == 35) {
            check(state.room.escaped(), "standalone can still complete school");
            check(!glfwWindowShouldClose(testWindow), "standalone completion keeps its window open");
            glfwSetWindowShouldClose(testWindow, GLFW_TRUE);
        }
        return;
    }
    if (phase == Phase::school) {
        auto& state = app(testWindow);
        if (schoolVisits == 1) {
            if (phaseFrames == 1) {
                approachExit(state.camera);
                check(!state.room.escaped(), "arrival fixture does not set the completion flag");
                walking = true;
                press(GLFW_KEY_ESCAPE);
                stoppedSchool = state.camera.feet;
            }
            if (phaseFrames == 5) {
                check(state.paused && state.camera.feet == stoppedSchool && !state.room.escaped(),
                      "pause stops walking through the school exit");
                press(GLFW_KEY_ESCAPE);
            }
        } else if (schoolVisits == 2) {
            walking = false;
            if (phaseFrames == 1) {
                check(state.room.escaped(), "F1 preserves completed school state");
                check(state.camera.movement == glm::vec3(0) && state.camera.focusTime == 0,
                      "school handoff cleared movement and camera focus");
            }
            if (phaseFrames == 6) {
                check(militaryVisits == 1, "completed school revisit does not immediately transition again");
                press(GLFW_KEY_F2);
            }
        }
    } else if (phase == Phase::military) {
        auto& state = sceneFor<MilitaryRoom>(testWindow);
        if (militaryVisits == 1) {
            if (phaseFrames == 1) {
                check(schoolVisits == 1, "walking out of school reaches the military room without F2");
                check(glm::distance(state.player.eyePosition(), MilitaryRoom::spawnEye) < .001f,
                      "school exit arrives at the barracks spawn");
                check(glm::dot(state.player.viewDirection(), Player{MilitaryRoom::spawnEye, MilitaryRoom::spawnYaw}.viewDirection()) > .999f,
                      "arrival faces the intended barracks direction");
                check(!state.pause->isPaused() && state.initialized, "arrival uses initialized military input and pause state");
            }
            if (phaseFrames == 6) {
                check(glm::distance(state.player.eyePosition(), MilitaryRoom::spawnEye) > .05f,
                      "held movement continues in the military room");
                press(GLFW_KEY_ESCAPE);
                stoppedMilitary = state.player.eyePosition();
            }
            if (phaseFrames == 10) {
                check(state.pause->isPaused() && state.player.eyePosition() == stoppedMilitary,
                      "shared pause still freezes the destination room");
                walking = false;
                press(GLFW_KEY_ESCAPE);
            }
            if (phaseFrames == 12) {
                school::Renderer capture;
                check(capture.saveFrame((output / "military-arrival.ppm").string(), 1280, 800),
                      "capture the military room after automatic arrival");
                lastMilitary = state.player.eyePosition();
                press(GLFW_KEY_F1);
            }
        } else if (militaryVisits == 2) {
            check(glm::distance(state.player.eyePosition(), lastMilitary) < .001f,
                  "manual F2 revisits the existing military position");
            glfwSetWindowShouldClose(testWindow, GLFW_TRUE);
        }
    }
}

int main(int argc, char** argv) {
    output = argc > 1 ? argv[1] : "build/school-transition-tests";
    std::filesystem::create_directories(output);
    if (!glfwInit()) return 2;
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    testWindow = glfwCreateWindow(1280, 800, "School transition checks", nullptr, nullptr);
    if (!testWindow) return 3;
    glfwMakeContextCurrent(testWindow);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) return 4;
    while (glGetError() != GL_NO_ERROR) {}
    glfwSwapInterval(0);
    check(runGame(testWindow, Map::School) == 0, "actual team game loop exits successfully");
    check(schoolVisits == 2 && militaryVisits == 2, "automatic exit and both manual revisits complete");
    check(glfwGetWindowUserPointer(testWindow) == nullptr, "destination loop clears its input context");
    check(glGetError() == GL_NO_ERROR, "team transition renders without OpenGL errors");
    glfwSetWindowShouldClose(testWindow, GLFW_FALSE);
    frames = phaseFrames = 0;
    walking = false;
    standalone = true;
    {
        school::PreviewState preview;
        check(school::runPreview(testWindow, preview) == 0 && phaseFrames == 35,
              "standalone completion does not return into an unavailable destination");
    }
    check(glGetError() == GL_NO_ERROR, "standalone completion renders without OpenGL errors");
    glfwDestroyWindow(testWindow);
    glfwTerminate();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
