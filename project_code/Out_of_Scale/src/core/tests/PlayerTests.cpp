// Standalone deterministic input test. Link with Player.cpp, not the GLFW library.
#include "../Player.h"
#include <GL/glfw3.h>
#include <algorithm>
#include <cmath>
#include <iostream>

struct GLFWwindow {
    bool focused = true;
    bool forward = false, backward = false, right = false;
};
extern "C" int glfwGetWindowAttrib(GLFWwindow* window, int attribute) {
    return attribute == GLFW_FOCUSED && window->focused ? GLFW_TRUE : GLFW_FALSE;
}
extern "C" int glfwGetKey(GLFWwindow* window, int key) {
    const bool pressed = key == GLFW_KEY_W ? window->forward :
                         key == GLFW_KEY_S ? window->backward :
                         key == GLFW_KEY_D ? window->right : false;
    return pressed ? GLFW_PRESS : GLFW_RELEASE;
}

namespace {
int checks = 0, failures = 0;
void check(bool result, const char* label) {
    ++checks;
    if (!result) {
        ++failures;
        std::cerr << "FAIL: " << label << '\n';
    }
}
void tick(Player& player, GLFWwindow& input, int frames, float dt) {
    for (int i = 0; i < frames; ++i)
        player.update(&input, dt, 100.f);
}
float horizontalDistance(glm::vec3 a, glm::vec3 b) {
    return glm::length(glm::vec2(a.x - b.x, a.z - b.z));
}
}

int main() {
    glm::vec3 endpoints[4];
    int index = 0;
    for (int fps : {20, 30, 60, 144}) {
        Player player;
        GLFWwindow input;
        input.forward = true;
        const float dt = 1.f / fps;
        const auto spawn = player.eyePosition();
        player.update(&input, dt, 100.f);
        const float firstStep = horizontalDistance(spawn, player.eyePosition());
        check(firstStep > 0 && firstStep < 4.f * dt, "walking starts without an instant speed jump");
        tick(player, input, fps - 1, dt);
        const auto before = player.eyePosition();
        player.update(&input, dt, 100.f);
        check(std::abs(horizontalDistance(before, player.eyePosition()) / dt - 4.f) < .001f,
              "walking keeps the original four metre per second top speed");
        input.forward = false;
        const auto released = player.eyePosition();
        tick(player, input, fps, dt);
        check(horizontalDistance(released, player.eyePosition()) < .096f,
              "release stops within 9.6 cm at every tested frame rate");
        const auto stopped = player.eyePosition();
        tick(player, input, fps, dt);
        check(player.eyePosition() == stopped, "released movement has no lingering drift");
        // Remove the extra full-speed frame used for the speed measurement.
        endpoints[index++] = player.eyePosition() + glm::vec3(0, 0, 4.f * dt);
    }
    for (int i = 1; i < 4; ++i)
        check(glm::distance(endpoints[0], endpoints[i]) < .0001f,
              "travel differs by under 0.1 mm across 20/30/60/144 FPS");

    GLFWwindow input;
    input.forward = true;
    Player straight, diagonal;
    tick(straight, input, 60, 1.f / 60);
    input.right = true;
    tick(diagonal, input, 60, 1.f / 60);
    const glm::vec3 spawn{0, Player::eyeHeight, 0};
    check(std::abs(horizontalDistance(spawn, straight.eyePosition()) -
                   horizontalDistance(spawn, diagonal.eyePosition())) < .0001f,
          "diagonal walking cannot exceed straight walking speed");

    Player wall;
    for (int i = 0; i < 60; ++i) {
        wall.update(&input, 1.f / 60, 100.f, true);
        auto resolved = wall.eyePosition();
        resolved.x = std::min(resolved.x, .15f);
        resolved.y = Player::eyeHeight;
        wall.setRoomPosition(resolved, true);
    }
    check(wall.eyePosition().x == .15f && wall.eyePosition().z < -2.5f,
          "room collision removes inward velocity while allowing wall sliding");
    input = {};
    const float blockedX = wall.eyePosition().x;
    wall.update(&input, 1.f / 60, 100.f);
    check(wall.eyePosition().x == blockedX, "release at a wall cannot leak rejected velocity");

    input.forward = true;
    tick(straight, input, 60, 1.f / 60);
    straight.resetMouseSample();
    input.forward = false;
    const auto pausedAt = straight.eyePosition();
    tick(straight, input, 60, 1.f / 60);
    check(straight.eyePosition() == pausedAt, "map re-entry and pause reset stale movement");

    input.forward = true;
    tick(straight, input, 60, 1.f / 60);
    input.focused = false;
    const auto unfocusedAt = straight.eyePosition();
    tick(straight, input, 60, 1.f / 60);
    input.focused = true;
    input.forward = false;
    tick(straight, input, 60, 1.f / 60);
    check(straight.eyePosition() == unfocusedAt, "focus loss clears motion without resuming drift");

    Player look;
    look.look(100, 100);
    const auto facing = look.viewDirection();
    look.look(200, 100);
    check(glm::distance(facing, look.viewDirection()) > .2f, "mouse look stays immediate");
    const auto turned = look.viewDirection();
    look.resetMouseSample();
    look.look(900, 700);
    check(glm::distance(turned, look.viewDirection()) < .0001f, "focus return discards stale mouse delta");

    Player jump;
    const auto groundedAt = jump.eyePosition();
    jump.jump();
    jump.update(&input, 0.f, 100.f);
    check(jump.eyePosition() == groundedAt, "zero delta cannot move an active jump");
    tick(jump, input, 120, 1.f / 120);
    check(jump.eyePosition().y == Player::eyeHeight, "unchanged jump returns exactly to the floor");

    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
