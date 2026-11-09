#include <GL/glew.h>
#include <GL/glfw3.h>
#include <GL/glm/gtc/matrix_transform.hpp>
#include <GL/glm/gtc/type_ptr.hpp>

#include "core/Player.h"

#include <algorithm>
#include <iostream>

namespace {
constexpr float floorHalfExtent = 20.0f;
constexpr float floorVertices[] = {
    -floorHalfExtent, 0.0f, -floorHalfExtent,
    -floorHalfExtent, 0.0f,  floorHalfExtent,
     floorHalfExtent, 0.0f,  floorHalfExtent,
    -floorHalfExtent, 0.0f, -floorHalfExtent,
     floorHalfExtent, 0.0f,  floorHalfExtent,
     floorHalfExtent, 0.0f, -floorHalfExtent,
};

constexpr char vertexShader[] = R"(#version 330 core
layout (location = 0) in vec3 position;
uniform mat4 viewProjection;
out vec2 floorPosition;
void main() {
    floorPosition = position.xz;
    gl_Position = viewProjection * vec4(position, 1.0);
}
)";

constexpr char fragmentShader[] = R"(#version 330 core
in vec2 floorPosition;
uniform float floorHalfExtent;
out vec4 pixel;

float grid(float spacing, float width) {
    vec2 distanceToLine = abs(fract(floorPosition / spacing + 0.5) - 0.5) * spacing;
    vec2 aa = max(fwidth(floorPosition), vec2(0.0001));
    vec2 lines = 1.0 - smoothstep(vec2(width), vec2(width) + aa, distanceToLine);
    return max(lines.x, lines.y);
}

void main() {
    vec3 color = mix(vec3(0.22, 0.29, 0.33), vec3(0.36, 0.45, 0.49), grid(1.0, 0.012));
    color = mix(color, vec3(0.44, 0.57, 0.61), grid(5.0, 0.026));
    float edge = floorHalfExtent - max(abs(floorPosition.x), abs(floorPosition.y));
    color = mix(vec3(0.87, 0.64, 0.29), color, smoothstep(0.18, 0.23, edge));
    pixel = vec4(color, 1.0);
}
)";

struct InputState {
    Player player{floorHalfExtent};
    bool hasCursorSample = false;
    double lastX = 0.0;
    double lastY = 0.0;
};

void cursorMoved(GLFWwindow* window, double x, double y) {
    auto& input = *static_cast<InputState*>(glfwGetWindowUserPointer(window));
    if (input.hasCursorSample) {
        input.player.look(static_cast<float>(x - input.lastX), static_cast<float>(y - input.lastY));
    }
    input.lastX = x;
    input.lastY = y;
    input.hasCursorSample = true;
}

void focusChanged(GLFWwindow* window, int) {
    auto& input = *static_cast<InputState*>(glfwGetWindowUserPointer(window));
    input.hasCursorSample = false;
}

void keyPressed(GLFWwindow* window, int key, int, int action, int) {
    if (action != GLFW_PRESS) {
        return;
    }
    if (key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    if (key == GLFW_KEY_SPACE) {
        auto& input = *static_cast<InputState*>(glfwGetWindowUserPointer(window));
        input.player.jump();
    }
}

GLuint compileShader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        char message[1024]{};
        glGetShaderInfoLog(shader, sizeof(message), nullptr, message);
        std::cerr << message << '\n';
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint makeProgram() {
    const GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexShader);
    const GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentShader);
    GLuint program = 0;
    if (vertex && fragment) {
        program = glCreateProgram();
        glAttachShader(program, vertex);
        glAttachShader(program, fragment);
        glLinkProgram(program);
        GLint linked = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (linked != GL_TRUE) {
            char message[1024]{};
            glGetProgramInfoLog(program, sizeof(message), nullptr, message);
            std::cerr << message << '\n';
            glDeleteProgram(program);
            program = 0;
        }
    }
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return program;
}

int run(GLFWwindow* window) {
    const GLuint program = makeProgram();
    if (!program) {
        return 1;
    }

    GLuint vertexArray = 0;
    GLuint vertexBuffer = 0;
    glGenVertexArrays(1, &vertexArray);
    glGenBuffers(1, &vertexBuffer);
    glBindVertexArray(vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(floorVertices), floorVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glUseProgram(program);
    const GLint viewProjectionLocation = glGetUniformLocation(program, "viewProjection");
    glUniform1f(glGetUniformLocation(program, "floorHalfExtent"), floorHalfExtent);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.12f, 0.19f, 0.25f, 1.0f);

    InputState input;
    glfwSetWindowUserPointer(window, &input);
    glfwSetKeyCallback(window, keyPressed);
    glfwSetCursorPosCallback(window, cursorMoved);
    glfwSetWindowFocusCallback(window, focusChanged);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    double previous = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        const double now = glfwGetTime();
        const float elapsed = static_cast<float>(std::min(now - previous, 0.05));
        previous = now;
        const int forward = glfwGetKey(window, GLFW_KEY_W) - glfwGetKey(window, GLFW_KEY_S);
        const int right = glfwGetKey(window, GLFW_KEY_D) - glfwGetKey(window, GLFW_KEY_A);
        input.player.update(static_cast<float>(forward), static_cast<float>(right), elapsed);

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        if (width > 0 && height > 0) {
            glViewport(0, 0, width, height);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            const glm::mat4 projection = glm::perspective(
                glm::radians(68.0f), static_cast<float>(width) / static_cast<float>(height), 0.05f, 100.0f);
            const glm::mat4 viewProjection = projection * input.player.viewMatrix();
            glUniformMatrix4fv(viewProjectionLocation, 1, GL_FALSE, glm::value_ptr(viewProjection));
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glfwSwapBuffers(window);
        }
    }

    glfwSetWindowUserPointer(window, nullptr);
    glDeleteBuffers(1, &vertexBuffer);
    glDeleteVertexArrays(1, &vertexArray);
    glDeleteProgram(program);
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
        1024, 640, "Out of Scale | WASD Move - Mouse Look - Space Jump - Esc Exit", nullptr, nullptr);
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
