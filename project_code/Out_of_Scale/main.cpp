#include "src/core/Player.h"
#include "../../rooms/test/TestRoom.h"

#include <GL/glew.h>
#include <GL/glfw3.h>
#include <GL/glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <iostream>

namespace {
Player& playerFor(GLFWwindow* window) {
    return *static_cast<Player*>(glfwGetWindowUserPointer(window));
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

void drawFrame(const Player& player, const TestRoom& room, int width, int height) {
    glViewport(0, 0, width, height);
    glClearColor(0.12f, 0.18f, 0.25f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    const glm::mat4 projection = glm::perspective(
        glm::radians(68.0f), static_cast<float>(width) / static_cast<float>(height), 0.05f, 100.0f);
    room.draw(projection * player.view());
    drawAimDot(width, height);
}

int run(GLFWwindow* window) {
    TestRoom room;
    if (!room.initialize()) {
        return 1;
    }
    glEnable(GL_DEPTH_TEST);

    Player player;
    glfwSetWindowUserPointer(window, &player);
    glfwSetCursorPosCallback(window, [](GLFWwindow* target, double x, double y) {
        playerFor(target).look(x, y);
    });
    glfwSetWindowFocusCallback(window, [](GLFWwindow* target, int) {
        playerFor(target).resetMouseSample();
    });
    glfwSetKeyCallback(window, [](GLFWwindow* target, int key, int, int action, int) {
        if (key == GLFW_KEY_SPACE && action == GLFW_PRESS &&
            glfwGetWindowAttrib(target, GLFW_FOCUSED) == GLFW_TRUE) {
            playerFor(target).jump();
        }
    });
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    double previous = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            break;
        }

        const double now = glfwGetTime();
        const float elapsed = static_cast<float>(std::min(now - previous, 0.05));
        previous = now;
        player.update(window, elapsed, TestRoom::halfSize);

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        if (width > 0 && height > 0) {
            drawFrame(player, room, width, height);
            glfwSwapBuffers(window);
        } else {
            glfwWaitEventsTimeout(0.05);
        }
    }

    glfwSetCursorPosCallback(window, nullptr);
    glfwSetWindowFocusCallback(window, nullptr);
    glfwSetKeyCallback(window, nullptr);
    glfwSetWindowUserPointer(window, nullptr);
    return 0;
}
}

int main() {
    glfwSetErrorCallback([](int, const char* message) { std::cerr << message << '\n'; });
    if (!glfwInit()) {
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(
        1024, 640, "Out of Scale | WASD Move | Mouse Look | Space Jump | Esc Exit", nullptr, nullptr);
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
        result = run(window);
    } else {
        std::cerr << glewGetErrorString(status) << '\n';
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}
